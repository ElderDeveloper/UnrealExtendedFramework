// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimNode_TurnInPlace.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "EGTurnInPlaceComponent.h"
#include "EGTurnInPlaceSet.h"
#include "EGTurnInPlaceSolver.h"
#include "Misc/MemStack.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"

namespace
{
	/** Sampling a clip exactly at its end can wrap to its first frame (the players loop). */
	constexpr double EndMargin = 1.e-3;

	float SmoothStep01(float Alpha)
	{
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}
}

// ---------------------------------------------------------------------------------------------
// Game thread
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const AActor* const Owner = InAnimInstance ? InAnimInstance->GetOwningActor() : nullptr;
	if (Owner != CachedOwner.Get() || !CachedComponent.IsValid())
	{
		CachedOwner = Owner;
		CachedComponent = UEGTurnInPlaceComponent::FindFor(Owner);
	}

	const UEGTurnInPlaceComponent* const Component = CachedComponent.Get();
	Input = Component ? Component->MakeGraphInput() : FEGTurnInPlaceGraphInput();

	if (!PendingWarning.IsEmpty())
	{
		EGAnimDebug::WarnOnce(InAnimInstance ? static_cast<const UObject*>(InAnimInstance->GetClass()) : nullptr, PendingWarning);
		PendingWarning.Reset();
	}
}

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::ResetState()
{
	Node = FEGTurnInPlaceNodeState();
	Phase = EPhase::Idle;
	bTurnedThisEntry = false;
	CurrentPlayer = INDEX_NONE;
	OutputCurves = FEGTurnInPlaceCurves();
	for (int32 Index = 0; Index < 1 + NumPlayers; ++Index)
	{
		Weights[Index] = FromWeights[Index] = Index == 0 ? 1.0f : 0.0f;
	}
	BlendTarget = 0;
	BlendDuration = 0.0f;
	BlendElapsed = 0.0f;
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		GetPlayerSequence(Index) = nullptr;
		GetPlayer(Index).SetSequence(nullptr);
	}
}

void FAnimNode_EGTurnInPlace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	Source.Initialize(Context);
	ResetState();
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		GetPlayer(Index).Initialize_AnyThread(Context);
	}
}

void FAnimNode_EGTurnInPlace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		GetPlayer(Index).CacheBones_AnyThread(Context);
	}
}

// ---------------------------------------------------------------------------------------------
// Blending
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::BlendTo(int32 Target, float BlendTime)
{
	for (int32 Index = 0; Index < 1 + NumPlayers; ++Index)
	{
		FromWeights[Index] = Weights[Index];
	}
	BlendTarget = Target;
	BlendDuration = FMath::Max(BlendTime, 0.0f);
	BlendElapsed = 0.0f;
	UpdateBlend(0.0f);
}

void FAnimNode_EGTurnInPlace::UpdateBlend(float DeltaTime)
{
	BlendElapsed += DeltaTime;
	const float Alpha = BlendDuration > UE_KINDA_SMALL_NUMBER ? SmoothStep01(FMath::Clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f)) : 1.0f;

	// The target rises to 1; every other source falls in proportion, so the sum stays 1.
	for (int32 Index = 0; Index < 1 + NumPlayers; ++Index)
	{
		Weights[Index] = Index == BlendTarget
			? FMath::Lerp(FromWeights[Index], 1.0f, Alpha)
			: FromWeights[Index] * (1.0f - Alpha);
	}
}

void FAnimNode_EGTurnInPlace::SetPlayerTime(int32 Index, double Time)
{
	const UAnimSequenceBase* const Sequence = GetPlayerSequence(Index);
	const double Length = Sequence ? static_cast<double>(Sequence->GetPlayLength()) : 0.0;
	const double Clamped = FMath::Clamp(Time, 0.0, FMath::Max(0.0, Length - EndMargin));
	GetPlayer(Index).SetExplicitTime(static_cast<float>(Clamped));
}

// ---------------------------------------------------------------------------------------------
// Turns
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::StartTurn(const FAnimationUpdateContext& Context, const FEGTurnInPlaceGraphInput& FrameInput)
{
	FEGTurnInPlaceSolver::BeginTurn(Node, FrameInput);
	Phase = EPhase::Turn;
	bTurnedThisEntry = false;

	UAnimSequenceBase* const Sequence = FEGTurnInPlaceSolver::GetTurnAnimation(FrameInput.GetEntry(), Node, false);
	if (!Sequence)
	{
		// Procedural: the source pose stays, the capsule turns on the computed curves.
		CurrentPlayer = INDEX_NONE;
		BlendTo(0, TurnBlendTime);
		return;
	}

	// The free player, or the quieter one when both still have weight (three turns inside a blend).
	int32 Slot = 0;
	if (CurrentPlayer != INDEX_NONE)
	{
		Slot = CurrentPlayer == 0 ? 1 : 0;
	}
	else if (Weights[1] > Weights[2])
	{
		Slot = 1;
	}

	// Whatever the slot still showed hands its weight to the source pose.
	Weights[0] += Weights[1 + Slot];
	Weights[1 + Slot] = 0.0f;

	GetPlayerSequence(Slot) = Sequence;
	FAnimNode_SequenceEvaluator_Standalone& Player = GetPlayer(Slot);
	Player.SetSequence(Sequence);
	Player.SetTeleportToExplicitTime(false);
	Player.SetReinitializationBehavior(ESequenceEvalReinit::ExplicitTime);
	SetPlayerTime(Slot, 0.0);

	// Restart it at the turn's start without firing the notifies in between.
	const FAnimationInitializeContext InitContext(Context.AnimInstanceProxy, Context.SharedContext);
	Player.Initialize_AnyThread(InitContext);

	CurrentPlayer = Slot;
	BlendTo(1 + Slot, TurnBlendTime);
}

void FAnimNode_EGTurnInPlace::EndTurn(float BlendTime)
{
	Phase = EPhase::Idle;
	Node.PlayRate = 1.0f;
	Node.bReachedMaxAngle = false;
	Node.bReachedAnimEnd = false;
	Node.bProcedural = false;
	bTurnedThisEntry = false;
	CurrentPlayer = INDEX_NONE;
	BlendTo(0, BlendTime);
}

// ---------------------------------------------------------------------------------------------
// Curves
// ---------------------------------------------------------------------------------------------

bool FAnimNode_EGTurnInPlace::SampleRootYaw(const UAnimSequenceBase* Sequence, const FBoneContainer& RequiredBones, TArray<double>& OutTimes, TArray<double>& OutYaws) const
{
	const double Length = Sequence ? static_cast<double>(Sequence->GetPlayLength()) : 0.0;
	if (Length <= 0.0 || !RequiredBones.IsValid())
	{
		return false;
	}

	FMemMark Mark(FMemStack::Get());
	FCompactPose Pose;
	Pose.SetBoneContainer(&RequiredBones);
	FBlendedCurve Curve;
	Curve.InitFrom(RequiredBones);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData PoseData(Pose, Curve, Attributes);

	// Without extracting root motion the root keeps its track (unless the clip forces a root lock).
	const int32 Samples = FMath::Max(2, FMath::CeilToInt32(static_cast<float>(Length) * RootYawSampleRate) + 1);
	OutTimes.Reset(Samples);
	OutYaws.Reset(Samples);
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Time = Index == Samples - 1 ? Length : FMath::Min(static_cast<double>(Index) / RootYawSampleRate, Length);
		Sequence->GetAnimationPose(PoseData, FAnimExtractContext(Time));
		OutTimes.Add(Time);
		OutYaws.Add(Pose[FCompactPoseBoneIndex(0)].GetRotation().Rotator().Yaw);
	}
	return true;
}

const FAnimNode_EGTurnInPlace::FClipInfo& FAnimNode_EGTurnInPlace::FindOrBuildClipInfo(const FAnimInstanceProxy* Proxy, const UAnimSequenceBase* Sequence)
{
	const TObjectKey<UAnimSequenceBase> Key(Sequence);
	if (const FClipInfo* const Existing = ClipInfos.Find(Key))
	{
		return *Existing;
	}

	FClipInfo& Info = ClipInfos.Add(Key);
	Info.bHasCurves = UEGTurnInPlaceSet::HasTurnCurves(Sequence, Input.CurveNames);
	if (Info.bHasCurves || !Proxy)
	{
		return Info;
	}

	TArray<double> Times;
	TArray<double> Yaws;
	if (SampleRootYaw(Sequence, Proxy->GetRequiredBones(), Times, Yaws)
		&& FEGTurnInPlaceSolver::BuildYawTrack(Times, Yaws, RootYawFinishedTolerance, RootYawRecoveryTime, Info.Track)
		&& FMath::Abs(Info.Track.TotalYaw) > FMath::Max(RootYawFinishedTolerance, 1.0f))
	{
		return Info;
	}

	// Neither curves nor a turning root: spread the step's yaw over the clip, so the turn still ends.
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();
	const float StepYaw = Entry.StepSizes.IsValidIndex(Node.StepIndex) ? static_cast<float>(Entry.StepSizes[Node.StepIndex]) : 90.0f;
	const float Length = FMath::Max(Sequence->GetPlayLength(), UE_KINDA_SMALL_NUMBER);
	const float End = FMath::Clamp(Length - RootYawRecoveryTime, Length * 0.5f, Length);
	const float Total = Node.bTurningRight ? StepYaw : -StepYaw;

	Info.bApproximated = true;
	Info.Track = FEGTurnYawTrack();
	Info.Track.Times = { 0.0f, End, Length };
	Info.Track.CumulativeYaw = { 0.0f, Total, Total };
	Info.Track.Length = Length;
	Info.Track.TotalYaw = Total;
	Info.Track.WeightEndTime = End;

	PendingWarning = FString::Printf(TEXT("Turn In Place node: %s has no turn curves and its root does not turn (Force Root Lock on, or no root motion). Its turn is approximated from the step size; turn Force Root Lock off (the node locks the root itself) or run the Turn In Place Curves modifier on it."),
		*GetNameSafe(Sequence));
	return Info;
}

FEGTurnInPlaceCurves FAnimNode_EGTurnInPlace::EvaluateTurnCurves(const FAnimInstanceProxy* Proxy, bool bRecovery)
{
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();
	if (Node.bProcedural)
	{
		return FEGTurnInPlaceSolver::EvaluateProceduralTurn(Node.ProceduralYaw, Node.AnimTime, Entry.Procedural);
	}

	const UAnimSequenceBase* const Sequence = FEGTurnInPlaceSolver::GetTurnAnimation(Entry, Node, bRecovery);
	if (!Sequence)
	{
		return FEGTurnInPlaceCurves();
	}

	const FClipInfo& Clip = FindOrBuildClipInfo(Proxy, Sequence);
	if (Clip.bHasCurves)
	{
		return FEGTurnInPlaceSolver::EvaluateTurnCurves(Entry, Node, bRecovery, Input.CurveNames);
	}

	FEGTurnInPlaceCurves Curves = FEGTurnInPlaceSolver::EvaluateYawTrack(Clip.Track, Node.AnimTime);
	const FAnimExtractContext Context(Node.AnimTime);
	Curves.PauseTurnInPlace = Sequence->EvaluateCurveData(Input.CurveNames.PauseTurnInPlace, Context, false);
	Curves.LockTurnInPlace = Sequence->EvaluateCurveData(Input.CurveNames.LockTurnInPlace, Context, false);
	return Curves;
}

// ---------------------------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	const float DeltaTime = Context.GetDeltaTime();
	const FAnimInstanceProxy* const Proxy = Context.AnimInstanceProxy;
	const FEGTurnInPlaceEntry& Entry = Input.GetEntry();

	// Decide on the curves at the time last evaluated: what the component consumed this frame.
	FEGTurnInPlaceGraphInput FrameInput = Input;
	FrameInput.bIsTurning = Phase != EPhase::Idle && EvaluateTurnCurves(Proxy, Phase == EPhase::Recovery).IsTurning();
	if (Phase == EPhase::Turn && FrameInput.bIsTurning)
	{
		bTurnedThisEntry = true;
	}

	FEGTurnInPlaceGraphOutput Output;
	FEGTurnInPlaceSolver::ComputeGraphOutput(FrameInput, bTurnedThisEntry, Node.bReachedAnimEnd, Output);

	// Locomotion takes over once the character moves; a Pseudo server plays nothing.
	const bool bStop = !Input.bValid || Input.bPseudo || Input.bIsMoving;

	switch (Phase)
	{
	case EPhase::Idle:
		if (!bStop && Output.bWantsToTurn)
		{
			StartTurn(Context, FrameInput);
		}
		break;

	case EPhase::Turn:
		if (bStop || Output.bAbortTurn)
		{
			EndTurn(AbortBlendTime);
		}
		else if (Output.bWantsRecovery)
		{
			// The recovery is the rest of the same clip at 1x: replaying it would turn twice.
			Phase = EPhase::Recovery;
			Node.bRecoveryTurningRight = Node.bTurningRight;
			Node.bReachedAnimEnd = false;
			Node.bRecoveryFinished = false;
			bTurnedThisEntry = false;
		}
		else
		{
			Node.bReachedAnimEnd = FEGTurnInPlaceSolver::AdvanceTurnTime(Entry, Node, DeltaTime, Node.PlayRate, false);
			FEGTurnInPlaceSolver::UpdateNodePlayRate(Node, FrameInput);
		}
		break;

	case EPhase::Recovery:
		if (bStop || Output.bAbortTurn)
		{
			EndTurn(AbortBlendTime);
		}
		else if (Output.bWantsToTurn)
		{
			StartTurn(Context, FrameInput);
		}
		else
		{
			Node.bRecoveryFinished = FEGTurnInPlaceSolver::AdvanceTurnTime(Entry, Node, DeltaTime, 1.0f, true);
			if (Node.bRecoveryFinished)
			{
				EndTurn(RecoveryBlendTime);
			}
		}
		break;
	}

	UpdateBlend(DeltaTime);

	// The curves at the time about to be evaluated, written into the pose in Evaluate.
	OutputCurves = Phase == EPhase::Idle ? FEGTurnInPlaceCurves() : EvaluateTurnCurves(Proxy, Phase == EPhase::Recovery);

	if (CurrentPlayer != INDEX_NONE)
	{
		SetPlayerTime(CurrentPlayer, Node.AnimTime);
	}

	// The source keeps updating at any weight, so a locomotion state machine behind it stays current.
	Source.Update(Context.FractionalWeight(Weights[0]));
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		if (GetPlayerSequence(Index) && Weights[1 + Index] > ZERO_ANIMWEIGHT_THRESH)
		{
			GetPlayer(Index).Update_AnyThread(Context.FractionalWeight(Weights[1 + Index]));
		}
		else if (Weights[1 + Index] <= ZERO_ANIMWEIGHT_THRESH && Index != CurrentPlayer)
		{
			// Faded out: let the clip go.
			GetPlayerSequence(Index) = nullptr;
			GetPlayer(Index).SetSequence(nullptr);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGTurnInPlace::Evaluate_AnyThread(FPoseContext& Output)
{
	struct FPart
	{
		int32 Source = 0;
		float Weight = 0.0f;
	};

	// A player with no clip left shows the source pose.
	float SourceWeight = Weights[0];
	TArray<FPart, TInlineAllocator<1 + NumPlayers>> Parts;
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		const float Weight = Weights[1 + Index];
		if (Weight <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}
		if (GetPlayerSequence(Index))
		{
			Parts.Add({ 1 + Index, Weight });
		}
		else
		{
			SourceWeight += Weight;
		}
	}
	if (SourceWeight > ZERO_ANIMWEIGHT_THRESH || Parts.Num() == 0)
	{
		Parts.Insert({ 0, FMath::Max(SourceWeight, ZERO_ANIMWEIGHT_THRESH) }, 0);
	}

	auto EvaluatePart = [this](const FPart& Part, FPoseContext& Pose)
	{
		if (Part.Source == 0)
		{
			Source.Evaluate(Pose);
			return;
		}
		GetPlayer(Part.Source - 1).Evaluate_AnyThread(Pose);
		if (bLockRootBone)
		{
			// The capsule turns; the root must not turn the mesh as well.
			const FCompactPoseBoneIndex Root(0);
			Pose.Pose[Root] = Pose.Pose.GetRefPose(Root);
		}
	};

	EvaluatePart(Parts[0], Output);
	float Accumulated = Parts[0].Weight;
	for (int32 Index = 1; Index < Parts.Num(); ++Index)
	{
		FPoseContext Other(Output);
		EvaluatePart(Parts[Index], Other);

		const float Total = Accumulated + Parts[Index].Weight;
		FPoseContext Blended(Output);
		const FAnimationPoseData First(Output);
		const FAnimationPoseData Second(Other);
		FAnimationPoseData BlendedData(Blended);
		FAnimationRuntime::BlendTwoPosesTogether(First, Second, Accumulated / Total, BlendedData);

		Output.Pose.MoveBonesFrom(Blended.Pose);
		Output.Curve.MoveFrom(Blended.Curve);
		Output.CustomAttributes.MoveFrom(Blended.CustomAttributes);
		Accumulated = Total;
	}

	// Unblended: the component consumes the turn exactly as the clip turns.
	if (Input.bValid)
	{
		Output.Curve.Set(Input.CurveNames.RemainingTurnYaw, OutputCurves.RemainingTurnYaw);
		Output.Curve.Set(Input.CurveNames.TurnYawWeight, OutputCurves.TurnYawWeight);
	}
}

void FAnimNode_EGTurnInPlace::GatherDebugData(FNodeDebugData& DebugData)
{
	static const TCHAR* const PhaseNames[] = { TEXT("Idle"), TEXT("Turn"), TEXT("Recovery") };
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(%s%s, offset %.1f, time %.2f, remaining %.1f, weight %.0f)"),
		PhaseNames[static_cast<int32>(Phase)], Node.bProcedural ? TEXT(" procedural") : TEXT(""),
		Input.TurnOffset, Node.AnimTime, OutputCurves.RemainingTurnYaw, OutputCurves.TurnYawWeight);
	DebugData.AddDebugItem(DebugLine);

	Source.GatherDebugData(DebugData.BranchFlow(Weights[0]));
	for (int32 Index = 0; Index < NumPlayers; ++Index)
	{
		if (GetPlayerSequence(Index) && Weights[1 + Index] > ZERO_ANIMWEIGHT_THRESH)
		{
			GetPlayer(Index).GatherDebugData(DebugData.BranchFlow(Weights[1 + Index]));
		}
	}
}

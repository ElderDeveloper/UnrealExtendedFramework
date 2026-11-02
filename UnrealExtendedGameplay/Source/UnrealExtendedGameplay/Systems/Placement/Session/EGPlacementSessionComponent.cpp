// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementSessionComponent.h"

#include "Assets/EFAssetLoading.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Messages/EFMessageSubsystem.h"
#include "Misc/App.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementDebug.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementPreviewRenderer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementCommitHandler.h"

#define LOCTEXT_NAMESPACE "EGPlacementSession"

UEGPlacementSessionComponent::UEGPlacementSessionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After the camera has moved this frame, so the ghost never trails the view.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

bool UEGPlacementSessionComponent::BeginSession(const FEGPlacementSelection& InSelection)
{
	if (State != EEGPlacementSessionState::Idle)
	{
		FinishSession(true);
	}
	const TArray<FEGPlacementMode>& Modes = InSelection.Modes.Num() > 0 ? InSelection.Modes : DefaultModes;
	if (Modes.Num() == 0)
	{
		UE_LOG(LogEGPlacement, Warning, TEXT("%s: placement session has no modes; nothing is placed."), *GetPathNameSafe(this));
		return false;
	}

	Selection = InSelection;
	if (!Selection.Source.IsValid())
	{
		Selection.Source = Selection.Spec.Source;
	}
	Selection.Scale = FMath::Max(Selection.Scale, 0.01f);
	ActiveModes = Modes;
	++Generation;
	++FEGPlacementStats::ActiveSessions;
	SpinDegrees = 0.0f;
	ClearModifiers();
	Cache.Reset();
	Cache.SetNum(ActiveModes.Num());
	CurrentEvaluation = FEGPlacementEvaluation();
	CurrentModeIndex = INDEX_NONE;
	bHasPose = false;
	for (const FEGPlacementMode& Mode : ActiveModes)
	{
		if (Mode.Aim)
		{
			Mode.Aim->ResetAim();
		}
	}
	if (CommitHandler)
	{
		CommitHandler->OnSessionBegin(this, Selection);
	}
	SetState(EEGPlacementSessionState::Preparing);

	TArray<FSoftObjectPath> Paths = Selection.Preload;
	Selection.Visual.GetSoftPaths(Paths);
	if (PreviewRenderer)
	{
		PreviewRenderer->GatherPreload(Selection, Paths);
	}
	const bool bAllLoaded = !Paths.ContainsByPredicate([](const FSoftObjectPath& Path) { return Path.IsValid() && !Path.ResolveObject(); });
	if (bAllLoaded)
	{
		StartPreviewing();
		return true;
	}
	const uint32 ForGeneration = Generation;
	TWeakObjectPtr<UEGPlacementSessionComponent> WeakThis(this);
	PreloadHandle = EFAssets::Preload(MoveTemp(Paths), [WeakThis, ForGeneration]()
	{
		UEGPlacementSessionComponent* Session = WeakThis.Get();
		if (Session && Session->Generation == ForGeneration && Session->State == EEGPlacementSessionState::Preparing)
		{
			Session->StartPreviewing();
		}
	});
	return true;
}

void UEGPlacementSessionComponent::StartPreviewing()
{
	if (PreviewRenderer)
	{
		PreviewRenderer->Begin(GetOwner(), Selection);
	}
	SetState(EEGPlacementSessionState::Previewing);
	SetComponentTickEnabled(true);
	Announce(TAG_EGPlacement_Event_SessionStarted, FGameplayTag(), FTransform::Identity);
	UpdatePreview(true);
}

void UEGPlacementSessionComponent::EndSession()
{
	FinishSession(false);
}

void UEGPlacementSessionComponent::FinishSession(bool bCancelled)
{
	if (State == EEGPlacementSessionState::Idle)
	{
		return;
	}
	++Generation;
	PreloadHandle.Reset();
	SetComponentTickEnabled(false);
	if (PreviewRenderer)
	{
		PreviewRenderer->End();
	}
	if (CommitHandler)
	{
		CommitHandler->OnSessionEnd(this, bCancelled);
	}
	ClearModifiers();
	const FGameplayTag ModeTag = GetModeTag();
	const FTransform Pose = CurrentEvaluation.Pose;
	SpinDegrees = 0.0f;
	Cache.Reset();
	CurrentEvaluation = FEGPlacementEvaluation();
	CurrentModeIndex = INDEX_NONE;
	bHasPose = false;
	FEGPlacementStats::ActiveSessions = FMath::Max(0, FEGPlacementStats::ActiveSessions - 1);
	SetState(EEGPlacementSessionState::Idle);

	if (bCancelled)
	{
		Announce(TAG_EGPlacement_Event_Cancelled, ModeTag, Pose);
	}
	Announce(TAG_EGPlacement_Event_SessionEnded, ModeTag, Pose);
	Selection = FEGPlacementSelection();
	ActiveModes.Reset();
	OnSessionEnded.Broadcast(bCancelled);
}

void UEGPlacementSessionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishSession(true);
	Super::EndPlay(EndPlayReason);
}

void UEGPlacementSessionComponent::SetState(EEGPlacementSessionState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged.Broadcast(State);
	}
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

EEGPlacementInputResult UEGPlacementSessionComponent::Confirm()
{
	if (State == EEGPlacementSessionState::Idle)
	{
		return EEGPlacementInputResult::NotHandled;
	}
	if (State != EEGPlacementSessionState::Previewing)
	{
		return EEGPlacementInputResult::Handled;
	}
	if (!bHasPose || !ActiveModes.IsValidIndex(CurrentModeIndex) || !CurrentEvaluation.IsValid())
	{
		OnConfirmRefused.Broadcast(CurrentEvaluation.Reasons);
		return EEGPlacementInputResult::Handled;
	}

	// The cache may be up to RuleResultMaxAge old: commit only a fresh answer.
	const FEGPlacementMode& Mode = ActiveModes[CurrentModeIndex];
	const FEGPlacementEvaluation Fresh = EGPlacement::Evaluate(Mode.Profile, Selection.Spec, CurrentIntent, MakeContext(EEGPlacementScope::Preview));
	if (!Fresh.IsValid())
	{
		CurrentEvaluation = Fresh;
		if (PreviewRenderer)
		{
			PreviewRenderer->Update(CurrentEvaluation, true);
		}
		OnConfirmRefused.Broadcast(Fresh.Reasons);
		return EEGPlacementInputResult::Handled;
	}
	if (!CommitHandler)
	{
		TArray<FEGPlacementReason> Reasons;
		Reasons.Emplace(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoHandler", "The placement session has no commit handler."), this);
		OnConfirmRefused.Broadcast(Reasons);
		return EEGPlacementInputResult::Handled;
	}

	FEGPlacementCommitRequest Request;
	Request.ModeTag = Mode.ModeTag;
	Request.Source = Selection.GetSource();
	Request.Spec = Selection.Spec;
	Request.Evaluation = Fresh;
	Request.Intent = CurrentIntent;
	Request.Proposal = FEGPlacementProposal::FromEvaluation(Fresh, CurrentIntent, Mode.ModeTag, Request.Source);
	Request.Profile = Mode.Profile;
	Request.Requester = MakeContext(EEGPlacementScope::Preview).Requester.Get();
	Request.Session = this;
	Request.ActorClass = Selection.ActorClass;
	Request.MovedActor = Selection.MovedActor;
	Request.ContextData = Selection.ContextData;

	const uint32 ForGeneration = Generation;
	const FGameplayTag ModeTag = Mode.ModeTag;
	SetState(EEGPlacementSessionState::AwaitingOutcome);
	TWeakObjectPtr<UEGPlacementSessionComponent> WeakThis(this);
	const FEGPlacementDispatchResult Dispatch = CommitHandler->Commit(Request, [WeakThis, ForGeneration, ModeTag](const FEGPlacementOutcome& Outcome)
	{
		if (UEGPlacementSessionComponent* Session = WeakThis.Get())
		{
			Session->HandleOutcome(ForGeneration, Outcome, ModeTag);
		}
	});
	if (Generation != ForGeneration)
	{
		// The handler ended or replaced the session from inside Commit.
		return EEGPlacementInputResult::Handled;
	}
	if (Dispatch.Dispatch == EEGPlacementDispatch::Refused)
	{
		SetState(EEGPlacementSessionState::Previewing);
		OnConfirmRefused.Broadcast(Dispatch.Reasons);
		return EEGPlacementInputResult::Handled;
	}

	OnCommitDispatched.Broadcast(Request);
	Announce(TAG_EGPlacement_Event_CommitDispatched, ModeTag, Fresh.Pose);
	if (Generation == ForGeneration && State == EEGPlacementSessionState::AwaitingOutcome && !CommitHandler->ReportsOutcomes())
	{
		// Dispatch is final for this handler; no outcome will come and none is invented.
		if (AfterCommit == EEGPlacementAfterCommit::EndSession)
		{
			FinishSession(false);
		}
		else
		{
			SetState(EEGPlacementSessionState::Previewing);
			bForceFreshNextTick = true;
		}
	}
	return EEGPlacementInputResult::Handled;
}

void UEGPlacementSessionComponent::HandleOutcome(uint32 ForGeneration, const FEGPlacementOutcome& Outcome, FGameplayTag ModeTag)
{
	if (ForGeneration != Generation || State != EEGPlacementSessionState::AwaitingOutcome)
	{
		return;
	}
	OnOutcome.Broadcast(Outcome, ModeTag);
	Announce(TAG_EGPlacement_Event_Outcome, ModeTag, Outcome.AppliedPose, &Outcome, &Outcome.Reasons);
	if (ForGeneration != Generation || State != EEGPlacementSessionState::AwaitingOutcome)
	{
		// A listener ended or replaced the session.
		return;
	}
	if (Outcome.Kind == EEGPlacementOutcomeKind::Applied && AfterCommit == EEGPlacementAfterCommit::EndSession)
	{
		FinishSession(false);
		return;
	}
	SetState(EEGPlacementSessionState::Previewing);
	Cache.Reset();
	Cache.SetNum(ActiveModes.Num());
	UpdatePreview(true);
}

EEGPlacementInputResult UEGPlacementSessionComponent::Cancel()
{
	if (State == EEGPlacementSessionState::Idle)
	{
		return EEGPlacementInputResult::NotHandled;
	}
	FinishSession(true);
	return EEGPlacementInputResult::Handled;
}

EEGPlacementInputResult UEGPlacementSessionComponent::Rotate(float Steps)
{
	if (State == EEGPlacementSessionState::Idle)
	{
		return EEGPlacementInputResult::NotHandled;
	}
	const float Step = Selection.Spec.Rotation.SpinStepDegrees > 0.0f ? Selection.Spec.Rotation.SpinStepDegrees : RotateStepDegrees;
	SpinDegrees = FRotator::NormalizeAxis(SpinDegrees + Steps * Step);
	bForceFreshNextTick = true;
	return EEGPlacementInputResult::Handled;
}

EEGPlacementInputResult UEGPlacementSessionComponent::AdjustDistance(float Delta)
{
	if (State == EEGPlacementSessionState::Idle)
	{
		return EEGPlacementInputResult::NotHandled;
	}
	for (const FEGPlacementMode& Mode : ActiveModes)
	{
		if (Mode.Aim)
		{
			Mode.Aim->AdjustDistance(Delta);
		}
	}
	bForceFreshNextTick = true;
	return EEGPlacementInputResult::Handled;
}

EEGPlacementInputResult UEGPlacementSessionComponent::SetFreePosition(bool bFree)
{
	bFreePosition = bFree;
	bForceFreshNextTick = true;
	return State == EEGPlacementSessionState::Idle ? EEGPlacementInputResult::NotHandled : EEGPlacementInputResult::Handled;
}

EEGPlacementInputResult UEGPlacementSessionComponent::SetFreeRotation(bool bFree)
{
	bFreeRotation = bFree;
	bForceFreshNextTick = true;
	return State == EEGPlacementSessionState::Idle ? EEGPlacementInputResult::NotHandled : EEGPlacementInputResult::Handled;
}

void UEGPlacementSessionComponent::ClearModifiers()
{
	if (bFreePosition || bFreeRotation)
	{
		bForceFreshNextTick = true;
	}
	bFreePosition = false;
	bFreeRotation = false;
}

// ---------------------------------------------------------------------------------------------
// View and context
// ---------------------------------------------------------------------------------------------

void UEGPlacementSessionComponent::SetViewOverride(const FEGPlacementView& View)
{
	ViewOverride = View;
}

void UEGPlacementSessionComponent::ClearViewOverride()
{
	ViewOverride.Reset();
}

bool UEGPlacementSessionComponent::BuildView_Implementation(FEGPlacementView& OutView) const
{
	if (ViewOverride.IsSet())
	{
		OutView = ViewOverride.GetValue();
		return true;
	}
	AActor* Owner = GetOwner();
	APlayerController* Player = Cast<APlayerController>(Owner);
	APawn* Pawn = Cast<APawn>(Owner);
	if (!Player && Pawn)
	{
		Player = Cast<APlayerController>(Pawn->GetController());
	}
	FVector Location;
	FRotator Rotation;
	if (Player)
	{
		Player->GetPlayerViewPoint(Location, Rotation);
		OutView.Requester = Pawn ? Pawn : static_cast<APawn*>(Player->GetPawn());
		OutView.PlayerController = Player;
	}
	else if (Owner)
	{
		Owner->GetActorEyesViewPoint(Location, Rotation);
		OutView.Requester = Owner;
	}
	else
	{
		return false;
	}
	OutView.Origin = Location;
	OutView.Direction = Rotation.Vector();
	return true;
}

FEGPlacementContext UEGPlacementSessionComponent::MakeContext(EEGPlacementScope Scope) const
{
	AActor* Owner = GetOwner();
	AActor* Requester = Owner;
	if (const AController* Controller = Cast<AController>(Owner))
	{
		APawn* ControlledPawn = Controller->GetPawn();
		Requester = ControlledPawn ? static_cast<AActor*>(ControlledPawn) : Owner;
	}
	if (bHasLastView && LastView.Requester.IsValid())
	{
		Requester = LastView.Requester.Get();
	}
	FEGPlacementContext Context = EGPlacement::MakeContext(GetWorld(), Requester, Scope);
	if (Owner && Owner != Requester)
	{
		Context.IgnoredActors.Add(Owner);
	}
	for (AActor* Actor : Selection.IgnoredActors)
	{
		if (Actor)
		{
			Context.IgnoredActors.Add(Actor);
		}
	}
	if (AActor* Moved = Selection.MovedActor.Get())
	{
		Context.IgnoredActors.Add(Moved);
	}
	if (bHasLastView)
	{
		Context.bHasViewLocation = true;
		Context.ViewLocation = LastView.Origin;
	}
	Context.ContextData = Selection.ContextData;
	return Context;
}

FGameplayTag UEGPlacementSessionComponent::GetModeTag() const
{
	return ActiveModes.IsValidIndex(CurrentModeIndex) ? ActiveModes[CurrentModeIndex].ModeTag : FGameplayTag();
}

void UEGPlacementSessionComponent::RefreshPreview()
{
	if (State == EEGPlacementSessionState::Previewing)
	{
		UpdatePreview(true);
	}
}

// ---------------------------------------------------------------------------------------------
// Preview
// ---------------------------------------------------------------------------------------------

void UEGPlacementSessionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bClearModifiersOnFocusLoss)
	{
		const bool bFocused = FApp::HasFocus();
		if (bWasFocused && !bFocused)
		{
			ClearModifiers();
		}
		bWasFocused = bFocused;
	}
	if (State == EEGPlacementSessionState::Previewing)
	{
		const bool bForce = bForceFreshNextTick;
		bForceFreshNextTick = false;
		UpdatePreview(bForce);
	}
}

void UEGPlacementSessionComponent::UpdatePreview(bool bForceFresh)
{
	FEGPlacementView View;
	bHasLastView = BuildView(View);
	if (bHasLastView)
	{
		LastView = View;
	}

	FEGPlacementEvaluation Chosen;
	FEGPlacementIntent ChosenIntent;
	int32 ChosenIndex = INDEX_NONE;
	bool bChosenHasPose = false;

	FEGPlacementEvaluation Fallback;
	FEGPlacementIntent FallbackIntent;
	int32 FallbackIndex = INDEX_NONE;

	if (bHasLastView)
	{
		for (int32 Index = 0; Index < ActiveModes.Num(); ++Index)
		{
			FEGPlacementEvaluation Evaluation;
			FEGPlacementIntent Intent;
			const bool bAimed = EvaluateMode(Index, View, bForceFresh, Evaluation, Intent);
			const bool bFallThrough = ActiveModes[Index].OnInvalid == EEGPlacementModeOnInvalid::FallThrough;
			if (!bAimed)
			{
				if (!bFallThrough)
				{
					ChosenIndex = Index;
					Chosen = Evaluation;
					break;
				}
				continue;
			}
			if (Evaluation.Status == EEGPlacementStatus::Invalid && bFallThrough)
			{
				if (FallbackIndex == INDEX_NONE)
				{
					Fallback = Evaluation;
					FallbackIntent = Intent;
					FallbackIndex = Index;
				}
				continue;
			}
			Chosen = Evaluation;
			ChosenIntent = Intent;
			ChosenIndex = Index;
			bChosenHasPose = true;
			break;
		}
		if (ChosenIndex == INDEX_NONE && FallbackIndex != INDEX_NONE)
		{
			// Every mode passed; show the first refusal that had a pose.
			Chosen = Fallback;
			ChosenIntent = FallbackIntent;
			ChosenIndex = FallbackIndex;
			bChosenHasPose = true;
		}
	}

	const bool bChanged = ChosenIndex != CurrentModeIndex
		|| bChosenHasPose != bHasPose
		|| Chosen.Status != CurrentEvaluation.Status
		|| Chosen.Anchor != CurrentEvaluation.Anchor
		|| Chosen.Support.Component != CurrentEvaluation.Support.Component;

	CurrentEvaluation = Chosen;
	CurrentIntent = ChosenIntent;
	CurrentModeIndex = ChosenIndex;
	bHasPose = bChosenHasPose;

	if (PreviewRenderer)
	{
		PreviewRenderer->Update(CurrentEvaluation, bHasPose);
	}
	if (CommitHandler && bHasPose)
	{
		CommitHandler->OnPreviewUpdated(this, CurrentEvaluation, GetModeTag());
	}
	if (bChanged)
	{
		OnEvaluationChanged.Broadcast(CurrentEvaluation, GetModeTag());
	}
	if (bHasPose && EGPlacementDebug::GetLevel() > 0)
	{
		EGPlacementDebug::DrawEvaluation(GetWorld(), CurrentEvaluation);
		if (EGPlacementDebug::GetLevel() > 1)
		{
			EGPlacementDebug::DrawSpatial(GetWorld(), CurrentEvaluation.Pose.GetLocation(), 500.0f);
		}
	}
}

UEGPlacementSessionComponent::FCacheEntry UEGPlacementSessionComponent::MakeCacheKey(const FEGPlacementIntent& Intent, const UEGPlacementProfile* Profile) const
{
	FCacheEntry Key;
	const FVector Location = Intent.Pose.GetLocation() / CacheLocationQuantum;
	Key.Location = FIntVector(FMath::RoundToInt32(Location.X), FMath::RoundToInt32(Location.Y), FMath::RoundToInt32(Location.Z));
	const FQuat Rotation = Intent.Pose.GetRotation().GetNormalized();
	// Quaternion components to 1e-4: about 0.01 degrees.
	Key.Rotation = FIntVector4(FMath::RoundToInt32(Rotation.X * 10000.0), FMath::RoundToInt32(Rotation.Y * 10000.0), FMath::RoundToInt32(Rotation.Z * 10000.0), FMath::RoundToInt32(Rotation.W * 10000.0));
	Key.Spin = FMath::RoundToInt32(Intent.SpinDegrees * 100.0f);
	Key.Flags = (Intent.bFreePosition ? 1 : 0) | (Intent.bFreeRotation ? 2 : 0) | (Intent.bHasSurfaceHint ? 4 : 0);
	Key.Target = Intent.Target.Kind == EEGPlacementTargetKind::Anchor ? static_cast<UObject*>(Intent.Target.Anchor.Get()) : static_cast<UObject*>(Intent.Target.Component.Get());
	Key.Profile = Profile;
	return Key;
}

bool UEGPlacementSessionComponent::IsCacheEntryCurrent(const FCacheEntry& Entry, const FCacheEntry& Key) const
{
	if (!Entry.bSet || Entry.Location != Key.Location || Entry.Rotation != Key.Rotation || Entry.Spin != Key.Spin
		|| Entry.Flags != Key.Flags || Entry.Target != Key.Target || Entry.Profile != Key.Profile)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - Entry.Time > RuleResultMaxAge)
	{
		return false;
	}
	for (const FEGPlacementRevision& Revision : Entry.Evaluation.Revisions)
	{
		if (!Revision.Source.IsValid() || EGPlacement::GetRevision(Revision.Source.Get()) != Revision.Revision)
		{
			return false;
		}
	}
	if (Entry.Support.IsValid())
	{
		if (!Entry.Support->GetComponentTransform().Equals(Entry.SupportTransform, 0.01))
		{
			return false;
		}
	}
	return true;
}

bool UEGPlacementSessionComponent::EvaluateMode(int32 ModeIndex, const FEGPlacementView& View, bool bForceFresh, FEGPlacementEvaluation& OutEvaluation, FEGPlacementIntent& OutIntent)
{
	const FEGPlacementMode& Mode = ActiveModes[ModeIndex];
	if (!Mode.Aim || !Mode.Profile)
	{
		OutEvaluation = FEGPlacementEvaluation();
		OutEvaluation.Status = EEGPlacementStatus::Invalid;
		OutEvaluation.Reasons.Emplace(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("ModeIncomplete", "The placement mode has no aim provider or no profile."), this);
		return false;
	}
	const FEGPlacementContext Context = MakeContext(EEGPlacementScope::Preview);
	FEGPlacementIntent Intent;
	Intent.Pose.SetScale3D(FVector(Selection.Scale));
	if (!Mode.Aim->BuildIntent(View, Selection.Spec, Mode.Profile, Context, Intent))
	{
		OutEvaluation = FEGPlacementEvaluation();
		OutEvaluation.Status = EEGPlacementStatus::Invalid;
		OutEvaluation.Reasons.Emplace(Mode.Profile->Solver ? TAG_EGPlacement_Reason_NoSurface : TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NothingAimed", "Nothing to place on here."), Mode.Aim.Get());
		return false;
	}
	Intent.Pose.SetScale3D(FVector(Selection.Scale));
	Intent.SpinDegrees = SpinDegrees;
	Intent.bFreePosition = bFreePosition;
	Intent.bFreeRotation = bFreeRotation;
	OutIntent = Intent;

	if (!Cache.IsValidIndex(ModeIndex))
	{
		Cache.SetNum(ActiveModes.Num());
	}
	FCacheEntry Key = MakeCacheKey(Intent, Mode.Profile);
	FCacheEntry& Entry = Cache[ModeIndex];
	if (!bForceFresh && IsCacheEntryCurrent(Entry, Key))
	{
		++FEGPlacementStats::CacheHits;
		OutEvaluation = Entry.Evaluation;
		return true;
	}
	++FEGPlacementStats::CacheMisses;
	OutEvaluation = EGPlacement::Evaluate(Mode.Profile, Selection.Spec, Intent, Context);

	Key.bSet = true;
	Key.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Key.Support = OutEvaluation.Support.Component;
	Key.SupportTransform = Key.Support.IsValid() ? Key.Support->GetComponentTransform() : FTransform::Identity;
	Key.Evaluation = OutEvaluation;
	Entry = MoveTemp(Key);
	return true;
}

void UEGPlacementSessionComponent::Announce(const FGameplayTag& Channel, const FGameplayTag& ModeTag, const FTransform& Pose, const FEGPlacementOutcome* Outcome, const TArray<FEGPlacementReason>* Reasons) const
{
	UEFMessageSubsystem* Messages = UEFMessageSubsystem::Get(this);
	if (!Messages)
	{
		return;
	}
	FEGPlacementSessionMessage Message;
	Message.Session = const_cast<UEGPlacementSessionComponent*>(this);
	Message.Requester = MakeContext(EEGPlacementScope::Preview).Requester.Get();
	Message.ModeTag = ModeTag;
	Message.Source = Selection.GetSource();
	Message.Pose = Pose;
	if (Outcome)
	{
		Message.bHasOutcome = true;
		Message.Outcome = Outcome->Kind;
	}
	if (Reasons)
	{
		Message.Reasons = *Reasons;
	}
	Messages->BroadcastMessage(Channel, Message);
}

#undef LOCTEXT_NAMESPACE

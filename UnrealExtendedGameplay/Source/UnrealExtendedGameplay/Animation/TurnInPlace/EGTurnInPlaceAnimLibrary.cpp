// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceAnimLibrary.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "EGTurnInPlaceComponent.h"
#include "EGTurnInPlaceSolver.h"

void UEGTurnInPlaceAnimLibrary::GatherTurnInPlace(const UEGTurnInPlaceComponent* Component, FEGTurnInPlaceAnimData& Data)
{
	Data.Input = Component ? Component->MakeGraphInput() : FEGTurnInPlaceGraphInput();
}

void UEGTurnInPlaceAnimLibrary::ThreadSafeUpdateTurnInPlace(const UAnimInstance* AnimInstance, FEGTurnInPlaceAnimData& Data)
{
	if (!AnimInstance || !Data.Input.bValid)
	{
		Data.Curves = FEGTurnInPlaceCurves();
		Data.Output = FEGTurnInPlaceGraphOutput();
		return;
	}

	// The single point the curves are read: the movement hook gets these through
	// IEGTurnInPlaceAnimSource, so gameplay and the state machine see the same frame.
	const FEGTurnInPlaceCurveNames& Names = Data.Input.CurveNames;
	Data.Curves.RemainingTurnYaw = AnimInstance->GetCurveValue(Names.RemainingTurnYaw);
	Data.Curves.TurnYawWeight = AnimInstance->GetCurveValue(Names.TurnYawWeight);
	Data.Curves.PauseTurnInPlace = AnimInstance->GetCurveValue(Names.PauseTurnInPlace);
	Data.Curves.LockTurnInPlace = AnimInstance->GetCurveValue(Names.LockTurnInPlace);

	// A procedural turn has no clip to carry its curves: compute them at the turn's time.
	if (Data.Node.bProcedural)
	{
		const FEGTurnInPlaceCurves Procedural = FEGTurnInPlaceSolver::EvaluateProceduralTurn(Data.Node.ProceduralYaw, Data.Node.AnimTime, Data.Input.GetEntry().Procedural);
		Data.Curves.RemainingTurnYaw = Procedural.RemainingTurnYaw;
		Data.Curves.TurnYawWeight = Procedural.TurnYawWeight;
	}

	// The game thread's copy lags the pose; refresh it before the transitions read it.
	Data.Input.bIsTurning = Data.Curves.IsTurning();
	if (Data.Input.bIsTurning)
	{
		Data.bTurnedThisEntry = true;
	}

	if (Data.Input.bPseudo)
	{
		// A dedicated server in Pseudo mode: the component runs the states; the graph stays idle.
		Data.Output = FEGTurnInPlaceGraphOutput();
		return;
	}
	FEGTurnInPlaceSolver::ComputeGraphOutput(Data.Input, Data.bTurnedThisEntry, Data.Node.bReachedAnimEnd, Data.Output);
}

void UEGTurnInPlaceAnimLibrary::SetupIdle(FEGTurnInPlaceAnimData& Data)
{
	Data.Node.bReachedMaxAngle = false;
	Data.Node.PlayRate = 1.0f;

	// The previous turn's latch and end flag must not carry into the next one's first frame.
	Data.bTurnedThisEntry = false;
	Data.Node.bReachedAnimEnd = false;
	Data.Node.bRecoveryFinished = false;
	Data.Node.bProcedural = false;
}

void UEGTurnInPlaceAnimLibrary::SetupTurnPose(FEGTurnInPlaceAnimData& Data)
{
	// Step, side, time and whether the turn is procedural, from this frame's input.
	FEGTurnInPlaceSolver::BeginTurn(Data.Node, Data.Input);

	// The next worker pass latches it again from this turn's own curves.
	Data.bTurnedThisEntry = false;
}

UAnimSequence* UEGTurnInPlaceAnimLibrary::SetupTurnAnim(FEGTurnInPlaceAnimData& Data)
{
	Data.Node.AnimTime = 0.0;
	Data.Node.bReachedMaxAngle = false;
	Data.Node.bReachedAnimEnd = false;
	FEGTurnInPlaceSolver::UpdateNodePlayRate(Data.Node, Data.Input);
	return GetTurnAnimation(Data, false);
}

float UEGTurnInPlaceAnimLibrary::UpdateTurnAnim(FEGTurnInPlaceAnimData& Data, float DeltaTime, UAnimSequence*& OutSequence)
{
	// Null for a procedural turn (and for a missing clip, which counts as played out).
	OutSequence = GetTurnAnimation(Data, false);
	Data.Node.bReachedAnimEnd = FEGTurnInPlaceSolver::AdvanceTurnTime(Data.Input.GetEntry(), Data.Node, DeltaTime, Data.Node.PlayRate, false);
	FEGTurnInPlaceSolver::UpdateNodePlayRate(Data.Node, Data.Input);
	return static_cast<float>(Data.Node.AnimTime);
}

void UEGTurnInPlaceAnimLibrary::SetupRecoveryPose(FEGTurnInPlaceAnimData& Data)
{
	Data.Node.bRecoveryTurningRight = Data.Node.bTurningRight;

	// Recovery -> Turn must start clean too. AnimTime is kept: recovery continues the clip.
	Data.bTurnedThisEntry = false;
	Data.Node.bReachedAnimEnd = false;
	Data.Node.bRecoveryFinished = false;
}

float UEGTurnInPlaceAnimLibrary::UpdateRecoveryAnim(FEGTurnInPlaceAnimData& Data, float DeltaTime, UAnimSequence*& OutSequence)
{
	OutSequence = GetTurnAnimation(Data, true);
	Data.Node.bRecoveryFinished = FEGTurnInPlaceSolver::AdvanceTurnTime(Data.Input.GetEntry(), Data.Node, DeltaTime, 1.0f, true);
	return static_cast<float>(Data.Node.AnimTime);
}

UAnimSequence* UEGTurnInPlaceAnimLibrary::GetTurnAnimation(const FEGTurnInPlaceAnimData& Data, bool bRecovery)
{
	return FEGTurnInPlaceSolver::GetTurnAnimation(Data.Input.GetEntry(), Data.Node, bRecovery);
}

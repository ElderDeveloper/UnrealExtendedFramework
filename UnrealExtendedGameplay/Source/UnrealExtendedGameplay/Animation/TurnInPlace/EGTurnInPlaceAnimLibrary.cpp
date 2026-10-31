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
}

void UEGTurnInPlaceAnimLibrary::SetupTurnPose(FEGTurnInPlaceAnimData& Data)
{
	Data.Node.StepIndex = Data.Input.StepIndex;
	Data.Node.bTurningRight = Data.Input.bTurnRight;

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
	OutSequence = GetTurnAnimation(Data, false);
	if (!OutSequence)
	{
		// Nothing to play: count it as played out so recovery can fire.
		Data.Node.AnimTime = 0.0;
		Data.Node.bReachedAnimEnd = true;
		return 0.0f;
	}

	Data.Node.AnimTime = FEGTurnInPlaceSolver::AdvanceAnimTime(OutSequence, Data.Node.AnimTime, DeltaTime, Data.Node.PlayRate);
	Data.Node.bReachedAnimEnd = Data.Node.AnimTime >= OutSequence->GetPlayLength() - UE_KINDA_SMALL_NUMBER;
	FEGTurnInPlaceSolver::UpdateNodePlayRate(Data.Node, Data.Input);
	return static_cast<float>(Data.Node.AnimTime);
}

void UEGTurnInPlaceAnimLibrary::SetupRecoveryPose(FEGTurnInPlaceAnimData& Data)
{
	Data.Node.bRecoveryTurningRight = Data.Node.bTurningRight;
}

UAnimSequence* UEGTurnInPlaceAnimLibrary::GetTurnAnimation(const FEGTurnInPlaceAnimData& Data, bool bRecovery)
{
	return FEGTurnInPlaceSolver::GetTurnAnimation(Data.Input.GetEntry(), Data.Node, bRecovery);
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceCurveModifier.h"

#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationBlueprintLibrary.h"

void UEGTurnInPlaceCurveModifier::RemoveCurveIfPresent(UAnimSequence* Animation, FName CurveName)
{
	if (UAnimationBlueprintLibrary::DoesCurveExist(Animation, CurveName, ERawCurveTrackTypes::RCT_Float))
	{
		Animation->GetController().RemoveCurve(FAnimationCurveIdentifier(CurveName, ERawCurveTrackTypes::RCT_Float));
	}
}

void UEGTurnInPlaceCurveModifier::WriteCurve(UAnimSequence* Animation, FName CurveName, const TArray<FRichCurveKey>& Keys)
{
	RemoveCurveIfPresent(Animation, CurveName);

	IAnimationDataController& Controller = Animation->GetController();
	const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
	if (Keys.Num() > 0 && Controller.AddCurve(CurveId))
	{
		Controller.SetCurveKeys(CurveId, Keys);
	}
}

void UEGTurnInPlaceCurveModifier::OnApply_Implementation(UAnimSequence* Animation)
{
	if (!Animation || !Animation->GetSkeleton())
	{
		UE_LOG(LogAnimation, Error, TEXT("Turn In Place Curves: no animation or skeleton."));
		return;
	}
	if (Animation->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(RootBoneName) == INDEX_NONE)
	{
		UE_LOG(LogAnimation, Error, TEXT("Turn In Place Curves: %s has no bone %s."), *GetNameSafe(Animation), *RootBoneName.ToString());
		return;
	}

	const double Length = Animation->GetPlayLength();
	if (Length <= 0.0)
	{
		UE_LOG(LogAnimation, Error, TEXT("Turn In Place Curves: %s has no length."), *GetNameSafe(Animation));
		return;
	}

	// Sample the raw root track, not the root-locked pose.
	TGuardValue<bool> RootLockGuard(Animation->bForceRootLock, false);

	const int32 Samples = FMath::Max(2, FMath::CeilToInt32(Length * SampleRate) + 1);
	TArray<double> Times;
	TArray<double> Yaws;
	Times.Reserve(Samples);
	Yaws.Reserve(Samples);

	// Accumulate frame-to-frame deltas, so a 180-degree turn is not ambiguous at its ends.
	double Cumulative = 0.0;
	double PreviousRaw = 0.0;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Time = Index == Samples - 1 ? Length : FMath::Min(static_cast<double>(Index) / SampleRate, Length);

		FTransform Pose;
		UAnimationBlueprintLibrary::GetBonePoseForTime(Animation, RootBoneName, static_cast<float>(Time), false, Pose);
		const double RawYaw = Pose.GetRotation().Rotator().Yaw;

		if (Index > 0)
		{
			Cumulative += FRotator::NormalizeAxis(RawYaw - PreviousRaw);
		}
		PreviousRaw = RawYaw;
		Times.Add(Time);
		Yaws.Add(Cumulative);
	}

	const double TotalYaw = Yaws.Last();
	if (FMath::Abs(TotalYaw) < FinishedTolerance)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Turn In Place Curves: the root of %s barely turns (%.2f degrees). Does it have root motion?"), *GetNameSafe(Animation), TotalYaw);
	}

	TArray<FRichCurveKey> RemainingKeys;
	TArray<FRichCurveKey> WeightKeys;
	RemainingKeys.Reserve(Samples);

	FRichCurveKey& WeightStart = WeightKeys.AddDefaulted_GetRef();
	WeightStart.Time = 0.0f;
	WeightStart.Value = 1.0f;
	WeightStart.InterpMode = RCIM_Constant;

	bool bPlacedWeightEnd = false;
	const double LatestEnd = FMath::Max(0.0, Length - RecoveryBlendTime);
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Remaining = TotalYaw - Yaws[Index];

		FRichCurveKey& Key = RemainingKeys.AddDefaulted_GetRef();
		Key.Time = static_cast<float>(Times[Index]);
		Key.Value = static_cast<float>(Remaining);

		// The turn ends where the remaining yaw is gone, or early enough to blend into recovery.
		// Never on the first sample: the weight must be seen at 1 for the turn to register.
		if (!bPlacedWeightEnd && Index > 0 && (FMath::Abs(Remaining) <= FinishedTolerance || Times[Index] >= LatestEnd))
		{
			bPlacedWeightEnd = true;
			FRichCurveKey& End = WeightKeys.AddDefaulted_GetRef();
			End.Time = static_cast<float>(Times[Index]);
			End.Value = 0.0f;
			End.InterpMode = RCIM_Constant;
		}
	}

	WriteCurve(Animation, RemainingTurnYawCurve, RemainingKeys);
	WriteCurve(Animation, TurnYawWeightCurve, WeightKeys);
}

void UEGTurnInPlaceCurveModifier::OnRevert_Implementation(UAnimSequence* Animation)
{
	if (Animation)
	{
		RemoveCurveIfPresent(Animation, RemainingTurnYawCurve);
		RemoveCurveIfPresent(Animation, TurnYawWeightCurve);
	}
}

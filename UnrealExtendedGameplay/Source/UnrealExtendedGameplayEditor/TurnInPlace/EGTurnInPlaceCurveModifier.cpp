// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceCurveModifier.h"

#include "AnimPose.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationBlueprintLibrary.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceSolver.h"

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
	Times.Reserve(Samples);

	TArray<double> RawYaws;
	RawYaws.Reserve(Samples);
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Time = Index == Samples - 1 ? Length : FMath::Min(static_cast<double>(Index) / SampleRate, Length);

		// The default options keep root motion in the pose (root lock ignored) and extract none.
		FAnimPose Pose;
		UAnimPoseExtensions::GetAnimPoseAtTime(Animation, Time, FAnimPoseEvaluationOptions(), Pose);
		Times.Add(Time);
		RawYaws.Add(UAnimPoseExtensions::GetBonePose(Pose, RootBoneName, EAnimPoseSpaces::Local).GetRotation().Rotator().Yaw);
	}

	// The same unwrapping and weight end the Turn In Place node uses for clips without curves.
	FEGTurnYawTrack Track;
	if (!FEGTurnInPlaceSolver::BuildYawTrack(Times, RawYaws, FinishedTolerance, RecoveryBlendTime, Track))
	{
		UE_LOG(LogAnimation, Error, TEXT("Turn In Place Curves: could not sample the root of %s."), *GetNameSafe(Animation));
		return;
	}
	if (FMath::Abs(Track.TotalYaw) < FinishedTolerance)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Turn In Place Curves: the root of %s barely turns (%.2f degrees). Does it have root motion?"), *GetNameSafe(Animation), Track.TotalYaw);
	}

	TArray<FRichCurveKey> RemainingKeys;
	RemainingKeys.Reserve(Track.Times.Num());
	for (int32 Index = 0; Index < Track.Times.Num(); ++Index)
	{
		FRichCurveKey& Key = RemainingKeys.AddDefaulted_GetRef();
		Key.Time = Track.Times[Index];
		Key.Value = Track.TotalYaw - Track.CumulativeYaw[Index];
	}

	// 1 from the start (it must be seen at 1 for the turn to register), 0 from the turn's end.
	TArray<FRichCurveKey> WeightKeys;
	FRichCurveKey& WeightStart = WeightKeys.AddDefaulted_GetRef();
	WeightStart.Time = 0.0f;
	WeightStart.Value = 1.0f;
	WeightStart.InterpMode = RCIM_Constant;
	FRichCurveKey& WeightEnd = WeightKeys.AddDefaulted_GetRef();
	WeightEnd.Time = Track.WeightEndTime;
	WeightEnd.Value = 0.0f;
	WeightEnd.InterpMode = RCIM_Constant;

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

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "AnimationModifier.h"
#include "CoreMinimal.h"
#include "Curves/RichCurve.h"

#include "EGTurnInPlaceCurveModifier.generated.h"

/**
 * Writes the two curves turn in place reads from a turn animation with root motion:
 *
 *  - RemainingTurnYaw: the yaw the root still turns from each frame to the end.
 *  - TurnYawWeight: 1 while turning, 0 from the frame the turn is finished (or RecoveryBlendTime
 *    before the end at the latest), so the graph can blend into recovery.
 *
 * Apply it to every sequence in UEGTurnInPlaceSet's LeftTurns and RightTurns. Root lock is ignored
 * while the root is sampled. The names must match the set's Curves.
 */
UCLASS(meta = (DisplayName = "Turn In Place Curves"))
class UNREALEXTENDEDGAMEPLAYEDITOR_API UEGTurnInPlaceCurveModifier : public UAnimationModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName RootBoneName = TEXT("root");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName RemainingTurnYawCurve = TEXT("RemainingTurnYaw");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName TurnYawWeightCurve = TEXT("TurnYawWeight");

	/** The turn counts as finished once no more than this yaw remains. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0", Units = "Degrees"))
	float FinishedTolerance = 0.5f;

	/** The weight reaches 0 at least this long before the end, leaving the graph time to blend out of the turn. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0", Units = "s"))
	float RecoveryBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "10", ClampMax = "240"))
	int32 SampleRate = 60;

	virtual void OnApply_Implementation(UAnimSequence* Animation) override;
	virtual void OnRevert_Implementation(UAnimSequence* Animation) override;

private:
	static void WriteCurve(UAnimSequence* Animation, FName CurveName, const TArray<FRichCurveKey>& Keys);
	static void RemoveCurveIfPresent(UAnimSequence* Animation, FName CurveName);
};

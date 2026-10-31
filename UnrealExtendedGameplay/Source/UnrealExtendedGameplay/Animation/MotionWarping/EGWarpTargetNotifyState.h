// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "EGWarpTargetNotifyState.generated.h"

/**
 * Feeds a Motion Warping target from the character's current target, so an attack's root motion
 * bends to reach the enemy (the job the Soft Lock notify did by turning the actor by hand).
 *
 * Place it so it covers, or starts before, the montage's Motion Warping window, with the same
 * Warp Target Name. The target is the lock-on target of UEGTargetingComponent /
 * UEGDynamicTargetingComponent, else the closest pawn in front within SearchRadius.
 *
 * Needs a UMotionWarpingComponent on the character. Runs on the server and the owning client;
 * for the two to agree, the target must be known on both (a lock-on chosen only on the client
 * makes the server warp elsewhere and correct the client).
 */
UCLASS(meta = (DisplayName = "Set Warp Target"))
class UNREALEXTENDEDGAMEPLAY_API UEGWarpTargetNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UEGWarpTargetNotifyState();

	/** Must match the Warp Target Name of the montage's Motion Warping window. */
	UPROPERTY(EditAnywhere, Category = "Warp Target")
	FName WarpTargetName = TEXT("Target");

	/** End this far from the target, on the line between the two. */
	UPROPERTY(EditAnywhere, Category = "Warp Target", meta = (ClampMin = "0", Units = "cm"))
	float StopDistance = 100.0f;

	/** Never warp further than this from where the character stands. 0 is unlimited. */
	UPROPERTY(EditAnywhere, Category = "Warp Target", meta = (ClampMin = "0", Units = "cm"))
	float MaxWarpDistance = 500.0f;

	/** With nothing locked on, the closest pawn within this range is used. 0 only uses lock-on. */
	UPROPERTY(EditAnywhere, Category = "Warp Target|Search", meta = (ClampMin = "0", Units = "cm"))
	float SearchRadius = 400.0f;

	/** Half-angle of the cone in front of the character the search accepts. 180 accepts any direction. */
	UPROPERTY(EditAnywhere, Category = "Warp Target|Search", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float SearchHalfAngle = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Warp Target|Search")
	TArray<TEnumAsByte<EObjectTypeQuery>> SearchObjectTypes;

	/** Keep updating the warp target while the window is open, following a moving target. */
	UPROPERTY(EditAnywhere, Category = "Warp Target")
	bool bTrackTarget = true;

	/** With no target, remove the warp target so the clip plays unwarped instead of reaching for an old one. */
	UPROPERTY(EditAnywhere, Category = "Warp Target")
	bool bClearWhenNoTarget = true;

	UPROPERTY(EditAnywhere, Category = "Warp Target")
	bool bClearOnEnd = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	void ApplyWarpTarget(USkeletalMeshComponent* MeshComp, AActor* Target) const;

	/**
	 * The target chosen when each window began, per mesh, oldest first (a window can overlap itself
	 * when a montage blends into itself). Tracking follows the newest instead of re-searching.
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<TWeakObjectPtr<AActor>>> Targets;

	/**
	 * Open windows per owner and warp target name, across every notify instance, so the end of a
	 * blending-out attack does not remove the warp target the next attack just set.
	 */
	static TMap<TPair<TWeakObjectPtr<AActor>, FName>, int32> OpenWindowCounts;
};

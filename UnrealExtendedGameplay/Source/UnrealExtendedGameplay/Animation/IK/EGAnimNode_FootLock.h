// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "CoreMinimal.h"

#include "EGAnimNode_FootLock.generated.h"

class UEGTurnInPlaceComponent;

/** One leg: the foot bone, with its parent and grandparent as the lower and upper leg the IK bends. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGFootLockFoot
{
	GENERATED_BODY()

	FEGFootLockFoot() = default;
	explicit FEGFootLockFoot(FName BoneName) : Foot(BoneName) {}

	UPROPERTY(EditAnywhere, Category = "Foot Lock")
	FBoneReference Foot;
};

/**
 * FAnimNode_EGFootLock ("Foot Lock" in the AnimBP)
 *
 * Keeps planted feet where they are in the world while the owner stands still and its capsule
 * rotates under them (procedural turns, settles, the max-angle drag of turn in place, or any
 * other in-place rotation), and steps a foot procedurally when the animation would put it too
 * far away or turned too far. A foot the animation moves or pivots (turn clips, stepping, montages)
 * follows the animation.
 *
 * Place it late in the graph, after Turn In Place, in component space. Defaults suit the UE5
 * mannequins (foot_l, foot_r); any leg where foot -> parent -> grandparent is ankle -> knee -> hip works.
 * Works without a turn in place component: it only needs the owner's velocity.
 */
USTRUCT(BlueprintInternalUseOnly)
struct UNREALEXTENDEDGAMEPLAY_API FAnimNode_EGFootLock : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_EGFootLock();

	UPROPERTY(EditAnywhere, Category = "Foot Lock")
	TArray<FEGFootLockFoot> Feet;

	/** Lock only while the owner moves slower than this, and is on the ground. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock", meta = (PinHiddenByDefault, ClampMin = "0", Units = "CentimetersPerSecond"))
	float MaxOwnerSpeed = 10.0f;

	/** Lock only while turn in place is turning (needs the component). Off: whenever standing still. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock", meta = (PinHiddenByDefault))
	bool bOnlyWhileTurning = false;

	/** A foot the animation moves faster than this, relative to the mesh, follows the animation. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Release", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float ReleaseSpeed = 30.0f;

	/** A foot the animation turns faster than this, relative to the mesh, follows the animation (turn clips pivot their feet). */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Release", meta = (ClampMin = "0", Units = "DegreesPerSecond"))
	float ReleaseAngularSpeed = 45.0f;

	/** Blend into a lock and back to the animation. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Release", meta = (ClampMin = "0", Units = "Seconds"))
	float BlendTime = 0.15f;

	/** A locked foot steps once the animation would put it this far away (across the up axis). */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Step", meta = (ClampMin = "1", Units = "Centimeters"))
	float StepDistance = 18.0f;

	/** ...or turned this far from it about the up axis. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Step", meta = (ClampMin = "1", ClampMax = "180", Units = "Degrees"))
	float StepAngle = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Foot Lock|Step", meta = (ClampMin = "0.05", Units = "Seconds"))
	float StepDuration = 0.25f;

	/** Peak lift of a procedural step. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|Step", meta = (ClampMin = "0", Units = "Centimeters"))
	float StepHeight = 7.0f;

	/** Let the leg stretch to reach a lock. Off: the foot stops short when the leg is fully extended. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock|IK")
	bool bAllowStretching = false;

	/** Moving further than this in one frame is a teleport: locks are dropped. */
	UPROPERTY(EditAnywhere, Category = "Foot Lock", meta = (ClampMin = "1", Units = "Centimeters"))
	float TeleportDistance = 100.0f;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

protected:
	// FAnimNode_SkeletalControlBase
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:
	struct FFootState
	{
		FCompactPoseBoneIndex FootIndex = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex KneeIndex = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex HipIndex = FCompactPoseBoneIndex(INDEX_NONE);

		/** The animation holds the foot still (with hysteresis). */
		bool bPlanted = false;
		/** LockedWorld is in use. */
		bool bLocked = false;
		/** 0: the animation; 1: the lock. */
		float LockAlpha = 0.0f;
		FTransform LockedWorld = FTransform::Identity;

		bool bHasPrevious = false;
		FTransform PreviousAnimComponent = FTransform::Identity;

		bool bStepping = false;
		float StepElapsed = 0.0f;
		FTransform StepFrom = FTransform::Identity;

		/** The last usable knee bend direction, for a frame where the leg is straight. */
		FVector BendDirection = FVector::ZeroVector;

		bool IsValid() const { return FootIndex.IsValid() && KneeIndex.IsValid() && HipIndex.IsValid(); }
	};

	void ResetFeet();

	TArray<FFootState> FootStates;

	/** From the game thread (PreUpdate). */
	bool bLockAllowed = false;
	float FrameDeltaTime = 0.0f;

	bool bHasLastComponent = false;
	FVector LastComponentLocation = FVector::ZeroVector;

	TWeakObjectPtr<const UEGTurnInPlaceComponent> CachedComponent;
	TWeakObjectPtr<const AActor> CachedOwner;
};

// Copyright Moonpunch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "EFAnimNode_WeaponPhysics.generated.h"

UENUM(BlueprintType)
enum class EEFWeaponPhysicsTargetMode : uint8
{
	SingleBone UMETA(DisplayName = "Single Bone"),
	RightArm UMETA(DisplayName = "Right Arm"),
	LeftArm UMETA(DisplayName = "Left Arm"),
	BothArms UMETA(DisplayName = "Both Arms")
};

USTRUCT()
struct UNREALEXTENDEDFRAMEWORK_API FEFWeaponPhysicsArmTarget
{
	GENERATED_BODY()

	/** Arm or hand bone driven by this target. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FBoneReference Bone;

	/** Translation contribution for this side. */
	UPROPERTY(EditAnywhere, Category = "Target", meta = (ClampMin = "0.0"))
	float TranslationAlpha = 1.0f;

	/** Rotation contribution for this side. */
	UPROPERTY(EditAnywhere, Category = "Target", meta = (ClampMin = "0.0"))
	float RotationAlpha = 1.0f;

	/** Mirrors lateral sway, roll, and yaw for the opposite arm. */
	UPROPERTY(EditAnywhere, Category = "Target")
	bool bMirrorLateralMotion = false;
};

struct FEFWeaponPhysicsRuntimeState
{
	FVector SpringVelocity = FVector::ZeroVector;
	FVector SpringDisplacement = FVector::ZeroVector;
	FRotator CurrentTilt = FRotator::ZeroRotator;
	FVector PreviousAnimatedPosition = FVector::ZeroVector;
	float BobPhase = 0.0f;
	bool bHasPreviousPosition = false;
};

/**
 * Adds physical weight, spring lag, tilt, and locomotion sway to weapon arms.
 *
 * The legacy BoneToModify field still drives the Single Bone mode. New setups
 * can target the right arm, left arm, or both arms from this single node.
 */
USTRUCT(BlueprintInternalUseOnly)
struct UNREALEXTENDEDFRAMEWORK_API FAnimNode_WeaponPhysics : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_WeaponPhysics();

	// Bone selection

	/** Which target set this node should drive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target")
	EEFWeaponPhysicsTargetMode TargetMode = EEFWeaponPhysicsTargetMode::SingleBone;

	/** Legacy single target bone, typically a weapon grip or dominant hand bone. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FBoneReference BoneToModify;

	/** Dominant/right side arm or hand bone. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FEFWeaponPhysicsArmTarget RightArmTarget;

	/** Off/left side arm or hand bone. Mirroring is enabled by default. */
	UPROPERTY(EditAnywhere, Category = "Target")
	FEFWeaponPhysicsArmTarget LeftArmTarget;

	// Spring translation

	/** How quickly the bone snaps back to its animated position. Higher = stiffer spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring Translation", meta = (ClampMin = "0.0"))
	float TranslationStiffness = 135.0f;

	/** How much the spring oscillation is dampened. Higher = less bouncing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring Translation", meta = (ClampMin = "0.0"))
	float TranslationDamping = 11.0f;

	/** Maximum displacement in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring Translation", meta = (ClampMin = "0.0"))
	float MaxTranslationOffset = 9.0f;

	/** How strongly character acceleration throws the weapon opposite movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring Translation", meta = (ClampMin = "0.0"))
	float AccelerationLagStrength = 1.35f;

	/** Extra pull from current velocity for a heavier carried feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spring Translation", meta = (ClampMin = "0.0"))
	float VelocityLagStrength = 0.35f;

	// Rotational tilt

	/** Degrees of roll per unit of lateral velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float LateralTiltMultiplier = 6.0f;

	/** Degrees of pitch per unit of vertical velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float VerticalTiltMultiplier = 4.0f;

	/** Degrees of pitch per unit of forward acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float ForwardTiltMultiplier = 5.0f;

	/** Degrees of yaw per unit of lateral velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float LateralYawMultiplier = 3.0f;

	/** Rotation added from spring displacement, tying location lag to weapon angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float DisplacementRotationMultiplier = 1.15f;

	/** How fast the tilt interpolates to the target angle. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float TiltSpeed = 14.0f;

	/** Maximum tilt in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotational Tilt", meta = (ClampMin = "0.0"))
	float MaxTiltDegrees = 16.0f;

	// Locomotion sway

	/** Adds a speed-driven breathing/walk sway on top of the spring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway")
	bool bEnableLocomotionSway = true;

	/** Overall locomotion bob strength in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway", meta = (ClampMin = "0.0"))
	float LocomotionSwayStrength = 1.4f;

	/** Forward/back bob size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway", meta = (ClampMin = "0.0"))
	float ForwardBobAmount = 0.9f;

	/** Side-to-side bob size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway", meta = (ClampMin = "0.0"))
	float LateralBobAmount = 0.55f;

	/** Up/down bob size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway", meta = (ClampMin = "0.0"))
	float VerticalBobAmount = 0.75f;

	/** Bob cycles per second at full movement speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion Sway", meta = (ClampMin = "0.0"))
	float BobFrequency = 2.2f;

	// Dynamic response

	/** Expected max movement speed used to normalize velocity input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "1.0"))
	float VelocityNormalizer = 600.0f;

	/** Expected max acceleration used to normalize acceleration input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "1.0"))
	float AccelerationNormalizer = 2000.0f;

	/** Baseline intensity before speed and acceleration are added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "0.0"))
	float BasePhysicsStrength = 1.0f;

	/** Intensity added as horizontal speed increases. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "0.0"))
	float SpeedPhysicsStrength = 0.35f;

	/** Intensity added during hard starts, stops, jumps, and impacts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "0.0"))
	float AccelerationPhysicsStrength = 0.45f;

	/** Upper bound for combined dynamic intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Response", meta = (ClampMin = "0.0"))
	float MaxDynamicStrength = 2.25f;

	// Input pins

	/** The owning character's current velocity in world space. Wire from AnimInstance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (PinShownByDefault))
	FVector CharacterVelocity = FVector::ZeroVector;

	/** The owning character's current acceleration in world space. Wire from AnimInstance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (PinShownByDefault))
	FVector CharacterAcceleration = FVector::ZeroVector;

	// FAnimNode_SkeletalControlBase interface

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

private:

	FEFWeaponPhysicsRuntimeState SingleBoneState;
	FEFWeaponPhysicsRuntimeState RightArmState;
	FEFWeaponPhysicsRuntimeState LeftArmState;

	/** Cached delta time from UpdateInternal. */
	float CachedDeltaTime = 0.0f;

	void ResetRuntimeState(FEFWeaponPhysicsRuntimeState& State) const;
	bool EvaluateTargetBone(
		FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms,
		const FBoneReference& TargetBone,
		FEFWeaponPhysicsRuntimeState& State,
		float TranslationAlpha,
		float RotationAlpha,
		bool bMirrorLateralMotion,
		const FVector& LocalVelocity,
		const FVector& LocalAcceleration,
		float DeltaTime,
		float DynamicStrength,
		float HorizontalSpeedAlpha);
};

// Copyright Moonpunch Games. All Rights Reserved.

#include "Animation/EFAnimNode_WeaponPhysics.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"

namespace
{
	float ClampSigned(float Value, float Limit)
	{
		return Limit > 0.0f ? FMath::Clamp(Value, -Limit, Limit) : Value;
	}

	float ScaleByAlpha(float Value, float Alpha)
	{
		return Value * FMath::Max(Alpha, 0.0f);
	}
}

FAnimNode_WeaponPhysics::FAnimNode_WeaponPhysics()
{
	LeftArmTarget.bMirrorLateralMotion = true;
	LeftArmTarget.TranslationAlpha = 0.92f;
	LeftArmTarget.RotationAlpha = 0.92f;
}

void FAnimNode_WeaponPhysics::ResetRuntimeState(FEFWeaponPhysicsRuntimeState& State) const
{
	State = FEFWeaponPhysicsRuntimeState();
}

void FAnimNode_WeaponPhysics::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	ResetRuntimeState(SingleBoneState);
	ResetRuntimeState(RightArmState);
	ResetRuntimeState(LeftArmState);
	CachedDeltaTime = 0.0f;
}

void FAnimNode_WeaponPhysics::UpdateInternal(const FAnimationUpdateContext& Context)
{
	Super::UpdateInternal(Context);
	CachedDeltaTime = Context.GetDeltaTime();
}

void FAnimNode_WeaponPhysics::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	BoneToModify.Initialize(RequiredBones);
	RightArmTarget.Bone.Initialize(RequiredBones);
	LeftArmTarget.Bone.Initialize(RequiredBones);
}

bool FAnimNode_WeaponPhysics::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	switch (TargetMode)
	{
	case EEFWeaponPhysicsTargetMode::RightArm:
		return RightArmTarget.Bone.IsValidToEvaluate(RequiredBones);
	case EEFWeaponPhysicsTargetMode::LeftArm:
		return LeftArmTarget.Bone.IsValidToEvaluate(RequiredBones);
	case EEFWeaponPhysicsTargetMode::BothArms:
		return RightArmTarget.Bone.IsValidToEvaluate(RequiredBones)
			|| LeftArmTarget.Bone.IsValidToEvaluate(RequiredBones);
	case EEFWeaponPhysicsTargetMode::SingleBone:
	default:
		return BoneToModify.IsValidToEvaluate(RequiredBones);
	}
}

void FAnimNode_WeaponPhysics::EvaluateSkeletalControl_AnyThread(
	FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const float DeltaTime = FMath::Min(CachedDeltaTime, 1.0f / 15.0f);
	if (DeltaTime <= SMALL_NUMBER)
	{
		return;
	}

	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector LocalVelocity = ComponentTransform.InverseTransformVectorNoScale(CharacterVelocity);
	const FVector LocalAcceleration = ComponentTransform.InverseTransformVectorNoScale(CharacterAcceleration);

	const float SafeVelocityNormalizer = FMath::Max(VelocityNormalizer, 1.0f);
	const float SafeAccelerationNormalizer = FMath::Max(AccelerationNormalizer, 1.0f);
	const float HorizontalSpeed = FVector2D(LocalVelocity.X, LocalVelocity.Y).Size();
	const float HorizontalSpeedAlpha = FMath::Clamp(HorizontalSpeed / SafeVelocityNormalizer, 0.0f, 1.0f);
	const float AccelerationAlpha = FMath::Clamp(LocalAcceleration.Size() / SafeAccelerationNormalizer, 0.0f, 1.0f);
	const float DynamicStrength = FMath::Clamp(
		BasePhysicsStrength
			+ (HorizontalSpeedAlpha * SpeedPhysicsStrength)
			+ (AccelerationAlpha * AccelerationPhysicsStrength),
		0.0f,
		FMath::Max(MaxDynamicStrength, 0.0f));

	switch (TargetMode)
	{
	case EEFWeaponPhysicsTargetMode::RightArm:
		EvaluateTargetBone(Output, OutBoneTransforms, RightArmTarget.Bone, RightArmState, RightArmTarget.TranslationAlpha,
			RightArmTarget.RotationAlpha, RightArmTarget.bMirrorLateralMotion, LocalVelocity, LocalAcceleration, DeltaTime,
			DynamicStrength, HorizontalSpeedAlpha);
		break;
	case EEFWeaponPhysicsTargetMode::LeftArm:
		EvaluateTargetBone(Output, OutBoneTransforms, LeftArmTarget.Bone, LeftArmState, LeftArmTarget.TranslationAlpha,
			LeftArmTarget.RotationAlpha, LeftArmTarget.bMirrorLateralMotion, LocalVelocity, LocalAcceleration, DeltaTime,
			DynamicStrength, HorizontalSpeedAlpha);
		break;
	case EEFWeaponPhysicsTargetMode::BothArms:
		EvaluateTargetBone(Output, OutBoneTransforms, RightArmTarget.Bone, RightArmState, RightArmTarget.TranslationAlpha,
			RightArmTarget.RotationAlpha, RightArmTarget.bMirrorLateralMotion, LocalVelocity, LocalAcceleration, DeltaTime,
			DynamicStrength, HorizontalSpeedAlpha);
		EvaluateTargetBone(Output, OutBoneTransforms, LeftArmTarget.Bone, LeftArmState, LeftArmTarget.TranslationAlpha,
			LeftArmTarget.RotationAlpha, LeftArmTarget.bMirrorLateralMotion, LocalVelocity, LocalAcceleration, DeltaTime,
			DynamicStrength, HorizontalSpeedAlpha);
		break;
	case EEFWeaponPhysicsTargetMode::SingleBone:
	default:
		EvaluateTargetBone(Output, OutBoneTransforms, BoneToModify, SingleBoneState, 1.0f, 1.0f, false, LocalVelocity,
			LocalAcceleration, DeltaTime, DynamicStrength, HorizontalSpeedAlpha);
		break;
	}

	if (OutBoneTransforms.Num() > 1)
	{
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());
	}
}

bool FAnimNode_WeaponPhysics::EvaluateTargetBone(
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
	float HorizontalSpeedAlpha)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	if (!TargetBone.IsValidToEvaluate(BoneContainer))
	{
		return false;
	}

	const FCompactPoseBoneIndex CompactBoneIndex = TargetBone.GetCompactPoseIndex(BoneContainer);
	FTransform BoneTransformCS = Output.Pose.GetComponentSpaceTransform(CompactBoneIndex);
	const FVector AnimatedPosition = BoneTransformCS.GetLocation();

	constexpr float TeleportThresholdSq = 22500.0f;
	if (State.bHasPreviousPosition && FVector::DistSquared(AnimatedPosition, State.PreviousAnimatedPosition) > TeleportThresholdSq)
	{
		ResetRuntimeState(State);
	}

	State.PreviousAnimatedPosition = AnimatedPosition;
	State.bHasPreviousPosition = true;

	const float SafeDynamicStrength = FMath::Max(DynamicStrength, 0.0f);
	const FVector SpringDrive =
		(LocalAcceleration * AccelerationLagStrength)
		+ (LocalVelocity * VelocityLagStrength);

	const int32 StepCount = FMath::Clamp(FMath::CeilToInt(DeltaTime / (1.0f / 60.0f)), 1, 4);
	const float StepDeltaTime = DeltaTime / static_cast<float>(StepCount);
	for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
	{
		const FVector SpringForce =
			(-TranslationStiffness * State.SpringDisplacement)
			- (TranslationDamping * State.SpringVelocity)
			- (SpringDrive * SafeDynamicStrength);

		State.SpringVelocity += SpringForce * StepDeltaTime;
		State.SpringDisplacement += State.SpringVelocity * StepDeltaTime;
	}

	const float DynamicOffsetScale = FMath::Lerp(1.0f, FMath::Max(SafeDynamicStrength, 1.0f), 0.5f);
	const float EffectiveMaxOffset = MaxTranslationOffset * DynamicOffsetScale;
	if (EffectiveMaxOffset > 0.0f)
	{
		const float DisplacementLength = State.SpringDisplacement.Size();
		if (DisplacementLength > EffectiveMaxOffset)
		{
			const FVector DisplacementDirection = State.SpringDisplacement.GetSafeNormal();
			State.SpringDisplacement = DisplacementDirection * EffectiveMaxOffset;

			const float VelocityAwayFromCenter = FVector::DotProduct(State.SpringVelocity, DisplacementDirection);
			if (VelocityAwayFromCenter > 0.0f)
			{
				State.SpringVelocity -= DisplacementDirection * VelocityAwayFromCenter;
			}
		}
	}

	FVector LocomotionBob = FVector::ZeroVector;
	if (bEnableLocomotionSway && LocomotionSwayStrength > 0.0f && HorizontalSpeedAlpha > KINDA_SMALL_NUMBER)
	{
		const float BobRate = FMath::Max(BobFrequency, 0.0f) * FMath::Lerp(0.35f, 1.0f, HorizontalSpeedAlpha);
		State.BobPhase = FMath::Fmod(State.BobPhase + (DeltaTime * BobRate * UE_TWO_PI), UE_TWO_PI);

		const float BobAlpha = FMath::Sqrt(HorizontalSpeedAlpha) * LocomotionSwayStrength * SafeDynamicStrength;
		const float SinPhase = FMath::Sin(State.BobPhase);
		const float CosPhase = FMath::Cos(State.BobPhase);
		const float CosDoublePhase = FMath::Cos(State.BobPhase * 2.0f);

		LocomotionBob.X = -FMath::Abs(SinPhase) * ForwardBobAmount * BobAlpha;
		LocomotionBob.Y = CosPhase * LateralBobAmount * BobAlpha;
		LocomotionBob.Z = CosDoublePhase * VerticalBobAmount * BobAlpha;
	}

	FVector FinalTranslationOffset = (State.SpringDisplacement + LocomotionBob) * FMath::Max(TranslationAlpha, 0.0f);
	if (bMirrorLateralMotion)
	{
		FinalTranslationOffset.Y *= -1.0f;
	}

	const float SafeVelocityNormalizer = FMath::Max(VelocityNormalizer, 1.0f);
	const float SafeAccelerationNormalizer = FMath::Max(AccelerationNormalizer, 1.0f);
	const float NormalizedLateralVelocity = FMath::Clamp(LocalVelocity.Y / SafeVelocityNormalizer, -1.0f, 1.0f);
	const float NormalizedVerticalVelocity = FMath::Clamp(LocalVelocity.Z / SafeVelocityNormalizer, -1.0f, 1.0f);
	const float NormalizedForwardAcceleration = FMath::Clamp(LocalAcceleration.X / SafeAccelerationNormalizer, -1.0f, 1.0f);

	FRotator TargetTilt = FRotator::ZeroRotator;
	TargetTilt.Roll =
		(-NormalizedLateralVelocity * LateralTiltMultiplier * SafeDynamicStrength)
		- (State.SpringDisplacement.Y * DisplacementRotationMultiplier);
	TargetTilt.Pitch =
		(NormalizedVerticalVelocity * VerticalTiltMultiplier * SafeDynamicStrength)
		+ (NormalizedForwardAcceleration * ForwardTiltMultiplier * SafeDynamicStrength)
		- (State.SpringDisplacement.X * DisplacementRotationMultiplier);
	TargetTilt.Yaw =
		(NormalizedLateralVelocity * LateralYawMultiplier * SafeDynamicStrength)
		+ (State.SpringDisplacement.Y * DisplacementRotationMultiplier * 0.5f);

	if (bEnableLocomotionSway && HorizontalSpeedAlpha > KINDA_SMALL_NUMBER)
	{
		const float SwayRotationAlpha = LocomotionSwayStrength * FMath::Sqrt(HorizontalSpeedAlpha) * SafeDynamicStrength;
		TargetTilt.Pitch += FMath::Sin(State.BobPhase) * 1.25f * SwayRotationAlpha;
		TargetTilt.Roll += FMath::Cos(State.BobPhase) * 0.85f * SwayRotationAlpha;
	}

	if (bMirrorLateralMotion)
	{
		TargetTilt.Roll *= -1.0f;
		TargetTilt.Yaw *= -1.0f;
	}

	const float EffectiveMaxTilt = MaxTiltDegrees * FMath::Lerp(1.0f, FMath::Max(SafeDynamicStrength, 1.0f), 0.35f);
	TargetTilt.Pitch = ClampSigned(TargetTilt.Pitch, EffectiveMaxTilt);
	TargetTilt.Roll = ClampSigned(TargetTilt.Roll, EffectiveMaxTilt);
	TargetTilt.Yaw = ClampSigned(TargetTilt.Yaw, EffectiveMaxTilt);

	const float EffectiveTiltSpeed = FMath::Max(TiltSpeed, 0.0f) * FMath::Lerp(0.85f, 1.2f, FMath::Clamp(SafeDynamicStrength, 0.0f, 1.0f));
	State.CurrentTilt.Pitch = FMath::FInterpTo(State.CurrentTilt.Pitch, TargetTilt.Pitch, DeltaTime, EffectiveTiltSpeed);
	State.CurrentTilt.Roll = FMath::FInterpTo(State.CurrentTilt.Roll, TargetTilt.Roll, DeltaTime, EffectiveTiltSpeed);
	State.CurrentTilt.Yaw = FMath::FInterpTo(State.CurrentTilt.Yaw, TargetTilt.Yaw, DeltaTime, EffectiveTiltSpeed);

	const FRotator FinalTilt(
		ScaleByAlpha(State.CurrentTilt.Pitch, RotationAlpha),
		ScaleByAlpha(State.CurrentTilt.Yaw, RotationAlpha),
		ScaleByAlpha(State.CurrentTilt.Roll, RotationAlpha));

	BoneTransformCS.AddToTranslation(FinalTranslationOffset);
	BoneTransformCS.SetRotation((BoneTransformCS.GetRotation() * FQuat(FinalTilt)).GetNormalized());

	for (FBoneTransform& ExistingTransform : OutBoneTransforms)
	{
		if (ExistingTransform.BoneIndex == CompactBoneIndex)
		{
			ExistingTransform.Transform = BoneTransformCS;
			return true;
		}
	}

	OutBoneTransforms.Add(FBoneTransform(CompactBoneIndex, BoneTransformCS));
	return true;
}

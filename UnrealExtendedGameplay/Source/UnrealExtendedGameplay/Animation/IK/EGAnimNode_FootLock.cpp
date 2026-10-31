// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimNode_FootLock.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "BonePose.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TwoBoneIK.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceComponent.h"

namespace
{
	float SmoothStep01(float Alpha)
	{
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}

	/** Blend from one world transform towards another; the scale is taken from the first. */
	FTransform BlendWorld(const FTransform& From, const FTransform& To, float Alpha)
	{
		return FTransform(FQuat::Slerp(From.GetRotation(), To.GetRotation(), Alpha).GetNormalized(),
			FMath::Lerp(From.GetLocation(), To.GetLocation(), Alpha), From.GetScale3D());
	}
}

FAnimNode_EGFootLock::FAnimNode_EGFootLock()
{
	// The UE5 mannequins (Manny, Quinn).
	Feet.Add(FEGFootLockFoot(TEXT("foot_l")));
	Feet.Add(FEGFootLockFoot(TEXT("foot_r")));
}

// ---------------------------------------------------------------------------------------------
// Game thread
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGFootLock::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const AActor* const Owner = InAnimInstance ? InAnimInstance->GetOwningActor() : nullptr;
	if (Owner != CachedOwner.Get() || !CachedComponent.IsValid())
	{
		CachedOwner = Owner;
		CachedComponent = UEGTurnInPlaceComponent::FindFor(Owner);
	}

	bLockAllowed = false;
	if (!Owner)
	{
		return;
	}

	FVector Velocity = Owner->GetVelocity();
	bool bOnGround = true;
	if (const UEGTurnInPlaceComponent* const Component = CachedComponent.Get())
	{
		// What turn in place reads, so both agree on "standing still" for any movement system.
		FEGTurnInPlaceMovementInfo Info;
		if (Component->GetMovementInfo(Info))
		{
			Velocity = Info.Velocity;
			bOnGround = Info.bMovingOnGround;
		}
		if (bOnlyWhileTurning && !Component->IsTurningInPlace())
		{
			return;
		}
	}
	else
	{
		if (bOnlyWhileTurning)
		{
			return;
		}
		const APawn* const Pawn = Cast<APawn>(Owner);
		if (const UPawnMovementComponent* const Movement = Pawn ? Pawn->GetMovementComponent() : nullptr)
		{
			bOnGround = !Movement->IsFalling();
		}
	}

	bLockAllowed = bOnGround && Velocity.SizeSquared() <= FMath::Square(MaxOwnerSpeed);
}

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGFootLock::ResetFeet()
{
	for (FFootState& State : FootStates)
	{
		State.bPlanted = false;
		State.bLocked = false;
		State.LockAlpha = 0.0f;
		State.bHasPrevious = false;
		State.bStepping = false;
		State.StepElapsed = 0.0f;
	}
	bHasLastComponent = false;
}

void FAnimNode_EGFootLock::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	ResetFeet();
}

void FAnimNode_EGFootLock::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	FootStates.SetNum(Feet.Num());
	for (int32 Index = 0; Index < Feet.Num(); ++Index)
	{
		FFootState& State = FootStates[Index];
		FBoneReference& Bone = Feet[Index].Foot;
		Bone.Initialize(RequiredBones);

		State.FootIndex = Bone.IsValidToEvaluate(RequiredBones) ? Bone.GetCompactPoseIndex(RequiredBones) : FCompactPoseBoneIndex(INDEX_NONE);
		State.KneeIndex = State.FootIndex.IsValid() ? RequiredBones.GetParentBoneIndex(State.FootIndex) : FCompactPoseBoneIndex(INDEX_NONE);
		State.HipIndex = State.KneeIndex.IsValid() ? RequiredBones.GetParentBoneIndex(State.KneeIndex) : FCompactPoseBoneIndex(INDEX_NONE);

		// Indices can change with the LOD: the speed history is no longer comparable.
		State.bHasPrevious = false;
	}
}

bool FAnimNode_EGFootLock::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	for (const FFootState& State : FootStates)
	{
		if (State.IsValid())
		{
			return true;
		}
	}
	return false;
}

void FAnimNode_EGFootLock::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	FrameDeltaTime = Context.GetDeltaTime();
}

// ---------------------------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------------------------

void FAnimNode_EGFootLock::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FTransform& ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	const FVector Up = ComponentToWorld.GetUnitAxis(EAxis::Z);
	const float DeltaTime = FMath::Max(FrameDeltaTime, UE_KINDA_SMALL_NUMBER);
	const float BlendRate = BlendTime > UE_KINDA_SMALL_NUMBER ? 1.0f / BlendTime : 1000.0f;

	if (bHasLastComponent && FVector::DistSquared(ComponentToWorld.GetLocation(), LastComponentLocation) > FMath::Square(TeleportDistance))
	{
		ResetFeet();
	}
	bHasLastComponent = true;
	LastComponentLocation = ComponentToWorld.GetLocation();

	// One procedural step at a time, so the character is never on no feet.
	bool bAnyStepping = FootStates.ContainsByPredicate([](const FFootState& State) { return State.bStepping; });

	for (FFootState& State : FootStates)
	{
		if (!State.IsValid())
		{
			continue;
		}

		const FTransform AnimComponent = Output.Pose.GetComponentSpaceTransform(State.FootIndex);
		const FTransform AnimWorld = AnimComponent * ComponentToWorld;

		// How fast the animation moves and turns the foot relative to the mesh. A capsule turning
		// under an idle pose leaves both at zero; a turn clip pivots its feet and steps.
		float Speed = 0.0f;
		float AngularSpeed = 0.0f;
		if (State.bHasPrevious)
		{
			Speed = static_cast<float>((AnimComponent.GetLocation() - State.PreviousAnimComponent.GetLocation()).Size()) / DeltaTime;
			AngularSpeed = FMath::RadiansToDegrees(static_cast<float>(AnimComponent.GetRotation().AngularDistance(State.PreviousAnimComponent.GetRotation()))) / DeltaTime;
		}
		State.PreviousAnimComponent = AnimComponent;
		State.bHasPrevious = true;

		// Hysteresis: planted below half the release speeds, released above them.
		const float SpeedLimit = State.bPlanted ? ReleaseSpeed : ReleaseSpeed * 0.5f;
		const float AngularLimit = State.bPlanted ? ReleaseAngularSpeed : ReleaseAngularSpeed * 0.5f;
		const bool bWasPlanted = State.bPlanted;
		State.bPlanted = bLockAllowed && Speed <= SpeedLimit && AngularSpeed <= AngularLimit;

		if (!bLockAllowed)
		{
			State.bStepping = false;
		}

		if (State.bPlanted || State.bStepping)
		{
			if (!State.bLocked)
			{
				State.bLocked = true;
				State.LockedWorld = AnimWorld;
			}
			else if (State.bPlanted && !bWasPlanted && !State.bStepping)
			{
				// Planted again while blending out: lock where the foot is shown now, not where it was.
				State.LockedWorld = BlendWorld(AnimWorld, State.LockedWorld, State.LockAlpha);
			}
			State.LockAlpha = FMath::FInterpConstantTo(State.LockAlpha, 1.0f, DeltaTime, BlendRate);
		}
		else
		{
			State.LockAlpha = FMath::FInterpConstantTo(State.LockAlpha, 0.0f, DeltaTime, BlendRate);
			if (State.LockAlpha <= 0.0f)
			{
				State.bLocked = false;
			}
		}

		// Too far from where the animation would put it, or turned too far: step there.
		if (State.bLocked && State.bPlanted && !State.bStepping && !bAnyStepping && State.LockAlpha >= 1.0f)
		{
			const FVector Drift = FVector::VectorPlaneProject(AnimWorld.GetLocation() - State.LockedWorld.GetLocation(), Up);
			const FQuat Delta = AnimWorld.GetRotation() * State.LockedWorld.GetRotation().Inverse();
			const float Twist = FMath::Abs(FMath::RadiansToDegrees(static_cast<float>(FMath::UnwindRadians(Delta.GetTwistAngle(Up)))));
			if (Drift.SizeSquared() > FMath::Square(StepDistance) || Twist > StepAngle)
			{
				State.bStepping = true;
				State.StepElapsed = 0.0f;
				State.StepFrom = State.LockedWorld;
				bAnyStepping = true;
			}
		}

		if (State.bStepping)
		{
			State.StepElapsed += DeltaTime;
			const float Alpha = FMath::Clamp(State.StepElapsed / FMath::Max(StepDuration, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);

			// Towards where the animation puts the foot now: the capsule may still be turning.
			State.LockedWorld = BlendWorld(State.StepFrom, AnimWorld, SmoothStep01(Alpha));
			State.LockedWorld.AddToTranslation(Up * (StepHeight * FMath::Sin(UE_PI * Alpha)));
			if (Alpha >= 1.0f)
			{
				State.bStepping = false;
				State.LockedWorld = AnimWorld;
			}
		}

		if (!State.bLocked || State.LockAlpha <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}

		const FTransform LockedComponent = State.LockedWorld.GetRelativeTransform(ComponentToWorld);
		const FVector Target = FMath::Lerp(AnimComponent.GetLocation(), LockedComponent.GetLocation(), static_cast<double>(State.LockAlpha));
		const FQuat TargetRotation = FQuat::Slerp(AnimComponent.GetRotation(), LockedComponent.GetRotation(), State.LockAlpha).GetNormalized();

		FTransform Hip = Output.Pose.GetComponentSpaceTransform(State.HipIndex);
		FTransform Knee = Output.Pose.GetComponentSpaceTransform(State.KneeIndex);
		FTransform Foot = AnimComponent;

		// Bend in the animation's own plane: away from the hip-foot line, through the knee.
		const FVector Bend = Knee.GetLocation() - (Hip.GetLocation() + Foot.GetLocation()) * 0.5;
		if (!Bend.IsNearlyZero(0.1))
		{
			State.BendDirection = Bend.GetSafeNormal();
		}
		if (State.BendDirection.IsNearlyZero())
		{
			continue;
		}
		const FVector JointTarget = Knee.GetLocation() + State.BendDirection * 50.0;

		AnimationCore::SolveTwoBoneIK(Hip, Knee, Foot, JointTarget, Target, bAllowStretching, 1.0, 1.2);
		Foot.SetRotation(TargetRotation);

		OutBoneTransforms.Add(FBoneTransform(State.HipIndex, Hip));
		OutBoneTransforms.Add(FBoneTransform(State.KneeIndex, Knee));
		OutBoneTransforms.Add(FBoneTransform(State.FootIndex, Foot));
	}

	// Blending component-space transforms expects them in bone order.
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

void FAnimNode_EGFootLock::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%, %s"), ActualAlpha * 100.0f, bLockAllowed ? TEXT("standing") : TEXT("moving"));
	for (int32 Index = 0; Index < FootStates.Num() && Index < Feet.Num(); ++Index)
	{
		const FFootState& State = FootStates[Index];
		DebugLine += FString::Printf(TEXT(", %s %s %.2f"), *Feet[Index].Foot.BoneName.ToString(),
			State.bStepping ? TEXT("stepping") : (State.bPlanted ? TEXT("planted") : TEXT("free")), State.LockAlpha);
	}
	DebugLine += TEXT(")");
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

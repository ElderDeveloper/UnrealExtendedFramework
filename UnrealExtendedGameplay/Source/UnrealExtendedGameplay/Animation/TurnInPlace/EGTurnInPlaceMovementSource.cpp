// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceMovementSource.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGRootMotionSource_SoftPush.h"

// ---------------------------------------------------------------------------------------------
// UEGTurnInPlaceMovementSource
// ---------------------------------------------------------------------------------------------

bool UEGTurnInPlaceMovementSource::GatherMovement_Implementation(const AActor* Owner, FEGTurnInPlaceMovementInfo& OutInfo) const
{
	OutInfo = FEGTurnInPlaceMovementInfo();
	const USceneComponent* const Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return false;
	}

	// Enough for a pawn that only rotates: no input, no root motion, facing its control rotation.
	OutInfo.bValid = true;
	OutInfo.Rotation = Root->GetComponentRotation();
	OutInfo.Velocity = Owner->GetVelocity();
	if (const APawn* const Pawn = Cast<APawn>(Owner))
	{
		OutInfo.bCanRotate = Pawn->GetController() != nullptr;
		OutInfo.bBotControlled = Pawn->IsBotControlled();
		OutInfo.ControlRotation = Pawn->GetControlRotation();
		OutInfo.bHasDesiredRotation = OutInfo.bCanRotate;
		OutInfo.DesiredRotation = OutInfo.ControlRotation;
	}
	return true;
}

void UEGTurnInPlaceMovementSource::ApplyRotation_Implementation(AActor* Owner, const FRotator& NewRotation) const
{
	USceneComponent* const Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}

	const FRotator Current = Root->GetComponentRotation();
	if (Current.Equals(NewRotation, 1.e-4f))
	{
		return;
	}

	// A yaw-only rotation of an upright capsule cannot collide: skip the sweep SetActorRotation does.
	if (FMath::IsNearlyEqual(Current.Pitch, NewRotation.Pitch, 1.e-3) && FMath::IsNearlyEqual(Current.Roll, NewRotation.Roll, 1.e-3))
	{
		Root->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::None);
	}
	else
	{
		Owner->SetActorRotation(NewRotation);
	}
}

USkeletalMeshComponent* UEGTurnInPlaceMovementSource::GetMesh_Implementation(const AActor* Owner) const
{
	return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UActorComponent* UEGTurnInPlaceMovementSource::GetMovementComponent_Implementation(const AActor* Owner) const
{
	const APawn* const Pawn = Cast<APawn>(Owner);
	return Pawn ? Pawn->GetMovementComponent() : nullptr;
}

UWorld* UEGTurnInPlaceMovementSource::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const UObject* const Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// UEGTurnInPlaceCharacterSource
// ---------------------------------------------------------------------------------------------

bool UEGTurnInPlaceCharacterSource::GatherMovement_Implementation(const AActor* Owner, FEGTurnInPlaceMovementInfo& OutInfo) const
{
	OutInfo = FEGTurnInPlaceMovementInfo();
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->UpdatedComponent || Character->IsPendingKillPending())
	{
		return false;
	}

	OutInfo.bValid = true;
	OutInfo.bCanRotate = Movement->MovementMode != MOVE_None && (Character->Controller || Movement->bRunPhysicsWithNoController);
	OutInfo.Rotation = Movement->UpdatedComponent->GetComponentRotation();
	OutInfo.Velocity = Movement->Velocity;
	OutInfo.Acceleration = Movement->GetCurrentAcceleration();
	OutInfo.MaxSpeed = Movement->GetMaxSpeed();
	OutInfo.MaxAcceleration = Movement->GetMaxAcceleration();
	OutInfo.bMovingOnGround = Movement->IsMovingOnGround();
	if (Movement->HasCustomGravity())
	{
		const FVector Up = (-Movement->GetGravityDirection()).GetSafeNormal();
		OutInfo.Up = Up.IsNearlyZero() ? FVector::UpVector : Up;
	}

	OutInfo.bOrientToMovement = Movement->bOrientRotationToMovement;
	if (!Movement->bOrientRotationToMovement && (Character->bUseControllerRotationPitch || Character->bUseControllerRotationYaw || Character->bUseControllerRotationRoll))
	{
		OutInfo.TurnMethod = EEGTurnMethod::FaceRotation;
	}
	else if (Movement->bOrientRotationToMovement || Movement->bUseControllerDesiredRotation)
	{
		OutInfo.TurnMethod = EEGTurnMethod::PhysicsRotation;
	}

	if (OutInfo.TurnMethod == EEGTurnMethod::FaceRotation && Character->Controller)
	{
		OutInfo.bHasDesiredRotation = true;
		OutInfo.DesiredRotation = Character->Controller->GetControlRotation();
	}
	else if (!Movement->bOrientRotationToMovement && Movement->bUseControllerDesiredRotation)
	{
		// As PhysicsRotation finds it: the controller, or the owner when running without one.
		const AController* Controller = Character->Controller;
		if (!Controller && Movement->bRunPhysicsWithNoController)
		{
			Controller = Cast<AController>(Character->GetOwner());
		}
		if (Controller)
		{
			OutInfo.bHasDesiredRotation = true;
			OutInfo.DesiredRotation = Controller->GetDesiredRotation();
		}
	}

	// Soft collision pushes are root motion sources too, but being jostled must not pause turning.
	OutInfo.bAnimRootMotion = Character->IsPlayingRootMotion();
	OutInfo.bHasRootMotionSources = EGSoftCollision::HasGameplayRootMotionSources(*Movement);
	if (Character->IsPlayingNetworkedRootMotionMontage())
	{
		const USkeletalMeshComponent* const Mesh = Character->GetMesh();
		const UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		const FAnimMontageInstance* const Instance = AnimInstance ? AnimInstance->GetRootMotionMontageInstance() : nullptr;
		OutInfo.RootMotionMontage = Instance ? Instance->Montage.Get() : nullptr;
	}

	OutInfo.bBotControlled = Character->IsBotControlled();
	OutInfo.ControlRotation = Character->GetControlRotation();

	if (!Movement->bIgnoreBaseRotation)
	{
		if (UPrimitiveComponent* const Base = Character->GetMovementBase())
		{
			OutInfo.Base = Base;
			OutInfo.BaseRotation = Base->GetComponentRotation();
		}
	}
	return true;
}

USkeletalMeshComponent* UEGTurnInPlaceCharacterSource::GetMesh_Implementation(const AActor* Owner) const
{
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	return Character ? Character->GetMesh() : Super::GetMesh_Implementation(Owner);
}

UActorComponent* UEGTurnInPlaceCharacterSource::GetMovementComponent_Implementation(const AActor* Owner) const
{
	const ACharacter* const Character = Cast<ACharacter>(Owner);
	return Character ? Character->GetCharacterMovement() : Super::GetMovementComponent_Implementation(Owner);
}

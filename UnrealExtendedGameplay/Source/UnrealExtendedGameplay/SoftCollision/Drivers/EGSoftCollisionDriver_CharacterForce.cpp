// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionDriver_CharacterForce.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionMath.h"

bool UEGSoftCollisionDriver_CharacterForce::SupportsPawn(const APawn& Pawn) const
{
	const ACharacter* Character = Cast<ACharacter>(&Pawn);
	return Character && Character->GetCharacterMovement();
}

bool UEGSoftCollisionDriver_CharacterForce::ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const
{
	OutHandle = 0;
	const UCharacterMovementComponent* Movement = Component.GetCharacterMovement();
	return Movement && Movement->MovementMode != MOVE_None && Command.Duration > 0.0f;
}

void UEGSoftCollisionDriver_CharacterForce::TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const
{
	UCharacterMovementComponent* Movement = Component.GetCharacterMovement();
	if (!Movement || Command.Duration <= 0.0f)
	{
		return;
	}

	const FVector Direction = Command.Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Alpha = Elapsed / Command.Duration;
	const float Target = static_cast<float>(Command.Velocity.Size2D()) * EGSoftCollision::EvaluateDecay(Command.Decay, Alpha);
	const float Deficit = ComputeVelocityDeficit(Movement->Velocity, Direction, Target);
	if (Deficit > UE_KINDA_SMALL_NUMBER)
	{
		Movement->AddImpulse(Direction * (Deficit * FMath::Clamp(Responsiveness, 0.0f, 1.0f)), true);
	}
}

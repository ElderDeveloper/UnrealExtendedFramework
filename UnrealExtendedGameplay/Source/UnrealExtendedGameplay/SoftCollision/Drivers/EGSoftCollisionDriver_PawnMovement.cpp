// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionDriver_PawnMovement.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionMath.h"

EEGSoftCollisionDriverRole UEGSoftCollisionDriver_PawnMovement::GetRole() const
{
	// This driver has no prediction keys of its own.
	return Role == EEGSoftCollisionDriverRole::Predicted ? EEGSoftCollisionDriverRole::OwnerAuthority : Role;
}

bool UEGSoftCollisionDriver_PawnMovement::SupportsPawn(const APawn& Pawn) const
{
	return Pawn.GetMovementComponent() != nullptr;
}

bool UEGSoftCollisionDriver_PawnMovement::ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const
{
	OutHandle = 0;
	const APawn* Pawn = Component.GetPawn();
	return Pawn && Pawn->GetMovementComponent() && Command.Duration > 0.0f;
}

void UEGSoftCollisionDriver_PawnMovement::TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const
{
	const APawn* Pawn = Component.GetPawn();
	UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	if (!Movement || Command.Duration <= 0.0f)
	{
		return;
	}

	const FVector Direction = Command.Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Target = static_cast<float>(Command.Velocity.Size2D()) * EGSoftCollision::EvaluateDecay(Command.Decay, Elapsed / Command.Duration);
	const float Deficit = ComputeVelocityDeficit(Movement->Velocity, Direction, Target);
	if (Deficit > UE_KINDA_SMALL_NUMBER)
	{
		Movement->Velocity += Direction * (Deficit * FMath::Clamp(Responsiveness, 0.0f, 1.0f));
	}
}

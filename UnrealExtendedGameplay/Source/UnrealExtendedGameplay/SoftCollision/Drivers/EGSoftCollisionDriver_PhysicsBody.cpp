// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionDriver_PhysicsBody.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionMath.h"

EEGSoftCollisionDriverRole UEGSoftCollisionDriver_PhysicsBody::GetRole() const
{
	return Role == EEGSoftCollisionDriverRole::Predicted ? EEGSoftCollisionDriverRole::ServerAuthority : Role;
}

UPrimitiveComponent* UEGSoftCollisionDriver_PhysicsBody::GetSimulatingRoot(const APawn* Pawn)
{
	UPrimitiveComponent* Root = Pawn ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	return Root && Root->IsSimulatingPhysics() ? Root : nullptr;
}

bool UEGSoftCollisionDriver_PhysicsBody::SupportsPawn(const APawn& Pawn) const
{
	return GetSimulatingRoot(&Pawn) != nullptr;
}

bool UEGSoftCollisionDriver_PhysicsBody::ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const
{
	OutHandle = 0;
	return GetSimulatingRoot(Component.GetPawn()) && Command.Duration > 0.0f;
}

void UEGSoftCollisionDriver_PhysicsBody::TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const
{
	UPrimitiveComponent* Root = GetSimulatingRoot(Component.GetPawn());
	if (!Root || Command.Duration <= 0.0f)
	{
		return;
	}

	const FVector Direction = Command.Velocity.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const float Target = static_cast<float>(Command.Velocity.Size2D()) * EGSoftCollision::EvaluateDecay(Command.Decay, Elapsed / Command.Duration);
	const float Deficit = ComputeVelocityDeficit(Root->GetPhysicsLinearVelocity(), Direction, Target);
	if (Deficit > UE_KINDA_SMALL_NUMBER)
	{
		Root->AddImpulse(Direction * (Deficit * FMath::Clamp(Responsiveness, 0.0f, 1.0f)), NAME_None, true);
	}
}

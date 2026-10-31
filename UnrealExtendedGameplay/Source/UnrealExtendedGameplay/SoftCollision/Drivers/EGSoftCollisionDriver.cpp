// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionDriver.h"

#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"

float UEGSoftCollisionDriver::ComputeVelocityDeficit(const FVector& CurrentVelocity, const FVector& Direction, float TargetSpeed)
{
	const float Along = static_cast<float>(FVector::DotProduct(CurrentVelocity, Direction));
	return FMath::Max(TargetSpeed - Along, 0.0f);
}

bool UEGSoftCollisionDriver_Blueprint::SupportsPawn(const APawn& Pawn) const
{
	return K2_SupportsPawn(&Pawn);
}

bool UEGSoftCollisionDriver_Blueprint::ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const
{
	OutHandle = K2_ApplyPush(&Component, Command);
	return OutHandle >= 0;
}

void UEGSoftCollisionDriver_Blueprint::TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const
{
	K2_TickPush(&Component, Command, Handle, Elapsed, DeltaSeconds);
}

void UEGSoftCollisionDriver_Blueprint::CancelPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle) const
{
	K2_CancelPush(&Component, Command, Handle);
}

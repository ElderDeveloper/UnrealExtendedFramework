// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceHooks.h"

bool EGTurnInPlace::HandleFaceRotation(ACharacter* Character, const FRotator& NewControlRotation, float DeltaTime)
{
	UEGTurnInPlaceComponent* const TurnInPlace = UEGTurnInPlaceComponent::FindFor(Character);
	return TurnInPlace && TurnInPlace->HandleFaceRotation(NewControlRotation, DeltaTime);
}

bool EGTurnInPlace::HandlePhysicsRotation(UCharacterMovementComponent* Movement, float DeltaTime)
{
	UEGTurnInPlaceComponent* const TurnInPlace = Movement ? UEGTurnInPlaceComponent::FindFor(Movement->GetCharacterOwner()) : nullptr;
	return TurnInPlace && TurnInPlace->HandlePhysicsRotation(Movement, DeltaTime);
}

bool EGTurnInPlace::ComputeOrientToMovementRotation(const UCharacterMovementComponent* Movement, const FRotator& CurrentRotation, FRotator& OutRotation)
{
	const UEGTurnInPlaceComponent* const TurnInPlace = Movement ? UEGTurnInPlaceComponent::FindFor(Movement->GetCharacterOwner()) : nullptr;
	return TurnInPlace && TurnInPlace->ComputeOrientToMovementRotation(Movement, CurrentRotation, OutRotation);
}

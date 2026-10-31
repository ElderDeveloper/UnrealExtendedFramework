// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionDriver_CharacterRootMotion.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGRootMotionSource_SoftPush.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionComponent.h"

bool UEGSoftCollisionDriver_CharacterRootMotion::SupportsPawn(const APawn& Pawn) const
{
	const ACharacter* Character = Cast<ACharacter>(&Pawn);
	return Character && Character->GetCharacterMovement();
}

bool UEGSoftCollisionDriver_CharacterRootMotion::ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const
{
	OutHandle = INDEX_NONE;
	UCharacterMovementComponent* Movement = Component.GetCharacterMovement();
	if (!Movement || Movement->MovementMode == MOVE_None || Command.Duration <= 0.0f)
	{
		return false;
	}

	const TSharedPtr<FEGRootMotionSource_SoftPush> Source = MakeShared<FEGRootMotionSource_SoftPush>();
	Source->InstanceName = InstanceName;
	Source->Priority = static_cast<uint16>(FMath::Clamp(Priority, 0, 65535));
	Source->Duration = Command.Duration;
	Source->Sequence = Command.Sequence;
	Source->Push = Command.Quantized;
	Source->Decay = Command.Decay;
	Source->bYieldToGameplayRootMotion = Command.bYieldToGameplayRootMotion;
	if (Command.bHasStartTime)
	{
		Source->StartTime = Command.StartTime;
	}
	if (Command.ElapsedTrim > 0.0f)
	{
		Source->SetTime(FMath::Min(Command.ElapsedTrim, Command.Duration));
	}

	const uint16 SourceId = Movement->ApplyRootMotionSource(Source);
	if (SourceId == static_cast<uint16>(ERootMotionSourceID::Invalid))
	{
		return false;
	}

	OutHandle = SourceId;
	return true;
}

void UEGSoftCollisionDriver_CharacterRootMotion::CancelPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle) const
{
	UCharacterMovementComponent* Movement = Component.GetCharacterMovement();
	if (Movement && Handle > 0 && Handle <= 65535)
	{
		Movement->RemoveRootMotionSourceByID(static_cast<uint16>(Handle));
	}
}

bool UEGSoftCollisionDriver_CharacterRootMotion::IsPushRunning(const UEGSoftCollisionComponent& Component, int32 Handle) const
{
	// GetRootMotionSourceByID is not const; the lookup changes nothing.
	UCharacterMovementComponent* Movement = Component.GetCharacterMovement();
	if (!Movement || Handle <= 0 || Handle > 65535)
	{
		return false;
	}

	const TSharedPtr<FRootMotionSource> Source = Movement->GetRootMotionSourceByID(static_cast<uint16>(Handle));
	return Source.IsValid() && !Source->Status.HasFlag(ERootMotionSourceStatusFlags::Finished);
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGControlMovementNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

TMap<TWeakObjectPtr<UCharacterMovementComponent>, UEGControlMovementNotify::FMovementLock> UEGControlMovementNotify::ActiveLocks;

UCharacterMovementComponent* UEGControlMovementNotify::GetMovement(const USkeletalMeshComponent* MeshComp)
{
	const ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	return Character ? Character->GetCharacterMovement() : nullptr;
}

void UEGControlMovementNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UCharacterMovementComponent* Movement = GetMovement(MeshComp);
	if (!Movement)
	{
		return;
	}

	// Drop entries for characters destroyed mid-window.
	for (auto It = ActiveLocks.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	FMovementLock& Lock = ActiveLocks.FindOrAdd(TWeakObjectPtr<UCharacterMovementComponent>(Movement));
	if (Lock.Count++ == 0)
	{
		Lock.SavedMaxAcceleration = Movement->MaxAcceleration;
	}
	Movement->MaxAcceleration = 0.0f;
}

void UEGControlMovementNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UCharacterMovementComponent* Movement = GetMovement(MeshComp);
	if (!Movement)
	{
		return;
	}

	const TWeakObjectPtr<UCharacterMovementComponent> Key(Movement);
	FMovementLock* Lock = ActiveLocks.Find(Key);
	if (!Lock)
	{
		return;
	}

	if (--Lock->Count <= 0)
	{
		Movement->MaxAcceleration = Lock->SavedMaxAcceleration;
		ActiveLocks.Remove(Key);
	}
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGControlMovementNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UEGControlMovementNotify::UEGControlMovementNotify()
{
	// Matches the notify's behaviour before modifiers existed: input locked, speed and turning untouched.
	Modifier.AccelerationScale = 0.0f;
}

void UEGControlMovementNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ACharacter* const Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	UEGMovementModifierComponent* const Modifiers = UEGMovementModifierComponent::FindOrAdd(Character);
	if (!Modifiers)
	{
		return;
	}

	for (auto It = ActiveHandles.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const FEGMovementModifierHandle Handle = Modifiers->PushModifier(Modifier);
	if (Handle.IsValid())
	{
		ActiveHandles.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(Handle);
	}
}

void UEGControlMovementNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	TArray<FEGMovementModifierHandle>* Handles = ActiveHandles.Find(Key);
	if (!Handles || Handles->Num() == 0)
	{
		return;
	}

	FEGMovementModifierHandle Handle = (*Handles)[0];
	Handles->RemoveAt(0);
	if (Handles->Num() == 0)
	{
		ActiveHandles.Remove(Key);
	}

	ACharacter* const Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (UEGMovementModifierComponent* Modifiers = Character ? Character->FindComponentByClass<UEGMovementModifierComponent>() : nullptr)
	{
		Modifiers->RemoveModifier(Handle);
	}
}

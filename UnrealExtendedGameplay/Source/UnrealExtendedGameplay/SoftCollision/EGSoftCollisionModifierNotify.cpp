// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionModifierNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGSoftCollisionComponent.h"

UEGSoftCollisionModifierNotify::UEGSoftCollisionModifierNotify()
{
	Modifier.bImmovable = true;
}

void UEGSoftCollisionModifierNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UEGSoftCollisionComponent* const SoftCollision = MeshComp ? UEGSoftCollisionComponent::FindSoftCollisionComponent(MeshComp->GetOwner()) : nullptr;
	if (!SoftCollision)
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

	const FEGSoftCollisionModifierHandle Handle = SoftCollision->PushModifier(Modifier);
	if (Handle.IsValid())
	{
		ActiveHandles.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(Handle);
	}
}

void UEGSoftCollisionModifierNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	TArray<FEGSoftCollisionModifierHandle>* Handles = ActiveHandles.Find(Key);
	if (!Handles || Handles->IsEmpty())
	{
		return;
	}

	FEGSoftCollisionModifierHandle Handle = (*Handles)[0];
	Handles->RemoveAt(0);
	if (Handles->IsEmpty())
	{
		ActiveHandles.Remove(Key);
	}

	if (UEGSoftCollisionComponent* const SoftCollision = MeshComp ? UEGSoftCollisionComponent::FindSoftCollisionComponent(MeshComp->GetOwner()) : nullptr)
	{
		SoftCollision->RemoveModifier(Handle);
	}
}

#if WITH_EDITOR
FString UEGSoftCollisionModifierNotify::GetNotifyName_Implementation() const
{
	return Modifier.bImmovable ? TEXT("Soft Collision: Immovable") : TEXT("Soft Collision Modifier");
}
#endif

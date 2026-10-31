// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimEventReceiver.h"

#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

AActor* FEGAnimEvent::GetOwner() const
{
	return Mesh ? Mesh->GetOwner() : nullptr;
}

int32 EGAnimEvents::NewWindowId()
{
	// Notifies fire on the game thread; the counter wraps long before it matters.
	static int32 NextId = 0;
	NextId = (NextId == MAX_int32) ? 1 : NextId + 1;
	return NextId;
}

void EGAnimEvents::Dispatch(const FEGAnimEvent& Event)
{
	USkeletalMeshComponent* const Mesh = Event.Mesh;
	if (!IsValid(Mesh) || !Event.Tag.IsValid())
	{
		return;
	}

	if (AActor* const Owner = Mesh->GetOwner())
	{
		if (Owner->Implements<UEGAnimEventReceiver>())
		{
			IEGAnimEventReceiver::Execute_ReceiveAnimEvent(Owner, Event);
		}

		// Copy first: a receiver may add or remove components while handling the event.
		TInlineComponentArray<UActorComponent*> Components(Owner);
		for (UActorComponent* Component : Components)
		{
			if (IsValid(Component) && Component->Implements<UEGAnimEventReceiver>())
			{
				IEGAnimEventReceiver::Execute_ReceiveAnimEvent(Component, Event);
			}
		}
	}

	if (UAnimInstance* const AnimInstance = Mesh->GetAnimInstance())
	{
		if (AnimInstance->Implements<UEGAnimEventReceiver>())
		{
			IEGAnimEventReceiver::Execute_ReceiveAnimEvent(AnimInstance, Event);
		}
	}
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimEventNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGAnimEventReceiver.h"

FString UEGAnimEventNotifyBase::GetNotifyName_Implementation() const
{
	const FGameplayTag Tag = GetEventTag();
	return Tag.IsValid() ? Tag.ToString() : Super::GetNotifyName_Implementation();
}

void UEGAnimEventNotifyBase::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	FEGAnimEvent Event;
	Event.Tag = GetEventTag();
	Event.Phase = EEGAnimEventPhase::Trigger;
	Event.Payload = GetEventPayload();
	Event.Mesh = MeshComp;
	Event.Animation = Animation;
	EGAnimEvents::Dispatch(Event);
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimEventComponent.h"

UEGAnimEventComponent::UEGAnimEventComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEGAnimEventComponent::PassesFilter(const FGameplayTag& Tag) const
{
	return TagFilter.IsEmpty() || Tag.MatchesAny(TagFilter);
}

void UEGAnimEventComponent::ReceiveAnimEvent_Implementation(const FEGAnimEvent& Event)
{
	if (!PassesFilter(Event.Tag))
	{
		return;
	}

	switch (Event.Phase)
	{
	case EEGAnimEventPhase::Begin:
		ActiveWindows.Add(Event.WindowId, Event.Tag);
		break;
	case EEGAnimEventPhase::End:
		ActiveWindows.Remove(Event.WindowId);
		break;
	default:
		break;
	}

	OnAnimEventNative.Broadcast(Event);
	OnAnimEvent.Broadcast(Event);
}

bool UEGAnimEventComponent::IsWindowActive(FGameplayTag Tag) const
{
	for (const TPair<int32, FGameplayTag>& Window : ActiveWindows)
	{
		if (Window.Value.MatchesTag(Tag))
		{
			return true;
		}
	}
	return false;
}

FGameplayTagContainer UEGAnimEventComponent::GetActiveWindowTags() const
{
	FGameplayTagContainer Tags;
	for (const TPair<int32, FGameplayTag>& Window : ActiveWindows)
	{
		Tags.AddTag(Window.Value);
	}
	return Tags;
}

void UEGAnimEventComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveWindows.Reset();
	Super::EndPlay(EndPlayReason);
}

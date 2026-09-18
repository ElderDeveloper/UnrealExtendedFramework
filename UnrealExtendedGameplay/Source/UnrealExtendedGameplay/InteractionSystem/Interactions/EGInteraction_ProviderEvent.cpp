// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteraction_ProviderEvent.h"

UEGInteraction_ProviderEvent::UEGInteraction_ProviderEvent()
{
	NetPolicy = EEGInteractionNetPolicy::ServerOnly;
}

void UEGInteraction_ProviderEvent::ActivateInteraction_Implementation()
{
	if (HasAuthority())
	{
		SendProviderEvent(EEGInteractionEventPhase::Completed);
	}
	EndInteraction(false);
}

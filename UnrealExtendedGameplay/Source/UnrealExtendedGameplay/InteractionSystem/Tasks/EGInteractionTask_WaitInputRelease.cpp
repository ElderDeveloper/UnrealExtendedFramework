// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTask_WaitInputRelease.h"

UEGInteractionTask_WaitInputRelease* UEGInteractionTask_WaitInputRelease::WaitInputRelease(UEGInteraction* OwningInteraction)
{
	return NewInteractionTask<UEGInteractionTask_WaitInputRelease>(OwningInteraction);
}

void UEGInteractionTask_WaitInputRelease::Activate()
{
	Super::Activate();

	UEGInteraction* Owner = Interaction.Get();
	if (!Owner || !Owner->IsActive())
	{
		EndTask();
		return;
	}

	ReleaseHandle = Owner->OnInputReleasedNative.AddUObject(this, &UEGInteractionTask_WaitInputRelease::HandleInputReleased);
}

void UEGInteractionTask_WaitInputRelease::HandleInputReleased()
{
	if (ShouldBroadcast())
	{
		OnReleased.Broadcast();
	}
	EndTask();
}

void UEGInteractionTask_WaitInputRelease::OnDestroy(bool bInOwnerFinished)
{
	if (UEGInteraction* Owner = Interaction.Get())
	{
		if (ReleaseHandle.IsValid())
		{
			Owner->OnInputReleasedNative.Remove(ReleaseHandle);
		}
	}
	ReleaseHandle.Reset();

	Super::OnDestroy(bInOwnerFinished);
}

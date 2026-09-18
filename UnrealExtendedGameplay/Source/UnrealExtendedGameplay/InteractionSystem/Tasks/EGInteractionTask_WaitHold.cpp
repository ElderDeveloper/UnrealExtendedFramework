// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTask_WaitHold.h"

UEGInteractionTask_WaitHold::UEGInteractionTask_WaitHold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UEGInteractionTask_WaitHold* UEGInteractionTask_WaitHold::WaitHold(UEGInteraction* OwningInteraction, float Duration, bool bCancelOnInputRelease)
{
	UEGInteractionTask_WaitHold* Task = NewInteractionTask<UEGInteractionTask_WaitHold>(OwningInteraction);
	if (Task)
	{
		Task->Duration = FMath::Max(Duration, 0.0f);
		Task->bCancelOnInputRelease = bCancelOnInputRelease;
	}
	return Task;
}

void UEGInteractionTask_WaitHold::Activate()
{
	Super::Activate();

	UEGInteraction* Owner = Interaction.Get();
	if (!Owner || !Owner->IsActive())
	{
		EndTask();
		return;
	}

	if (bCancelOnInputRelease)
	{
		ReleaseHandle = Owner->OnInputReleasedNative.AddUObject(this, &UEGInteractionTask_WaitHold::HandleInputReleased);
	}

	if (Duration <= 0.0f)
	{
		if (ShouldBroadcast())
		{
			OnProgress.Broadcast(1.0f);
			OnCompleted.Broadcast();
		}
		EndTask();
	}
}

void UEGInteractionTask_WaitHold::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	Elapsed += DeltaTime;
	const float Progress = GetProgress();
	if (ShouldBroadcast())
	{
		OnProgress.Broadcast(Progress);
	}

	if (Elapsed < Duration)
	{
		return;
	}

	if (ShouldBroadcast())
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}

float UEGInteractionTask_WaitHold::GetProgress() const
{
	return Duration > 0.0f ? FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f) : 1.0f;
}

void UEGInteractionTask_WaitHold::Cancel()
{
	if (IsFinished())
	{
		return;
	}

	if (ShouldBroadcast())
	{
		OnCancelled.Broadcast();
	}
	EndTask();
}

void UEGInteractionTask_WaitHold::ExternalCancel()
{
	Cancel();
}

void UEGInteractionTask_WaitHold::HandleInputReleased()
{
	Cancel();
}

void UEGInteractionTask_WaitHold::OnDestroy(bool bInOwnerFinished)
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

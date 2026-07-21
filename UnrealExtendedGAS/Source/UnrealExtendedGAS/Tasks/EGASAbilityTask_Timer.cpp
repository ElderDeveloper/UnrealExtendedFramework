#include "Tasks/EGASAbilityTask_Timer.h"

#include "AbilitySystemComponent.h"
#include "TimerManager.h"

UEGASAbilityTask_Timer* UEGASAbilityTask_Timer::WaitTimer(UGameplayAbility* OwningAbility, float Time, bool bLooping, bool bTriggerImmediately)
{
	UEGASAbilityTask_Timer* Task = NewAbilityTask<UEGASAbilityTask_Timer>(OwningAbility);
	Task->Time = Time;
	Task->bUseAttributeInterval = false;
	Task->bLooping = bLooping;
	Task->bTriggerImmediately = bTriggerImmediately;
	return Task;
}

UEGASAbilityTask_Timer* UEGASAbilityTask_Timer::WaitTimerFromAttribute(UGameplayAbility* OwningAbility, FGameplayAttribute IntervalAttribute, bool bLooping, bool bTriggerImmediately)
{
	UEGASAbilityTask_Timer* Task = NewAbilityTask<UEGASAbilityTask_Timer>(OwningAbility);
	Task->IntervalAttribute = IntervalAttribute;
	Task->bUseAttributeInterval = true;
	Task->bLooping = bLooping;
	Task->bTriggerImmediately = bTriggerImmediately;
	return Task;
}

void UEGASAbilityTask_Timer::Activate()
{
	if (bUseAttributeInterval && !IntervalAttribute.IsValid())
	{
		BroadcastFinished();
		EndTask();
		return;
	}

	if (!bUseAttributeInterval && Time < 0.0f)
	{
		BroadcastFinished();
		EndTask();
		return;
	}

	if (bTriggerImmediately)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnTimer.Broadcast();
		}

		if (!bLooping || bStopped)
		{
			BroadcastFinished();
			EndTask();
			return;
		}
	}

	ScheduleNext();
}

void UEGASAbilityTask_Timer::OnDestroy(bool bInOwnerFinished)
{
	ClearTimer();
	Super::OnDestroy(bInOwnerFinished);
}

void UEGASAbilityTask_Timer::StopTimer()
{
	if (bStopped)
	{
		return;
	}

	bStopped = true;
	ClearTimer();
	BroadcastFinished();
	EndTask();
}

float UEGASAbilityTask_Timer::GetNextInterval() const
{
	constexpr float MinInterval = 0.05f;

	if (bUseAttributeInterval)
	{
		if (const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
		{
			return FMath::Max(MinInterval, ASC->GetNumericAttribute(IntervalAttribute));
		}
		return MinInterval;
	}

	return FMath::Max(MinInterval, Time);
}

void UEGASAbilityTask_Timer::ClearTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}

void UEGASAbilityTask_Timer::ScheduleNext()
{
	UWorld* World = GetWorld();
	if (!World || bStopped)
	{
		BroadcastFinished();
		EndTask();
		return;
	}

	World->GetTimerManager().SetTimer(TimerHandle, this, &UEGASAbilityTask_Timer::HandleTimer, GetNextInterval(), false);
}

void UEGASAbilityTask_Timer::HandleTimer()
{
	if (bStopped)
	{
		return;
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTimer.Broadcast();
	}

	if (bLooping && !bStopped && !IsFinished())
	{
		ScheduleNext();
		return;
	}

	BroadcastFinished();
	EndTask();
}

void UEGASAbilityTask_Timer::BroadcastFinished()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}
}

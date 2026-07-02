#include "Tasks/EGASAbilityTask_OnTick.h"

UEGASAbilityTask_OnTick::UEGASAbilityTask_OnTick()
{
	bTickingTask = true;
}

UEGASAbilityTask_OnTick* UEGASAbilityTask_OnTick::WaitOnTick(UGameplayAbility* OwningAbility, FName TaskInstanceName, float Duration)
{
	UEGASAbilityTask_OnTick* Task = NewAbilityTask<UEGASAbilityTask_OnTick>(OwningAbility, TaskInstanceName);
	Task->Duration = Duration;
	return Task;
}

void UEGASAbilityTask_OnTick::Activate()
{
	ElapsedTime = 0.0f;
}

void UEGASAbilityTask_OnTick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	ElapsedTime += DeltaTime;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnTick.Broadcast(DeltaTime, ElapsedTime);
	}

	if (Duration >= 0.0f && ElapsedTime >= Duration)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast(DeltaTime, ElapsedTime);
		}
		EndTask();
	}
}

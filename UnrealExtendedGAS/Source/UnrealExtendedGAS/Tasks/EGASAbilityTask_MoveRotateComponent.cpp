#include "Tasks/EGASAbilityTask_MoveRotateComponent.h"

UEGASAbilityTask_MoveRotateComponent::UEGASAbilityTask_MoveRotateComponent()
{
	bTickingTask = true;
}

UEGASAbilityTask_MoveRotateComponent* UEGASAbilityTask_MoveRotateComponent::MoveRotateComponent(UGameplayAbility* OwningAbility, USceneComponent* Component, FVector TargetRelativeLocation, FRotator TargetRelativeRotation, float Duration)
{
	UEGASAbilityTask_MoveRotateComponent* Task = NewAbilityTask<UEGASAbilityTask_MoveRotateComponent>(OwningAbility);
	Task->Component = Component;
	Task->TargetRelativeLocation = TargetRelativeLocation;
	Task->TargetRelativeRotation = TargetRelativeRotation;
	Task->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	return Task;
}

void UEGASAbilityTask_MoveRotateComponent::Activate()
{
	if (!Component)
	{
		EndTask();
		return;
	}

	StartRelativeLocation = Component->GetRelativeLocation();
	StartRelativeRotation = Component->GetRelativeRotation();
}

void UEGASAbilityTask_MoveRotateComponent::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!Component)
	{
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	Component->SetRelativeLocationAndRotation(
		FMath::Lerp(StartRelativeLocation, TargetRelativeLocation, Alpha),
		FMath::Lerp(StartRelativeRotation, TargetRelativeRotation, Alpha));

	if (Alpha >= 1.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast();
		}
		EndTask();
	}
}

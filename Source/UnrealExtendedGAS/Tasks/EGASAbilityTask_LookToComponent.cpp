#include "Tasks/EGASAbilityTask_LookToComponent.h"

#include "GameFramework/Actor.h"

UEGASAbilityTask_LookToComponent::UEGASAbilityTask_LookToComponent()
{
	bTickingTask = true;
}

UEGASAbilityTask_LookToComponent* UEGASAbilityTask_LookToComponent::LookToComponent(UGameplayAbility* OwningAbility, USceneComponent* TargetComponent, float Duration, bool bYawOnly)
{
	UEGASAbilityTask_LookToComponent* Task = NewAbilityTask<UEGASAbilityTask_LookToComponent>(OwningAbility);
	Task->TargetComponent = TargetComponent;
	Task->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	Task->bYawOnly = bYawOnly;
	return Task;
}

void UEGASAbilityTask_LookToComponent::Activate()
{
	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor || !TargetComponent)
	{
		EndTask();
		return;
	}

	StartRotation = AvatarActor->GetActorRotation();
}

void UEGASAbilityTask_LookToComponent::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor || !TargetComponent)
	{
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	FRotator TargetRotation = (TargetComponent->GetComponentLocation() - AvatarActor->GetActorLocation()).Rotation();
	if (bYawOnly)
	{
		TargetRotation.Pitch = StartRotation.Pitch;
		TargetRotation.Roll = StartRotation.Roll;
	}

	AvatarActor->SetActorRotation(FMath::Lerp(StartRotation, TargetRotation, Alpha));

	if (Alpha >= 1.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast();
		}
		EndTask();
	}
}

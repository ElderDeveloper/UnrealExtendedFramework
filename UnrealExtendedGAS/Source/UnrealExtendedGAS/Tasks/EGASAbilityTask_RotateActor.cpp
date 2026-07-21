#include "Tasks/EGASAbilityTask_RotateActor.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"

UEGASAbilityTask_RotateActor::UEGASAbilityTask_RotateActor()
{
	bTickingTask = true;
}

UEGASAbilityTask_RotateActor* UEGASAbilityTask_RotateActor::CreateRotateActorTask(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	AActor* InTargetActor,
	FRotator InStartRotation,
	FRotator InTargetRotation,
	float InDuration,
	UCurveFloat* OptionalInterpolationCurve)
{
	UEGASAbilityTask_RotateActor* Task = NewAbilityTask<UEGASAbilityTask_RotateActor>(OwningAbility, TaskInstanceName);
	Task->TargetActor = InTargetActor;
	Task->StartRotation = InStartRotation;
	Task->TargetRotation = InTargetRotation;
	Task->Duration = FMath::Max(InDuration, KINDA_SMALL_NUMBER);
	Task->Curve = OptionalInterpolationCurve;
	return Task;
}

void UEGASAbilityTask_RotateActor::Activate()
{
	Super::Activate();

	if (!TargetActor)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast();
		}
		EndTask();
		return;
	}

	ElapsedTime = 0.0f;
	UpdateRotation(0.0f);
}

void UEGASAbilityTask_RotateActor::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!TargetActor)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast();
		}
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;
	const float LinearAlpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	const float RotationAlpha = Curve ? Curve->GetFloatValue(LinearAlpha) : LinearAlpha;
	UpdateRotation(RotationAlpha);

	if (ElapsedTime >= Duration)
	{
		CompleteTask();
	}
}

void UEGASAbilityTask_RotateActor::UpdateRotation(float Alpha) const
{
	if (TargetActor)
	{
		TargetActor->SetActorRotation(FMath::Lerp(StartRotation, TargetRotation, Alpha));
	}
}

void UEGASAbilityTask_RotateActor::CompleteTask()
{
	UpdateRotation(Curve ? Curve->GetFloatValue(1.0f) : 1.0f);
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}
	EndTask();
}

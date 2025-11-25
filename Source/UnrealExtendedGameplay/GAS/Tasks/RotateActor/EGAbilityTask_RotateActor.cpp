#include "EGAbilityTask_RotateActor.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"

UEGAbilityTask_RotateActor::UEGAbilityTask_RotateActor()
{
	bTickingTask = true;
	CurrentTime = 0.0f;
	Duration = 1.0f;
	StartRotation = FRotator::ZeroRotator;
	TargetRotation = FRotator::ZeroRotator;
	TargetActor = nullptr;
	Curve = nullptr;
}

UEGAbilityTask_RotateActor* UEGAbilityTask_RotateActor::CreateRotateActorTask(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	AActor* TargetActor,
	FRotator StartRotation,
	FRotator TargetRotation,
	float Duration,
	UCurveFloat* OptionalInterpolationCurve)
{
	UEGAbilityTask_RotateActor* MyTask = NewAbilityTask<UEGAbilityTask_RotateActor>(OwningAbility, TaskInstanceName);

	MyTask->TargetActor = TargetActor;
	MyTask->StartRotation = StartRotation;
	MyTask->TargetRotation = TargetRotation;
	MyTask->Duration = FMath::Max(Duration, 0.001f); // Avoid division by zero
	MyTask->Curve = OptionalInterpolationCurve;

	return MyTask;
}

void UEGAbilityTask_RotateActor::Activate()
{
	Super::Activate();

	if (!TargetActor)
	{
		// If no target is set, we can try to use the avatar, or just fail.
		// Here we fail to be safe and explicit as per the input arguments.
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast();
		}
		EndTask();
		return;
	}

	// Set initial state
	CurrentTime = 0.0f;
	UpdateRotation(0.0f);
}

void UEGAbilityTask_RotateActor::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

void UEGAbilityTask_RotateActor::TickTask(float DeltaTime)
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

	CurrentTime += DeltaTime;

	float Alpha = FMath::Clamp(CurrentTime / Duration, 0.0f, 1.0f);

	if (Curve)
	{
		Alpha = Curve->GetFloatValue(Alpha);
	}

	UpdateRotation(Alpha);

	if (CurrentTime >= Duration)
	{
		CompleteTask();
	}
}

void UEGAbilityTask_RotateActor::UpdateRotation(float Alpha)
{
	if (TargetActor)
	{
		FRotator NewRotation = FMath::Lerp(StartRotation, TargetRotation, Alpha);
		TargetActor->SetActorRotation(NewRotation);
	}
}

void UEGAbilityTask_RotateActor::CompleteTask()
{
	// Ensure we hit the exact target at the end
	if (Curve)
	{
		// If using a curve, the end value depends on the curve's value at 1.0
		// usually curves are normalized 0-1, but we respect the curve's output.
		UpdateRotation(Curve->GetFloatValue(1.0f));
	}
	else
	{
		UpdateRotation(1.0f);
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinished.Broadcast();
	}
	EndTask();
}
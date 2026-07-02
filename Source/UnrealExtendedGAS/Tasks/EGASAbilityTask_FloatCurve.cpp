#include "Tasks/EGASAbilityTask_FloatCurve.h"

#include "Curves/CurveFloat.h"

UEGASAbilityTask_FloatCurve::UEGASAbilityTask_FloatCurve()
{
	bTickingTask = true;
}

UEGASAbilityTask_FloatCurve* UEGASAbilityTask_FloatCurve::PlayFloatCurve(UGameplayAbility* OwningAbility, UCurveFloat* Curve, float Duration, bool bUseCurveTimeRange)
{
	UEGASAbilityTask_FloatCurve* Task = NewAbilityTask<UEGASAbilityTask_FloatCurve>(OwningAbility);
	Task->Curve = Curve;
	Task->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
	Task->bUseCurveTimeRange = bUseCurveTimeRange;
	return Task;
}

void UEGASAbilityTask_FloatCurve::Activate()
{
	if (!Curve)
	{
		EndTask();
		return;
	}

	if (bUseCurveTimeRange)
	{
		Curve->GetTimeRange(MinCurveTime, MaxCurveTime);
		Duration = FMath::Max(MaxCurveTime - MinCurveTime, KINDA_SMALL_NUMBER);
	}
}

void UEGASAbilityTask_FloatCurve::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!Curve)
	{
		EndTask();
		return;
	}

	ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	const float CurveTime = bUseCurveTimeRange ? FMath::Lerp(MinCurveTime, MaxCurveTime, Alpha) : ElapsedTime;
	const float Value = Curve->GetFloatValue(CurveTime);

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnUpdate.Broadcast(Value, ElapsedTime);
	}

	if (Alpha >= 1.0f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast(Value, ElapsedTime);
		}
		EndTask();
	}
}

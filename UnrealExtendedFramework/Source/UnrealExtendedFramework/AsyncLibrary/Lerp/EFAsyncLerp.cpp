// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncLerp.h"

UEFAsyncLerp* UEFAsyncLerp::AsyncLerp(const UObject* WorldContextObject, float Duration, bool Reversed,bool EaseInEaseOut)
{
	UEFAsyncLerp* Node = NewObject<UEFAsyncLerp>();
	Node->LerpDuration = Duration;
	Node->Reversed = Reversed;
	Node->EaseInEaseOut = EaseInEaseOut;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}

void UEFAsyncLerp::Timer()
{
	if (IsFinished())
	{
		return;
	}

	// Whoever started the lerp (or its world) is gone: stop without firing anything.
	UWorld* LerpWorld = LerpTimer.GetTimerWorld();
	if (!LerpWorld || !GetWorldContextObject())
	{
		CancelWork();
		FinishAction();
		return;
	}

	const double CurrentGameTime = LerpWorld->GetTimeSeconds();

	if (CurrentGameTime > TargetGameTime)
	{
		if (!FinalLoop)
		{
			FinalLoop = true;
			Loop.Broadcast(Reversed ? 0.f : 1.f);
			return;
		}

		CancelWork();
		if (FinishAction())
		{
			Finished.Broadcast();
		}
		return;
	}

	const double Span = TargetGameTime - CreatedGameTime;
	const float Ratio = Span > 0.0 ? static_cast<float>((CurrentGameTime - CreatedGameTime) / Span) : 1.f;

	if (Reversed)
	{
		LerpAlpha = EaseInEaseOut ? FMath::InterpEaseInOut(1.f, 0.f, Ratio, 2.f) : 1.f - Ratio;
	}
	else
	{
		LerpAlpha = EaseInEaseOut ? FMath::InterpEaseInOut(0.f, 1.f, Ratio, 2.f) : Ratio;
	}

	Loop.Broadcast(LerpAlpha);
}

void UEFAsyncLerp::ExecuteAction()
{
	if (!LerpTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncLerp::Timer), 0.01f, true))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
		return;
	}

	CreatedGameTime = LerpTimer.GetTimerWorld()->GetTimeSeconds();
	TargetGameTime = CreatedGameTime + LerpDuration;
	Timer();
}

void UEFAsyncLerp::CancelWork()
{
	LerpTimer.Clear();
}

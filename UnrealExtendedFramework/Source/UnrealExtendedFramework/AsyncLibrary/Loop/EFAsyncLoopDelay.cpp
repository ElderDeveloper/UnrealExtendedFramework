// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncLoopDelay.h"

#include "UnrealExtendedFramework/UnrealExtendedFramework.h"


void UEFAsyncLoopDelay::TickLoop()
{
	if (IsFinished())
	{
		return;
	}

	// Whoever started the loop is gone: stop without firing anything.
	if (!GetWorldContextObject())
	{
		StopLoop();
		return;
	}

	if (!ShouldLoop)
	{
		// A single delay: done once it fires.
		CancelWork();
		if (FinishAction())
		{
			Loop.Broadcast();
		}
		return;
	}

	Loop.Broadcast();
}



void UEFAsyncLoopDelay::ExecuteAction()
{
	if (LoopTime <= 0.f && ShouldLoop)
	{
		UE_LOG(LogExtendedFramework, Warning, TEXT("Extended Loop Delay: Delay must be greater than zero to loop (got %f); the loop does not run."), LoopTime);
		FailInvalidArgument(TEXT("Delay must be greater than zero"));
		return;
	}

	if (!LoopTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncLoopDelay::TickLoop), LoopTime, ShouldLoop))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}



UEFAsyncLoopDelay* UEFAsyncLoopDelay::LoopDelay(const UObject* WorldContextObject, bool shouldLoop, float Delay)
{
	UEFAsyncLoopDelay* Node = NewObject<UEFAsyncLoopDelay>();
	Node->LoopTime = Delay;
	Node->ShouldLoop = shouldLoop;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}

void UEFAsyncLoopDelay::StopLoop()
{
	CancelWork();
	FinishAction();
}

void UEFAsyncLoopDelay::CancelWork()
{
	LoopTimer.Clear();
}

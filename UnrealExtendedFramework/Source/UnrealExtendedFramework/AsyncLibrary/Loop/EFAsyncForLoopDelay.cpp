// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncForLoopDelay.h"

#include "UnrealExtendedFramework/UnrealExtendedFramework.h"






void UEFAsyncForLoopDelay::InternalTick()
{
	if (IsFinished())
	{
		return;
	}

	// Whoever started the loop is gone: stop without firing anything.
	if (!GetWorldContextObject())
	{
		CancelWork();
		FinishAction();
		return;
	}

	if (LoopIndex < LastArrayIndex)
	{
		const int32 CurrentIndex = LoopIndex++;
		Loop.Broadcast(CurrentIndex);
	}

	else
	{
		InternalCompleted();
	}

}


void UEFAsyncForLoopDelay::InternalCompleted()
{
	CancelWork();
	if (FinishAction())
	{
		Completed.Broadcast(LastArrayIndex);
	}
}


UEFAsyncForLoopDelay* UEFAsyncForLoopDelay::EFForLoopWithDelay(const int32 FirstIndex , const int32 LastIndex ,const UObject* WorldContextObject , float Delay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UEFAsyncForLoopDelay* Node = NewObject<UEFAsyncForLoopDelay>();
	Node->LoopTime = Delay;
	Node->FirstArrayIndex = FirstIndex;
	Node->LastArrayIndex = LastIndex;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}

void UEFAsyncForLoopDelay::ExecuteAction()
{
	if (LoopTime <= 0.f)
	{
		UE_LOG(LogExtendedFramework, Warning, TEXT("For Loop With Delay: Delay must be greater than zero (got %f); the loop does not run."), LoopTime);
		FailInvalidArgument(TEXT("Delay must be greater than zero"));
		return;
	}

	LoopIndex = FirstArrayIndex;
	if (!LoopTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncForLoopDelay::InternalTick), LoopTime, true))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}

void UEFAsyncForLoopDelay::CancelWork()
{
	LoopTimer.Clear();
}

// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncForEachLoopDelay.h"

#include "UnrealExtendedFramework/UnrealExtendedFramework.h"



void UEFAsyncForEachLoopDelay::InternalTick()
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

	if (!Array.IsValidIndex(LoopIndex))
	{
		InternalCompleted();
		return;
	}

	const int32 CurrentIndex = LoopIndex++;
	Loop.Broadcast(CurrentIndex);

	if (CurrentIndex == Array.Num() - 1)
	{
		InternalCompleted();
	}
}


void UEFAsyncForEachLoopDelay::InternalCompleted()
{
	CancelWork();
	if (FinishAction())
	{
		Completed.Broadcast(Array.Num() - 1);
	}
}


UEFAsyncForEachLoopDelay* UEFAsyncForEachLoopDelay::ForEachLoopDelayObject(const TArray<UObject*>& TargetArray,const UObject* WorldContextObject, float Delay, bool bExecuteFirstWithoutDelay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UEFAsyncForEachLoopDelay* Node = NewObject<UEFAsyncForEachLoopDelay>();
	Node->LoopTime = Delay;
	Node->Array = TargetArray;
	Node->ExecuteFirstWithoutDelay = bExecuteFirstWithoutDelay;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}


void UEFAsyncForEachLoopDelay::ExecuteAction()
{
	if (LoopTime <= 0.f)
	{
		UE_LOG(LogExtendedFramework, Warning, TEXT("For Each Loop Delay: Delay must be greater than zero (got %f); the loop does not run."), LoopTime);
		FailInvalidArgument(TEXT("Delay must be greater than zero"));
		return;
	}

	if (!LoopTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncForEachLoopDelay::InternalTick), LoopTime, true))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
		return;
	}

	if (ExecuteFirstWithoutDelay)
	{
		InternalTick();
	}
}


void UEFAsyncForEachLoopDelay::CancelWork()
{
	LoopTimer.Clear();
}

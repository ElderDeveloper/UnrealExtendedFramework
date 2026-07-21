// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncForEachLoopDelay.h"



void UEFAsyncForEachLoopDelay::InternalTick()
{
	if (Array.IsValidIndex(LoopIndex))
	{
		Loop.Broadcast(LoopIndex);
		
		if (Array.Num()-1 == LoopIndex)
		{
			InternalCompleted();
		}
	}
	else
	{
		InternalCompleted();
	}
	LoopIndex++;
}


void UEFAsyncForEachLoopDelay::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
		SetReadyToDestroy();
		ConditionalBeginDestroy();
	}
}


UEFAsyncForEachLoopDelay* UEFAsyncForEachLoopDelay::ForEachLoopDelayObject(const TArray<UObject*>& TargetArray,const UObject* WorldContextObject, float Delay, bool bExecuteFirstWithoutDelay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UEFAsyncForEachLoopDelay* Node = NewObject<UEFAsyncForEachLoopDelay>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = Delay;
		Node->Array = TargetArray;
		Node->ExecuteFirstWithoutDelay = bExecuteFirstWithoutDelay;
	}
	return Node;
}


void UEFAsyncForEachLoopDelay::Activate()
{
	if (WorldContext)
	{
		if (ExecuteFirstWithoutDelay)
		{
			InternalTick();
		}
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UEFAsyncForEachLoopDelay::InternalTick,LoopTime,true);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncForLoopDelay.h"






void UEFAsyncForLoopDelay::InternalTick()
{
	if (LoopIndex < LastArrayIndex)
	{
		Loop.Broadcast(LoopIndex);
		LoopIndex++;
	}
	
	else
	{
		InternalCompleted();
	}

}


void UEFAsyncForLoopDelay::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
	TimerHandle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}


UEFAsyncForLoopDelay* UEFAsyncForLoopDelay::EFForLoopWithDelay(const int32 FirstIndex , const int32 LastIndex ,const UObject* WorldContextObject , float Delay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UEFAsyncForLoopDelay* Node = NewObject<UEFAsyncForLoopDelay>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = Delay;
		Node->FirstArrayIndex = FirstIndex;
		Node->LastArrayIndex = LastIndex;
	}
	return Node;
}

void UEFAsyncForLoopDelay::Activate()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UEFAsyncForLoopDelay::InternalTick,LoopTime,true);
	}
}

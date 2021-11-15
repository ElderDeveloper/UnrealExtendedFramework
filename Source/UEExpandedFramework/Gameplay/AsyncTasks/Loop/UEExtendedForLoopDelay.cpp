// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedForLoopDelay.h"






void UUEExtendedForLoopDelay::InternalTick()
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


void UUEExtendedForLoopDelay::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
	TimerHandle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}


UUEExtendedForLoopDelay* UUEExtendedForLoopDelay::ForLoopWithDelay(const int32 FirstIndex , const int32 LastIndex ,const UObject* WorldContextObject , float Delay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UUEExtendedForLoopDelay* Node = NewObject<UUEExtendedForLoopDelay>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = Delay;
		Node->FirstArrayIndex = FirstIndex;
		Node->LastArrayIndex = LastIndex;
	}
	return Node;
}

void UUEExtendedForLoopDelay::Activate()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UUEExtendedForLoopDelay::InternalTick,LoopTime,true);
	}
}

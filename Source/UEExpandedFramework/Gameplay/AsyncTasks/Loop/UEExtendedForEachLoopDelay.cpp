// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedForEachLoopDelay.h"

#include "Kismet/KismetSystemLibrary.h"




void UUEExtendedForEachLoopDelay::InternalTick()
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


void UUEExtendedForEachLoopDelay::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
		SetReadyToDestroy();
		MarkPendingKill();
	}
}


UUEExtendedForEachLoopDelay* UUEExtendedForEachLoopDelay::ForEachLoopDelayObject(const TArray<UObject*>& TargetArray,const UObject* WorldContextObject, float Delay)
{
	if (!ensure(WorldContextObject))	return nullptr;

	UUEExtendedForEachLoopDelay* Node = NewObject<UUEExtendedForEachLoopDelay>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = Delay;
		Node->Array = TargetArray;
	}
	return Node;
}

void UUEExtendedForEachLoopDelay::Activate()
{
	if (WorldContext)
	{

		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UUEExtendedForEachLoopDelay::InternalTick,LoopTime,true);
	}
}

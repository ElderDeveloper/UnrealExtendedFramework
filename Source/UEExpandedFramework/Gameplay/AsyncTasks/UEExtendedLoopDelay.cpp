// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedLoopDelay.h"

void UUEExtendedLoopDelay::TickLoop()
{
	Loop.Broadcast();
	if (!ShouldLoop)
	{
		StopLoop();
	}
}



void UUEExtendedLoopDelay::Activate()
{
	Super::Activate();

	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UUEExtendedLoopDelay::TickLoop,LoopTime,ShouldLoop);
	}
}



UUEExtendedLoopDelay* UUEExtendedLoopDelay::LoopDelay(const UObject* WorldContextObject, bool shouldLoop, float Delay)
{
	UUEExtendedLoopDelay* Node = NewObject<UUEExtendedLoopDelay>();
	if (Node)
	{
		Node->WorldContext=WorldContextObject;
		Node->LoopTime=Delay;
		Node->ShouldLoop=shouldLoop;
	}
	return Node;
}

void UUEExtendedLoopDelay::StopLoop()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}

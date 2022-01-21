// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncLoopDelay.h"


void UEFAsyncLoopDelay::TickLoop()
{
	Loop.Broadcast();
	if (!ShouldLoop)
	{
		StopLoop();
	}
}



void UEFAsyncLoopDelay::Activate()
{
	Super::Activate();

	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFAsyncLoopDelay::TickLoop,LoopTime,ShouldLoop);
	}
}



UEFAsyncLoopDelay* UEFAsyncLoopDelay::LoopDelay(const UObject* WorldContextObject, bool shouldLoop, float Delay)
{
	UEFAsyncLoopDelay* Node = NewObject<UEFAsyncLoopDelay>();
	if (Node)
	{
		Node->WorldContext=WorldContextObject;
		Node->LoopTime=Delay;
		Node->ShouldLoop=shouldLoop;
	}
	return Node;
}

void UEFAsyncLoopDelay::StopLoop()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}

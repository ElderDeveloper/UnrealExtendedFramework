// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncDelay.h"



void UEFAsyncDelay::FinishDelay()
{
	Finished.Broadcast();
}


void UEFAsyncDelay::Activate()
{
	Super::Activate();

	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFAsyncDelay::FinishDelay,DelayTime,false);
	}
}


UEFAsyncDelay* UEFAsyncDelay::EFAsyncDelay(const UObject* WorldContextObject, float Delay)
{
	UEFAsyncDelay* Node = NewObject<UEFAsyncDelay>();
	if (Node)
	{
		Node->WorldContext=WorldContextObject;
		Node->DelayTime = Delay;
	}
	return Node;
}

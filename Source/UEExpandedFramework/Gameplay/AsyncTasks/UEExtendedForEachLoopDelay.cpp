// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedForEachLoopDelay.h"

#include "Kismet/KismetSystemLibrary.h"

bool UUEExtendedForEachLoopDelay::bActive=false; //Init bactive for all instances





void UUEExtendedForEachLoopDelay::InternalTick()
{
	LoopIndex++;
	if (Array.IsValidIndex(LoopIndex))
	{
		Loop.Broadcast(LoopIndex);
		OnTookTake(1);
		if (Array.Num()-1 == LoopIndex)
		{
			InternalCompleted();
		}
	}
	else
	{
		InternalCompleted();
	}
}


void UUEExtendedForEachLoopDelay::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
		bActive=false;
	}
}


UUEExtendedForEachLoopDelay* UUEExtendedForEachLoopDelay::ForEachLoopDelay(const TArray<FProperty*>& TargetArray,const UObject* WorldContextObject, float Delay)
{
	UUEExtendedForEachLoopDelay* Node = NewObject<UUEExtendedForEachLoopDelay>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime=Delay;
		Node->Array = TargetArray;
		Node->LoopIndex=-1;
	}
	return Node;
}

void UUEExtendedForEachLoopDelay::Activate()
{
	if (bActive)
	{
		FFrame::KismetExecutionMessage(TEXT("Async action is already running"), ELogVerbosity::Warning);
		return;
	}

	FFrame::KismetExecutionMessage(TEXT("Started Activate!"), ELogVerbosity::Log);

	if (WorldContext)
	{
		bActive=true;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this,&UUEExtendedForEachLoopDelay::InternalTick);
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, LoopTime, true);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid world context obj"), ELogVerbosity::Error);
	}

}

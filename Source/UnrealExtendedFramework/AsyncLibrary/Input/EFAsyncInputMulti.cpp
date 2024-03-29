﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncInputMulti.h"




UEFAsyncInputMulti* UEFAsyncInputMulti::EFInputMultiClick(const UObject* WorldContextObject,FName ActionName, float clickWaitTime , int32 maxClickCount)
{
	if (MultiInputName.Num()>0)
	{
		if (const auto Action = MultiInputName.Find(ActionName) )
		{
			Action->Get()->BroadcastClickIndex();
			return Action->Get();
		}

		if (const auto ActionObject = NewObject<UEFAsyncInputMulti>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->ClickWaitTime=clickWaitTime;
			ActionObject->ClickIndex = 0;
			ActionObject->ActionName = ActionName;
			ActionObject->MaxClickCount = maxClickCount;
			MultiInputName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	
	}
	else
	{
		if (const auto ActionObject = NewObject<UEFAsyncInputMulti>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->ClickWaitTime=clickWaitTime;
			ActionObject->ClickIndex = 0;
			ActionObject->ActionName = ActionName;
			ActionObject->MaxClickCount = maxClickCount;
			MultiInputName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	}

	return nullptr;
}

void UEFAsyncInputMulti::BroadcastClickIndex()
{
	switch (ClickIndex)
	{
	case 0: DoubleClick.Broadcast(ClickIndex); ClickIndex++;
		return;
	case 1: TripleClick.Broadcast(ClickIndex); ClickIndex++;
		return;
	default: ClickIndex = FMath::Clamp(ClickIndex + 1 ,  0 , MaxClickCount); break;
	}
}


void UEFAsyncInputMulti::PassTime()
{
	MultiInputName.Remove(ActionName);
	End.Broadcast(ClickIndex);
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	ConditionalBeginDestroy();
	
}




void UEFAsyncInputMulti::Activate()
{
	Super::Activate();
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFAsyncInputMulti::PassTime,ClickWaitTime,false);
	}
}

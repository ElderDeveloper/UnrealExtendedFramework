// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncInputHold.h"



UEFAsyncInputHold* UEFAsyncInputHold::EFStartInputAction(const UObject* WorldContextObject,float holdTime, int32 ActionIndex)
{
	if (InputHolds.Num()>0)
	{
		if (const auto Action = InputHolds.Find(ActionIndex) )
		{
			return Action->Get();
		}
		else
		{
			if (const auto ActionObject = NewObject<UEFAsyncInputHold>())
			{
				ActionObject->WorldContext=WorldContextObject;
				ActionObject->HoldTime=holdTime;
				InputHolds.Add(ActionIndex,ActionObject);
				return ActionObject;
			}
		}
	}
	else
	{
		if (const auto ActionObject = NewObject<UEFAsyncInputHold>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->HoldTime=holdTime;
			InputHolds.Add(ActionIndex,ActionObject);
			return ActionObject;
		}
	}

	return nullptr;
	
}



void UEFAsyncInputHold::EFEndInputAction(const UObject* WorldContextObject, int32 ActionIndex)
{
	if(InputHolds.Num()>0)
	{
		if(const auto Action = InputHolds.Find(ActionIndex))
		{
			if (Action->Get()->isHold)
			{
				Action->Get()->BroadcastHold();
			}
			else
			{
				Action->Get()->BroadcastPress();
			}
			Action->Get()->StopAction();
			InputHolds.Remove(ActionIndex);
		}
	}
}



void UEFAsyncInputHold::StopAction()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	ConditionalBeginDestroy();
}




void UEFAsyncInputHold::PassTime()
{
	TimePassed +=0.02;
	if (TimePassed >= HoldTime)
	{
		HoldExceeded.Broadcast();
		isHold = true;
		if (WorldContext)
		{
			WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
		}
	}
}




void UEFAsyncInputHold::Activate()
{
	Super::Activate();
	TimePassed = 0;
	isHold = false;
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFAsyncInputHold::PassTime,0.02,true);
	}
}

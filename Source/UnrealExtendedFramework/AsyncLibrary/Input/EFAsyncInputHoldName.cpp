// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncInputHoldName.h"




UEFAsyncInputHoldName* UEFAsyncInputHoldName::EFStartInputActionName(const UObject* WorldContextObject,FName ActionName, float holdTime)
{
	if (InputHoldsName.Num()>0)
	{
		if (const auto Action = InputHoldsName.Find(ActionName) )
		{
			return Action->Get();
		}
		else
		{
			if (const auto ActionObject = NewObject<UEFAsyncInputHoldName>())
			{
				ActionObject->WorldContext=WorldContextObject;
				ActionObject->HoldTime=holdTime;
				InputHoldsName.Add(ActionName,ActionObject);
				return ActionObject;
			}
		}
	}
	else
	{
		if (const auto ActionObject = NewObject<UEFAsyncInputHoldName>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->HoldTime=holdTime;
			InputHoldsName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	}

	return nullptr;
}




void UEFAsyncInputHoldName::EFEndInputActionName(const UObject* WorldContextObject, FName ActionName)
{
	if(InputHoldsName.Num()>0)
	{
		if(const auto Action = InputHoldsName.Find(ActionName))
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
			InputHoldsName.Remove(ActionName);
		}
	}
}




void UEFAsyncInputHoldName::StopAction()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}




void UEFAsyncInputHoldName::PassTime()
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




void UEFAsyncInputHoldName::Activate()
{
	Super::Activate();
	TimePassed = 0;
	isHold = false;
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFAsyncInputHoldName::PassTime,0.02,true);
	}
}

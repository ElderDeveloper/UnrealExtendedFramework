// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedButtonHoldName.h"

#include "UEExtendedButtonHold.h"



UUEExtendedButtonHoldName* UUEExtendedButtonHoldName::ExtendedStartButtonActionName(const UObject* WorldContextObject,FName ActionName, float holdTime)
{
	if (ButtonHoldsName.Num()>0)
	{
		if (const auto Action = ButtonHoldsName.Find(ActionName) )
		{
			return Action->Get();
		}
		else
		{
			if (const auto ActionObject = NewObject<UUEExtendedButtonHoldName>())
			{
				ActionObject->WorldContext=WorldContextObject;
				ActionObject->HoldTime=holdTime;
				ButtonHoldsName.Add(ActionName,ActionObject);
				return ActionObject;
			}
		}
	}
	else
	{
		if (const auto ActionObject = NewObject<UUEExtendedButtonHoldName>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->HoldTime=holdTime;
			ButtonHoldsName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	}

	return nullptr;
}




void UUEExtendedButtonHoldName::ExtendedEndButtonActionName(const UObject* WorldContextObject, FName ActionName)
{
	if(ButtonHoldsName.Num()>0)
	{
		if(const auto Action = ButtonHoldsName.Find(ActionName))
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
			ButtonHoldsName.Remove(ActionName);
		}
	}
}




void UUEExtendedButtonHoldName::StopAction()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}




void UUEExtendedButtonHoldName::PassTime()
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




void UUEExtendedButtonHoldName::Activate()
{
	Super::Activate();
	TimePassed = 0;
	isHold = false;
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UUEExtendedButtonHoldName::PassTime,0.02,true);
	}
}

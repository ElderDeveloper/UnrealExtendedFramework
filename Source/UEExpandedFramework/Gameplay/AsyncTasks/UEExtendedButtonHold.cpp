// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedButtonHold.h"


UUEExtendedButtonHold* UUEExtendedButtonHold::ExtendedStartButtonAction(const UObject* WorldContextObject,float holdTime, int32 ActionIndex)
{
	ButtonHolds.SetNum(256);

	if (ButtonHolds[ActionIndex].IsValid())
	{
		return nullptr;
	}
		
	if (const auto ActionObject = NewObject<UUEExtendedButtonHold>())
	{
		ActionObject->WorldContext=WorldContextObject;
		ActionObject->HoldTime=holdTime;
		ButtonHolds.EmplaceAt(ActionIndex,ActionObject);
		return ActionObject;
	}

	
	return nullptr;
	
}

void UUEExtendedButtonHold::ExtendedEndButtonAction(const UObject* WorldContextObject, int32 ActionIndex)
{
	if(ButtonHolds.IsValidIndex(ActionIndex))
	{
		if (ButtonHolds[ActionIndex].IsValid())
		{
			if(ButtonHolds[ActionIndex].Get()->isHold)
			{
				ButtonHolds[ActionIndex].Get()->BroadcastHold();
			}
			else
			{
				ButtonHolds[ActionIndex].Get()->BroadcastPress();
			}
		}
	}
		ButtonHolds[ActionIndex] = nullptr;
}


void UUEExtendedButtonHold::StopAction()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
	SetReadyToDestroy();
	MarkPendingKill();
}




void UUEExtendedButtonHold::PassTime()
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



void UUEExtendedButtonHold::Activate()
{
	Super::Activate();
	TimePassed = 0;
	isHold = false;
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UUEExtendedButtonHold::PassTime,0.02,true);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedButtonHold.h"


UUEExtendedButtonHold* UUEExtendedButtonHold::ExtendedStartButtonAction(const UObject* WorldContextObject,float holdTime, int32 ActionIndex)
{
	if (LoopReferenceArray.IsValidIndex(ActionIndex))
	{
		if (LoopReferenceArray[ActionIndex])
		{
			return nullptr;
		}

		const auto ActionObject = NewObject<UUEExtendedButtonHold>();
		if (ActionObject)
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->HoldTime=holdTime;
			LoopReferenceArray.Insert(ActionObject,ActionIndex);
		}
		return ActionObject;
	}

		const auto ActionObject = NewObject<UUEExtendedButtonHold>();
			if (ActionObject)
			{
				ActionObject->WorldContext=WorldContextObject;
				ActionObject->HoldTime=holdTime;
				LoopReferenceArray.Insert(ActionObject,ActionIndex);
			}
			return ActionObject;
	

	
	
}

void UUEExtendedButtonHold::ExtendedEndButtonAction(const UObject* WorldContextObject, int32 ActionIndex)
{
	if(LoopReferenceArray.IsValidIndex(ActionIndex))
	{
		if (LoopReferenceArray[ActionIndex])
		{
			if(LoopReferenceArray[ActionIndex]->isHold)
			{
				LoopReferenceArray[ActionIndex]->BroadcastHold();
			}
			else
			{
				LoopReferenceArray[ActionIndex]->BroadcastPress();
			}
		}
	}
		LoopReferenceArray[ActionIndex] = nullptr;
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

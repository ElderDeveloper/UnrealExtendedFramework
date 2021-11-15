// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedButtonMultiClick.h"



UUEExtendedButtonMultiClick* UUEExtendedButtonMultiClick::ExtendedButtonMultiClick(const UObject* WorldContextObject,FName ActionName, float clickWaitTime , int32 maxClickCount)
{
	if (ButtonClicksName.Num()>0)
	{
		if (const auto Action = ButtonClicksName.Find(ActionName) )
		{
			Action->Get()->BroadcastClickIndex();
			return Action->Get();
		}

		if (const auto ActionObject = NewObject<UUEExtendedButtonMultiClick>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->ClickWaitTime=clickWaitTime;
			ActionObject->ClickIndex = 0;
			ActionObject->ActionName = ActionName;
			ActionObject->MaxClickCount = maxClickCount;
			ButtonClicksName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	
	}
	else
	{
		if (const auto ActionObject = NewObject<UUEExtendedButtonMultiClick>())
		{
			ActionObject->WorldContext=WorldContextObject;
			ActionObject->ClickWaitTime=clickWaitTime;
			ActionObject->ClickIndex = 0;
			ActionObject->ActionName = ActionName;
			ActionObject->MaxClickCount = maxClickCount;
			ButtonClicksName.Add(ActionName,ActionObject);
			return ActionObject;
		}
	}

	return nullptr;
}

void UUEExtendedButtonMultiClick::BroadcastClickIndex()
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


void UUEExtendedButtonMultiClick::PassTime()
{
	ButtonClicksName.Remove(ActionName);
	End.Broadcast(ClickIndex);
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
    SetReadyToDestroy();
    MarkPendingKill();
	
}




void UUEExtendedButtonMultiClick::Activate()
{
	Super::Activate();
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(Handle,this,&UUEExtendedButtonMultiClick::PassTime,ClickWaitTime,false);
	}
}

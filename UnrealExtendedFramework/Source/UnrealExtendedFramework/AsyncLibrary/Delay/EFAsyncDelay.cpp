// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncDelay.h"



UEFAsyncDelay* UEFAsyncDelay::EFAsyncDelay(const UObject* WorldContextObject, float Delay)
{
	UEFAsyncDelay* Node = NewObject<UEFAsyncDelay>();
	Node->DelayTime = Delay;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}


void UEFAsyncDelay::ExecuteAction()
{
	if (!DelayTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncDelay::FinishDelay), DelayTime, false))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}


void UEFAsyncDelay::CancelWork()
{
	DelayTimer.Clear();
}


void UEFAsyncDelay::FinishDelay()
{
	DelayTimer.Clear();
	if (!FinishAction())
	{
		return;
	}
	Finished.Broadcast();
}

// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncInputMulti.h"


namespace EFAsyncInputMultiPrivate
{
	/** The click sequence waiting for its next click, per Action Name. */
	TMap<FName, TWeakObjectPtr<UEFAsyncInputMulti>> RunningClicks;

	UEFAsyncInputMulti* FindRunning(FName ActionName)
	{
		const TWeakObjectPtr<UEFAsyncInputMulti>* Found = RunningClicks.Find(ActionName);
		UEFAsyncInputMulti* Running = Found ? Found->Get() : nullptr;
		return Running && !Running->IsFinished() ? Running : nullptr;
	}
}


UEFAsyncInputMulti* UEFAsyncInputMulti::EFInputMultiClick(const UObject* WorldContextObject,FName ActionName, float clickWaitTime , int32 maxClickCount)
{
	// Another click while the sequence waits: count it and wait again.
	if (UEFAsyncInputMulti* Running = EFAsyncInputMultiPrivate::FindRunning(ActionName))
	{
		Running->BroadcastClickIndex();
		Running->StartClickWindow();
		return Running;
	}

	UEFAsyncInputMulti* ActionObject = NewObject<UEFAsyncInputMulti>();
	ActionObject->ClickWaitTime = clickWaitTime;
	ActionObject->ClickIndex = 0;
	ActionObject->ActionName = ActionName;
	ActionObject->MaxClickCount = maxClickCount;
	ActionObject->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	EFAsyncInputMultiPrivate::RunningClicks.Add(ActionName, ActionObject);
	return ActionObject;
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
	CancelWork();
	if (FinishAction())
	{
		End.Broadcast(ClickIndex);
	}
}




void UEFAsyncInputMulti::ExecuteAction()
{
	StartClickWindow();
}


void UEFAsyncInputMulti::StartClickWindow()
{
	if (IsFinished())
	{
		return;
	}

	// A wait of zero or less ends the sequence on the next tick.
	if (!ClickTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncInputMulti::PassTime), ClickWaitTime, false))
	{
		CancelWork();
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}


void UEFAsyncInputMulti::CancelWork()
{
	ClickTimer.Clear();

	const TWeakObjectPtr<UEFAsyncInputMulti>* Registered = EFAsyncInputMultiPrivate::RunningClicks.Find(ActionName);
	if (Registered && (!Registered->IsValid() || Registered->Get() == this))
	{
		EFAsyncInputMultiPrivate::RunningClicks.Remove(ActionName);
	}
}


void UEFAsyncInputMulti::BroadcastFailure(const FEFResult& /*Result*/)
{
	End.Broadcast(ClickIndex);
}

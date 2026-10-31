// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncInputHold.h"


namespace EFAsyncInputHoldPrivate
{
	/** The running hold of each Action Index. */
	TMap<int32, TWeakObjectPtr<UEFAsyncInputHold>> RunningHolds;

	UEFAsyncInputHold* FindRunning(int32 ActionIndex)
	{
		const TWeakObjectPtr<UEFAsyncInputHold>* Found = RunningHolds.Find(ActionIndex);
		UEFAsyncInputHold* Running = Found ? Found->Get() : nullptr;
		return Running && !Running->IsFinished() ? Running : nullptr;
	}
}


UEFAsyncInputHold* UEFAsyncInputHold::EFStartInputAction(const UObject* WorldContextObject,float holdTime, int32 ActionIndex)
{
	// Already running for this index: the same node measures a new hold. One whose starter is gone
	// (destroyed while the button was held) ends silently instead, so it cannot swallow this press.
	if (UEFAsyncInputHold* Running = EFAsyncInputHoldPrivate::FindRunning(ActionIndex))
	{
		if (Running->GetWorldContextObject())
		{
			Running->StartHold();
			return Running;
		}
		Running->Cancel();
	}

	UEFAsyncInputHold* ActionObject = NewObject<UEFAsyncInputHold>();
	ActionObject->HoldTime = holdTime;
	ActionObject->MappedActionIndex = ActionIndex;
	ActionObject->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	EFAsyncInputHoldPrivate::RunningHolds.Add(ActionIndex, ActionObject);
	return ActionObject;
}



void UEFAsyncInputHold::EFEndInputAction(const UObject* WorldContextObject, int32 ActionIndex)
{
	if (UEFAsyncInputHold* Running = EFAsyncInputHoldPrivate::FindRunning(ActionIndex))
	{
		Running->ReleaseInput();
	}
	else
	{
		EFAsyncInputHoldPrivate::RunningHolds.Remove(ActionIndex);
	}
}



void UEFAsyncInputHold::ReleaseInput()
{
	const bool bWasHeld = isHold;
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bWasHeld)
	{
		Hold.Broadcast();
	}
	else
	{
		Press.Broadcast();
	}
}




void UEFAsyncInputHold::PassTime()
{
	if (IsFinished())
	{
		return;
	}

	// Whoever started the hold is gone: stop without firing anything.
	if (!GetWorldContextObject())
	{
		CancelWork();
		FinishAction();
		return;
	}

	TimePassed += 0.02f;
	if (TimePassed >= HoldTime)
	{
		isHold = true;
		HoldTimer.Clear();
		HoldExceeded.Broadcast();
	}
}




void UEFAsyncInputHold::ExecuteAction()
{
	StartHold();
}


void UEFAsyncInputHold::StartHold()
{
	if (IsFinished())
	{
		return;
	}

	TimePassed = 0;
	isHold = false;
	if (!HoldTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncInputHold::PassTime), 0.02f, true))
	{
		CancelWork();
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}


void UEFAsyncInputHold::CancelWork()
{
	HoldTimer.Clear();

	const TWeakObjectPtr<UEFAsyncInputHold>* Registered = EFAsyncInputHoldPrivate::RunningHolds.Find(MappedActionIndex);
	if (Registered && (!Registered->IsValid() || Registered->Get() == this))
	{
		EFAsyncInputHoldPrivate::RunningHolds.Remove(MappedActionIndex);
	}
}

// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncInputHoldName.h"


namespace EFAsyncInputHoldNamePrivate
{
	/** The running hold of each Action Name. */
	TMap<FName, TWeakObjectPtr<UEFAsyncInputHoldName>> RunningHolds;

	UEFAsyncInputHoldName* FindRunning(FName ActionName)
	{
		const TWeakObjectPtr<UEFAsyncInputHoldName>* Found = RunningHolds.Find(ActionName);
		UEFAsyncInputHoldName* Running = Found ? Found->Get() : nullptr;
		return Running && !Running->IsFinished() ? Running : nullptr;
	}
}


UEFAsyncInputHoldName* UEFAsyncInputHoldName::EFStartInputActionName(const UObject* WorldContextObject,FName ActionName, float holdTime)
{
	// Already running for this name: the same node measures a new hold. One whose starter is gone
	// (destroyed while the button was held) ends silently instead, so it cannot swallow this press.
	if (UEFAsyncInputHoldName* Running = EFAsyncInputHoldNamePrivate::FindRunning(ActionName))
	{
		if (Running->GetWorldContextObject())
		{
			Running->StartHold();
			return Running;
		}
		Running->Cancel();
	}

	UEFAsyncInputHoldName* ActionObject = NewObject<UEFAsyncInputHoldName>();
	ActionObject->HoldTime = holdTime;
	ActionObject->MappedActionName = ActionName;
	ActionObject->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	EFAsyncInputHoldNamePrivate::RunningHolds.Add(ActionName, ActionObject);
	return ActionObject;
}




void UEFAsyncInputHoldName::EFEndInputActionName(const UObject* WorldContextObject, FName ActionName)
{
	if (UEFAsyncInputHoldName* Running = EFAsyncInputHoldNamePrivate::FindRunning(ActionName))
	{
		Running->ReleaseInput();
	}
	else
	{
		EFAsyncInputHoldNamePrivate::RunningHolds.Remove(ActionName);
	}
}




void UEFAsyncInputHoldName::ReleaseInput()
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




void UEFAsyncInputHoldName::PassTime()
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




void UEFAsyncInputHoldName::ExecuteAction()
{
	StartHold();
}


void UEFAsyncInputHoldName::StartHold()
{
	if (IsFinished())
	{
		return;
	}

	TimePassed = 0;
	isHold = false;
	if (!HoldTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncInputHoldName::PassTime), 0.02f, true))
	{
		CancelWork();
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}


void UEFAsyncInputHoldName::CancelWork()
{
	HoldTimer.Clear();

	const TWeakObjectPtr<UEFAsyncInputHoldName>* Registered = EFAsyncInputHoldNamePrivate::RunningHolds.Find(MappedActionName);
	if (Registered && (!Registered->IsValid() || Registered->Get() == this))
	{
		EFAsyncInputHoldNamePrivate::RunningHolds.Remove(MappedActionName);
	}
}

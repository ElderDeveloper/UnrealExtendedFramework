// Copyright Moon Punch Games. All Rights Reserved.

#include "Async/EFAsyncActionBase.h"

void UEFAsyncActionBase::InitAsyncAction(UObject* WorldContextObject, float InTimeoutSeconds)
{
	WorldContext = WorldContextObject;
	TimeoutSeconds = FMath::Max(InTimeoutSeconds, 0.f);
	if (WorldContextObject)
	{
		RegisterWithGameInstance(WorldContextObject);
	}
}

void UEFAsyncActionBase::Activate()
{
	if (bActivated)
	{
		return;
	}
	bActivated = true;

	// A failure found in the factory waited for this moment, when the output pins are bound.
	if (PendingFailure.IsSet())
	{
		const FEFResult Failure = PendingFailure.GetValue();
		PendingFailure.Reset();
		BroadcastFailure(Failure);
		return;
	}

	if (bFinished)
	{
		return;
	}

	if (TimeoutSeconds > 0.f)
	{
		TWeakObjectPtr<UEFAsyncActionBase> WeakThis(this);
		TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (UEFAsyncActionBase* This = WeakThis.Get())
			{
				This->TimeoutHandle.Reset();
				This->HandleTimeout();
			}
			return false;
		}), TimeoutSeconds);
	}

	ExecuteAction();
}

void UEFAsyncActionBase::Cancel()
{
	if (bFinished)
	{
		return;
	}

	CancelWork();
	FailAction(FEFResult::Cancelled(GetResultSource()));
}

bool UEFAsyncActionBase::IsActive() const
{
	return bActivated && !bFinished;
}

bool UEFAsyncActionBase::FinishAction()
{
	if (bFinished)
	{
		return false;
	}
	bFinished = true;

	if (TimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
		TimeoutHandle.Reset();
	}

	SetReadyToDestroy();
	return true;
}

void UEFAsyncActionBase::FailAction(const FEFResult& Result)
{
	if (!FinishAction())
	{
		return;
	}

	if (bActivated)
	{
		BroadcastFailure(Result);
	}
	else
	{
		PendingFailure = Result;
	}
}

void UEFAsyncActionBase::FailInvalidArgument(const FString& Problem)
{
	FailAction(FEFResult::InvalidArgument(GetResultSource(), Problem));
}

void UEFAsyncActionBase::HandleTimeout()
{
	if (bFinished)
	{
		return;
	}

	CancelWork();
	FailAction(FEFResult::TimedOut(GetResultSource(), TimeoutSeconds));
}

void UEFAsyncActionBase::CallWithContext(TFunctionRef<void()> Call)
{
	if (CallContext == 0)
	{
		CallContext = EFCallContext::New();
	}

	EFCallContext::FScope Scope(CallContext);
	Call();
}

bool UEFAsyncActionBase::IsResultForThisCall() const
{
	// A node that never used CallWithContext opted out of filtering.
	const int64 Active = EFCallContext::GetActive();
	return CallContext == 0 || Active == 0 || Active == CallContext;
}

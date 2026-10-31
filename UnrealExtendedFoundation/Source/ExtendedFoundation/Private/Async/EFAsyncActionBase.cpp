// Copyright Moon Punch Games. All Rights Reserved.

#include "Async/EFAsyncActionBase.h"

void UEFAsyncActionBase::InitAsyncAction(UObject* WorldContextObject, float InTimeoutSeconds)
{
	EFContextObject = WorldContextObject;
	EFTimeoutSeconds = FMath::Max(InTimeoutSeconds, 0.f);
	if (WorldContextObject)
	{
		RegisterWithGameInstance(WorldContextObject);
	}
}

void UEFAsyncActionBase::Activate()
{
	if (bEFActivated)
	{
		return;
	}
	bEFActivated = true;

	// A failure found in the factory waited for this moment, when the output pins are bound.
	if (EFPendingFailure.IsSet())
	{
		const FEFResult Failure = EFPendingFailure.GetValue();
		EFPendingFailure.Reset();
		BroadcastFailure(Failure);
		return;
	}

	if (bEFFinished)
	{
		return;
	}

	if (EFTimeoutSeconds > 0.f)
	{
		TWeakObjectPtr<UEFAsyncActionBase> WeakThis(this);
		EFTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (UEFAsyncActionBase* This = WeakThis.Get())
			{
				This->EFTimeoutHandle.Reset();
				This->HandleTimeout();
			}
			return false;
		}), EFTimeoutSeconds);
	}

	ExecuteAction();
}

void UEFAsyncActionBase::Cancel()
{
	if (bEFFinished)
	{
		return;
	}

	CancelWork();
	FailAction(FEFResult::Cancelled(GetResultSource()));
}

void UEFAsyncActionBase::BeginDestroy()
{
	// Never call into Blueprint from destruction. Marking the node finished first also keeps the
	// engine's own cancel-on-destroy from broadcasting the failure pin.
	if (!bEFFinished)
	{
		bEFFinished = true;
		EFPendingFailure.Reset();
		if (EFTimeoutHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(EFTimeoutHandle);
			EFTimeoutHandle.Reset();
		}
	}

	Super::BeginDestroy();
}

bool UEFAsyncActionBase::IsActive() const
{
	return bEFActivated && !bEFFinished;
}

bool UEFAsyncActionBase::FinishAction()
{
	if (bEFFinished)
	{
		return false;
	}
	bEFFinished = true;

	if (EFTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(EFTimeoutHandle);
		EFTimeoutHandle.Reset();
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

	if (bEFActivated)
	{
		BroadcastFailure(Result);
	}
	else
	{
		EFPendingFailure = Result;
	}
}

void UEFAsyncActionBase::FailInvalidArgument(const FString& Problem)
{
	FailAction(FEFResult::InvalidArgument(GetResultSource(), Problem));
}

void UEFAsyncActionBase::HandleTimeout()
{
	if (bEFFinished)
	{
		return;
	}

	CancelWork();
	FailAction(FEFResult::TimedOut(GetResultSource(), EFTimeoutSeconds));
}

void UEFAsyncActionBase::CallWithContext(TFunctionRef<void()> Call)
{
	if (EFCallContextId == 0)
	{
		EFCallContextId = EFCallContext::New();
	}

	EFCallContext::FScope Scope(EFCallContextId);
	Call();
}

bool UEFAsyncActionBase::IsResultForThisCall() const
{
	// A node that never used CallWithContext opted out of filtering.
	const int64 Active = EFCallContext::GetActive();
	return EFCallContextId == 0 || Active == 0 || Active == EFCallContextId;
}

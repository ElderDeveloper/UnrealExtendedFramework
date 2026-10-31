// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/EEOSAsyncActionBase.h"

void UEEOSAsyncActionBase::BroadcastFailure(const FEFResult& Result)
{
	OnFailure.Broadcast(DescribeFailure(Result));
}

void UEEOSAsyncActionBase::FailWithError(const FString& Error)
{
	if (FinishAction())
	{
		OnFailure.Broadcast(Error);
	}
}

FString UEEOSAsyncActionBase::DescribeFailure(const FEFResult& Result)
{
	return Result.Message.IsEmpty() ? Result.ToString() : Result.Message;
}

FEEOSOperationOutcome UEEOSAsyncActionBase::MakeFailureOutcome(FName Operation, const FEFResult& Result)
{
	FEEOSOperationOutcome Outcome;
	Outcome.Operation = Operation;
	Outcome.bSuccess = false;
	Outcome.Source = EEOSResultSource::Plugin;
	Outcome.Message = DescribeFailure(Result);
	Outcome.StartedUtc = Outcome.CompletedUtc = Outcome.LastTransitionUtc = FDateTime::UtcNow();

	if (Result.HasCode(EFResultCodes::NotAvailable))
	{
		Outcome.Code = EEOSOperationCode::UnsupportedCapability;
		Outcome.Phase = TEXT("Rejected");
	}
	else if (Result.HasCode(EFResultCodes::InvalidArgument))
	{
		Outcome.Code = EEOSOperationCode::InvalidInput;
		Outcome.Phase = TEXT("Rejected");
	}
	else if (Result.IsCancelled())
	{
		Outcome.Code = EEOSOperationCode::Canceled;
		Outcome.Phase = TEXT("Canceled");
	}
	else if (Result.IsTimedOut())
	{
		// The node stopped waiting; the subsystem operation may still finish on its own.
		Outcome.Code = EEOSOperationCode::Canceled;
		Outcome.Phase = TEXT("TimedOut");
	}
	else
	{
		Outcome.Code = EEOSOperationCode::NativeFailure;
		Outcome.Phase = TEXT("Complete");
	}
	return Outcome;
}

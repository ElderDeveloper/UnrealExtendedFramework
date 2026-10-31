// Copyright Moon Punch Games. All Rights Reserved.

#include "Http/EFAsyncHttpRequest.h"

#include "Http/EFHttpSubsystem.h"

UEFAsyncHttpRequest* UEFAsyncHttpRequest::SendHttpRequestAsync(UObject* WorldContextObject, const FEFHttpRequest& InRequest)
{
	UEFAsyncHttpRequest* Action = NewObject<UEFAsyncHttpRequest>();
	Action->Request = InRequest;
	// The request carries its own timeout; the node does not add a second one.
	Action->InitAsyncAction(WorldContextObject);
	return Action;
}

void UEFAsyncHttpRequest::ExecuteAction()
{
	TFunction<void(const FEFHttpResponse&)> OnDone = MakeWeakCallback<const FEFHttpResponse&>([this](const FEFHttpResponse& Response)
	{
		HandleResponse(Response);
	});

	if (UEFHttpSubsystem* Http = GetGameInstanceSubsystem<UEFHttpSubsystem>())
	{
		HandleSubsystem = Http;
		SubsystemHandle = Http->Send(Request, MoveTemp(OnDone));
	}
	else
	{
		DirectOperation = FEFHttpOperation::Start(Request, MoveTemp(OnDone));
	}
}

void UEFAsyncHttpRequest::CancelWork()
{
	// Silent: the base class reports the cancellation through BroadcastFailure. The subsystem is the
	// one the request went to: the world context that found it may be gone by now.
	if (SubsystemHandle.IsValid())
	{
		if (UEFHttpSubsystem* Http = HandleSubsystem.Get())
		{
			Http->CancelRequest(SubsystemHandle, /*bNotify*/ false);
		}
	}
	if (DirectOperation.IsValid())
	{
		DirectOperation->Cancel(/*bNotify*/ false);
		DirectOperation.Reset();
	}
}

void UEFAsyncHttpRequest::BroadcastFailure(const FEFResult& Result)
{
	FEFHttpResponse Response;
	Response.Url = Request.Url;
	Response.ErrorMessage = Result.Message;
	Response.Result = Result.IsCancelled() ? EEFHttpResult::Cancelled
		: Result.IsTimedOut() ? EEFHttpResult::TimedOut
		: EEFHttpResult::InvalidRequest;
	OnFailure.Broadcast(Response);
}

void UEFAsyncHttpRequest::HandleResponse(const FEFHttpResponse& Response)
{
	DirectOperation.Reset();
	if (!FinishAction())
	{
		return;
	}

	if (Response.IsSuccess())
	{
		OnSuccess.Broadcast(Response);
	}
	else
	{
		OnFailure.Broadcast(Response);
	}
}

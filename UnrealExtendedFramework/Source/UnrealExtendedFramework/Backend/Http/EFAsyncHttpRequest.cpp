// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/Http/EFAsyncHttpRequest.h"

#include "Backend/Http/EFHttpSubsystem.h"

UEFAsyncHttpRequest* UEFAsyncHttpRequest::SendHttpRequestAsync(UObject* WorldContextObject, const FEFHttpRequest& InRequest)
{
	UEFAsyncHttpRequest* Action = NewObject<UEFAsyncHttpRequest>();
	Action->Request = InRequest;
	Action->WorldContext = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UEFAsyncHttpRequest::Activate()
{
	Super::Activate();

	TWeakObjectPtr<UEFAsyncHttpRequest> WeakThis(this);
	auto OnDone = [WeakThis](const FEFHttpResponse& Response)
	{
		if (UEFAsyncHttpRequest* This = WeakThis.Get())
		{
			This->HandleResponse(Response);
		}
	};

	if (UEFHttpSubsystem* Http = UEFHttpSubsystem::Get(WorldContext.Get()))
	{
		SubsystemHandle = Http->Send(Request, MoveTemp(OnDone));
	}
	else
	{
		DirectOperation = FEFHttpOperation::Start(Request, MoveTemp(OnDone));
	}
}

void UEFAsyncHttpRequest::Cancel()
{
	if (bCompleted)
	{
		return;
	}

	if (SubsystemHandle.IsValid())
	{
		if (UEFHttpSubsystem* Http = UEFHttpSubsystem::Get(WorldContext.Get()))
		{
			if (Http->CancelRequest(SubsystemHandle))
			{
				return;
			}
		}
	}
	else if (DirectOperation.IsValid() && !DirectOperation->IsFinished())
	{
		DirectOperation->Cancel(/*bNotify*/ true);
		return;
	}

	// Not started, or its owner is already gone: report the cancellation here.
	FEFHttpResponse Response;
	Response.Result = EEFHttpResult::Cancelled;
	Response.Url = Request.Url;
	Response.ErrorMessage = TEXT("The request was cancelled.");
	HandleResponse(Response);
}

void UEFAsyncHttpRequest::HandleResponse(const FEFHttpResponse& Response)
{
	if (bCompleted)
	{
		return;
	}
	bCompleted = true;
	DirectOperation.Reset();

	if (Response.IsSuccess())
	{
		OnSuccess.Broadcast(Response);
	}
	else
	{
		OnFailure.Broadcast(Response);
	}

	SetReadyToDestroy();
}

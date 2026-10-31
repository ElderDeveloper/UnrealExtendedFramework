// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/Http/EFHttpSubsystem.h"

#include "Backend/Json/EFJsonLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UEFHttpSubsystem* UEFHttpSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UEFHttpSubsystem>() : nullptr;
}

void UEFHttpSubsystem::Deinitialize()
{
	// Callers may be mid-teardown with the game instance, so do not call back into them.
	TMap<int64, TSharedRef<FEFHttpOperation>> Operations = MoveTemp(ActiveOperations);
	ActiveOperations.Reset();
	for (const TPair<int64, TSharedRef<FEFHttpOperation>>& Pair : Operations)
	{
		Pair.Value->Cancel(/*bNotify*/ false);
	}

	Super::Deinitialize();
}

FEFHttpRequestHandle UEFHttpSubsystem::SendRequest(const FEFHttpRequest& Request, const FEFHttpResponseDelegate& OnComplete)
{
	return Send(Request, [OnComplete](const FEFHttpResponse& Response)
	{
		OnComplete.ExecuteIfBound(Response);
	});
}

FEFHttpRequestHandle UEFHttpSubsystem::Send(const FEFHttpRequest& Request, FEFHttpOperation::FOnComplete OnComplete)
{
	const int64 RequestId = NextRequestId++;

	TWeakObjectPtr<UEFHttpSubsystem> WeakThis(this);
	const TSharedRef<FEFHttpOperation> Operation = FEFHttpOperation::Start(PrepareRequest(Request),
		[WeakThis, RequestId, Callback = MoveTemp(OnComplete)](const FEFHttpResponse& Response)
		{
			if (UEFHttpSubsystem* This = WeakThis.Get())
			{
				This->ActiveOperations.Remove(RequestId);
			}
			if (Callback)
			{
				Callback(Response);
			}
		});

	// Start() never completes synchronously, but stay correct if that ever changes.
	if (!Operation->IsFinished())
	{
		ActiveOperations.Add(RequestId, Operation);
	}

	FEFHttpRequestHandle Handle;
	Handle.Id = RequestId;
	return Handle;
}

bool UEFHttpSubsystem::CancelRequest(FEFHttpRequestHandle Handle)
{
	TSharedRef<FEFHttpOperation>* Operation = ActiveOperations.Find(Handle.Id);
	if (!Operation)
	{
		return false;
	}

	// Cancel() calls back synchronously, and the callback removes the entry; hold a reference.
	const TSharedRef<FEFHttpOperation> KeepAlive = *Operation;
	KeepAlive->Cancel(/*bNotify*/ true);
	ActiveOperations.Remove(Handle.Id);
	return true;
}

void UEFHttpSubsystem::CancelAllRequests()
{
	TArray<TSharedRef<FEFHttpOperation>> Operations;
	ActiveOperations.GenerateValueArray(Operations);
	ActiveOperations.Reset();
	for (const TSharedRef<FEFHttpOperation>& Operation : Operations)
	{
		Operation->Cancel(/*bNotify*/ true);
	}
}

bool UEFHttpSubsystem::IsRequestActive(FEFHttpRequestHandle Handle) const
{
	return ActiveOperations.Contains(Handle.Id);
}

int32 UEFHttpSubsystem::GetActiveRequestCount() const
{
	return ActiveOperations.Num();
}

void UEFHttpSubsystem::SetBaseUrl(const FString& InBaseUrl)
{
	BaseUrl = InBaseUrl.TrimStartAndEnd();
}

void UEFHttpSubsystem::SetDefaultHeader(const FString& Name, const FString& Value)
{
	if (!Name.IsEmpty())
	{
		DefaultHeaders.Add(Name, Value);
	}
}

void UEFHttpSubsystem::RemoveDefaultHeader(const FString& Name)
{
	DefaultHeaders.Remove(Name);
}

void UEFHttpSubsystem::ClearDefaultHeaders()
{
	DefaultHeaders.Reset();
}

FEFHttpRequest UEFHttpSubsystem::PrepareRequest(const FEFHttpRequest& Request) const
{
	FEFHttpRequest Prepared = Request;
	Prepared.Url = EFHttp::ResolveUrl(BaseUrl, Request.Url);
	for (const TPair<FString, FString>& Header : DefaultHeaders)
	{
		if (!Prepared.Headers.Contains(Header.Key))
		{
			Prepared.Headers.Add(Header.Key, Header.Value);
		}
	}
	return Prepared;
}

// ─── Deprecated ─────────────────────────────────────────────────────────────────

void UEFHttpSubsystem::SendHttpRequest(const FString& Url, const FString& RequestContent)
{
	// The old node sent a POST and only logged the response. It took RequestContent but never sent
	// it; it is sent as the body now, as the parameter promised.
	FEFHttpRequest Request;
	Request.Url = Url;
	Request.Verb = EEFHttpVerb::Post;
	Request.Body = RequestContent;
	Request.ContentType = TEXT("text/plain; charset=utf-8");

	Send(Request, [](const FEFHttpResponse& Response)
	{
		UE_LOG(LogEFHttp, Log, TEXT("SendHttpRequest: HTTP %d, %d characters received."), Response.StatusCode, Response.Body.Len());
	});
}

FString UEFHttpSubsystem::JsonString(TArray<FString> StringArray)
{
	// Pairs of [key, value, key, value, ...] into a flat object of string fields. A trailing key
	// without a value is ignored. The old version wrote unescaped text and read past the array.
	FEFJsonObject Object = FEFJsonObject::MakeEmpty();
	for (int32 Index = 0; Index + 1 < StringArray.Num(); Index += 2)
	{
		UEFJsonLibrary::SetStringField(Object, StringArray[Index], StringArray[Index + 1]);
	}
	return UEFJsonLibrary::JsonObjectToString(Object);
}

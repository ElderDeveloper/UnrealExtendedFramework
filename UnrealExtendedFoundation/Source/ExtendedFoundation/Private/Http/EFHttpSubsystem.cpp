// Copyright Moon Punch Games. All Rights Reserved.

#include "Http/EFHttpSubsystem.h"

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

bool UEFHttpSubsystem::CancelRequest(FEFHttpRequestHandle Handle, bool bNotify)
{
	TSharedRef<FEFHttpOperation>* Operation = ActiveOperations.Find(Handle.Id);
	if (!Operation)
	{
		return false;
	}

	// Cancel() can call back synchronously, and the callback removes the entry; hold a reference.
	const TSharedRef<FEFHttpOperation> KeepAlive = *Operation;
	KeepAlive->Cancel(bNotify);
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

int32 UEFHttpSubsystem::CancelRequestsWithTag(FName Tag, bool bNotify)
{
	if (Tag.IsNone())
	{
		return 0;
	}

	// Cancel() can call back synchronously and remove entries, so collect the matches first.
	TArray<TPair<int64, TSharedRef<FEFHttpOperation>>> Matches;
	for (const TPair<int64, TSharedRef<FEFHttpOperation>>& Pair : ActiveOperations)
	{
		if (Pair.Value->GetRequest().Tags.Contains(Tag))
		{
			Matches.Emplace(Pair.Key, Pair.Value);
		}
	}
	for (const TPair<int64, TSharedRef<FEFHttpOperation>>& Match : Matches)
	{
		ActiveOperations.Remove(Match.Key);
		Match.Value->Cancel(bNotify);
	}
	return Matches.Num();
}

bool UEFHttpSubsystem::IsRequestActive(FEFHttpRequestHandle Handle) const
{
	return ActiveOperations.Contains(Handle.Id);
}

int32 UEFHttpSubsystem::GetActiveRequestCount() const
{
	return ActiveOperations.Num();
}

int32 UEFHttpSubsystem::GetActiveRequestCountWithTag(FName Tag) const
{
	int32 Count = 0;
	if (!Tag.IsNone())
	{
		for (const TPair<int64, TSharedRef<FEFHttpOperation>>& Pair : ActiveOperations)
		{
			Count += Pair.Value->GetRequest().Tags.Contains(Tag) ? 1 : 0;
		}
	}
	return Count;
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

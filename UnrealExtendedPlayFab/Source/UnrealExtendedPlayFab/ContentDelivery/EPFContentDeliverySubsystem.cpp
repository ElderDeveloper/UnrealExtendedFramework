// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFContentDeliverySubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "Async/EFCallContext.h"
#include "Http/EFHttpClient.h"

void UEPFContentDeliverySubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UEPFContentDeliverySubsystem::Deinitialize() { Super::Deinitialize(); }

// ── Get Content Download URL ─────────────────────────────────────────────────

void UEPFContentDeliverySubsystem::GetContentDownloadUrl(const FString& Key, bool bThruCDN, const FString& HttpMethod)
{
	if (Key.IsEmpty()) { OnContentUrlReceived.Broadcast(FEPFResult::Failure(TEXT("Key cannot be empty")), TEXT("")); return; }

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Key"), Key);
	Body->SetBoolField(TEXT("ThruCDN"), bThruCDN);
	if (!HttpMethod.IsEmpty()) Body->SetStringField(TEXT("HttpMethod"), HttpMethod);

	SendPlayFabRequestDetailed(TEXT("/Client/GetContentDownloadUrl"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			FString Url;
			if (Result.bSuccess && Response.IsValid())
			{
				Url = Response->GetStringField(TEXT("URL"));
				EF_LOG(ExtendedPlayFab, Log, TEXT("EPFContentDelivery — URL: %s"), *Url);
			}
			OnContentUrlReceived.Broadcast(Result, Url);
		}));
}

// ── Download Content As String ───────────────────────────────────────────────

void UEPFContentDeliverySubsystem::DownloadContentAsString(const FString& Key)
{
	if (Key.IsEmpty()) { OnContentDownloaded.Broadcast(FEPFResult::Failure(TEXT("Key cannot be empty")), TEXT("")); return; }

	// First get the download URL, then fetch the content
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("Key"), Key);
	Body->SetBoolField(TEXT("ThruCDN"), true);

	SendPlayFabRequestDetailed(TEXT("/Client/GetContentDownloadUrl"), Body, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (!Result.bSuccess || !Response.IsValid())
			{
				OnContentDownloaded.Broadcast(Result, TEXT(""));
				return;
			}

			FString Url = Response->GetStringField(TEXT("URL"));
			if (Url.IsEmpty())
			{
				OnContentDownloaded.Broadcast(FEPFResult::Failure(TEXT("Content URL was empty")), TEXT(""));
				return;
			}

			// Now download the actual content. The callback keeps the caller's call context, so the
			// broadcast is tagged as theirs (see EFCallContext).
			FEFHttpRequest Download;
			Download.Url = Url;
			Download.Verb = EEFHttpVerb::Get;
			Download.MaxRetries = 2;
			TWeakObjectPtr<UEPFContentDeliverySubsystem> WeakThis(this);
			FEFHttpOperation::Start(Download, EFCallContext::Bind<const FEFHttpResponse&>([WeakThis](const FEFHttpResponse& HttpResponse)
			{
				UEPFContentDeliverySubsystem* This = WeakThis.Get();
				if (!This)
				{
					return;
				}
				if (HttpResponse.IsSuccess())
				{
					This->OnContentDownloaded.Broadcast(FEPFResult::Success(), HttpResponse.Body);
				}
				else
				{
					This->OnContentDownloaded.Broadcast(FEPFResult::Failure(TEXT("Failed to download content")), TEXT(""));
				}
			}));
		}));
}

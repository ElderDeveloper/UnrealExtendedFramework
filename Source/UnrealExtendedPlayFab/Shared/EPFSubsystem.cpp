// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFSubsystem.h"
#include "EPFSettings.h"
#include "UnrealExtendedPlayFab.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

namespace
{
	static EEPFAuthMode ResolveAuthModeForPath(const FString& ApiPath, bool bRequiresAuth)
	{
		if (!bRequiresAuth)
		{
			return EEPFAuthMode::None;
		}

		if (
			ApiPath.StartsWith(TEXT("/Server/")) ||
			ApiPath.StartsWith(TEXT("/Admin/"))
		)
		{
			return EEPFAuthMode::SecretKey;
		}

		if (
			ApiPath.StartsWith(TEXT("/Group/")) ||
			ApiPath.StartsWith(TEXT("/Object/")) ||
			ApiPath.StartsWith(TEXT("/Match/")) ||
			ApiPath.StartsWith(TEXT("/Authentication/")) ||
			ApiPath.StartsWith(TEXT("/Data/")) ||
			ApiPath.StartsWith(TEXT("/Profiles/")) ||
			ApiPath.StartsWith(TEXT("/Economy/")) ||
			ApiPath.StartsWith(TEXT("/Events/")) ||
			ApiPath.StartsWith(TEXT("/Insights/")) ||
			ApiPath.StartsWith(TEXT("/Progression/")) ||
			ApiPath.StartsWith(TEXT("/Multiplayer/"))
		)
		{
			return EEPFAuthMode::EntityToken;
		}

		return EEPFAuthMode::SessionTicket;
	}
}

FString UEPFSubsystem::SharedSessionTicket;
FString UEPFSubsystem::SharedPlayFabId;
FEPFAuthContext UEPFSubsystem::SharedAuthContext;

void UEPFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RequestsThisSecond = 0;
}

void UEPFSubsystem::Deinitialize()
{
	PendingRequestQueue.Empty();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(RateLimitResetHandle);
		}
	}

	Super::Deinitialize();
}

bool UEPFSubsystem::IsAuthenticated() const
{
	return SharedAuthContext.HasSessionTicket() || SharedAuthContext.HasEntityToken();
}

FString UEPFSubsystem::GetPlayFabId() const
{
	return SharedAuthContext.PlayFabId;
}

FString UEPFSubsystem::GetSessionTicket() const
{
	return SharedAuthContext.SessionTicket;
}

FString UEPFSubsystem::GetEntityToken() const
{
	return SharedAuthContext.EntityToken;
}

FString UEPFSubsystem::GetEntityId() const
{
	return SharedAuthContext.EntityId;
}

FString UEPFSubsystem::GetEntityType() const
{
	return SharedAuthContext.EntityType;
}

FEPFError UEPFSubsystem::GetLastError() const
{
	return LastError;
}

const UEPFSettings* UEPFSubsystem::GetSettings() const
{
	return UEPFSettings::Get();
}

bool UEPFSubsystem::IsConfigured() const
{
	const UEPFSettings* Settings = GetSettings();
	return Settings && !Settings->TitleId.IsEmpty();
}

void UEPFSubsystem::LogNotConfigured(const FString& FunctionName) const
{
	UE_LOG(LogExtendedPlayFab, Error, TEXT("EPFSubsystem::%s - PlayFab TitleId is not configured. Set it in Project Settings -> Extended Framework -> PlayFab."), *FunctionName);
}

void UEPFSubsystem::SetSharedAuthContext(const FEPFAuthContext& InAuthContext)
{
	SharedAuthContext = InAuthContext;
	SharedSessionTicket = InAuthContext.SessionTicket;
	SharedPlayFabId = InAuthContext.PlayFabId;
}

void UEPFSubsystem::ClearSharedAuthContext()
{
	SharedAuthContext.Reset();
	SharedSessionTicket.Empty();
	SharedPlayFabId.Empty();
}

void UEPFSubsystem::SendPlayFabRequest(
	const FString& ApiPath,
	const TSharedRef<FJsonObject>& RequestBody,
	bool bRequiresAuth,
	const FOnPlayFabResponse& OnComplete)
{
	SendPlayFabRequest(ApiPath, RequestBody, ResolveAuthModeForPath(ApiPath, bRequiresAuth), OnComplete);
}

void UEPFSubsystem::SendPlayFabRequest(
	const FString& ApiPath,
	const TSharedRef<FJsonObject>& RequestBody,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponse& OnComplete)
{
	SendPlayFabRequestDetailed(
		ApiPath,
		RequestBody,
		AuthMode,
		FOnPlayFabResponseDetailed::CreateLambda([OnComplete](const FEPFResult& Result, TSharedPtr<FJsonObject> JsonResponse)
		{
			if (OnComplete.IsBound())
			{
				OnComplete.Execute(Result.bSuccess, JsonResponse);
			}
		})
	);
}

void UEPFSubsystem::SendPlayFabRequestDetailed(
	const FString& ApiPath,
	const TSharedRef<FJsonObject>& RequestBody,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete)
{
	if (!IsConfigured())
	{
		LogNotConfigured(TEXT("SendPlayFabRequest"));
		LastError = FEPFError::Failure(TEXT("PlayFab TitleId is not configured"), TEXT("NotConfigured"));
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(LastError), nullptr);
		}
		return;
	}

	const UEPFSettings* Settings = GetSettings();
	if (AuthMode == EEPFAuthMode::SecretKey && !GIsEditor && !IsRunningDedicatedServer())
	{
		LastError = FEPFError::Failure(TEXT("Developer secret key requests are only allowed in editor tools or dedicated server contexts"), TEXT("SecretKeyForbidden"));
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(LastError), nullptr);
		}
		OnRequestError.Broadcast(LastError);
		OnRequestComplete.Broadcast(FEPFResult::Failure(LastError));
		return;
	}

	const bool bMissingSession = AuthMode == EEPFAuthMode::SessionTicket && SharedAuthContext.SessionTicket.IsEmpty();
	const bool bMissingEntity = AuthMode == EEPFAuthMode::EntityToken && SharedAuthContext.EntityToken.IsEmpty();
	const bool bMissingSecret = AuthMode == EEPFAuthMode::SecretKey && SharedAuthContext.DeveloperSecretKey.IsEmpty() && (!Settings || Settings->DeveloperSecretKey.IsEmpty());
	if (bMissingSession || bMissingEntity || bMissingSecret)
	{
		const TCHAR* MissingAuthLabel = bMissingEntity ? TEXT("entity token") : (bMissingSecret ? TEXT("developer secret key") : TEXT("session ticket"));
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFSubsystem::SendPlayFabRequest - Missing %s. Endpoint: %s"), MissingAuthLabel, *ApiPath);
		LastError = FEPFError::Failure(FString::Printf(TEXT("Missing %s"), MissingAuthLabel), TEXT("MissingAuthentication"));
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(LastError), nullptr);
		}
		OnRequestError.Broadcast(LastError);
		OnRequestComplete.Broadcast(FEPFResult::Failure(LastError));
		return;
	}

	FString RequestBodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody, Writer);

	if (RequestsThisSecond >= MaxRequestsPerSecond)
	{
		UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFSubsystem - Rate limited, queueing request: %s"), *ApiPath);
		FPendingRequest Pending;
		Pending.ApiPath = ApiPath;
		Pending.RequestBodyString = RequestBodyString;
		Pending.AuthMode = AuthMode;
		Pending.OnComplete = OnComplete;
		PendingRequestQueue.Add(MoveTemp(Pending));
		return;
	}

	ExecuteRequest(ApiPath, RequestBodyString, AuthMode, OnComplete, 0);
}

void UEPFSubsystem::ExecuteRequest(
	const FString& ApiPath,
	const FString& RequestBodyString,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete,
	int32 RetryAttempt)
{
	const UEPFSettings* Settings = GetSettings();
	const FString Url = Settings->GetApiBaseUrl() + ApiPath;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Url);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("X-ReportErrorAsSuccess"), TEXT("true"));

	if (Settings->bIncludeSdkHeader && !Settings->SDKHeaderValue.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("X-PlayFabSDK"), Settings->SDKHeaderValue);
	}

	switch (AuthMode)
	{
	case EEPFAuthMode::SessionTicket:
		HttpRequest->SetHeader(TEXT("X-Authorization"), SharedAuthContext.SessionTicket);
		break;
	case EEPFAuthMode::EntityToken:
		HttpRequest->SetHeader(TEXT("X-EntityToken"), SharedAuthContext.EntityToken);
		break;
	case EEPFAuthMode::SecretKey:
		HttpRequest->SetHeader(TEXT("X-SecretKey"), !SharedAuthContext.DeveloperSecretKey.IsEmpty() ? SharedAuthContext.DeveloperSecretKey : Settings->DeveloperSecretKey);
		break;
	case EEPFAuthMode::None:
	default:
		break;
	}

	HttpRequest->SetContentAsString(RequestBodyString);

	if (Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedPlayFab, Log, TEXT("PlayFab Request -> %s (attempt %d)"), *Url, RetryAttempt + 1);
		UE_LOG(LogExtendedPlayFab, Log, TEXT("  Body: %s"), *RequestBodyString);
	}

	RequestsThisSecond++;
	if (!RateLimitResetHandle.IsValid())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				World->GetTimerManager().SetTimer(RateLimitResetHandle, this, &UEPFSubsystem::ResetRateLimitCounter, 1.0f, false);
			}
		}
	}

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			HandleHttpResponse(Request, Response, bConnectedSuccessfully, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt);
		}
	);

	HttpRequest->ProcessRequest();
}

void UEPFSubsystem::HandleHttpResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bConnectedSuccessfully,
	const FString& ApiPath,
	const FString& RequestBodyString,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete,
	int32 RetryAttempt)
{
	const UEPFSettings* Settings = GetSettings();
	FEPFError Error = FEPFError::None();

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		if (RetryAttempt < MaxRetries)
		{
			const float Delay = BaseRetryDelaySec * FMath::Pow(2.0f, static_cast<float>(RetryAttempt));
			UE_LOG(LogExtendedPlayFab, Warning, TEXT("PlayFab Connection Failed - Retrying %s in %.1fs (attempt %d/%d)"), *ApiPath, Delay, RetryAttempt + 1, MaxRetries);

			if (UGameInstance* GI = GetGameInstance())
			{
				if (UWorld* World = GI->GetWorld())
				{
					FTimerHandle RetryHandle;
					World->GetTimerManager().SetTimer(
						RetryHandle,
						[this, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt]()
						{
							ExecuteRequest(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt + 1);
						},
						Delay,
						false
					);
				}
			}
			return;
		}

		Error = FEPFError::Failure(TEXT("Unable to contact PlayFab"), TEXT("ConnectionFailure"), 503);
		LastError = Error;
		UE_LOG(LogExtendedPlayFab, Error, TEXT("PlayFab Request Failed - No response for %s after %d attempts"), *ApiPath, MaxRetries);
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(Error), nullptr);
		}
		OnRequestError.Broadcast(Error);
		OnRequestComplete.Broadcast(FEPFResult::Failure(Error));
		return;
	}

	const FString ResponseBody = Response->GetContentAsString();
	const int32 ResponseCode = Response->GetResponseCode();

	if (Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedPlayFab, Log, TEXT("PlayFab Response <- %s [%d]"), *ApiPath, ResponseCode);
		UE_LOG(LogExtendedPlayFab, Log, TEXT("  Body: %s"), *ResponseBody);
	}

	if ((ResponseCode >= 500 || ResponseCode == 429) && RetryAttempt < MaxRetries)
	{
		const float Delay = BaseRetryDelaySec * FMath::Pow(2.0f, static_cast<float>(RetryAttempt));
		const TCHAR* Reason = ResponseCode == 429 ? TEXT("Rate Limited") : TEXT("Server Error");
		UE_LOG(LogExtendedPlayFab, Warning, TEXT("PlayFab %s [%d] - Retrying %s in %.1fs (attempt %d/%d)"), Reason, ResponseCode, *ApiPath, Delay, RetryAttempt + 1, MaxRetries);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				FTimerHandle RetryHandle;
				World->GetTimerManager().SetTimer(
					RetryHandle,
					[this, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt]()
					{
						ExecuteRequest(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt + 1);
					},
					Delay,
					false
				);
			}
		}
		return;
	}

	TSharedPtr<FJsonObject> JsonResponse;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
	{
		Error = FEPFError::Failure(TEXT("Failed to parse PlayFab response JSON"), TEXT("InvalidJson"), ResponseCode);
		Error.RawResponse = ResponseBody;
		LastError = Error;
		UE_LOG(LogExtendedPlayFab, Error, TEXT("PlayFab Response - Failed to parse JSON for %s"), *ApiPath);
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(Error), nullptr);
		}
		OnRequestError.Broadcast(Error);
		OnRequestComplete.Broadcast(FEPFResult::Failure(Error));
		return;
	}

	int32 PlayFabCode = ResponseCode;
	JsonResponse->TryGetNumberField(TEXT("code"), PlayFabCode);
	if (PlayFabCode >= 200 && PlayFabCode < 300)
	{
		LastError = FEPFError::None();
		const TSharedPtr<FJsonObject>* DataObject = nullptr;
		if (JsonResponse->TryGetObjectField(TEXT("data"), DataObject) && DataObject)
		{
			if (OnComplete.IsBound())
			{
				OnComplete.Execute(FEPFResult::Success(), *DataObject);
			}
			OnRequestComplete.Broadcast(FEPFResult::Success());
		}
		else if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Success(), JsonResponse);
			OnRequestComplete.Broadcast(FEPFResult::Success());
		}
		return;
	}

	FString ErrorMessage;
	JsonResponse->TryGetStringField(TEXT("errorMessage"), ErrorMessage);
	FString ErrorCode;
	JsonResponse->TryGetStringField(TEXT("error"), ErrorCode);
	FString ErrorName;
	JsonResponse->TryGetStringField(TEXT("status"), ErrorName);

	FString ErrorDetailsString;
	const TSharedPtr<FJsonObject>* ErrorDetailsObject = nullptr;
	if (JsonResponse->TryGetObjectField(TEXT("errorDetails"), ErrorDetailsObject) && ErrorDetailsObject)
	{
		TSharedRef<TJsonWriter<>> ErrorWriter = TJsonWriterFactory<>::Create(&ErrorDetailsString);
		FJsonSerializer::Serialize((*ErrorDetailsObject).ToSharedRef(), ErrorWriter);
	}

	Error.bHasError = true;
	Error.HttpStatusCode = ResponseCode;
	Error.ErrorCode = ErrorCode;
	Error.ErrorName = ErrorName.IsEmpty() ? ErrorCode : ErrorName;
	Error.ErrorMessage = ErrorMessage.IsEmpty() ? TEXT("PlayFab request failed") : ErrorMessage;
	Error.ErrorDetails = ErrorDetailsString;
	Error.RawResponse = ResponseBody;
	LastError = Error;

	UE_LOG(LogExtendedPlayFab, Warning, TEXT("PlayFab Error [%s] %s - %s"), *ApiPath, *Error.ErrorCode, *Error.ErrorMessage);
	if (OnComplete.IsBound())
	{
		OnComplete.Execute(FEPFResult::Failure(Error), JsonResponse);
	}
	OnRequestError.Broadcast(Error);
	OnRequestComplete.Broadcast(FEPFResult::Failure(Error));
}

void UEPFSubsystem::ResetRateLimitCounter()
{
	RequestsThisSecond = 0;
	RateLimitResetHandle.Invalidate();
	ProcessRequestQueue();
}

void UEPFSubsystem::ProcessRequestQueue()
{
	while (PendingRequestQueue.Num() > 0 && RequestsThisSecond < MaxRequestsPerSecond)
	{
		FPendingRequest Pending = MoveTemp(PendingRequestQueue[0]);
		PendingRequestQueue.RemoveAt(0);

		UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFSubsystem - Processing queued request: %s"), *Pending.ApiPath);
		ExecuteRequest(Pending.ApiPath, Pending.RequestBodyString, Pending.AuthMode, Pending.OnComplete, 0);
	}
}

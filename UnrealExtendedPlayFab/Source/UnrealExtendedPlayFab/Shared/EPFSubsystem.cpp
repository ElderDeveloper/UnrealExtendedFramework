// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFSubsystem.h"
#include "EPFLogFormatting.h"
#include "EPFSettings.h"
#include "UnrealExtendedPlayFab.h"
#include "Auth/EPFAuthSubsystem.h"

#include "Async/EFCallContext.h"
#include "Containers/Ticker.h"
#include "Http/EFHttpClient.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

FEPFAuthContext UEPFSubsystem::SharedAuthContext;

void UEPFSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bDeinitialized = false;
	RequestsThisSecond = 0;
}

void UEPFSubsystem::Deinitialize()
{
	bDeinitialized = true;
	PendingRequestQueue.Empty();

	FTSTicker::RemoveTicker(RateLimitResetHandle);
	RateLimitResetHandle.Reset();

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
	EF_LOG(ExtendedPlayFab, Error, TEXT("EPFSubsystem::%s - PlayFab TitleId is not configured. Set it in Project Settings -> Extended Framework -> PlayFab."), *FunctionName);
}

void UEPFSubsystem::SetSharedAuthContext(const FEPFAuthContext& InAuthContext)
{
	SharedAuthContext = InAuthContext;
}

void UEPFSubsystem::ClearSharedAuthContext()
{
	SharedAuthContext.Reset();
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
	const FOnPlayFabResponseDetailed& InOnComplete)
{
	// Carry the caller's call context (EFCallContext) through queueing, retries and re-auth replays
	// to the completion, so the broadcast it makes is tagged as that caller's. This is what lets two
	// async nodes running the same call at once each take their own result.
	const int64 CallContext = EFCallContext::GetActive();
	const FOnPlayFabResponseDetailed OnComplete = CallContext == 0 ? InOnComplete :
		FOnPlayFabResponseDetailed::CreateLambda([CallContext, InOnComplete](const FEPFResult& Result, TSharedPtr<FJsonObject> JsonResponse)
		{
			EFCallContext::FScope Scope(CallContext);
			InOnComplete.ExecuteIfBound(Result, JsonResponse);
		});

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
		EF_LOG(ExtendedPlayFab, Warning, TEXT("EPFSubsystem::SendPlayFabRequest - Missing %s. Endpoint: %s"), MissingAuthLabel, *ApiPath);
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
		EF_LOG(ExtendedPlayFab, Verbose, TEXT("EPFSubsystem - Rate limited, queueing request: %s"), *ApiPath);
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

	FEFHttpRequest HttpRequest;
	HttpRequest.Url = Url;
	HttpRequest.Verb = EEFHttpVerb::Post;
	HttpRequest.ContentType = TEXT("application/json");
	HttpRequest.Body = RequestBodyString;
	// Retries stay here, not in FEFHttpOperation: this subsystem also re-authenticates and replays.
	HttpRequest.MaxRetries = 0;
	// This subsystem logs its own retries and errors with the PlayFab error details.
	HttpRequest.bLogFailuresAsWarnings = false;
	HttpRequest.Headers.Add(TEXT("X-ReportErrorAsSuccess"), TEXT("true"));

	if (Settings->bIncludeSdkHeader && !Settings->SDKHeaderValue.IsEmpty())
	{
		HttpRequest.Headers.Add(TEXT("X-PlayFabSDK"), Settings->SDKHeaderValue);
	}

	switch (AuthMode)
	{
	case EEPFAuthMode::SessionTicket:
		HttpRequest.Headers.Add(TEXT("X-Authorization"), SharedAuthContext.SessionTicket);
		break;
	case EEPFAuthMode::EntityToken:
		HttpRequest.Headers.Add(TEXT("X-EntityToken"), SharedAuthContext.EntityToken);
		break;
	case EEPFAuthMode::SecretKey:
		HttpRequest.Headers.Add(TEXT("X-SecretKey"), !SharedAuthContext.DeveloperSecretKey.IsEmpty() ? SharedAuthContext.DeveloperSecretKey : Settings->DeveloperSecretKey);
		break;
	case EEPFAuthMode::None:
	default:
		break;
	}

	// One entry per call, body included, credentials redacted. Verbose, so it costs nothing (not even
	// the redaction) until the category is raised.
	EF_LOG(ExtendedPlayFab, Verbose, TEXT("PlayFab Request -> %s (attempt %d)\n%s"),
		*Url, RetryAttempt + 1, *EPFLogFormatting::FormatBodyForLog(RequestBodyString, Settings->LoggedBodyMaxChars));

	RequestsThisSecond++;
	if (!RateLimitResetHandle.IsValid())
	{
		// The core ticker, not a world timer: a world timer stops while the game is paused and is lost
		// with its world on map travel, and a reset that never comes leaves every later request queued.
		TWeakObjectPtr<UEPFSubsystem> WeakThis(this);
		RateLimitResetHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("PlayFab.RateLimitReset"), 1.0f, [WeakThis](float)
		{
			UEPFSubsystem* This = WeakThis.Get();
			if (This && !This->bDeinitialized)
			{
				This->ResetRateLimitCounter();
			}
			return false;
		});
	}

	// Weak-gated: a response can arrive after PIE shutdown destroyed this subsystem.
	TWeakObjectPtr<UEPFSubsystem> WeakThis(this);
	FEFHttpOperation::Start(HttpRequest,
		[WeakThis, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt](const FEFHttpResponse& Response)
		{
			if (UEPFSubsystem* This = WeakThis.Get())
			{
				This->HandleHttpResponse(Response, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt);
			}
		});
}

void UEPFSubsystem::HandleHttpResponse(
	const FEFHttpResponse& Response,
	const FString& ApiPath,
	const FString& RequestBodyString,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete,
	int32 RetryAttempt)
{
	check(IsInGameThread());
	if (bDeinitialized)
	{
		return;
	}

	// MoviePlayer::BlockingTick pumps HTTP from inside package loading on the game thread.
	// UE 5.8 can have an unset loading-thread PIE ID (-2) there, so even SoftObjectPtr::Get
	// in an inventory/UI listener asserts. Wait for a normal game-thread tick before parsing
	// or broadcasting anything; AsyncTask(GameThread) can also run inside loading and is unsafe.
	if (IsInAsyncLoadingThread())
	{
		TWeakObjectPtr<UEPFSubsystem> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(TEXT("PlayFab.DeferredHttpResponse"), 0.0f,
			[WeakThis, Response, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt](float)
			{
				UEPFSubsystem* Subsystem = WeakThis.Get();
				if (!Subsystem || Subsystem->bDeinitialized)
				{
					return false;
				}
				if (IsInAsyncLoadingThread())
				{
					return true;
				}
				Subsystem->HandleHttpResponse(Response, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt);
				return false;
			});
		return;
	}

	const UEPFSettings* Settings = GetSettings();
	FEPFError Error = FEPFError::None();

	const bool bConnectedSuccessfully = Response.Result == EEFHttpResult::Success || Response.Result == EEFHttpResult::HttpError;
	if (!bConnectedSuccessfully)
	{
		if (RetryAttempt < MaxRetries)
		{
			const float Delay = BaseRetryDelaySec * FMath::Pow(2.0f, static_cast<float>(RetryAttempt));
			EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab Connection Failed - Retrying %s in %.1fs (attempt %d/%d)"), *ApiPath, Delay, RetryAttempt + 1, MaxRetries);

			ScheduleRetry(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt, Delay);
			return;
		}

		Error = FEPFError::Failure(TEXT("Unable to contact PlayFab"), TEXT("ConnectionFailure"), 503);
		LastError = Error;
		EF_LOG(ExtendedPlayFab, Error, TEXT("PlayFab Request Failed - No response for %s after %d attempts"), *ApiPath, MaxRetries);
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FEPFResult::Failure(Error), nullptr);
		}
		OnRequestError.Broadcast(Error);
		OnRequestComplete.Broadcast(FEPFResult::Failure(Error));
		return;
	}

	const FString& ResponseBody = Response.Body;
	const int32 ResponseCode = Response.StatusCode;

	EF_LOG(ExtendedPlayFab, Verbose, TEXT("PlayFab Response <- %s [%d]\n%s"),
		*ApiPath, ResponseCode, *EPFLogFormatting::FormatBodyForLog(ResponseBody, Settings->LoggedBodyMaxChars));

	if ((ResponseCode >= 500 || ResponseCode == 429) && RetryAttempt < MaxRetries)
	{
		const float Delay = BaseRetryDelaySec * FMath::Pow(2.0f, static_cast<float>(RetryAttempt));
		const TCHAR* Reason = ResponseCode == 429 ? TEXT("Rate Limited") : TEXT("Server Error");
		EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab %s [%d] - Retrying %s in %.1fs (attempt %d/%d)"), Reason, ResponseCode, *ApiPath, Delay, RetryAttempt + 1, MaxRetries);

		ScheduleRetry(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt, Delay);
		return;
	}

	TSharedPtr<FJsonObject> JsonResponse;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
	{
		Error = FEPFError::Failure(TEXT("Failed to parse PlayFab response JSON"), TEXT("InvalidJson"), ResponseCode);
		Error.RawResponse = ResponseBody;
		LastError = Error;
		EF_LOG(ExtendedPlayFab, Error, TEXT("PlayFab Response - Failed to parse JSON for %s"), *ApiPath);
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

	EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab Error [%s] %s - %s"), *ApiPath, *Error.ErrorCode, *Error.ErrorMessage);

	if (TryReauthenticateAndReplay(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt, PlayFabCode, ErrorCode, Error, JsonResponse))
	{
		return;
	}

	DeliverFailure(Error, JsonResponse, OnComplete);
}

void UEPFSubsystem::DeliverFailure(const FEPFError& Error, TSharedPtr<FJsonObject> JsonResponse, const FOnPlayFabResponseDetailed& OnComplete)
{
	if (OnComplete.IsBound())
	{
		OnComplete.Execute(FEPFResult::Failure(Error), JsonResponse);
	}
	OnRequestError.Broadcast(Error);
	OnRequestComplete.Broadcast(FEPFResult::Failure(Error));
}

bool UEPFSubsystem::IsSessionRejection(int32 PlayFabCode, const FString& ErrorCode)
{
	if (PlayFabCode == 401)
	{
		return true;
	}

	// PlayFab reports the same condition under several names depending on which token failed.
	static const TCHAR* RejectionCodes[] = {
		TEXT("NotAuthenticated"),
		TEXT("NotAuthorized"),
		TEXT("InvalidSessionTicket"),
		TEXT("AuthTokenDoesNotExist"),
		TEXT("ExpiredAuthToken"),
		TEXT("InvalidEntityToken"),
		TEXT("EntityTokenExpired"),
	};

	for (const TCHAR* Code : RejectionCodes)
	{
		if (ErrorCode.Equals(Code, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UEPFSubsystem::TryReauthenticateAndReplay(
	const FString& ApiPath,
	const FString& RequestBodyString,
	EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete,
	int32 RetryAttempt,
	int32 PlayFabCode,
	const FString& ErrorCode,
	const FEPFError& Error,
	TSharedPtr<FJsonObject> JsonResponse)
{
	// Only a call that carried a token can have had a stale one. Login itself uses AuthMode::None,
	// so the request that would fix the session can never trigger this and recurse.
	if (AuthMode != EEPFAuthMode::SessionTicket && AuthMode != EEPFAuthMode::EntityToken)
	{
		return false;
	}

	if (!IsSessionRejection(PlayFabCode, ErrorCode))
	{
		return false;
	}

	// Shares the transport retry budget rather than adding a second one, so a server that
	// rejects every token cannot bounce one request between re-auth and replay forever.
	if (RetryAttempt >= MaxRetries)
	{
		EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab %s still rejected after re-authentication; giving up."), *ApiPath);
		return false;
	}

	const UEPFSettings* Settings = GetSettings();
	if (!Settings || !Settings->bReauthenticateOnSessionRejected)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UEPFAuthSubsystem* Auth = GameInstance ? GameInstance->GetSubsystem<UEPFAuthSubsystem>() : nullptr;
	if (!Auth || !Auth->CanReauthenticate())
	{
		return false;
	}

	EF_LOG(ExtendedPlayFab, Log, TEXT("PlayFab session rejected on %s; re-authenticating and replaying."), *ApiPath);

	// The handle has to outlive this scope so the waiter can unregister itself from inside its
	// own broadcast, which UE multicast delegates support.
	TSharedRef<FDelegateHandle> WaiterHandle = MakeShared<FDelegateHandle>();
	*WaiterHandle = Auth->OnReauthFinished.AddWeakLambda(this,
		[this, WaiterHandle, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt, Error, JsonResponse](bool bReauthSucceeded)
		{
			// Removing this waiter destroys the lambda that is running, captures and all, so
			// everything the rest of the call needs is copied out first.
			UEPFSubsystem* const Self = this;
			const FString LocalApiPath = ApiPath;
			const FString LocalRequestBody = RequestBodyString;
			const EEPFAuthMode LocalAuthMode = AuthMode;
			const FOnPlayFabResponseDetailed LocalOnComplete = OnComplete;
			const int32 LocalRetryAttempt = RetryAttempt;
			const FEPFError LocalError = Error;
			const TSharedPtr<FJsonObject> LocalJsonResponse = JsonResponse;
			const FDelegateHandle LocalWaiterHandle = *WaiterHandle;

			if (UGameInstance* GI = Self->GetGameInstance())
			{
				if (UEPFAuthSubsystem* AuthSubsystem = GI->GetSubsystem<UEPFAuthSubsystem>())
				{
					AuthSubsystem->OnReauthFinished.Remove(LocalWaiterHandle);
				}
			}

			if (!bReauthSucceeded)
			{
				// Hand back the original rejection, not a re-auth error: the caller asked about
				// its own request, and that is still the failure it needs to see.
				Self->DeliverFailure(LocalError, LocalJsonResponse, LocalOnComplete);
				return;
			}

			// ExecuteRequest reads the token out of SharedAuthContext as it builds the headers,
			// so the replay picks up the one the re-login just installed.
			Self->ExecuteRequest(LocalApiPath, LocalRequestBody, LocalAuthMode, LocalOnComplete, LocalRetryAttempt + 1);
		});

	Auth->NotifySessionRejected();
	return true;
}

void UEPFSubsystem::ResetRateLimitCounter()
{
	RequestsThisSecond = 0;
	RateLimitResetHandle.Reset();
	ProcessRequestQueue();
}

void UEPFSubsystem::ScheduleRetry(const FString& ApiPath, const FString& RequestBodyString, EEPFAuthMode AuthMode,
	const FOnPlayFabResponseDetailed& OnComplete, int32 RetryAttempt, float Delay)
{
	// The core ticker, not a world timer: a world timer does not run while the game is paused, and is
	// lost with its world on map travel (or never set without one), which dropped the request unanswered.
	TWeakObjectPtr<UEPFSubsystem> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(TEXT("PlayFab.Retry"), Delay,
		[WeakThis, ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt](float)
		{
			UEPFSubsystem* This = WeakThis.Get();
			if (This && !This->bDeinitialized)
			{
				This->ExecuteRequest(ApiPath, RequestBodyString, AuthMode, OnComplete, RetryAttempt + 1);
			}
			return false;
		});
}

void UEPFSubsystem::ProcessRequestQueue()
{
	while (PendingRequestQueue.Num() > 0 && RequestsThisSecond < MaxRequestsPerSecond)
	{
		FPendingRequest Pending = MoveTemp(PendingRequestQueue[0]);
		PendingRequestQueue.RemoveAt(0);

		EF_LOG(ExtendedPlayFab, Verbose, TEXT("EPFSubsystem - Processing queued request: %s"), *Pending.ApiPath);
		ExecuteRequest(Pending.ApiPath, Pending.RequestBodyString, Pending.AuthMode, Pending.OnComplete, 0);
	}
}

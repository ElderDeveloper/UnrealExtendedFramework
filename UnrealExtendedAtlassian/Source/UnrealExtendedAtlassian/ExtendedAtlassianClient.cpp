// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianClient.h"

#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianUserCache.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Http/EFHttpClient.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianClient"

/** A request plus the state needed to re-send it after a retryable failure. */
struct FExtendedAtlassianPendingRequest
{
	EEFHttpVerb Verb = EEFHttpVerb::Get;
	FString Url;
	TArray<uint8> Body;
	FString ContentType;
	TMap<FString, FString> ExtraHeaders;
	FExtendedAtlassianResponseDelegate OnComplete;
	int32 Attempt = 0;
};

namespace ExtendedAtlassianClientPrivate
{
	/**
	 * Flattens whichever error shape the response used.
	 *
	 * Jira returns { errorMessages: [...], errors: { field: msg } }; Confluence v2 returns
	 * { errors: [ { title, detail } ] }; a few endpoints just return { message }.
	 */
	FString AggregateErrorMessage(const FString& Body)
	{
		if (Body.IsEmpty())
		{
			return FString();
		}

		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			return FString();
		}

		TArray<FString> Parts;

		const TArray<TSharedPtr<FJsonValue>>* ErrorMessages = nullptr;
		if (Json->TryGetArrayField(TEXT("errorMessages"), ErrorMessages))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ErrorMessages)
			{
				const FString Text = Value->AsString();
				if (!Text.IsEmpty())
				{
					Parts.Add(Text);
				}
			}
		}

		// Jira's field-level errors are an object; Confluence's are an array. Try both.
		const TSharedPtr<FJsonObject>* FieldErrors = nullptr;
		if (Json->TryGetObjectField(TEXT("errors"), FieldErrors) && FieldErrors->IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*FieldErrors)->Values)
			{
				Parts.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Pair.Value->AsString()));
			}
		}
		else
		{
			const TArray<TSharedPtr<FJsonValue>>* ErrorArray = nullptr;
			if (Json->TryGetArrayField(TEXT("errors"), ErrorArray))
			{
				for (const TSharedPtr<FJsonValue>& Value : *ErrorArray)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (Value->TryGetObject(Object) && Object->IsValid())
					{
						FString Title;
						FString Detail;
						(*Object)->TryGetStringField(TEXT("title"), Title);
						(*Object)->TryGetStringField(TEXT("detail"), Detail);

						if (!Detail.IsEmpty() && !Title.IsEmpty())
						{
							Parts.Add(FString::Printf(TEXT("%s - %s"), *Title, *Detail));
						}
						else if (!Detail.IsEmpty() || !Title.IsEmpty())
						{
							Parts.Add(Detail.IsEmpty() ? Title : Detail);
						}
					}
				}
			}
		}

		if (Parts.Num() == 0)
		{
			FString Message;
			if (Json->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
			{
				Parts.Add(Message);
			}
		}

		return FString::Join(Parts, TEXT("; "));
	}

	/**
	 * Atlassian sends Retry-After in whole seconds. Falls back to exponential backoff.
	 * ResponseHeaders is empty when no response arrived; lookups ignore case.
	 */
	float ComputeRetryDelay(const TMap<FString, FString>& ResponseHeaders, int32 Attempt)
	{
		if (const FString* RetryAfter = ResponseHeaders.Find(TEXT("Retry-After")))
		{
			if (!RetryAfter->IsEmpty())
			{
				const float Seconds = FCString::Atof(**RetryAfter);
				if (Seconds > 0.0f)
				{
					return FMath::Clamp(Seconds, 1.0f, 120.0f);
				}
			}
		}

		return FMath::Clamp(FMath::Pow(2.0f, static_cast<float>(Attempt)), 1.0f, 60.0f);
	}

	FExtendedAtlassianError ClassifyFailure(int32 Status, const FString& Body, const TMap<FString, FString>& ResponseHeaders)
	{
		FExtendedAtlassianError Error;
		Error.HttpStatus = Status;

		const FString Detail = AggregateErrorMessage(Body);

		switch (Status)
		{
		case 400:
			Error.Code = TEXT("BadRequest");
			Error.Message = Detail.IsEmpty() ? TEXT("Atlassian rejected the request.") : Detail;
			break;

		case 401:
			Error.Code = TEXT("Unauthorized");
			Error.Message = TEXT("Atlassian rejected the credentials. Check the e-mail address and API token.");
			break;

		case 403:
		{
			Error.Code = TEXT("Forbidden");
			// Repeated failed logins trigger a CAPTCHA that the API cannot satisfy; the user has to
			// sign in through a browser once to clear it. Say so rather than reporting a bare 403.
			const FString DeniedReason = ResponseHeaders.FindRef(TEXT("X-Authentication-Denied-Reason"));

			if (DeniedReason.Contains(TEXT("CAPTCHA")))
			{
				Error.Message = TEXT("Atlassian requires a CAPTCHA for this account. Sign in through a browser once to clear it, then retry.");
			}
			else
			{
				Error.Message = Detail.IsEmpty()
					? TEXT("The account does not have permission for this operation.")
					: Detail;
			}
			break;
		}

		case 404:
			Error.Code = TEXT("NotFound");
			Error.Message = Detail.IsEmpty() ? TEXT("Not found. Check the site URL and project key.") : Detail;
			break;

		case 429:
			Error.Code = TEXT("RateLimited");
			Error.Message = TEXT("Rate limited by Atlassian.");
			Error.bRetryable = true;
			break;

		default:
			if (Status >= 500)
			{
				Error.Code = TEXT("ServerError");
				Error.Message = Detail.IsEmpty() ? TEXT("Atlassian reported a server error.") : Detail;
				Error.bRetryable = true;
			}
			else
			{
				Error.Code = FString::Printf(TEXT("Http%d"), Status);
				Error.Message = Detail.IsEmpty() ? TEXT("Unexpected response from Atlassian.") : Detail;
			}
			break;
		}

		return Error;
	}

	void ParseBody(const FString& Body, FExtendedAtlassianResponse& OutResponse)
	{
		if (Body.IsEmpty())
		{
			return;
		}

		const FString Trimmed = Body.TrimStart();
		if (Trimmed.StartsWith(TEXT("[")))
		{
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
			if (FJsonSerializer::Deserialize(Reader, OutResponse.Array))
			{
				OutResponse.bIsArray = true;
			}
			return;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		FJsonSerializer::Deserialize(Reader, OutResponse.Object);
	}

	/** Maps an HTTP method name onto the foundation's verbs, ignoring case. An empty name is GET. */
	bool TryParseVerb(const FString& VerbName, EEFHttpVerb& OutVerb)
	{
		static const EEFHttpVerb SupportedVerbs[] =
		{
			EEFHttpVerb::Get,
			EEFHttpVerb::Post,
			EEFHttpVerb::Put,
			EEFHttpVerb::Patch,
			EEFHttpVerb::Delete,
			EEFHttpVerb::Head,
			EEFHttpVerb::Options,
		};

		const FString Trimmed = VerbName.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			OutVerb = EEFHttpVerb::Get;
			return true;
		}

		for (const EEFHttpVerb Candidate : SupportedVerbs)
		{
			if (Trimmed.Equals(EFHttp::VerbToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutVerb = Candidate;
				return true;
			}
		}

		return false;
	}
}

FExtendedAtlassianClient::FExtendedAtlassianClient()
{
}

void FExtendedAtlassianClient::ReloadCredentials()
{
	++CredentialGeneration;
	Credentials.Reset();
	FExtendedAtlassianCredentialStore::Load(Credentials);

	VerifiedUser.Reset();
	LastAuthError.Reset();
	bAutomaticUserRefreshStarted = false;
	bAutomaticUserRefreshInFlight = false;

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const bool bLoadedCachedUser =
		Credentials.IsValid()
		&& Settings
		&& Settings->IsConfigured()
		&& FExtendedAtlassianUserCache::Load(
			Settings->GetNormalizedSiteUrl(),
			Credentials.Email,
			VerifiedUser);
	SetAuthState(
		!Credentials.IsValid()
			? EExtendedAtlassianAuthState::NotConfigured
			: (bLoadedCachedUser
				? EExtendedAtlassianAuthState::Verified
				: EExtendedAtlassianAuthState::Unverified),
		FExtendedAtlassianError());
}

bool FExtendedAtlassianClient::SetCredentials(const FExtendedAtlassianCredentials& InCredentials)
{
	if (!FExtendedAtlassianCredentialStore::Save(InCredentials))
	{
		return false;
	}

	++CredentialGeneration;
	Credentials = InCredentials;
	FExtendedAtlassianUserCache::Clear();
	VerifiedUser.Reset();
	LastAuthError.Reset();
	bAutomaticUserRefreshStarted = false;
	bAutomaticUserRefreshInFlight = false;
	SetAuthState(EExtendedAtlassianAuthState::Unverified, FExtendedAtlassianError());
	return true;
}

void FExtendedAtlassianClient::ClearCredentials()
{
	++CredentialGeneration;
	FExtendedAtlassianCredentialStore::Clear();
	FExtendedAtlassianUserCache::Clear();
	Credentials.Reset();
	VerifiedUser.Reset();
	LastAuthError.Reset();
	bAutomaticUserRefreshStarted = false;
	bAutomaticUserRefreshInFlight = false;
	SetAuthState(EExtendedAtlassianAuthState::NotConfigured, FExtendedAtlassianError());
}

bool FExtendedAtlassianClient::IsReady() const
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	return Settings && Settings->IsConfigured() && Credentials.IsValid();
}

void FExtendedAtlassianClient::EnsureUserInfo()
{
	if (!IsReady()
		|| bAutomaticUserRefreshStarted
		|| bAutomaticUserRefreshInFlight)
	{
		return;
	}

	bAutomaticUserRefreshStarted = true;
	bAutomaticUserRefreshInFlight = true;
	const uint64 RefreshGeneration = CredentialGeneration;
	const TWeakPtr<FExtendedAtlassianClient> WeakClient = AsShared();
	TestConnection(
		[WeakClient, RefreshGeneration](
			bool bSuccess,
			const FExtendedAtlassianUser&,
			const FExtendedAtlassianError&)
		{
			if (const TSharedPtr<FExtendedAtlassianClient> Client = WeakClient.Pin())
			{
				if (Client->CredentialGeneration != RefreshGeneration)
				{
					return;
				}
				Client->bAutomaticUserRefreshInFlight = false;
				// A later tab opening may retry a transient startup failure.
				Client->bAutomaticUserRefreshStarted = bSuccess;
			}
		});
}

void FExtendedAtlassianClient::SetAuthState(EExtendedAtlassianAuthState NewState, const FExtendedAtlassianError& InError)
{
	LastAuthError = InError;

	if (AuthState == NewState)
	{
		// Still broadcast: the error detail may have changed even when the state has not.
		AuthStateChanged.Broadcast();
		return;
	}

	AuthState = NewState;
	AuthStateChanged.Broadcast();
}

FString FExtendedAtlassianClient::BuildAuthorizationHeader() const
{
	if (!Credentials.IsValid())
	{
		return FString();
	}

	// Encode the UTF-8 bytes explicitly rather than relying on FString overload behaviour.
	const FString Combined = FString::Printf(TEXT("%s:%s"), *Credentials.Email, *Credentials.ApiToken);
	const FTCHARToUTF8 Utf8(*Combined);
	const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

	return FString::Printf(TEXT("Basic %s"), *Encoded);
}

void FExtendedAtlassianClient::Get(const FString& Url, FExtendedAtlassianResponseDelegate OnComplete)
{
	Request(TEXT("GET"), Url, FString(), OnComplete);
}

void FExtendedAtlassianClient::PostJson(const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete)
{
	Request(TEXT("POST"), Url, JsonBody, OnComplete);
}

void FExtendedAtlassianClient::PutJson(const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete)
{
	Request(TEXT("PUT"), Url, JsonBody, OnComplete);
}

void FExtendedAtlassianClient::Delete(const FString& Url, FExtendedAtlassianResponseDelegate OnComplete)
{
	Request(TEXT("DELETE"), Url, FString(), OnComplete);
}

void FExtendedAtlassianClient::Request(const FString& Verb, const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete)
{
	TArray<uint8> Body;
	if (!JsonBody.IsEmpty())
	{
		const FTCHARToUTF8 Utf8(*JsonBody);
		Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	RequestRaw(Verb, Url, MoveTemp(Body), JsonBody.IsEmpty() ? FString() : TEXT("application/json"), TMap<FString, FString>(), OnComplete);
}

void FExtendedAtlassianClient::RequestRaw(
	const FString& Verb,
	const FString& Url,
	TArray<uint8> Body,
	const FString& ContentType,
	const TMap<FString, FString>& ExtraHeaders,
	FExtendedAtlassianResponseDelegate OnComplete)
{
	// Fail fast with something actionable rather than firing a request at a malformed URL.
	auto FailImmediately = [&OnComplete](const TCHAR* Code, const TCHAR* Message)
	{
		FExtendedAtlassianResponse Response;
		Response.Error.Code = Code;
		Response.Error.Message = Message;
		OnComplete.ExecuteIfBound(Response);
	};

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Settings || !Settings->IsConfigured())
	{
		FailImmediately(TEXT("NotConfigured"), TEXT("No Atlassian site URL is set. Configure it in Project Settings > Extended Framework > Extended Atlassian."));
		return;
	}

	if (!Credentials.IsValid())
	{
		FailImmediately(TEXT("NoCredentials"), TEXT("No Atlassian credentials are stored. Enter your e-mail and API token in Project Settings > Extended Framework > Extended Atlassian."));
		return;
	}

	EEFHttpVerb ParsedVerb = EEFHttpVerb::Get;
	if (!ExtendedAtlassianClientPrivate::TryParseVerb(Verb, ParsedVerb))
	{
		UE_LOG(LogExtendedAtlassian, Warning, TEXT("Refusing to send %s %s: unsupported HTTP method."), *Verb, *Url);
		FailImmediately(TEXT("InvalidRequest"), TEXT("Unsupported HTTP method. Use GET, POST, PUT, PATCH, DELETE, HEAD or OPTIONS."));
		return;
	}

	TSharedRef<FExtendedAtlassianPendingRequest> Pending = MakeShared<FExtendedAtlassianPendingRequest>();
	Pending->Verb = ParsedVerb;
	Pending->Url = Url;
	Pending->Body = MoveTemp(Body);
	Pending->ContentType = ContentType;
	Pending->ExtraHeaders = ExtraHeaders;
	Pending->OnComplete = OnComplete;

	SendPending(Pending);
}

void FExtendedAtlassianClient::SendPending(TSharedRef<FExtendedAtlassianPendingRequest> Pending)
{
	using namespace ExtendedAtlassianClientPrivate;

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const int32 TimeoutSeconds = Settings ? Settings->RequestTimeoutSeconds : 30;
	const int32 MaxRetries = Settings ? Settings->MaxRetries : 3;

	FEFHttpRequest HttpRequest;
	HttpRequest.Url = Pending->Url;
	HttpRequest.Verb = Pending->Verb;
	HttpRequest.Headers.Add(TEXT("Accept"), TEXT("application/json"));
	HttpRequest.Headers.Add(TEXT("Authorization"), BuildAuthorizationHeader());
	HttpRequest.Headers.Add(TEXT("User-Agent"), TEXT("UnrealExtendedAtlassian"));
	HttpRequest.TimeoutSeconds = static_cast<float>(TimeoutSeconds);

	// Content-Type is sent whenever the caller names one, body or not, so it goes in as a header.
	// Clearing ContentType stops the foundation adding its JSON default to a body that has none.
	if (!Pending->ContentType.IsEmpty())
	{
		HttpRequest.Headers.Add(TEXT("Content-Type"), Pending->ContentType);
	}
	HttpRequest.ContentType.Reset();

	// Header names are case-insensitive keys, so extra headers replace the defaults above.
	for (const TPair<FString, FString>& Header : Pending->ExtraHeaders)
	{
		HttpRequest.Headers.Add(Header.Key, Header.Value);
	}

	// JSON arrives here already UTF-8 encoded; multipart uploads are raw bytes.
	HttpRequest.BodyBytes = Pending->Body;

	// Retries stay in this client, not in FEFHttpOperation: they apply to every verb (POST
	// included), on 429, 5xx and connection failures only, with Atlassian's Retry-After limits.
	HttpRequest.MaxRetries = 0;
	// This client logs its own retries and final failures, so the foundation only logs at Verbose.
	HttpRequest.bLogFailuresAsWarnings = false;

	UE_LOG(LogExtendedAtlassian, Verbose, TEXT("%s %s (attempt %d)"), EFHttp::VerbToString(Pending->Verb), *Pending->Url, Pending->Attempt + 1);

	TWeakPtr<FExtendedAtlassianClient> WeakClient = AsShared();

	FEFHttpOperation::Start(HttpRequest,
		[WeakClient, Pending, MaxRetries](const FEFHttpResponse& HttpResponse)
		{
			FExtendedAtlassianResponse Response;

			// Success and HttpError carry a status; anything else means no response arrived.
			const bool bReceivedResponse =
				HttpResponse.Result == EEFHttpResult::Success
				|| HttpResponse.Result == EEFHttpResult::HttpError;

			if (bReceivedResponse)
			{
				Response.HttpStatus = HttpResponse.StatusCode;
				Response.Body = HttpResponse.Body;

				if (Response.HttpStatus >= 200 && Response.HttpStatus < 300)
				{
					ParseBody(Response.Body, Response);
				}
				else
				{
					Response.Error = ClassifyFailure(Response.HttpStatus, Response.Body, HttpResponse.Headers);
				}
			}
			else if (HttpResponse.Result == EEFHttpResult::Cancelled)
			{
				// Only the engine cancels these (this client never does), e.g. while shutting down.
				Response.Error.Code = TEXT("Cancelled");
				Response.Error.Message = TEXT("The request to Atlassian was cancelled.");
			}
			else if (HttpResponse.Result == EEFHttpResult::InvalidRequest)
			{
				// Rejected before sending; resending the same URL cannot help.
				Response.Error.Code = TEXT("InvalidRequest");
				Response.Error.Message = FString::Printf(TEXT("The request could not be sent: %s Check the Atlassian site URL."), *HttpResponse.ErrorMessage);
			}
			else
			{
				Response.Error.Code = TEXT("Network");
				Response.Error.Message = TEXT("Could not reach the Atlassian site. Check the site URL and your network connection.");
				Response.Error.bRetryable = true;
			}

			TSharedPtr<FExtendedAtlassianClient> Client = WeakClient.Pin();

			// Retry transient failures, but only while the client is still alive.
			if (Client.IsValid() && Response.Error.bRetryable && Pending->Attempt < MaxRetries)
			{
				const float Delay = ComputeRetryDelay(HttpResponse.Headers, Pending->Attempt);
				Pending->Attempt++;

				UE_LOG(LogExtendedAtlassian, Log, TEXT("%s %s failed (%s); retrying in %.0fs (attempt %d of %d)."),
					EFHttp::VerbToString(Pending->Verb), *Pending->Url, *Response.Error.Code, Delay, Pending->Attempt + 1, MaxRetries + 1);

				Client->ScheduleRetry(Pending, Delay);
				return;
			}

			if (Response.Error.IsSet())
			{
				UE_LOG(LogExtendedAtlassian, Warning, TEXT("%s %s -> %s"),
					EFHttp::VerbToString(Pending->Verb), *Pending->Url, *Response.Error.ToString());
			}

			// FEFHttpOperation completes on the game thread; keep the guarantee explicit so Slate
			// callers never have to care, whatever the transport does.
			if (IsInGameThread())
			{
				Pending->OnComplete.ExecuteIfBound(Response);
			}
			else
			{
				AsyncTask(ENamedThreads::GameThread, [Pending, Response = MoveTemp(Response)]()
				{
					Pending->OnComplete.ExecuteIfBound(Response);
				});
			}
		});
}

void FExtendedAtlassianClient::ScheduleRetry(TSharedRef<FExtendedAtlassianPendingRequest> Pending, float DelaySeconds)
{
	TWeakPtr<FExtendedAtlassianClient> WeakClient = AsShared();

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakClient, Pending](float) -> bool
		{
			if (const TSharedPtr<FExtendedAtlassianClient> Client = WeakClient.Pin())
			{
				Client->SendPending(Pending);
			}
			return false; // one shot
		}),
		DelaySeconds);
}

void FExtendedAtlassianClient::TestConnection(TFunction<void(bool, const FExtendedAtlassianUser&, const FExtendedAtlassianError&)> OnDone)
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const FString Url = Settings ? Settings->GetJiraApiBaseUrl() + TEXT("/myself") : FString();
	const FString SiteUrl = Settings ? Settings->GetNormalizedSiteUrl() : FString();
	const FString CredentialEmail = Credentials.Email;
	const uint64 RequestCredentialGeneration = CredentialGeneration;

	TWeakPtr<FExtendedAtlassianClient> WeakClient = AsShared();

	Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[
			WeakClient,
			SiteUrl,
			CredentialEmail,
			RequestCredentialGeneration,
			OnDone = MoveTemp(OnDone)
		](const FExtendedAtlassianResponse& Response)
		{
			FExtendedAtlassianUser User;

			if (Response.IsSuccess() && Response.Object.IsValid())
			{
				Response.Object->TryGetStringField(TEXT("accountId"), User.AccountId);
				Response.Object->TryGetStringField(TEXT("displayName"), User.DisplayName);
				Response.Object->TryGetStringField(TEXT("emailAddress"), User.EmailAddress);

				const TSharedPtr<FJsonObject>* Avatars = nullptr;
				if (Response.Object->TryGetObjectField(TEXT("avatarUrls"), Avatars) && Avatars->IsValid())
				{
					(*Avatars)->TryGetStringField(TEXT("48x48"), User.AvatarUrl);
				}
			}

			bool bSuccess = Response.IsSuccess() && User.IsValid();

			FExtendedAtlassianError Error = Response.Error;
			if (Response.IsSuccess() && !User.IsValid())
			{
				// A 200 without an accountId usually means the URL resolved to something that is
				// not a Jira API — a wrong site URL, or an SSO login page returning HTML.
				Error.Code = TEXT("UnexpectedResponse");
				Error.HttpStatus = Response.HttpStatus;
				Error.Message = TEXT("The site responded but did not return a Jira account. Check that the site URL points at your Atlassian Cloud instance.");
			}

			if (const TSharedPtr<FExtendedAtlassianClient> Client = WeakClient.Pin())
			{
				const UExtendedAtlassianSettings* CurrentSettings =
					UExtendedAtlassianSettings::Get();
				const bool bRequestStillCurrent =
					Client->CredentialGeneration == RequestCredentialGeneration
					&& Client->Credentials.Email.Equals(
						CredentialEmail,
						ESearchCase::IgnoreCase)
					&& CurrentSettings
					&& CurrentSettings->GetNormalizedSiteUrl().Equals(
						SiteUrl,
						ESearchCase::IgnoreCase);
				if (!bRequestStillCurrent)
				{
					bSuccess = false;
					Error.Code = TEXT("StaleAuthRequest");
					Error.Message = TEXT("Atlassian credentials or site changed while the connection test was running. Test the current connection again.");
				}
				else if (bSuccess)
				{
					Client->VerifiedUser = User;
					Client->bAutomaticUserRefreshStarted = true;
					FExtendedAtlassianUserCache::Save(
						SiteUrl,
						CredentialEmail,
						User);
					Client->SetAuthState(EExtendedAtlassianAuthState::Verified, FExtendedAtlassianError());
					UE_LOG(LogExtendedAtlassian, Log, TEXT("Connected to Atlassian as %s (%s)."), *User.DisplayName, *User.AccountId);
				}
				else
				{
					Client->bAutomaticUserRefreshStarted = false;
					if (Error.Code == TEXT("Unauthorized"))
					{
						Client->VerifiedUser.Reset();
						FExtendedAtlassianUserCache::Clear();
					}
					Client->SetAuthState(EExtendedAtlassianAuthState::Failed, Error);
				}
			}

			if (OnDone)
			{
				OnDone(bSuccess, User, Error);
			}
		}));
}

#undef LOCTEXT_NAMESPACE

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebLog.h"
#include "Core/ESteamWebSettings.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"

void FESteamWebRequest::AddParam(const FString& Key, const FString& Value)
{
	Params.Emplace(Key, Value);
}

void FESteamWebRequest::AddParam(const FString& Key, int32 Value)
{
	Params.Emplace(Key, FString::FromInt(Value));
}

void FESteamWebRequest::AddParam(const FString& Key, int64 Value)
{
	Params.Emplace(Key, FString::Printf(TEXT("%lld"), Value));
}

void FESteamWebRequest::AddParam(const FString& Key, bool bValue)
{
	// The Web API documents boolean params as 1/0.
	Params.Emplace(Key, bValue ? TEXT("1") : TEXT("0"));
}

void FESteamWebRequest::AddParam(const FString& Key, float Value)
{
	Params.Emplace(Key, FString::SanitizeFloat(Value));
}

void FESteamWebRequest::AddParam(const FString& Key, const TArray<FString>& Values)
{
	Params.Emplace(Key, FString::Join(Values, TEXT(",")));
}

bool FESteamWebRequest::HasParam(const FString& Key) const
{
	for (const TPair<FString, FString>& Param : Params)
	{
		if (Param.Key == Key)
		{
			return true;
		}
	}
	return false;
}

FString FESteamWebRequest::BuildQueryString(const FString& ApiKeyOverride) const
{
	FString Query;

	if (bRequiresApiKey && !HasParam(TEXT("key")))
	{
		const FString ApiKey = !ApiKeyOverride.IsEmpty() ? ApiKeyOverride : UESteamWebSettings::Get()->ApiKey;
		if (!ApiKey.IsEmpty())
		{
			Query += TEXT("key=") + EFHttp::UrlEncode(ApiKey);
		}
	}

	for (const TPair<FString, FString>& Param : Params)
	{
		if (!Query.IsEmpty())
		{
			Query += TEXT("&");
		}
		Query += EFHttp::UrlEncode(Param.Key) + TEXT("=") + EFHttp::UrlEncode(Param.Value);
	}

	return Query;
}

FString FESteamWebRequest::BuildUrl(const FString& ApiKeyOverride) const
{
	const TCHAR* Host = bUsePartnerHost ? TEXT("https://partner.steam-api.com") : TEXT("https://api.steampowered.com");
	FString Url = FString::Printf(TEXT("%s/%s/%s/v%d/"), Host, *Interface, *Method, Version);

	if (Verb == EESteamWebVerb::Get)
	{
		const FString Query = BuildQueryString(ApiKeyOverride);
		if (!Query.IsEmpty())
		{
			Url += TEXT("?") + Query;
		}
	}

	return Url;
}

FString FESteamWebRequest::BuildBody(const FString& ApiKeyOverride) const
{
	return Verb == EESteamWebVerb::Post ? BuildQueryString(ApiKeyOverride) : FString();
}

void FESteamWebClient::Send(const FESteamWebRequest& Request, FOnSteamWebResponseNative Callback)
{
	const UESteamWebSettings* Settings = UESteamWebSettings::Get();

	if (Settings->bUseDevMode)
	{
		// Deterministic offline short-circuit: nothing hits the network (see UESteamWebSettings::bUseDevMode).
		UE_LOG(LogExtendedSteamWeb, Verbose, TEXT("Dev mode: %s/%s/v%d not sent"), *Request.Interface, *Request.Method, Request.Version);
		Callback.ExecuteIfBound(TEXT("{\"devmode\":true}"), false, 0);
		return;
	}

	// The query string is built here, in the request's own parameter order, and passed as part of
	// the URL; FEFHttpRequest::QueryParameters is a map and would not keep that order.
	FEFHttpRequest HttpRequest;
	HttpRequest.Url = Request.BuildUrl();
	HttpRequest.TimeoutSeconds = Settings->RequestTimeoutSeconds;
	// One attempt, as before: whether a Web API call is safe to repeat is the caller's decision.
	HttpRequest.MaxRetries = 0;

	if (Request.Verb == EESteamWebVerb::Post)
	{
		HttpRequest.Verb = EEFHttpVerb::Post;
		// Set as a header so it is sent even when the body is empty, as before.
		HttpRequest.ContentType = TEXT("application/x-www-form-urlencoded");
		HttpRequest.Headers.Add(TEXT("Content-Type"), HttpRequest.ContentType);
		HttpRequest.Body = Request.BuildBody();
	}
	else
	{
		HttpRequest.Verb = EEFHttpVerb::Get;
		HttpRequest.ContentType.Reset();
	}

	// Warn on a required-but-missing API key: the request would otherwise go out keyless and
	// come back as an opaque 403 with no local diagnostic.
	if (Request.bRequiresApiKey && Settings->ApiKey.IsEmpty())
	{
		UE_LOG(LogExtendedSteamWeb, Warning,
			TEXT("%s/%s/v%d requires a Web API key but none is configured (Project Settings -> Extended Framework -> Extended Steam Web); sending without a key."),
			*Request.Interface, *Request.Method, Request.Version);
	}

	if (Settings->bVerboseLogging)
	{
		// Never log the full URL: for GET endpoints the query string contains key=<ApiKey>, which
		// would leak the Web API key into Saved/Logs (and any bundled report). Log the call identity
		// only. FEFHttpOperation's own log lines strip the query string as well.
		UE_LOG(LogExtendedSteamWeb, Log, TEXT("Sending %s %s/%s/v%d"),
			Request.Verb == EESteamWebVerb::Post ? TEXT("POST") : TEXT("GET"),
			*Request.Interface, *Request.Method, Request.Version);
	}

	// FEFHttpOperation keeps itself alive until it finishes and calls back once, on the game thread.
	FEFHttpOperation::Start(HttpRequest,
		[OnResponse = MoveTemp(Callback), InterfaceName = Request.Interface, MethodName = Request.Method](const FEFHttpResponse& Response)
		{
			// Same contract as before: bSuccess = connected && HTTP 2xx; HttpCode is 0 when no
			// response arrived (connection failure, timeout); the body is whatever the server sent.
			const bool bConnected = Response.Result == EEFHttpResult::Success || Response.Result == EEFHttpResult::HttpError;
			const bool bSuccess = Response.IsSuccess();
			const int32 HttpCode = Response.StatusCode;

			if (!bSuccess)
			{
				UE_LOG(LogExtendedSteamWeb, Warning, TEXT("%s/%s failed (connected=%d, code=%d)"),
					*InterfaceName, *MethodName, bConnected ? 1 : 0, HttpCode);
			}
			else if (UESteamWebSettings::Get()->bVerboseLogging)
			{
				UE_LOG(LogExtendedSteamWeb, Log, TEXT("%s/%s completed (code=%d, %d bytes)"),
					*InterfaceName, *MethodName, HttpCode, Response.Body.Len());
			}

			OnResponse.ExecuteIfBound(Response.Body, bSuccess, HttpCode);
		});
}

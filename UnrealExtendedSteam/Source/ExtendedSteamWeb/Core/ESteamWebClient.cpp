// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebLog.h"
#include "Core/ESteamWebSettings.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

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
			Query += TEXT("key=") + FGenericPlatformHttp::UrlEncode(ApiKey);
		}
	}

	for (const TPair<FString, FString>& Param : Params)
	{
		if (!Query.IsEmpty())
		{
			Query += TEXT("&");
		}
		Query += FGenericPlatformHttp::UrlEncode(Param.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Param.Value);
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

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Request.BuildUrl());
	HttpRequest->SetTimeout(Settings->RequestTimeoutSeconds);

	if (Request.Verb == EESteamWebVerb::Post)
	{
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
		HttpRequest->SetContentAsString(Request.BuildBody());
	}
	else
	{
		HttpRequest->SetVerb(TEXT("GET"));
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
		// would leak the Web API key into Saved/Logs (and any bundled report). Log the call identity only.
		UE_LOG(LogExtendedSteamWeb, Log, TEXT("Sending %s %s/%s/v%d"),
			Request.Verb == EESteamWebVerb::Post ? TEXT("POST") : TEXT("GET"),
			*Request.Interface, *Request.Method, Request.Version);
	}

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[Callback = MoveTemp(Callback), Interface = Request.Interface, Method = Request.Method]
		(FHttpRequestPtr /*CompletedRequest*/, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			const int32 HttpCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			const bool bSuccess = bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(HttpCode);
			const FString Json = Response.IsValid() ? Response->GetContentAsString() : FString();

			if (!bSuccess)
			{
				UE_LOG(LogExtendedSteamWeb, Warning, TEXT("%s/%s failed (connected=%d, code=%d)"),
					*Interface, *Method, bConnectedSuccessfully ? 1 : 0, HttpCode);
			}
			else if (UESteamWebSettings::Get()->bVerboseLogging)
			{
				UE_LOG(LogExtendedSteamWeb, Log, TEXT("%s/%s completed (code=%d, %d bytes)"),
					*Interface, *Method, HttpCode, Json.Len());
			}

			Callback.ExecuteIfBound(Json, bSuccess, HttpCode);
		});

	HttpRequest->ProcessRequest();
}

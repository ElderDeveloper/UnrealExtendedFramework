// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebTypes.h"

/**
 * Description of one Steam Web API call.
 * URL scheme: {host}/{Interface}/{Method}/v{Version}/?params
 * Hosts: https://api.steampowered.com (public) and https://partner.steam-api.com (publisher key).
 *
 * BuildUrl/BuildBody are deterministic (no request is sent) so tests can assert exact strings.
 * Query/body parameter order is insertion order; an auto-injected "key" param goes first.
 */
struct EXTENDEDSTEAMWEB_API FESteamWebRequest
{
	/** Web API interface, e.g. "ISteamNews". */
	FString Interface;

	/** Web API method, e.g. "GetNewsForApp". */
	FString Method;

	/** Method version — the v{N} path segment. */
	int32 Version = 1;

	/** HTTP verb. For Post, params are sent as an application/x-www-form-urlencoded body. */
	EESteamWebVerb Verb = EESteamWebVerb::Get;

	/** Use https://partner.steam-api.com (publisher-key endpoints) instead of the public host. */
	bool bUsePartnerHost = false;

	/** Auto-inject the "key" param from UESteamWebSettings::ApiKey when not already present. */
	bool bRequiresApiKey = true;

	void AddParam(const FString& Key, const FString& Value);
	/** Overload for string literals: without it TEXT("...") binds to the bool overload. */
	void AddParam(const FString& Key, const TCHAR* Value) { AddParam(Key, FString(Value)); }
	void AddParam(const FString& Key, int32 Value);
	void AddParam(const FString& Key, int64 Value);
	void AddParam(const FString& Key, bool bValue);
	void AddParam(const FString& Key, float Value);

	/** Comma-joins the values into a single param (e.g. "steamids"). */
	void AddParam(const FString& Key, const TArray<FString>& Values);

	/**
	 * Full request URL. For Get, params are URL-encoded into the query string;
	 * for Post, they belong in the body and the URL carries no query.
	 * ApiKeyOverride (when non-empty) replaces the settings ApiKey — lets tests avoid config state.
	 */
	FString BuildUrl(const FString& ApiKeyOverride = FString()) const;

	/** application/x-www-form-urlencoded body for Post requests; empty for Get. */
	FString BuildBody(const FString& ApiKeyOverride = FString()) const;

private:
	/** Ordered key/value pairs — order is preserved into the query string / body. */
	TArray<TPair<FString, FString>> Params;

	bool HasParam(const FString& Key) const;

	/** URL-encoded "k=v&k=v" string with the API key injected when required. */
	FString BuildQueryString(const FString& ApiKeyOverride) const;
};

/**
 * The single HTTP core every Steam Web interface subsystem sends through.
 * Pure HTTPS + JSON — no Steamworks SDK, no Steam client dependency.
 *
 * Requests go out through UnrealExtendedFoundation's FEFHttpOperation, so they can be answered
 * offline by FEFScopedHttpMock in tests. The Web API key travels in the query string (GET) or the
 * form body (POST) and never reaches the log: this client logs only the interface/method, and
 * FEFHttpOperation strips query strings from every URL it logs.
 */
class EXTENDEDSTEAMWEB_API FESteamWebClient
{
public:
	/**
	 * Sends the request (one attempt, UESteamWebSettings::RequestTimeoutSeconds) and invokes
	 * Callback once on completion, on the game thread after Send() has returned.
	 * bSuccess = connected && HTTP 2xx; HttpCode is 0 when no response arrived. In dev mode
	 * (UESteamWebSettings::bUseDevMode) nothing is sent and the callback fires immediately, inside
	 * Send(), with bSuccess=false, code 0 and a {"devmode":true} body.
	 */
	static void Send(const FESteamWebRequest& Request, FOnSteamWebResponseNative Callback);
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ESteamWebTypes.generated.h"

/**
 * Completion delegate for a Steam Web API call, exposed to Blueprint.
 * Single-cast dynamic delegate: passed as a parameter to BlueprintCallable
 * functions so Blueprint wires an event to each request.
 *
 * JsonResponse   - raw JSON body returned by the endpoint (empty when no response arrived).
 * bSuccess       - true when the request connected and returned an HTTP 2xx code.
 * HttpStatusCode - the HTTP status code, or 0 when no response was received (or in dev mode).
 */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnSteamWebResponse, const FString&, JsonResponse, bool, bSuccess, int32, HttpStatusCode);

/** Native (C++) counterpart of FOnSteamWebResponse. */
DECLARE_DELEGATE_ThreeParams(FOnSteamWebResponseNative, const FString& /*Json*/, bool /*bSuccess*/, int32 /*HttpCode*/);

/** HTTP verb for a Steam Web API request. */
UENUM(BlueprintType)
enum class EESteamWebVerb : uint8
{
	Get,
	Post
};

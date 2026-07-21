// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/ESteamWebClient.h"
#include "Core/ESteamWebTypes.h"
#include "ESteamWebSubsystem.generated.h"

class UESteamWebSettings;

/**
 * Abstract base for all Steam Web API interface subsystems.
 * Deliberately NOT derived from UESteamSubsystem: this module is SDK-free and must work
 * without the Steam client (dedicated servers, non-Steam platforms, plain tools).
 */
UCLASS(Abstract)
class EXTENDEDSTEAMWEB_API UESteamWebSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	/**
	 * Sends the request through FESteamWebClient, wrapping the Blueprint delegate
	 * into the native one (ExecuteIfBound — an unbound callback is fine).
	 */
	void SendWebRequest(FESteamWebRequest&& Request, const FOnSteamWebResponse& Callback);

	/**
	 * Builds a request pre-filled with the interface/method/version and the host/verb/key flags,
	 * so call sites only add their params. Mirrors the FESteamWebRequest defaults (public host,
	 * GET, key required) unless overridden.
	 */
	FESteamWebRequest MakeRequest(const FString& Interface, const FString& Method, int32 Version,
		EESteamWebVerb Verb = EESteamWebVerb::Get, bool bUsePartnerHost = false, bool bRequiresApiKey = true) const;

	/** AppId when positive, otherwise the configured default (UESteamWebSettings::AppId). */
	int32 ResolveAppId(int32 AppId) const;

	/** SteamId when non-empty, otherwise the configured development fallback (UESteamWebSettings::DevSteamId). */
	FString ResolveSteamId(const FString& SteamId) const;

	const UESteamWebSettings* GetWebSettings() const;
};

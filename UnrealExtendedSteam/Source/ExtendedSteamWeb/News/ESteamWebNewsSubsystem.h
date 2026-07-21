// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebNewsSubsystem.generated.h"

/**
 * ISteamNews — game news feed access (public endpoints, no API key required).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebNewsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamNews/GetNewsForApp/v2 — news items for an app.
	 * AppId <= 0 falls back to the configured AppId; Count/MaxLength <= 0 use the endpoint defaults
	 * (MaxLength 0 would mean full content anyway, so it is simply omitted).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|News", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetNewsForApp(int32 AppId, int32 Count, int32 MaxLength, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamNews/GetNewsForAppAuthed/v2 — the publisher-authed variant of GetNewsForApp: same news
	 * items but on the partner host with a Web API key, so it can read news for unreleased apps.
	 * AppId <= 0 falls back to the configured AppId; Count/MaxLength <= 0 use the endpoint defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|News", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetNewsForAppAuthed(int32 AppId, int32 Count, int32 MaxLength, const FOnSteamWebResponse& OnResponse);
};

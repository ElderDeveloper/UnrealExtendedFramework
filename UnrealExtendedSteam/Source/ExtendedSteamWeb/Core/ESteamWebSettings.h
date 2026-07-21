// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ESteamWebSettings.generated.h"

/**
 * Project settings for the Steam Web API module (Project Settings -> Extended Framework -> Extended Steam Web).
 * This module is pure HTTPS + JSON — it never touches the Steamworks SDK or the Steam client.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Extended Steam Web"))
class EXTENDEDSTEAMWEB_API UESteamWebSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UESteamWebSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override;

	static const UESteamWebSettings* Get() { return GetDefault<UESteamWebSettings>(); }

	/**
	 * Steam Web API key, auto-injected as the "key" query parameter for endpoints that require one.
	 *
	 * WARNING: publisher Web API keys must NEVER ship in client builds. Anything stored here ends up
	 * in DefaultGame.ini and therefore in packaged builds, where it can be extracted trivially.
	 * This field is a development convenience only — production calls that need a publisher key
	 * belong on a trusted server, never in the game client.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|Auth")
	FString ApiKey;

	/**
	 * Steam application id used when a call does not specify one. 480 is Valve's public test app (Spacewar).
	 *
	 * NOTE: this is SEPARATE from the Steamworks-client app id (UESteamSettings::SteamAppId). Setting one
	 * does not affect the other — for a non-480 app you must set BOTH (this key drives the Web API module;
	 * SteamAppId drives the Steam client SDK).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|General")
	int32 AppId = 480;

	/** SteamID64 used as a development fallback for calls that take a Steam id and were given none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|General")
	FString DevSteamId;

	/** Seconds before an in-flight web request is considered timed out. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|General", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float RequestTimeoutSeconds = 10.0f;

	/**
	 * Dev mode: requests are not sent over the network. The callback fires with bSuccess=false,
	 * HttpCode=0 and a "{\"devmode\":true}" body — deterministic for tests and offline development.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|Debug")
	bool bUseDevMode = false;

	/** Verbose logging for the LogExtendedSteamWeb category (logs full request URLs — may include the API key). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|Debug")
	bool bVerboseLogging = false;

	/** Routes ISteamMicroTxn calls to ISteamMicroTxnSandbox for testing purchases without real money. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam Web|MicroTxn")
	bool bMicroTxnSandbox = false;
};

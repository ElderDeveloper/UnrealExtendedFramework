// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ESteamSettings.generated.h"

/**
 * Project settings for the Extended Steam plugin (Project Settings -> Extended Framework -> Extended Steam).
 * Settings for the Steam Web API live separately in UESteamWebSettings.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Extended Steam"))
class EXTENDEDSTEAMSHARED_API UESteamSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UESteamSettings();

	//~ UDeveloperSettings
	virtual FName GetCategoryName() const override;

	static const UESteamSettings* Get() { return GetDefault<UESteamSettings>(); }

	/**
	 * Steam application id. 480 is Valve's public test app (Spacewar).
	 *
	 * NOTE: this is SEPARATE from the Web API app id (UESteamWebSettings::AppId). Setting one does not
	 * affect the other — for a non-480 app you must set BOTH (this key drives the Steam client SDK;
	 * UESteamWebSettings::AppId drives the Web API module).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General")
	int32 SteamAppId = 480;

	/** Initialize the Steam client API automatically when the plugin loads. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General")
	bool bInitializeSteamOnStartup = true;

	/** Also initialize the Steam client API in the editor (enables Steam features in PIE). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General", meta = (EditCondition = "bInitializeSteamOnStartup"))
	bool bInitializeSteamInEditor = true;

	/**
	 * Shipping builds only: relaunch through the Steam client when the game was started
	 * directly from the executable (SteamAPI_RestartAppIfNecessary).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General")
	bool bRelaunchInSteam = false;

	/** Seconds before an in-flight Steam async operation is considered timed out. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float AsyncTaskTimeoutSeconds = 10.0f;

	/** Verbose logging for the LogExtendedSteam category. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|Debug")
	bool bVerboseLogging = false;
};

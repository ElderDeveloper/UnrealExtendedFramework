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

	/**
	 * Also initialize the Steam client API in the editor (enables Steam features in PIE).
	 *
	 * Turning this OFF takes effect on the next editor start — the Steam client API cannot be
	 * un-initialized safely mid-session once something has begun using it.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General", meta = (EditCondition = "bInitializeSteamOnStartup", ConfigRestartRequired = true))
	bool bInitializeSteamInEditor = true;

	/**
	 * Editor only: hold the Steam client API back until a PIE session starts, and shut it down again
	 * when it ends, instead of initializing it for the whole editor session.
	 *
	 * Initializing Steam registers the *process* with the Steam client, so an always-on editor makes
	 * Steam report the account as playing the game (and accrue store playtime) for as long as the
	 * editor is open, whether or not anyone pressed Play. Deferring to PIE keeps Steam features
	 * working while you actually play and leaves the account idle while you edit.
	 *
	 * Partial by nature: SteamAPI_Shutdown releases the API but does NOT remove the process from the
	 * Steam client's running-games list — Steam drops a tracked pid only when it exits. So this stops
	 * an editor that never played from counting as in-game, but once someone presses Play the editor
	 * stays in-game until it closes. For a session that ends cleanly, play as Standalone Game (its own
	 * process), or turn bInitializeSteamInEditor off entirely.
	 *
	 * Has no effect on cooked/standalone runs, which are their own process and initialize normally.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Steam|General", meta = (EditCondition = "bInitializeSteamOnStartup && bInitializeSteamInEditor"))
	bool bDeferEditorInitToPIE = true;

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

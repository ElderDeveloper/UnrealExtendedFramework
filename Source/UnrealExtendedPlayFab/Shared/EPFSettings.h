// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlayerProfile/EPFPlayerProfileSubsystem.h"
#include "EPFSettings.generated.h"


/**
 * Developer settings for PlayFab configuration.
 * These settings appear in Project Settings → Extended Framework → PlayFab.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Extended PlayFab"))
class UNREALEXTENDEDPLAYFAB_API UEPFSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UEPFSettings();

	// ── Credentials ──────────────────────────────────────────────────────────

	/** The Title ID from the PlayFab Game Manager */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "Title ID"))
	FString TitleId;

	/** Optional override for the PlayFab API base URL. Leave empty to use the standard title endpoint. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Credentials",
		meta = (DisplayName = "API Base URL Override"))
	FString ApiBaseUrlOverride;


	// ── Auth Settings ────────────────────────────────────────────────────────

	/** If true, automatically create a new PlayFab account when logging in for the first time */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auth",
		meta = (DisplayName = "Create Account on First Login"))
	bool bCreateAccountOnFirstLogin = true;


	// ── Debug ────────────────────────────────────────────────────────────────

	/** If true, log all PlayFab HTTP requests and responses */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "Enable Verbose Logging"))
	bool bEnableVerboseLogging = false;

	/** If true, sends an identifying SDK header with each request. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "Include SDK Header"))
	bool bIncludeSdkHeader = true;

	/** Value used for the X-PlayFabSDK header when enabled. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "SDK Header Value"))
	FString SDKHeaderValue;

	/** Optional developer secret key for server-only tooling. Do not configure this in a client build. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Server",
		meta = (DisplayName = "Developer Secret Key"))
	FString DeveloperSecretKey;


	// ── Profile Constraints ───────────────────────────────────────────────────

	/**
	 * Controls which fields PlayFab returns for GetPlayerProfile.
	 *
	 * IMPORTANT: Only enable fields that your title has permitted under
	 * PlayFab Dashboard → Title Settings → Client Profile Constraints.
	 * Requesting a field that is not permitted returns error 1303
	 * (RequestViewConstraintParamsNotAllowed).
	 *
	 * Configured once here — all GetPlayerProfile calls use these settings.
	 * This replaces the old per-call FEPFProfileConstraints parameter.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Profile Constraints",
		meta = (ShowOnlyInnerProperties))
	FEPFProfileConstraints ProfileConstraints;


	// ── Auto Analytics ────────────────────────────────────────────────────────

	/**
	 * If true, the Analytics subsystem automatically hooks into PlayFab login/logout,
	 * level transitions, and app lifecycle events and sends them as telemetry.
	 * Events captured while offline are persisted to disk and flushed on next login.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auto Analytics",
		meta = (DisplayName = "Enable Auto Analytics"))
	bool bAutoAnalyticsEnabled = false;

	/**
	 * Controls which lifecycle events are automatically tracked.
	 * Only relevant when Enable Auto Analytics is true.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Auto Analytics",
		meta = (DisplayName = "Auto Analytics Config", EditCondition = "bAutoAnalyticsEnabled", ShowOnlyInnerProperties))
	FEPFAutoAnalyticsConfig AutoAnalyticsConfig;

	/**
	 * Maximum number of events to keep in the offline queue.
	 * When the limit is reached, the oldest events are dropped to make room for new ones.
	 * Range: 10–500.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Auto Analytics",
		meta = (DisplayName = "Offline Queue Limit", EditCondition = "bAutoAnalyticsEnabled", ClampMin = "10", ClampMax = "500"))
	int32 OfflineQueueLimit = 100;


	// ── Helpers ──────────────────────────────────────────────────────────────


	/** Get the singleton settings instance */
	static const UEPFSettings* Get()
	{
		return GetDefault<UEPFSettings>();
	}

	/** Returns the PlayFab API base URL for this title */
	FString GetApiBaseUrl() const
	{
		if (!ApiBaseUrlOverride.IsEmpty())
		{
			return ApiBaseUrlOverride;
		}
		return FString::Printf(TEXT("https://%s.playfabapi.com"), *TitleId);
	}

	/** Category path for Project Settings UI */
	virtual FName GetCategoryName() const override { return FName(TEXT("Extended Framework")); }
};

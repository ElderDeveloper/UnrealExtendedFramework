// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shared/EPFSettings.h"
#include "Shared/EPFSubsystem.h"
#include "Shared/EPFTypes.h"

/**
 * Gives a test a configured PlayFab title and a signed-in session without a real login, and puts
 * back the project's settings and the previous session when restored or destroyed.
 *
 * The session lives in UEPFSubsystem's protected statics, so this is a friend of that class. Restore
 * only after the test game instance has shut down: its auth subsystem clears the session then.
 * Shared by the plugin's test files; tests only.
 */
struct FEPFSharedAuthTestScope
{
	FEPFSharedAuthTestScope()
	{
		const UEPFSettings* Settings = GetDefault<UEPFSettings>();
		SavedTitleId = Settings->TitleId;
		SavedApiBaseUrlOverride = Settings->ApiBaseUrlOverride;
		bSavedAutoLoginOnStart = Settings->bAutoLoginOnStart;
		bSavedAutoAnalyticsEnabled = Settings->bAutoAnalyticsEnabled;
		bSavedSendAnalyticsEvents = Settings->bSendAnalyticsEvents;
		bSavedEnablePlatformProvider = Settings->bEnablePlatformProvider;
		SavedPlatformCallTimeoutSeconds = Settings->PlatformCallTimeoutSeconds;

		SavedAuthContext = UEPFSubsystem::SharedAuthContext;
		SavedSessionTicket = UEPFSubsystem::SharedSessionTicket;
		SavedPlayFabId = UEPFSubsystem::SharedPlayFabId;
	}

	~FEPFSharedAuthTestScope()
	{
		Restore();
	}

	FEPFSharedAuthTestScope(const FEPFSharedAuthTestScope&) = delete;
	FEPFSharedAuthTestScope& operator=(const FEPFSharedAuthTestScope&) = delete;

	/** Call before creating the game instance, so its subsystems start with these settings. */
	void ConfigureTitle(const FString& InTitleId, const FString& InApiBaseUrl)
	{
		UEPFSettings* Settings = GetMutableDefault<UEPFSettings>();
		Settings->TitleId = InTitleId;
		Settings->ApiBaseUrlOverride = InApiBaseUrl;
		// Keep the test game instance quiet: no sign-in attempt and no telemetry of its own. A test of
		// the analytics sink turns bSendAnalyticsEvents back on itself.
		Settings->bAutoLoginOnStart = false;
		Settings->bAutoAnalyticsEnabled = false;
		Settings->bSendAnalyticsEvents = false;
		Settings->bEnablePlatformProvider = true;
	}

	/**
	 * What a successful login would leave behind. Session-ticket calls need only the ticket; entity
	 * calls (statistics, leaderboards, telemetry) also need InEntityToken.
	 */
	void SignIn(const FString& InSessionTicket, const FString& InPlayFabId, const FString& InEntityToken = FString(),
		const FString& InEntityId = FString(), const FString& InEntityType = TEXT("title_player_account"))
	{
		FEPFAuthContext Context;
		Context.SessionTicket = InSessionTicket;
		Context.PlayFabId = InPlayFabId;
		Context.EntityToken = InEntityToken;
		Context.EntityId = InEntityId;
		Context.EntityType = InEntityToken.IsEmpty() ? FString() : InEntityType;
		UEPFSubsystem::SharedAuthContext = Context;
		UEPFSubsystem::SharedSessionTicket = InSessionTicket;
		UEPFSubsystem::SharedPlayFabId = InPlayFabId;
	}

	/** No session at all, whatever an earlier test or a real login left behind. */
	void SignOut()
	{
		UEPFSubsystem::SharedAuthContext.Reset();
		UEPFSubsystem::SharedSessionTicket.Empty();
		UEPFSubsystem::SharedPlayFabId.Empty();
	}

	void Restore()
	{
		if (bRestored)
		{
			return;
		}
		bRestored = true;

		UEPFSettings* Settings = GetMutableDefault<UEPFSettings>();
		Settings->TitleId = SavedTitleId;
		Settings->ApiBaseUrlOverride = SavedApiBaseUrlOverride;
		Settings->bAutoLoginOnStart = bSavedAutoLoginOnStart;
		Settings->bAutoAnalyticsEnabled = bSavedAutoAnalyticsEnabled;
		Settings->bSendAnalyticsEvents = bSavedSendAnalyticsEvents;
		Settings->bEnablePlatformProvider = bSavedEnablePlatformProvider;
		Settings->PlatformCallTimeoutSeconds = SavedPlatformCallTimeoutSeconds;

		UEPFSubsystem::SharedAuthContext = SavedAuthContext;
		UEPFSubsystem::SharedSessionTicket = SavedSessionTicket;
		UEPFSubsystem::SharedPlayFabId = SavedPlayFabId;
	}

private:
	FString SavedTitleId;
	FString SavedApiBaseUrlOverride;
	bool bSavedAutoLoginOnStart = false;
	bool bSavedAutoAnalyticsEnabled = false;
	bool bSavedSendAnalyticsEvents = true;
	bool bSavedEnablePlatformProvider = true;
	float SavedPlatformCallTimeoutSeconds = 30.0f;
	FEPFAuthContext SavedAuthContext;
	FString SavedSessionTicket;
	FString SavedPlayFabId;
	bool bRestored = false;
};

#endif // WITH_DEV_AUTOMATION_TESTS

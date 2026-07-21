// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAuthSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFLoginComplete, const FEPFResult&, Result, const FString&, PlayFabId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFLogoutComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFDisplayNameUpdated, const FEPFResult&, Result, const FString&, NewDisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFRegistrationComplete, const FEPFResult&, Result, const FString&, PlayFabId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAccountRecoveryEmailSent, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFUsernamePasswordAdded, const FEPFResult&, Result);

/**
 * Manages PlayFab authentication — Steam, Custom ID, Device ID, Email, and PlayFab login.
 * Supports automatic session refresh to prevent token expiry.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAuthSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Login with a Steam session ticket */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void LoginWithSteam(const FString& SteamTicket);

	/** Login with a custom identifier (e.g. player name, GUID) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void LoginWithCustomId(const FString& CustomId);

	/** Login with a device-generated identifier (anonymous) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void LoginWithDeviceId();

	/** Login with email address and password (existing accounts only) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void LoginWithEmail(const FString& Email, const FString& Password);

	/** Login with PlayFab username and password */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void LoginWithPlayFab(const FString& Username, const FString& Password);

	/** Register a new PlayFab account with username, email, and password */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void RegisterUser(const FString& Username, const FString& Email, const FString& Password);

	/** Add username/password to an existing anonymous account */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void AddUsernamePassword(const FString& Username, const FString& Email, const FString& Password);

	/** Send a password recovery email */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void SendAccountRecoveryEmail(const FString& Email);

	/** Update the player's display name (shown in leaderboards) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void UpdateDisplayName(const FString& DisplayName);

	/** Clear the current session */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void Logout();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if a PlayFab session is active */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	bool IsLoggedIn() const;

	/** Get the display name from the last login (if available) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	FString GetDisplayName() const;

	/** Check if this was a newly created account */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	bool WasNewlyCreated() const;

	/** Entity ID from the login response (used by Entity API: Groups, Matchmaking) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	FString GetEntityId() const;

	/** Entity Type from the login response (typically "title_player_account") */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	FString GetEntityType() const;

	/** Entity Token for Entity API calls */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Auth")
	FString GetEntityToken() const;

	/** Enable or disable automatic session refresh (default: enabled after login) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void SetAutoSessionRefresh(bool bEnabled);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFLoginComplete OnLoginComplete;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFLogoutComplete OnLogoutComplete;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFDisplayNameUpdated OnDisplayNameUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFRegistrationComplete OnRegistrationComplete;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFAccountRecoveryEmailSent OnAccountRecoveryEmailSent;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Auth")
	FOnEPFUsernamePasswordAdded OnUsernamePasswordAdded;
	
	enum ELastLoginMethod { LM_None, LM_Steam, LM_CustomId, LM_DeviceId, LM_Email, LM_PlayFab };
private:

	FString CachedDisplayName;
	FString CachedEntityId;
	FString CachedEntityType;
	FString CachedEntityToken;
	bool bNewlyCreated = false;

	/** Shared login response handler */
	void HandleLoginResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> JsonResponse);

	// ── Session Refresh ─────────────────────────────────────────────────────

	ELastLoginMethod LastLoginMethod = LM_None;
	FString SavedCredential1;
	FString SavedCredential2;

	FTimerHandle SessionRefreshTimer;
	bool bAutoRefreshEnabled = true;

	/** Start session refresh timer (30 min intervals) */
	void StartSessionRefreshTimer();

	/** Called by timer — silently re-authenticates using stored credentials */
	void RefreshSession();
};

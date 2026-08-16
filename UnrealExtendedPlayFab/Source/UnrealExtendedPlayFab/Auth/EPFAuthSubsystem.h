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
 * How a PlayFab sign-in authenticates. File scope rather than class scope so credential
 * providers can name it; UEPFAuthSubsystem::ELastLoginMethod still resolves via the alias below.
 */
enum EEPFLoginMethod { LM_None, LM_Steam, LM_CustomId, LM_DeviceId, LM_Email, LM_PlayFab };

/**
 * One sign-in's worth of credentials, minted on demand by the game.
 *
 * Credential1 is the ticket / custom id / e-mail / username; Credential2 is the password for
 * the two methods that have one.
 */
struct FEPFLoginCredentials
{
	EEPFLoginMethod Method = LM_None;
	FString Credential1;
	FString Credential2;
};

/**
 * A provider's answer. Reply with bSuccess = false to abandon the attempt — the auth subsystem
 * cannot tell "still working" from "gave up", so a provider that never replies stalls sign-in
 * until the request times out.
 */
DECLARE_DELEGATE_TwoParams(FEPFCredentialsReady, bool /*bSuccess*/, const FEPFLoginCredentials& /*Credentials*/);

/**
 * Asked to mint fresh credentials, possibly asynchronously. This is the seam that keeps the
 * PlayFab plugin free of Steam/EOS dependencies: the game registers a provider that knows how
 * to obtain a credential, and the plugin only knows how to spend one.
 */
DECLARE_DELEGATE_OneParam(FEPFCredentialProvider, FEPFCredentialsReady /*Reply*/);

/**
 * A re-authentication attempt settled. Non-dynamic and internal: this coordinates the request
 * layer with auth, so that many subsystems hitting a rejected session at once share one re-login.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEPFReauthFinished, bool /*bSuccess*/);

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
	
	using ELastLoginMethod = EEPFLoginMethod;


	// ── Credential Provider ──────────────────────────────────────────────────

	/**
	 * Register the game's credential source. The provider is asked for a *fresh* credential on
	 * auto-login and on every session refresh, which is what makes ticket-based methods work:
	 * a Steam auth ticket is spent once, so replaying the stored one re-authenticates with
	 * something PlayFab rejects. Registering from a module that already depends on Steam/EOS
	 * keeps that dependency out of this plugin.
	 */
	void SetCredentialProvider(FEPFCredentialProvider Provider);

	void ClearCredentialProvider();

	bool HasCredentialProvider() const { return CredentialProvider.IsBound(); }

	/**
	 * Sign in using the registered provider. Called automatically at startup when
	 * bAutoLoginOnStart is set; safe to call by hand for a retry.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Auth")
	void TryAutoLogin();


	// ── Re-authentication ────────────────────────────────────────────────────

	/**
	 * Report that PlayFab rejected an authenticated call, so the session should be rebuilt.
	 * Coalesced: concurrent reports from different subsystems share one re-login, and every
	 * caller hears the outcome on OnReauthFinished. Safe to call when nothing can be done —
	 * it answers false rather than doing nothing.
	 */
	void NotifySessionRejected();

	/** True when a re-login is possible at all: a credential provider, or a stored credential. */
	bool CanReauthenticate() const;

	bool IsReauthenticating() const { return bReauthInFlight; }

	/** Settles exactly once per NotifySessionRejected, success or failure. */
	FOnEPFReauthFinished OnReauthFinished;

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

	/** Called by timer — silently re-authenticates via the provider, or stored credentials. */
	void RefreshSession();


	// ── Credential Provider ─────────────────────────────────────────────────

	FEPFCredentialProvider CredentialProvider;

	/** Ask the provider for a credential and sign in with whatever comes back. */
	void RequestCredentialsAndLogin(const TCHAR* Reason);

	void HandleProvidedCredentials(bool bSuccess, const FEPFLoginCredentials& Credentials);

	void LoginWithCredentials(const FEPFLoginCredentials& Credentials);

	/** Single exit point, so an unanswered request cannot latch the in-flight flag. */
	void FinishCredentialRequest();

	void OnCredentialRequestTimeout();

	bool bCredentialRequestInFlight = false;
	FTimerHandle CredentialRequestTimeoutTimer;


	// ── Re-authentication ───────────────────────────────────────────────────

	bool bReauthInFlight = false;
	FTimerHandle ReauthTimeoutTimer;

	/**
	 * Broadcast OnReauthFinished and clear the flag. Called from every point a login attempt
	 * settles; a latched flag would make the *first* dropped session disable re-auth for good.
	 */
	void CompleteReauth(bool bSuccess);

	void OnReauthTimeout();

	/** Backstop: covers a login that neither succeeds nor reports failure. */
	static constexpr float ReauthTimeoutSeconds = 60.0f;

	/** A provider may have to reach a platform backend; generous, but bounded. */
	static constexpr float CredentialRequestTimeoutSeconds = 30.0f;
};

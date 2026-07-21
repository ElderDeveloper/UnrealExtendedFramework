// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAuthSubsystem.generated.h"

class FOnlineAccountCredentials;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLoginComplete, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSLogoutComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLoginStatusChanged, EEOSLoginStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAuthTokenRefreshed, bool, bSuccess, const FString&, NewToken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPersistentAuthDeleted, bool, bSuccess);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSAuthConnectLoginComplete, bool, bSuccess, const FString&, ProductUserId, const FString&, ErrorMessage);

/**
 * Handles Epic Account Services authentication AND EOS Connect (Game Services) authentication.
 *
 * Two authentication layers:
 * - Auth Login: Epic Account login (needed for social features, overlay)
 * - Connect Login: Platform login for Game Services (Steam, PSN, Xbox, DeviceId, etc.)
 *
 * For Steam/console releases: use ConnectLogin directly without Auth Login.
 * For Epic releases: Auth Login → auto-chain → Connect Login.
 *
 * Return-value convention (all action methods):
 * - true  = the operation was started (or completed synchronously); its completion
 *           delegate WILL fire with the result.
 * - false = rejected / failed to start. When the rejection is because an operation of
 *           the SAME kind is already in flight (login-while-login-pending,
 *           logout-while-logout-pending, already-logged-in), NO delegate fires for THIS
 *           call — rejections are never echoed on the shared completion delegates, so
 *           the in-flight operation's waiters cannot be poisoned by a duplicate call.
 *           Other pre-flight failures (EOS unavailable, bad arguments, missing settings)
 *           still broadcast a failure — but only when nothing of the same kind is in
 *           flight.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAuthSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Epic Auth Actions ─────────────────────────────────────────────────────

	/**
	 * Login using the specified credential type (Epic Account Services).
	 *
	 * Notes:
	 * - DeviceCode is NOT supported by the EOS SDK (superseded by ExternalAuth,
	 *   see eos_auth_types.h) — passing it logs an error and fails.
	 * - ExternalAuth requires the external token type; use LoginWithExternalAuth()
	 *   instead. Passing ExternalAuth here logs an error and fails.
	 *
	 * @return true = login started; OnLoginComplete will fire. false = rejected/failed
	 *         to start. If rejected because a login is already in flight or the user is
	 *         already logged in, NO delegate fires for THIS call; other pre-flight
	 *         failures broadcast OnLoginComplete(false, ...) when no login is in flight.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool Login(EEOSLoginType LoginType, const FString& Id = TEXT(""), const FString& Token = TEXT(""));

	/**
	 * Login to Epic Account Services with an external provider token (Steam, PSN, Xbox, etc.).
	 *
	 * The engine requires the credential string "externalauth:<TokenType>" — a bare
	 * "externalauth" is rejected at parse (FUserManagerEOS::CallEOSAuthLogin). This function
	 * builds the correct token-type suffix from the credential type.
	 *
	 * @param CredentialType   Which external provider issued the token
	 * @param Token            The external auth token (hex-encoded for Steam session tickets)
	 * @param ExternalAccountId Optional external account ID owning the token. If set, it must match
	 *                          the token's account or login fails (eos_auth_types.h, EOS_LCT_ExternalAuth).
	 * @return true = login started; OnLoginComplete will fire. false = rejected/failed
	 *         to start (same rules as Login — an in-flight/already-logged-in rejection
	 *         fires NO delegate for this call).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool LoginWithExternalAuth(EEOSExternalCredentialType CredentialType, const FString& Token, const FString& ExternalAccountId = TEXT(""));

	/**
	 * Login using the default login type from Developer Settings.
	 * @return See Login() — same start/rejection semantics.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool LoginWithDefaults();

	/**
	 * Logout the current user.
	 * @return true = logout started; OnLogoutComplete/OnLoginStatusChanged will fire.
	 *         false = rejected/failed to start. If a logout is already in flight, NO
	 *         delegate fires for THIS call (the pending logout's OnLogoutComplete is the
	 *         single completion for both); other pre-flight failures still broadcast
	 *         OnLogoutComplete.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool Logout();

	/**
	 * Report the current auth token via OnAuthTokenRefreshed.
	 *
	 * The EOS SDK refreshes the Epic auth token internally for the lifetime of the session and
	 * offers no in-process refresh primitive (EOS_LCT_RefreshToken exists only for handing a token
	 * to ANOTHER process, e.g. custom launchers, and the engine's OSS does not accept it). This
	 * therefore broadcasts OnAuthTokenRefreshed(true, <current token>) when logged in with a valid
	 * token, and OnAuthTokenRefreshed(false, "") otherwise. Login state is never modified.
	 *
	 * @return true if a valid token was reported. This method is synchronous and ALWAYS
	 *         broadcasts OnAuthTokenRefreshed (it has no in-flight state to poison).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool RefreshAuthToken();

	/**
	 * Delete persistent auth credentials (clear 'remember me').
	 *
	 * - Logged in: routes through Logout — the engine's logout path calls
	 *   EOS_Auth_DeletePersistentAuth internally, so login state updates and
	 *   OnLogoutComplete/OnLoginStatusChanged fire exactly like Logout(); the deletion
	 *   result then arrives via OnPersistentAuthDeleted after the logout completes.
	 * - NOT logged in (the primary use case — clearing 'remember me' from a login
	 *   screen): calls EOS_Auth_DeletePersistentAuth directly on the platform handle,
	 *   no session required. Only OnPersistentAuthDeleted fires.
	 *
	 * @return true = deletion started; OnPersistentAuthDeleted will fire with the
	 *         result. false = rejected/failed to start. If rejected because a logout is
	 *         already in flight, NO delegate fires for THIS call; other pre-flight
	 *         failures broadcast OnPersistentAuthDeleted(false).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	bool DeletePersistentAuth();

	// ── EOS Connect Actions (Game Services) ──────────────────────────────────

	/**
	 * Login to EOS Connect (Game Services) using a platform token.
	 * This gives you a ProductUserId needed for all EOS Game Services.
	 *
	 * @param LoginType The platform to authenticate with (Steam, PSN, Xbox, etc.)
	 * @param Token The platform authentication token (hex-encoded for Steam)
	 * @param DisplayName Optional display name (required for DeviceId, Apple, Google, Nintendo)
	 * @return true = Connect login started; OnConnectLoginComplete will fire.
	 *         false = rejected/failed to start. If rejected because a Connect login is
	 *         already in flight, NO delegate fires for THIS call; other pre-flight
	 *         failures broadcast OnConnectLoginComplete(false, ...).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool ConnectLogin(EEOSConnectLoginType LoginType, const FString& Token = TEXT(""), const FString& DisplayName = TEXT(""));

	/**
	 * Login using anonymous Device ID — no platform credentials needed.
	 * Great for development, testing, or mobile games that don't require a real account.
	 * The device identity persists across app restarts on the same device.
	 *
	 * @return true = device-id create→login chain started; OnConnectLoginComplete will
	 *         fire. false = rejected/failed to start (same rules as ConnectLogin — an
	 *         in-flight rejection fires NO delegate for this call).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool ConnectLoginWithDeviceId(const FString& DisplayName = TEXT("Player"));

	/**
	 * Login to EOS Connect using the default settings from Project Settings.
	 * Uses DefaultConnectLoginType from EOS Settings.
	 * @return See ConnectLogin() — same start/rejection semantics.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool ConnectLoginWithDefaults();

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the current login status */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	EEOSLoginStatus GetLoginStatus() const;

	/** Get the logged-in user's Epic Account ID as string */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	FString GetLoggedInUserId() const;

	/** Get the logged-in user's display name */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	FString GetDisplayName() const;

	/** Check if the user is currently logged in (Epic Auth) */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	bool IsLoggedIn() const;

	/** Get the current auth token (if available) */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	FString GetAuthToken() const;

	/** Get the login type that was used to authenticate */
	UFUNCTION(BlueprintPure, Category = "EOS|Auth")
	EEOSLoginType GetCurrentLoginType() const;

	/** Get the Product User ID from EOS Connect login (needed for all Game Services) */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	FString GetProductUserId() const;

	/** Check if EOS Connect (Game Services) login is active */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	bool IsConnectedToGameServices() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Fired when an Epic Auth login attempt completes */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
	FOnEOSLoginComplete OnLoginComplete;

	/** Fired when logout completes */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
	FOnEOSLogoutComplete OnLogoutComplete;

	/** Fired when the login status changes */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
	FOnEOSLoginStatusChanged OnLoginStatusChanged;

	/** Fired when auth token is refreshed */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
	FOnEOSAuthTokenRefreshed OnAuthTokenRefreshed;

	/**
	 * Fired when DeletePersistentAuth completes — true when the stored 'remember me'
	 * credentials were deleted (via the direct sessionless SDK call, or after the
	 * logout-based path finishes when a user was logged in).
	 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Auth")
	FOnEOSPersistentAuthDeleted OnPersistentAuthDeleted;

	/** Fired when EOS Connect (Game Services) login completes */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSAuthConnectLoginComplete OnConnectLoginComplete;

	/** Internal: called by static EOS callbacks to update Connect login state. Not intended for Blueprint use. */
	void SetConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& Error);

private:

	EEOSLoginStatus CurrentLoginStatus = EEOSLoginStatus::NotLoggedIn;
	EEOSLoginType UsedLoginType = EEOSLoginType::AccountPortal;

	FDelegateHandle LoginDelegateHandle;
	FDelegateHandle LogoutDelegateHandle;

	/** Cached Product User ID from EOS Connect login */
	FString CachedProductUserId;
	bool bConnectedToGameServices = false;

	/** In-flight guard for the raw EOS_Connect_Login path (including the device-id
	 *  create→login chain). Set when the SDK call is issued, cleared in
	 *  SetConnectLoginResult before the completion broadcast. */
	bool bConnectLoginInFlight = false;

	/** True when the current logout was initiated by DeletePersistentAuth — makes
	 *  HandleLogoutComplete follow up with OnPersistentAuthDeleted. */
	bool bPendingPersistentAuthDeleteViaLogout = false;

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);

	/** Auto-chain Connect login after successful Auth login */
	void AutoConnectLoginAfterAuth();

	/**
	 * Shared Auth login entry: in-progress/logged-in guards, state transition, delegate
	 * registration. Returns true when the engine login was issued; false on rejection
	 * (in-flight/already-logged-in — no broadcast) or pre-flight failure (broadcasts).
	 */
	bool PerformAuthLogin(const FOnlineAccountCredentials& Credentials, EEOSLoginType LoginType);

	/**
	 * Broadcast a pre-flight login failure only when no login is in flight — a rejection
	 * echo on the shared OnLoginComplete would be indistinguishable from the in-flight
	 * login's completion and would poison its waiters.
	 */
	void BroadcastLoginPreflightFailure(const FString& Error);

	/** Kick off the configured auto-login. Deferred one tick from Initialize so Blueprints can bind first. */
	void KickOffAutoLogin();

	/**
	 * Map an external credential type to the engine's "externalauth:<TokenType>" suffix.
	 * Valid strings are the EOSShared LexFromString(EOS_EExternalCredentialType&) names
	 * (e.g. "SteamSessionTicket", "PSNIdToken"). Returns empty for None/unknown.
	 */
	static FString ExternalCredentialTypeToTokenTypeString(EEOSExternalCredentialType CredentialType);

#if WITH_EOS_SDK
	/**
	 * Internal: perform the actual EOS_Connect_Login SDK call. Returns true when the SDK
	 * call was issued; false on in-flight rejection (no broadcast) or pre-flight failure
	 * (broadcasts OnConnectLoginComplete(false, ...)).
	 */
	bool PerformConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName);
#endif
};

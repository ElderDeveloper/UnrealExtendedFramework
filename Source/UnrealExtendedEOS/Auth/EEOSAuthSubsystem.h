// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAuthSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLoginComplete, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSLogoutComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSLoginStatusChanged, EEOSLoginStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAuthTokenRefreshed, bool, bSuccess, const FString&, NewToken);

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
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAuthSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Epic Auth Actions ─────────────────────────────────────────────────────

	/** Login using the specified credential type (Epic Account Services) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	void Login(EEOSLoginType LoginType, const FString& Id = TEXT(""), const FString& Token = TEXT(""));

	/** Login using the default login type from Developer Settings */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	void LoginWithDefaults();

	/** Logout the current user */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	void Logout();

	/** Attempt to refresh the auth token (re-authenticate silently) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	void RefreshAuthToken();

	/** Delete persistent auth credentials (clear 'remember me') */
	UFUNCTION(BlueprintCallable, Category = "EOS|Auth")
	void DeletePersistentAuth();

	// ── EOS Connect Actions (Game Services) ──────────────────────────────────

	/**
	 * Login to EOS Connect (Game Services) using a platform token.
	 * This gives you a ProductUserId needed for all EOS Game Services.
	 * 
	 * @param LoginType The platform to authenticate with (Steam, PSN, Xbox, etc.)
	 * @param Token The platform authentication token (hex-encoded for Steam)
	 * @param DisplayName Optional display name (required for DeviceId, Apple, Google, Nintendo)
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void ConnectLogin(EEOSConnectLoginType LoginType, const FString& Token = TEXT(""), const FString& DisplayName = TEXT(""));

	/**
	 * Login using anonymous Device ID — no platform credentials needed.
	 * Great for development, testing, or mobile games that don't require a real account.
	 * The device identity persists across app restarts on the same device.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void ConnectLoginWithDeviceId(const FString& DisplayName = TEXT("Player"));

	/**
	 * Login to EOS Connect using the default settings from Project Settings.
	 * Uses DefaultConnectLoginType from EOS Settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void ConnectLoginWithDefaults();

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

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);

	/** Auto-chain Connect login after successful Auth login */
	void AutoConnectLoginAfterAuth();

#if WITH_EOS_SDK
	/** Internal: perform the actual EOS_Connect_Login SDK call */
	void PerformConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName);
#endif
};

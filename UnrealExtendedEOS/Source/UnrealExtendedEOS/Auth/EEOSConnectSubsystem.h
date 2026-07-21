// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "eos_connect_types.h"
#include "EEOSConnectSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectLoginComplete, bool, bSuccess, const FString&, ProductUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSDeviceIdCreated, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSDeviceIdDeleted, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSAccountLinked, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSAccountUnlinked, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSDeviceIdAccountTransferred, bool, bSuccess, const FString&, PreservedProductUserId);

/**
 * Handles the EOS Connect interface for cross-platform identity.
 * The Connect interface manages Product User IDs used by game services (stats, lobbies, etc.)
 *
 * Return-value convention (all action methods):
 * - true  = the operation was started (or completed synchronously); its completion
 *           delegate WILL fire with the result.
 * - false = rejected / failed to start. When the rejection is because an operation of
 *           the SAME kind is already in flight (CreateDeviceId/LoginWithDeviceId share
 *           one identity-login slot), NO delegate fires for THIS call — rejections are
 *           never echoed on the shared completion delegates. Other pre-flight failures
 *           still broadcast a failure.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSConnectSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Login to the Connect interface (requires prior Auth login).
	 * Synchronous: reads the existing Connect session off the identity interface.
	 * @return true if an active Connect session (Product User ID) was found; false
	 *         otherwise. OnConnectLoginComplete ALWAYS fires (this method has no
	 *         in-flight state to poison).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool ConnectLogin();

	/**
	 * Create a Device ID for anonymous authentication.
	 * @return true = started; OnDeviceIdCreated will fire. false = rejected/failed to
	 *         start. If rejected because a device-id login/creation is already in
	 *         flight, NO delegate fires for THIS call; other pre-flight failures
	 *         broadcast OnDeviceIdCreated(false).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool CreateDeviceId();

	/**
	 * Delete the local device ID.
	 * @return true = started; OnDeviceIdDeleted will fire. false = failed to start
	 *         (OnDeviceIdDeleted(false) is broadcast).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool DeleteDeviceId();

	/**
	 * Login with Device ID (creates if needed, then connects).
	 * @return true = started; OnConnectLoginComplete will fire. false = rejected/failed
	 *         to start. If rejected because a device-id login/creation is already in
	 *         flight, NO delegate fires for THIS call; other pre-flight failures
	 *         broadcast OnConnectLoginComplete(false, "").
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool LoginWithDeviceId(const FString& DisplayName = TEXT("Player"));

	/**
	 * Link an external account to the current Product User ID.
	 *
	 * The stored ContinuanceToken is single-use: it is consumed (and cleared from the
	 * cache) the moment the SDK call is issued, so a second LinkAccount while one is in
	 * flight fails the token check — obtain a fresh token by attempting the Connect
	 * login again.
	 *
	 * @return true = started; OnAccountLinked will fire. false = failed to start
	 *         (OnAccountLinked(false) is broadcast).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool LinkAccount(EEOSExternalCredentialType CredentialType, const FString& Token);

	/**
	 * Unlink an external account from the current Product User ID.
	 * @return true = started; OnAccountUnlinked will fire. false = failed to start
	 *         (OnAccountUnlinked(false) is broadcast).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool UnlinkAccount(EEOSExternalCredentialType CredentialType);

	/**
	 * Transfer the anonymous Device ID login into the account-linking keychain of a real
	 * external-account user (EOS_Connect_TransferDeviceIdAccount).
	 *
	 * Both product users must be currently logged in (eos_connect_types.h contract):
	 * - DeviceIdProductUserId  → Options.LocalDeviceUserId   (user created via Device ID login)
	 * - ExternalProductUserId  → Options.PrimaryLocalUserId  (user backed by a real external account)
	 *
	 * @param DeviceIdProductUserId  PUID originally created with the anonymous Device ID login
	 * @param ExternalProductUserId  PUID already associated with a real external account (Epic, PSN, Xbox, ...)
	 * @param bKeepExternalAccountProgression  Which game progression survives (Options.ProductUserIdToPreserve):
	 *        true → the external-account user's PUID is preserved; false → the Device ID user's PUID is
	 *        preserved. The OTHER product user is discarded forever.
	 *
	 * Completion is reported via OnDeviceIdAccountTransferred on every path.
	 *
	 * Edge: if the transfer succeeds but the preserved PUID cannot be stringified,
	 * success is still broadcast with an EMPTY PreservedProductUserId; the cached
	 * Product User ID is cleared (the old value may describe the discarded user) and
	 * IsConnected() reports true. A warning is logged.
	 *
	 * @return true = started; OnDeviceIdAccountTransferred will fire. false = failed to
	 *         start (OnDeviceIdAccountTransferred(false, "") is broadcast).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	bool TransferDeviceIdAccount(const FString& DeviceIdProductUserId, const FString& ExternalProductUserId, bool bKeepExternalAccountProgression = true);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the current Product User ID as string */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	FString GetProductUserId() const;

	/** Check if a Connect login is active */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	bool IsConnected() const;

	/** Get the display name used with Device ID */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	FString GetDeviceIdDisplayName() const;

	/** Check if a ContinuanceToken is available for LinkAccount */
	UFUNCTION(BlueprintPure, Category = "EOS|Connect")
	bool HasContinuanceToken() const;

	/**
	 * Store a ContinuanceToken received from a login that returned EOS_InvalidUser.
	 * This token is required by LinkAccount(). Broadcasts OnInvalidUserDetected when non-null.
	 *
	 * Lifetime: a ContinuanceToken is single-use and short-lived. The SDK only guarantees it
	 * for continuing the CURRENT login flow (eos_connect.h documents it as "a continuance token
	 * from a previous call to Login — always try login first"); it expires shortly after the
	 * failed login and cannot be persisted across sessions. Consume it promptly via
	 * LinkAccount()/CreateUser — a stale token makes those calls fail and a new one must be
	 * obtained by attempting the Connect login again.
	 */
	void StoreContinuanceToken(EOS_ContinuanceToken Token);

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSConnectLoginComplete OnConnectLoginComplete;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSDeviceIdCreated OnDeviceIdCreated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSDeviceIdDeleted OnDeviceIdDeleted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSAccountLinked OnAccountLinked;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSAccountUnlinked OnAccountUnlinked;

	/** Fired when TransferDeviceIdAccount completes (on success, PreservedProductUserId is the surviving PUID) */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSDeviceIdAccountTransferred OnDeviceIdAccountTransferred;

	/** Fired when a Connect login returns EOS_InvalidUser — a ContinuanceToken is now available for LinkAccount */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Connect")
	FOnEOSAccountLinked OnInvalidUserDetected;

private:

	FString CachedProductUserId;
	FString CachedDeviceDisplayName;
	bool bIsConnected = false;

	/** Delegate handle for login complete — prevents accumulation on repeated calls */
	FDelegateHandle LoginCompleteDelegateHandle;

	/** ContinuanceToken from a prior EOS_InvalidUser login, needed by LinkAccount */
	EOS_ContinuanceToken CachedContinuanceToken = nullptr;
};

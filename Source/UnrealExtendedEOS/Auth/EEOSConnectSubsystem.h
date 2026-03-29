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

/**
 * Handles the EOS Connect interface for cross-platform identity.
 * The Connect interface manages Product User IDs used by game services (stats, lobbies, etc.)
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSConnectSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Login to the Connect interface (requires prior Auth login) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void ConnectLogin();

	/** Create a Device ID for anonymous authentication */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void CreateDeviceId();

	/** Delete the local device ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void DeleteDeviceId();

	/** Login with Device ID (creates if needed, then connects) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void LoginWithDeviceId(const FString& DisplayName = TEXT("Player"));

	/** Link an external account to the current Product User ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void LinkAccount(EEOSExternalCredentialType CredentialType, const FString& Token);

	/** Unlink an external account from the current Product User ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void UnlinkAccount(EEOSExternalCredentialType CredentialType);

	/** Transfer a Device ID account's data to a real account */
	UFUNCTION(BlueprintCallable, Category = "EOS|Connect")
	void TransferDeviceIdAccount(const FString& RealProductUserId);

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
	 * This token is required by LinkAccount().
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

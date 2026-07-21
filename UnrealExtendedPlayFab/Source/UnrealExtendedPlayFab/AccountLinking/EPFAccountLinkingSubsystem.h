// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAccountLinkingSubsystem.generated.h"


UENUM(BlueprintType)
enum class EEPFLinkAction : uint8
{
	Linked,
	Unlinked,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFAccountLinked, const FEPFResult&, Result, const FString&, Platform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFAccountUnlinked, const FEPFResult&, Result, const FString&, Platform);

/**
 * Account Linking — link/unlink external platform accounts to the PlayFab account.
 * Supports Steam, Custom ID, and Device ID.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAccountLinkingSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Link Actions ─────────────────────────────────────────────────────────

	/** Link a Steam account to the current PlayFab account */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void LinkSteamAccount(const FString& SteamTicket, bool bForceLink = false);

	/** Link a custom ID to the current PlayFab account */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void LinkCustomId(const FString& CustomId, bool bForceLink = false);

	/** Link the device ID to the current PlayFab account */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void LinkDeviceId(bool bForceLink = false);

	// ── Unlink Actions ───────────────────────────────────────────────────────

	/** Unlink Steam account */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void UnlinkSteamAccount();

	/** Unlink a custom ID */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void UnlinkCustomId(const FString& CustomId);

	/** Unlink the device ID */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|AccountLinking")
	void UnlinkDeviceId();

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|AccountLinking")
	FOnEPFAccountLinked OnAccountLinked;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|AccountLinking")
	FOnEPFAccountUnlinked OnAccountUnlinked;
};

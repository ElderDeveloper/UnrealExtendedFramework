// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFPlayerDataSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPlayerDataReceived, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFPlayerDataUpdated, const FEPFResult&, Result);

/**
 * Manages PlayFab player data — read/write key-value pairs (XP, rewards, inventory).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFPlayerDataSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Get specific player data keys from the server */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|PlayerData")
	void GetPlayerData(const TArray<FString>& Keys);

	/** Get all player data from the server */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|PlayerData")
	void GetAllPlayerData();

	/**
	 * Set player data (write one or more key-value pairs).
	 * @param Data     Map of keys to string values to write.
	 * @param bPublic  If true, sets permission to "Public" so other players can
	 *                 read this data (e.g., via GetFriendLeaderboard enrichment).
	 *                 Defaults to false (Private — owning player only).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|PlayerData")
	void SetPlayerData(const TMap<FString, FString>& Data, bool bPublic = false);

	/** Delete a player data key */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|PlayerData")
	void DeletePlayerData(const FString& Key);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get a cached value by key (returns empty string if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|PlayerData")
	FString GetCachedValue(const FString& Key) const;

	/** Check if a key exists in the local cache */
	UFUNCTION(BlueprintPure, Category = "PlayFab|PlayerData")
	bool HasKey(const FString& Key) const;

	/** Get all cached data */
	UFUNCTION(BlueprintPure, Category = "PlayFab|PlayerData")
	TMap<FString, FString> GetAllCachedData() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|PlayerData")
	FOnEPFPlayerDataReceived OnPlayerDataReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|PlayerData")
	FOnEPFPlayerDataUpdated OnPlayerDataUpdated;

private:

	TMap<FString, FString> CachedData;
};

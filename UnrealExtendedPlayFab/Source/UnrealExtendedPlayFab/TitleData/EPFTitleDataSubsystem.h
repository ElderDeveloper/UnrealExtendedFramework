// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFTitleDataSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFTitleDataReceived, const FEPFResult&, Result);

/**
 * Reads server-side Title Data — remote config, feature flags, balance values, event schedules.
 * Managed entirely from the PlayFab dashboard. Change game behavior without deploying an update.
 *
 * Example keys: "XPPerKill", "EventActive", "LootTable_v2", "FeatureFlags"
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFTitleDataSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch specific title data keys from the server */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|TitleData")
	void GetTitleData(const TArray<FString>& Keys);

	/** Fetch all title data from the server */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|TitleData")
	void GetAllTitleData();

	/** Fetch internal title data (not visible to clients — admin use only) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|TitleData")
	void GetTitleInternalData(const TArray<FString>& Keys);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get a cached value by key (returns empty string if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	FString GetCachedValue(const FString& Key) const;

	/** Get a cached value as int (returns DefaultValue if not found or not numeric) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	int32 GetCachedInt(const FString& Key, int32 DefaultValue = 0) const;

	/** Get a cached value as float */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	float GetCachedFloat(const FString& Key, float DefaultValue = 0.0f) const;

	/** Get a cached value as bool ("true"/"1" → true) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	bool GetCachedBool(const FString& Key, bool DefaultValue = false) const;

	/** Check if a key exists in the local cache */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	bool HasKey(const FString& Key) const;

	/** Get all cached title data */
	UFUNCTION(BlueprintPure, Category = "PlayFab|TitleData")
	TMap<FString, FString> GetAllCachedData() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|TitleData")
	FOnEPFTitleDataReceived OnTitleDataReceived;

private:

	TMap<FString, FString> CachedData;
};

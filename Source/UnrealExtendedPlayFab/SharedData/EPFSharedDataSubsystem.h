// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFSharedDataSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFSharedGroupCreated, const FEPFResult&, Result, const FString&, SharedGroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFSharedDataReceived, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFSharedDataUpdated, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFSharedMembersChanged, const FEPFResult&, Result);

/**
 * Shared Group Data — mutable data shared between players.
 * Perfect for party state, co-op session data, guild shared storage.
 * Any member of the group can read/write the data.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFSharedDataSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Create a new shared group (returns group ID) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void CreateSharedGroup(const FString& SharedGroupId);

	/** Get all data from a shared group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void GetSharedGroupData(const FString& SharedGroupId, const TArray<FString>& Keys);

	/** Update data in a shared group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void UpdateSharedGroupData(const FString& SharedGroupId, const TMap<FString, FString>& Data);

	/** Remove keys from a shared group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void RemoveSharedGroupData(const FString& SharedGroupId, const TArray<FString>& KeysToRemove);

	/** Add members to a shared group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void AddSharedGroupMembers(const FString& SharedGroupId, const TArray<FString>& PlayFabIds);

	/** Remove members from a shared group */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|SharedData")
	void RemoveSharedGroupMembers(const FString& SharedGroupId, const TArray<FString>& PlayFabIds);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached data for a group */
	UFUNCTION(BlueprintPure, Category = "PlayFab|SharedData")
	TMap<FString, FString> GetCachedData(const FString& SharedGroupId) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|SharedData")
	FOnEPFSharedGroupCreated OnGroupCreated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|SharedData")
	FOnEPFSharedDataReceived OnDataReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|SharedData")
	FOnEPFSharedDataUpdated OnDataUpdated;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|SharedData")
	FOnEPFSharedMembersChanged OnMembersChanged;

private:

	TMap<FString, TMap<FString, FString>> CachedGroupData;
};

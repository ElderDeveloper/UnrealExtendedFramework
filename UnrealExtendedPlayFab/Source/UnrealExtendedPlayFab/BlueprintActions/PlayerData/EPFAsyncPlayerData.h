// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncPlayerData.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncPlayerDataUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncPlayerDataDeleted);

// ── Get ──────────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Player Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetPlayerData : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Player Data"), Category = "PlayFab|Async|PlayerData")
	static UEPFAsyncGetPlayerData* GetPlayerData(UObject* WorldContext, const TArray<FString>& Keys);

	/** Fires on success — call GetAllCachedData() on the PlayerData subsystem to access the map */
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncPlayerDataUpdated OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TArray<FString> RequestedKeys;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Set ──────────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Set Player Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSetPlayerData : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Set Player Data"), Category = "PlayFab|Async|PlayerData")
	static UEPFAsyncSetPlayerData* SetPlayerData(UObject* WorldContext, const TMap<FString, FString>& Data);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncPlayerDataUpdated OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TMap<FString, FString> UpdatedData;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Delete ───────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Delete Player Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncDeletePlayerData : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Delete Player Data"), Category = "PlayFab|Async|PlayerData")
	static UEPFAsyncDeletePlayerData* DeletePlayerData(UObject* WorldContext, const FString& Key);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncPlayerDataDeleted OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString DataKey;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Shared/EPFTypes.h"
#include "EPFAsyncStats.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncStatsReceived, const TArray<FEPFStatistic>&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncStatsUpdated);

// ── Get ──────────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Stats"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetStats : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Stats"), Category = "PlayFab|Async|Stats")
	static UEPFAsyncGetStats* GetStats(UObject* WorldContext, const TArray<FString>& StatNames);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncStatsReceived OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	TArray<FString> StatNames;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFStatistic>& Stats);
};

// ── Update ───────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Update Stats"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncUpdateStats : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Update Stats"), Category = "PlayFab|Async|Stats")
	static UEPFAsyncUpdateStats* UpdateStats(UObject* WorldContext, const TMap<FString, int32>& Stats);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncStatsUpdated OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	TMap<FString, int32> Stats;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Currency/EPFCurrencySubsystem.h"
#include "EPFAsyncCurrency.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncBalancesReceived, const TArray<FEPFCurrencyBalance>&, Balances);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCurrencyModified, const FString&, CurrencyCode);

// ── Get Balances ─────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Currency Balances"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetBalances : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Currency Balances"), Category = "PlayFab|Async|Currency")
	static UEPFAsyncGetBalances* GetBalances(UObject* WorldContext);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncBalancesReceived OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFCurrencyBalance>& Balances);
};

// ── Add Currency ─────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Add Currency"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAddCurrency : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Add Currency"), Category = "PlayFab|Async|Currency")
	static UEPFAsyncAddCurrency* AddCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCurrencyModified OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString CurrencyCode;
	int32 Amount;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Code);
};

// ── Subtract Currency ────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Subtract Currency"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSubtractCurrency : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Subtract Currency"), Category = "PlayFab|Async|Currency")
	static UEPFAsyncSubtractCurrency* SubtractCurrency(UObject* WorldContext, const FString& CurrencyCode, int32 Amount);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCurrencyModified OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString CurrencyCode;
	int32 Amount;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Code);
};

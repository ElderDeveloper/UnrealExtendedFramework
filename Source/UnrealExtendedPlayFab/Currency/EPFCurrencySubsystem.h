// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFCurrencySubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFCurrencyBalance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Currency")
	FString CurrencyCode;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Currency")
	int32 Amount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCurrencyBalanceReceived, const FEPFResult&, Result, const TArray<FEPFCurrencyBalance>&, Balances);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCurrencyModified, const FEPFResult&, Result, const FString&, CurrencyCode);

/**
 * Manages PlayFab Economy v2 currency balances stored as inventory items.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFCurrencySubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Refresh all currency balances from the server */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Currency")
	void GetBalances();

	/** Add currency to the player's balance */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Currency")
	void AddCurrency(const FString& CurrencyCode, int32 Amount, const FString& IdempotencyId = TEXT(""));

	/** Subtract currency from the player's balance */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Currency")
	void SubtractCurrency(const FString& CurrencyCode, int32 Amount, const FString& IdempotencyId = TEXT(""));

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached balance for a specific currency (-1 if not fetched yet) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Currency")
	int32 GetCachedBalance(const FString& CurrencyCode) const;

	/** Get all cached balances */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Currency")
	TArray<FEPFCurrencyBalance> GetAllCachedBalances() const;

	/** Check if the player can afford a specific amount */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Currency")
	bool CanAfford(const FString& CurrencyCode, int32 Amount) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Currency")
	FOnEPFCurrencyBalanceReceived OnBalancesReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Currency")
	FOnEPFCurrencyModified OnCurrencyModified;

private:

	TMap<FString, int32> CachedBalances;

	/** Parse currency inventory items from Economy v2 inventory responses */
	void ParseCurrencyResponse(const FEPFResult& Result, TSharedPtr<FJsonObject> Response);
};

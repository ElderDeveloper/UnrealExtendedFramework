// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Inventory/EPFInventorySubsystem.h"
#include "EPFAsyncInventory.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncInventoryReceived, const TArray<FEPFInventoryItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncCatalogReceived, const TArray<FEPFCatalogItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncItemPurchased, const FString&, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncItemConsumed, int32, RemainingUses);

// ── Get Inventory ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Inventory"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetInventory : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Inventory"), Category = "PlayFab|Async|Inventory")
	static UEPFAsyncGetInventory* GetInventory(UObject* WorldContext);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncInventoryReceived OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFInventoryItem>& Items);
};

// ── Get Catalog ──────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Catalog"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetCatalog : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Catalog"), Category = "PlayFab|Async|Inventory")
	static UEPFAsyncGetCatalog* GetCatalog(UObject* WorldContext, const FString& CatalogVersion = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncCatalogReceived OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString CatalogVersion;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFCatalogItem>& Items);
};

// ── Purchase Item ────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Purchase Item"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncPurchaseItem : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Purchase Item"), Category = "PlayFab|Async|Inventory")
	static UEPFAsyncPurchaseItem* PurchaseItem(UObject* WorldContext, const FString& ItemId, const FString& CurrencyCode, int32 Price);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncItemPurchased OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString ItemId;
	FString CurrencyCode;
	int32 Price;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& ItemInstanceId);
};

// ── Consume Item ─────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Consume Item"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncConsumeItem : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Consume Item"), Category = "PlayFab|Async|Inventory")
	static UEPFAsyncConsumeItem* ConsumeItem(UObject* WorldContext, const FString& ItemInstanceId, int32 ConsumeCount = 1);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncItemConsumed OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	FString ItemInstanceId;
	int32 ConsumeCount;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, int32 RemainingUses);
};

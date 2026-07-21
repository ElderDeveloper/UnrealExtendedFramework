// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFInventorySubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFInventoryItem
{
	GENERATED_BODY()

	/** Inventory stack id. Preserved in ItemInstanceId for compatibility. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString StackId;

	/** Catalog item ID (matches the catalog definition) */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemId;

	/** Display name of the item */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString DisplayName;

	/** Item type (e.g., currency, catalogItem, bundle) */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemClass;

	/** Current stack amount. Mirrored into RemainingUses for compatibility. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	int32 Amount = 0;

	/** Remaining amount (-1 = unknown) */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	int32 RemainingUses = -1;

	/** Custom data attached to this item instance */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	TMap<FString, FString> CustomData;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFCatalogItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemClass;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemType;

	/** Flattened Economy price amounts keyed by price item id */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	TMap<FString, int32> VirtualCurrencyPrices;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFInventoryReceived, const FEPFResult&, Result, const TArray<FEPFInventoryItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCatalogReceived, const FEPFResult&, Result, const TArray<FEPFCatalogItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFItemPurchased, const FEPFResult&, Result, const FString&, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFItemConsumed, const FEPFResult&, Result, int32, RemainingUses);

/**
 * Manages PlayFab Economy v2 inventory and catalog reads plus purchase/subtract operations.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFInventorySubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch the authenticated entity's current inventory items */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void GetInventory();

	/** Fetch the public catalog or an optional store-specific catalog view, including item and currency definitions */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void GetCatalog(const FString& StoreId = TEXT(""));

	/** Purchase an item from the catalog using Economy price item ids */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void PurchaseItem(const FString& ItemId, const FString& CurrencyCode, int32 Price, const FString& StoreId = TEXT(""));

	/** Subtract amount from an inventory stack */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void ConsumeItem(const FString& ItemInstanceId, int32 ConsumeCount = 1);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached inventory items */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Inventory")
	TArray<FEPFInventoryItem> GetCachedInventory() const;

	/** Get cached catalog items */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Inventory")
	TArray<FEPFCatalogItem> GetCachedCatalog() const;

	/** Check if the player owns an item by ItemId */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Inventory")
	bool OwnsItem(const FString& ItemId) const;

	/** Get the count of a specific item the player owns */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Inventory")
	int32 GetItemCount(const FString& ItemId) const;

	/** Find an inventory item by ItemId (returns null if not found) */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Inventory")
	FEPFInventoryItem FindItem(const FString& ItemId, bool& bFound) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Inventory")
	FOnEPFInventoryReceived OnInventoryReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Inventory")
	FOnEPFCatalogReceived OnCatalogReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Inventory")
	FOnEPFItemPurchased OnItemPurchased;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Inventory")
	FOnEPFItemConsumed OnItemConsumed;

private:

	TArray<FEPFInventoryItem> CachedInventory;
	TArray<FEPFCatalogItem> CachedCatalog;
};

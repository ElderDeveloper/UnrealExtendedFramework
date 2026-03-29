// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFInventorySubsystem.generated.h"


USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFInventoryItem
{
	GENERATED_BODY()

	/** Unique instance ID of this item in the player's inventory */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemInstanceId;

	/** Catalog item ID (matches the catalog definition) */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemId;

	/** Display name of the item */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString DisplayName;

	/** Item class (e.g., "weapon", "consumable", "armor") */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	FString ItemClass;

	/** Remaining uses (-1 = unlimited) */
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

	/** Virtual currency prices (key = currency code, value = price) */
	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Inventory")
	TMap<FString, int32> VirtualCurrencyPrices;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFInventoryReceived, const FEPFResult&, Result, const TArray<FEPFInventoryItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFCatalogReceived, const FEPFResult&, Result, const TArray<FEPFCatalogItem>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFItemPurchased, const FEPFResult&, Result, const FString&, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFItemConsumed, const FEPFResult&, Result, int32, RemainingUses);

/**
 * Manages PlayFab Player Inventory and Catalog — browse items, purchase, consume.
 * Items and catalog are defined in the PlayFab dashboard.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFInventorySubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Fetch the player's current inventory (items owned) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void GetInventory();

	/** Fetch the store catalog (items available for purchase) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void GetCatalog(const FString& CatalogVersion = TEXT(""));

	/** Purchase an item from the catalog using virtual currency */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Inventory")
	void PurchaseItem(const FString& ItemId, const FString& CurrencyCode, int32 Price, const FString& CatalogVersion = TEXT(""));

	/** Consume a use of a consumable item */
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

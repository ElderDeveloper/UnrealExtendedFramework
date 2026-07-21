// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebInventoryServiceSubsystem.generated.h"

/**
 * IInventoryService — Steam Inventory Service item grants, consumption and exchange.
 * Every endpoint here is a publisher-key call on the partner host: call these from a trusted
 * server/tooling context only, never from a shipped client.
 *
 * Note: IInventoryService/AddPromoItems (the multi-item promo variant) is deliberately not wrapped —
 * AddPromoItem covers the promo-grant use case; call AddItem for multi-item grants instead.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebInventoryServiceSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IInventoryService/AddItem/v1 (POST) — grants items to a player (partner host, publisher key).
	 * ItemDefIds are sent as the service-interface indexed array form itemdefid[0]..itemdefid[N].
	 * ItemPropsJson (optional JSON per-item property block) and bNotify (show a Steam notification)
	 * are omitted when empty/false.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void AddItem(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, FString ItemPropsJson, bool bNotify, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/AddPromoItem/v1 (POST) — grants a single promo item definition to a player
	 * (partner host, publisher key). ItemPropsJson is omitted when empty.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void AddPromoItem(int32 AppId, FString SteamId, int32 ItemDefId, FString ItemPropsJson, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/ConsumeItem/v1 (POST) — consumes units of a player's item (partner host, publisher key).
	 * ItemId is the item instance id; Quantity the number of units; RequestId (optional) makes the call
	 * idempotent — resending the same RequestId will not double-consume — and is omitted when empty.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void ConsumeItem(int32 AppId, FString SteamId, FString ItemId, FString Quantity, FString RequestId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/ExchangeItem/v1 (POST) — crafts an item by consuming materials
	 * (partner host, publisher key). MaterialItemIds/MaterialQuantities are parallel arrays sent as the
	 * indexed params materialsitemid[N] / materialsquantity[N]; OutputItemDefId is the recipe result.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void ExchangeItem(int32 AppId, FString SteamId, const TArray<FString>& MaterialItemIds, const TArray<int32>& MaterialQuantities, int32 OutputItemDefId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/GetInventory/v1 — a player's full inventory (partner host, publisher key).
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetInventory(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/GetItemDefs/v1 — the app's item definitions (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetItemDefs(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/GetPriceSheet/v1 — item price sheet (partner host, publisher key).
	 * Ecurrency is the numeric ECurrencyCode; values < 0 are omitted (endpoint default).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPriceSheet(int32 Ecurrency, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/ModifyItems/v1 (POST) — modifies dynamic properties on a player's items
	 * (partner host, publisher key). UpdatesJson is the raw JSON array of update objects
	 * ([{"itemid":"...","property_name":"...","property_value_string":"..."}, ...]) and is sent as the
	 * "updates" form field — Valve's docs describe this array-of-message param loosely, so verify against
	 * your partner account before shipping.
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void ModifyItems(int32 AppId, FString SteamId, FString UpdatesJson, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/Consolidate/v1 (POST) — merges stacks of the given item definitions in a player's
	 * inventory (partner host, publisher key). ItemDefIds are sent as the indexed params itemdefid[N].
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void Consolidate(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * IInventoryService/GetQuantity/v1 — how many units of the given item definitions a player owns
	 * (partner host, publisher key). The docs take an itemdefid[] array (sent here as the indexed form
	 * itemdefid[0]..itemdefid[N] — the sketch's single ItemId is widened to match) plus an optional
	 * bForce that bypasses the cache (omitted when false). An empty SteamId falls back to the
	 * configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|InventoryService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetQuantity(int32 AppId, FString SteamId, const TArray<int32>& ItemDefIds, bool bForce, const FOnSteamWebResponse& OnResponse);
};

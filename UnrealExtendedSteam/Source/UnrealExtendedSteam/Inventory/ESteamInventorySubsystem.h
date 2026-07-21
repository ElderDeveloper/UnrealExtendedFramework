// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamInventorySubsystem.generated.h"

/**
 * A single item instance in the user's Steam inventory (mirrors SteamItemDetails_t).
 *
 * ItemId is a SteamItemInstanceID_t (uint64) passed through Blueprint as int64 — Blueprint has
 * no unsigned 64-bit type, so the bits are reinterpreted. Treat it as opaque and only feed back
 * values received from OnInventoryItemsReceived.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamInventoryItem
{
	GENERATED_BODY()

	/** Globally-unique id of this specific item instance (SteamItemInstanceID_t, see struct docs). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Inventory")
	int64 ItemId = 0;

	/** Item definition number ("what kind of item"), as configured on the Steamworks site. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Inventory")
	int32 Definition = 0;

	/** Stack size of this instance (1 for non-stackable items). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Inventory")
	int32 Quantity = 0;

	/**
	 * Raw item status/action bitmask (m_unFlags from SteamItemDetails_t). Test bits against
	 * ESteamItemFlags in isteaminventory.h: no-trade (1<<0), removed (1<<8), consumed (1<<9).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Inventory")
	int32 Flags = 0;

	/**
	 * All dynamic string properties attached to this item instance, keyed by property name.
	 * Populated from ISteamInventory::GetResultItemProperty while parsing the result set: the
	 * available property names are enumerated first (empty-name query returns a comma-separated
	 * list), then each value is read. Empty when the item carries no dynamic properties. Values
	 * are strings as stored by the Inventory Service — parse to numbers/bools yourself if needed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Inventory")
	TMap<FString, FString> Properties;
};

/**
 * Fired when an inventory result set completed (GetAllItems, GetItemsById, TriggerItemDrop,
 * ConsumeItem and completed purchases). Items holds the contents of the result set — for
 * modification calls this is the set of affected items.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamInventoryItemsReceived, bool, bSuccess, const TArray<FESteamInventoryItem>&, Items);

/** Fired when item definitions were loaded or refreshed (after LoadItemDefinitions or any server-driven update). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamItemDefinitionsUpdated);

/**
 * Fired when Steam reports the user's inventory changed out-of-band (SteamInventoryFullUpdate_t) —
 * e.g. a completed purchase, or an item granted/consumed/traded by another session. Signals
 * "the inventory changed — re-query it". Per the SDK a full ResultReady carrying the fresh set is
 * posted immediately afterwards, so consumers can either react here by re-querying (GetAllItems)
 * or simply consume that accompanying ResultReady on OnInventoryItemsReceived.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamInventoryUpdated);

/** Fired when a RequestPrices call completed. Currency is a 3-letter ISO 4217 code, e.g. "USD". */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamInventoryPricesReceived, bool, bSuccess, FString, Currency);

/**
 * Fired when a StartPurchase call was acknowledged by Steam (the transaction was initialized,
 * NOT completed — completion of an authorized purchase arrives on OnInventoryItemsReceived).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamPurchaseStarted, bool, bSuccess, int64, OrderId, int64, TransactionId);

/**
 * Fired when a RequestEligiblePromoItemDefinitionsIDs call completed. DefIds holds the item
 * definition numbers the queried user can be manually granted via AddPromoItem/AddPromoItems
 * (empty on failure, or when the user is eligible for no manual promotions).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamEligiblePromoItemDefs, bool, bSuccess, const TArray<int32>&, DefIds);

/**
 * Wraps ISteamInventory: item queries, playtime drops, consumption, item definitions,
 * prices and purchases.
 *
 * All query/modification calls are asynchronous: they return true when the request was
 * issued and the parsed result arrives later on OnInventoryItemsReceived. Result handles
 * are tracked internally and destroyed after parsing.
 *
 * Concurrency: the item queries above (GetAllItems/GetItemsById/TriggerItemDrop/ConsumeItem)
 * and the item-mutation calls (ExchangeItems/TransferItemQuantity/GenerateItems/GrantPromoItems/
 * AddPromoItem(s)/SubmitUpdateProperties) all run fully concurrently via internally tracked
 * result handles and complete on OnInventoryItemsReceived. RequestPrices, StartPurchase and
 * RequestEligiblePromoItemDefinitionsIDs each use a single Steam call-result, so same-type
 * requests are serialized via an internal FIFO queue; they complete in order, none are dropped.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamInventorySubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Item queries ----

	/**
	 * Requests the full inventory of the current user. Result arrives on
	 * OnInventoryItemsReceived. Rate-limited by Steam; frequent calls may return cached data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetAllItems();

	/**
	 * Requests a subset of the current user's inventory by item instance ids
	 * (e.g. ids previously received from OnInventoryItemsReceived).
	 * Result arrives on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetItemsById(const TArray<int64>& ItemIds);

	// ---- Item modification ----

	/**
	 * Converts accumulated playtime credit into an item drop from the given drop list
	 * definition (must be marked as a playtime item generator on the Steamworks site).
	 * The granted items (or an empty set when no credit was available) arrive on
	 * OnInventoryItemsReceived. Too-frequent calls are suppressed by the Steam client.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool TriggerItemDrop(int32 ItemDefinition);

	/**
	 * Permanently removes Quantity units of an item instance from the inventory.
	 * THIS CANNOT BE UNDONE — gate it behind explicit user confirmation.
	 * The affected items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool ConsumeItem(int64 ItemId, int32 Quantity);

	/**
	 * Atomically consumes and generates items in one operation (crafting / transmutation / unpacking).
	 * The four arrays are paired index-wise: GenerateDefs[i] x GenerateQty[i] are produced and
	 * DestroyItemIds[i] x DestroyQty[i] are consumed. The exchange must match a recipe configured on
	 * the item definition or it fails atomically. The resulting items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool ExchangeItems(const TArray<int32>& GenerateDefs, const TArray<int32>& GenerateQty, const TArray<int64>& DestroyItemIds, const TArray<int32>& DestroyQty);

	/**
	 * Splits or merges a stack of stackable items. Moves Quantity units from SourceItemId onto
	 * DestItemId (which must be the same definition). Pass DestItemId = 0 to split off a new stack.
	 * The affected items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool TransferItemQuantity(int64 SourceItemId, int32 Quantity, int64 DestItemId);

	/**
	 * PROTOTYPING ONLY. Directly creates items (Defs[i] x Quantities[i]); only works for Steam
	 * accounts in your game's publisher group, otherwise it fails server-side. Ship real drops via
	 * TriggerItemDrop / purchases instead. The granted items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GenerateItems(const TArray<int32>& Defs, const TArray<int32>& Quantities);

	// ---- Promotions ----

	/**
	 * Grants every promotional item the user is currently eligible for (one time each). Being
	 * eligible for none is still a success (an empty set). The granted items arrive on
	 * OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GrantPromoItems();

	/**
	 * Restricted GrantPromoItems that only checks the single promo item definition Def. Useful for
	 * a custom "claim this reward" UI. The granted items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool AddPromoItem(int32 Def);

	/**
	 * Restricted GrantPromoItems that only checks the given promo item definitions. The granted
	 * items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool AddPromoItems(const TArray<int32>& Defs);

	/**
	 * Requests the list of "manual" promo item definitions the given user is eligible for (promos
	 * that are not granted automatically, e.g. a weekly reward). The result arrives on
	 * OnEligiblePromoItemDefsReceived; afterwards GetEligiblePromoItemDefinitionIDs returns the ids.
	 * Same-type requests are serialized via an internal FIFO queue (never dropped).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool RequestEligiblePromoItemDefinitionsIDs(FESteamId SteamId);

	/**
	 * Reads the cached eligible manual-promo item definition ids for the given user. Requires a
	 * completed RequestEligiblePromoItemDefinitionsIDs for that user. Returns false (empty output)
	 * when nothing is cached.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetEligiblePromoItemDefinitionIDs(FESteamId SteamId, TArray<int32>& OutDefIds) const;

	// ---- Dynamic item properties (write) ----

	/**
	 * Opens a property-update request and returns its handle (-1, k_SteamInventoryUpdateHandleInvalid,
	 * when Steam is unavailable). Stage changes with the SetProperty* / RemoveProperty calls against
	 * the handle, then commit them with SubmitUpdateProperties. The handle is a
	 * SteamInventoryUpdateHandle_t (uint64) passed through Blueprint as int64 — treat it as opaque.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	int64 StartUpdateProperties();

	/** Stages a boolean property on an item within an update request opened by StartUpdateProperties. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SetPropertyBool(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, bool bValue);

	/** Stages an integer property on an item within an update request opened by StartUpdateProperties. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SetPropertyInt64(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, int64 Value);

	/** Stages a float property on an item within an update request opened by StartUpdateProperties. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SetPropertyFloat(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, float Value);

	/** Stages a string property on an item within an update request opened by StartUpdateProperties. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SetPropertyString(int64 UpdateHandle, int64 ItemId, const FString& PropertyName, const FString& Value);

	/** Stages removal of a property from an item within an update request opened by StartUpdateProperties. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool RemoveProperty(int64 UpdateHandle, int64 ItemId, const FString& PropertyName);

	/**
	 * Commits a property-update request opened by StartUpdateProperties. The handle is consumed.
	 * The updated items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SubmitUpdateProperties(int64 UpdateHandle);

	// ---- Result serialization (secure item exchange) ----

	/**
	 * Copies the signed serialization of the most recent tracked result set into OutBuffer. The
	 * subsystem serializes each tracked result during parsing (before its handle is destroyed) and
	 * caches the bytes, so this returns the last GetAllItems/GetItemsById/mutation result. The blob
	 * can be transmitted to other players who verify it with DeserializeResult + CheckResultSteamID.
	 * Serialized results expire after roughly one hour. Returns false (empty output) when no result
	 * has been serialized yet. Prefer GetItemsById first to keep the serialized set minimal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool SerializeLastResult(TArray<uint8>& OutBuffer) const;

	/**
	 * Deserializes and signature-verifies a result blob produced by SerializeLastResult on another
	 * client. On success a result handle is issued and tracked like any other, so the parsed items
	 * arrive on OnInventoryItemsReceived. Use CheckResultSteamID afterwards to confirm the blob
	 * belongs to the expected user before trusting its contents.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool DeserializeResult(const TArray<uint8>& Buffer);

	/**
	 * Verifies that the most recently serialized/deserialized result belongs to ExpectedSteamId,
	 * guarding against a remote player claiming another user's inventory. Works off the cached
	 * serialization (see SerializeLastResult): the blob is deserialized into a throwaway handle,
	 * checked, and immediately destroyed. Returns false when no result is cached or the id differs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool CheckResultSteamID(FESteamId ExpectedSteamId) const;

	// ---- Item definitions ----

	/**
	 * Triggers the load/refresh of item definitions. OnItemDefinitionsUpdated fires every
	 * time new definitions become available (also on server-driven updates).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool LoadItemDefinitions();

	/**
	 * Reads a string property (e.g. "name", "description") from an item definition.
	 * Pass an empty PropertyName to get a comma-separated list of available property names.
	 * Some properties are localized to the current Steam language. Empty when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	FString GetItemDefinitionProperty(int32 Definition, const FString& PropertyName) const;

	/**
	 * Returns every defined item definition id (configured on the Steamworks site; not necessarily
	 * contiguous). Best called after LoadItemDefinitions / OnItemDefinitionsUpdated. Returns false
	 * (empty output) when Steam is unavailable or the ids cannot be read.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetItemDefinitionIDs(TArray<int32>& OutDefIds) const;

	// ---- Prices and purchases ----

	/**
	 * Requests current prices for all purchasable item definitions in the user's local
	 * currency. Result arrives on OnPricesReceived; afterwards GetItemPrice is usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool RequestPrices();

	/**
	 * Reads the cached price of an item definition in the user's local currency, in the
	 * smallest denomination (e.g. cents). Requires a completed RequestPrices. Returns false
	 * (outputs 0) when no price is stored for the definition.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetItemPrice(int32 Definition, int64& OutCurrentPrice, int64& OutBasePrice) const;

	/** Number of item definitions that have a cached price. Requires a completed RequestPrices. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	int32 GetNumItemsWithPrices() const;

	/**
	 * Reads all cached prices at once (parallel arrays: OutDefs[i] costs OutPrices[i], base
	 * OutBasePrices[i], in the user's local currency in the smallest denomination). Requires a
	 * completed RequestPrices. Returns false (empty output) when Steam is unavailable or the read
	 * failed; an empty-but-true result means no definitions are currently priced.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool GetItemsWithPrices(TArray<int32>& OutDefs, TArray<int64>& OutPrices, TArray<int64>& OutBasePrices) const;

	/**
	 * Starts the purchase process for the given item definitions (arrays must have equal
	 * length; Quantities[i] is the amount of Definitions[i] to buy). Steam acknowledges the
	 * transaction on OnPurchaseStarted; once the user authorized and completed it, the
	 * granted items arrive on OnInventoryItemsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Inventory")
	bool StartPurchase(const TArray<int32>& Definitions, const TArray<int32>& Quantities);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamInventoryItemsReceived OnInventoryItemsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamItemDefinitionsUpdated OnItemDefinitionsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamInventoryUpdated OnInventoryUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamInventoryPricesReceived OnPricesReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamPurchaseStarted OnPurchaseStarted;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Inventory")
	FOnSteamEligiblePromoItemDefs OnEligiblePromoItemDefsReceived;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamInventoryCallbacks;
	TSharedPtr<class FESteamInventoryCallbacks> Callbacks;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Inventory/ESteamInventorySubsystem.h"
#include "ESteamInventoryAsyncActions.generated.h"

/** Completion pin for the inventory items node (Items is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncInventoryItemsPin, const TArray<FESteamInventoryItem>&, Items);

/**
 * Requests the full Steam inventory of the current user and completes when the parsed
 * result arrives from UESteamInventorySubsystem.
 *
 * Note: OnInventoryItemsReceived is shared by all inventory operations; a result from a
 * concurrent GetItemsById/TriggerItemDrop/ConsumeItem call completes this node too.
 */
UCLASS()
class USteamAsyncGetAllInventoryItems : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests the full inventory of the current user. Rate-limited by Steam;
	 * frequent calls may return cached data.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Inventory", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get All Inventory Items"))
	static USteamAsyncGetAllInventoryItems* GetAllInventoryItems(UObject* WorldContext, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The inventory arrived; Items holds every item instance the user owns. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryItemsPin OnSuccess;

	/** Steam is unavailable or the request failed; Items is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryItemsPin OnFailure;

private:
	UFUNCTION()
	void HandleInventoryItemsReceived(bool bSuccess, const TArray<FESteamInventoryItem>& Items);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamInventoryItem>& Items);

	TWeakObjectPtr<UESteamInventorySubsystem> InventorySubsystem;
};

/** Completion pin for the RequestPrices node (Currency is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncInventoryPricesPin, const FString&, Currency);

/**
 * Requests current prices for all purchasable item definitions and completes when Steam replies.
 * After success, read individual prices with the subsystem's GetItemPrice / GetItemsWithPrices.
 */
UCLASS()
class USteamAsyncRequestInventoryPrices : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Inventory", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Inventory Prices"))
	static USteamAsyncRequestInventoryPrices* RequestInventoryPrices(UObject* WorldContext, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** Prices are ready; Currency is the user's 3-letter ISO 4217 currency code (e.g. "USD"). */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryPricesPin OnSuccess;

	/** Steam is unavailable or the request failed; Currency is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryPricesPin OnFailure;

private:
	UFUNCTION()
	void HandlePricesReceived(bool bSuccess, FString Currency);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FString& Currency);

	TWeakObjectPtr<UESteamInventorySubsystem> InventorySubsystem;
};

/** Completion pin for the StartPurchase node (OrderId/TransactionId are 0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncInventoryPurchasePin, int64, OrderId, int64, TransactionId);

/**
 * Starts a microtransaction purchase for the given item definitions and completes when Steam
 * acknowledges the transaction (NOT when the user finishes paying — the granted items later arrive
 * on the subsystem's OnInventoryItemsReceived). Definitions and Quantities must be equal length.
 */
UCLASS()
class USteamAsyncStartInventoryPurchase : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Inventory", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Start Inventory Purchase"))
	static USteamAsyncStartInventoryPurchase* StartInventoryPurchase(UObject* WorldContext, const TArray<int32>& Definitions, const TArray<int32>& Quantities, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** Steam initialized the transaction; OrderId/TransactionId identify it. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryPurchasePin OnSuccess;

	/** Steam is unavailable or the transaction could not be started; ids are 0. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncInventoryPurchasePin OnFailure;

private:
	UFUNCTION()
	void HandlePurchaseStarted(bool bSuccess, int64 OrderId, int64 TransactionId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 OrderId, int64 TransactionId);

	TWeakObjectPtr<UESteamInventorySubsystem> InventorySubsystem;
	TArray<int32> Definitions;
	TArray<int32> Quantities;
};

/** Completion pin for the eligible-promo node (DefIds is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncEligiblePromoPin, const TArray<int32>&, DefIds);

/**
 * Requests the manual-promo item definitions a user is eligible for and completes with the ids
 * (which can then be granted via the subsystem's AddPromoItem / AddPromoItems).
 */
UCLASS()
class USteamAsyncRequestEligiblePromoItemDefs : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Inventory", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Eligible Promo Item Defs"))
	static USteamAsyncRequestEligiblePromoItemDefs* RequestEligiblePromoItemDefs(UObject* WorldContext, FESteamId SteamId, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The eligible manual-promo item definition ids for the queried user. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncEligiblePromoPin OnSuccess;

	/** Steam is unavailable or the request failed; DefIds is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncEligiblePromoPin OnFailure;

private:
	UFUNCTION()
	void HandleEligiblePromoReceived(bool bSuccess, const TArray<int32>& DefIds);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<int32>& DefIds);

	TWeakObjectPtr<UESteamInventorySubsystem> InventorySubsystem;
	FESteamId SteamId;
};

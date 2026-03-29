// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EPFSubsystem.h"
#include "EPFTradingSubsystem.generated.h"


UENUM(BlueprintType)
enum class EEPFTradeStatus : uint8
{
	Opening,
	Open,
	Accepting,
	Accepted,
	Filled,
	Cancelled,
	Invalid
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDPLAYFAB_API FEPFTradeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	FString TradeId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	FString OfferingPlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	EEPFTradeStatus Status = EEPFTradeStatus::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	TArray<FString> OfferedItemInstanceIds;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	TArray<FString> RequestedCatalogItemIds;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	FString FilledByPlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "PlayFab|Trading")
	FString OpenedAt;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTradeOpened, const FEPFResult&, Result, const FString&, TradeId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTradeAccepted, const FEPFResult&, Result, const FEPFTradeInfo&, Trade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFTradeCanceled, const FEPFResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTradesReceived, const FEPFResult&, Result, const TArray<FEPFTradeInfo>&, Trades);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEPFTradeInfoReceived, const FEPFResult&, Result, const FEPFTradeInfo&, Trade);

/**
 * Player-to-Player Trading — open, accept, cancel trades using inventory items.
 * Players offer their item instances and request catalog items in return.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFTradingSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/**
	 * Open a trade offer.
	 * @param OfferedItemInstanceIds  Item instance IDs from your inventory to offer.
	 * @param RequestedCatalogItemIds Catalog item IDs you want in return.
	 * @param AllowedPlayerIds        Optional: only these players can accept (empty = anyone).
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Trading")
	void OpenTrade(const TArray<FString>& OfferedItemInstanceIds, const TArray<FString>& RequestedCatalogItemIds, const TArray<FString>& AllowedPlayerIds);

	/**
	 * Accept a trade from another player.
	 * @param TradeId              The trade to accept.
	 * @param OfferingPlayerId     PlayFab ID of the player who opened the trade.
	 * @param AcceptedItemInstanceIds  Your item instance IDs that fulfill the request.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Trading")
	void AcceptTrade(const FString& TradeId, const FString& OfferingPlayerId, const TArray<FString>& AcceptedItemInstanceIds);

	/** Cancel a trade you opened */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Trading")
	void CancelTrade(const FString& TradeId);

	/** Get all your active trades */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Trading")
	void GetPlayerTrades();

	/** Get info about a specific trade */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Trading")
	void GetTradeStatus(const FString& TradeId, const FString& OfferingPlayerId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get cached trades */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Trading")
	TArray<FEPFTradeInfo> GetCachedTrades() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Trading")
	FOnEPFTradeOpened OnTradeOpened;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Trading")
	FOnEPFTradeAccepted OnTradeAccepted;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Trading")
	FOnEPFTradeCanceled OnTradeCanceled;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Trading")
	FOnEPFTradesReceived OnTradesReceived;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Trading")
	FOnEPFTradeInfoReceived OnTradeInfoReceived;

private:

	TArray<FEPFTradeInfo> CachedTrades;

	static EEPFTradeStatus ParseTradeStatus(const FString& StatusStr);
	FEPFTradeInfo ParseTradeJson(const TSharedPtr<FJsonObject>& TradeObj) const;
};

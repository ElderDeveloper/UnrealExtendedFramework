// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebEconServiceSubsystem.generated.h"

/**
 * IEconService — trade history and trade offers for the account tied to the Web API key.
 * All endpoints here are publisher-key calls on the partner host: trusted server/tooling only.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebEconServiceSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IEconService/GetTradeHistory/v1 — a page of completed trades (partner host, publisher key).
	 * MaxTrades <= 0 uses the endpoint default. Paging: pass the last row's time/tradeid as
	 * StartAfterTime/StartAfterTradeId (omitted when <= 0 / empty) and bNavigatingBack to page backwards.
	 * Language is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetTradeHistory(int32 MaxTrades, int64 StartAfterTime, FString StartAfterTradeId, bool bNavigatingBack, bool bGetDescriptions, FString Language, bool bIncludeFailed, bool bIncludeTotal, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/GetTradeOffers/v1 — the account's sent and/or received trade offers
	 * (partner host, publisher key). At least one of bGetSentOffers/bGetReceivedOffers must be true.
	 * TimeHistoricalCutoff (unix time) bounds historical offers and is omitted when <= 0;
	 * Language is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetTradeOffers(bool bGetSentOffers, bool bGetReceivedOffers, bool bGetDescriptions, FString Language, bool bActiveOnly, bool bHistoricalOnly, int64 TimeHistoricalCutoff, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/GetTradeOffer/v1 — details of a single trade offer (partner host, publisher key).
	 * Language is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetTradeOffer(FString TradeOfferId, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/GetTradeOffersSummary/v1 — counts of pending/new trade offers
	 * (partner host, publisher key). TimeLastVisit (unix time) marks what counts as "new";
	 * omitted when <= 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetTradeOffersSummary(int64 TimeLastVisit, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/DeclineTradeOffer/v1 (POST) — declines a trade offer the account received
	 * (partner host, publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void DeclineTradeOffer(FString TradeOfferId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/CancelTradeOffer/v1 (POST) — cancels a trade offer the account sent
	 * (partner host, publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void CancelTradeOffer(FString TradeOfferId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/GetTradeStatus/v1 — status and asset detail of a specific trade
	 * (partner host, publisher key). Language is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetTradeStatus(FString TradeId, bool bGetDescriptions, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/FlushInventoryCache/v1 (POST) — drops Steam's cached copy of a user's inventory in
	 * the given context so the next read is authoritative (partner host, publisher key).
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void FlushInventoryCache(FString SteamId, int32 AppId, int32 ContextId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/FlushContextCache/v1 (POST) — drops Steam's cached list of items in the app's
	 * context (partner host, publisher key). AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void FlushContextCache(int32 AppId, int32 ContextId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconService/FlushAssetAppearanceCache/v1 (POST) — drops Steam's cached asset-class display data
	 * for the app so updated descriptions take effect (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconService", meta = (AutoCreateRefTerm = "OnResponse"))
	void FlushAssetAppearanceCache(int32 AppId, const FOnSteamWebResponse& OnResponse);
};

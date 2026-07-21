// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebEconomySubsystem.generated.h"

/**
 * ISteamEconomy — asset class info, store prices and market prices for economy-enabled apps
 * (partner host; publisher Web API key required for the partner-only endpoints).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebEconomySubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamEconomy/GetAssetClassInfo/v1 — display metadata for asset classes.
	 * ClassIds/InstanceIds are parallel arrays sent as class_count + numeric-suffixed
	 * classid0/classid1/... and instanceid0/instanceid1/... params (an empty InstanceId entry is
	 * omitted — instance ids are optional per class). Language is omitted when empty.
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAssetClassInfo(int32 AppId, FString Language, const TArray<FString>& ClassIds, const TArray<FString>& InstanceIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/GetAssetPrices/v1 — prices and categories for purchasable items.
	 * Currency (ISO 4217, e.g. "USD") and Language are omitted when empty (all currencies / default).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAssetPrices(int32 AppId, FString Currency, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/GetExportedAssetsForUser/v1 — assets a user exported from another game,
	 * available for import (publisher key). An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetExportedAssetsForUser(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/GetMarketPrices/v1 — Steam Community Market price data for the app's items
	 * (publisher key, server-side only).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetMarketPrices(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/CanTrade/v1 — whether SteamId may trade with TargetSteamId (publisher key).
	 * The docs require an appid (sent as "appid"; sketched without one — added here) plus "steamid"
	 * and "targetid". An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back
	 * to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void CanTrade(int32 AppId, FString SteamId, FString TargetSteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/StartTrade/v1 — begins a trade between two accounts (publisher key). Params are
	 * "appid", "partya" and "partyb". Per the official docs this is a GET call (the sketch listed POST);
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void StartTrade(int32 AppId, FString PartyA, FString PartyB, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/StartAssetTransaction/v1 (POST) — starts an in-game store asset purchase
	 * (publisher key). AssetIds/AssetQuantities are parallel arrays sent as the numeric-suffixed
	 * params assetid0/assetid1/... and assetquantity0/assetquantity1/... Currency, Language and
	 * IpAddress are required by the docs; Referrer and bClientAuth are optional (omitted when
	 * empty/false). An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to
	 * the configured AppId. Valve documents this endpoint loosely — verify against your partner
	 * account before shipping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void StartAssetTransaction(int32 AppId, FString SteamId, const TArray<FString>& AssetIds, const TArray<int32>& AssetQuantities, FString Currency, FString Language, FString IpAddress, FString Referrer, bool bClientAuth, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamEconomy/FinalizeAssetTransaction/v1 (POST) — settles an asset purchase begun with
	 * StartAssetTransaction (publisher key). The docs list "steamid", "txnid" and a required
	 * "language" (the sketch omitted steamid/language — added here). An empty SteamId falls back to
	 * the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Economy", meta = (AutoCreateRefTerm = "OnResponse"))
	void FinalizeAssetTransaction(int32 AppId, FString SteamId, FString TxnId, FString Language, const FOnSteamWebResponse& OnResponse);
};

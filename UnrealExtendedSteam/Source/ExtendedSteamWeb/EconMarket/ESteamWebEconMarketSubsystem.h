// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebEconMarketSubsystem.generated.h"

/**
 * IEconMarketService — Steam Community Market eligibility, listings and popularity data.
 * Every method requires a PUBLISHER Web API key and therefore uses the partner host —
 * these calls belong on a trusted server, never in a shipped client.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebEconMarketSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IEconMarketService/GetMarketEligibility/v1 — checks whether a user is allowed to use the
	 * Community Market (publisher key). An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconMarket", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetMarketEligibility(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconMarketService/CancelAppListingsForUser/v1 (POST) — cancels all of a user's Market
	 * listings for an app (publisher key). bSynchronous waits for completion before responding.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconMarket", meta = (AutoCreateRefTerm = "OnResponse"))
	void CancelAppListingsForUser(int32 AppId, FString SteamId, bool bSynchronous, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconMarketService/GetAssetID/v1 — resolves the asset id currently held in escrow for a
	 * Market listing (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconMarket", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAssetID(int32 AppId, int64 ListingId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IEconMarketService/GetPopular/v1 — the most popular Market listings (publisher key).
	 * Rows/FilterAppId/ECurrency <= 0 and an empty Language are omitted (endpoint defaults);
	 * Start is always sent (0 is a valid offset).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|EconMarket", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPopular(FString Language, int32 Rows, int32 Start, int32 FilterAppId, int32 ECurrency, const FOnSteamWebResponse& OnResponse);
};

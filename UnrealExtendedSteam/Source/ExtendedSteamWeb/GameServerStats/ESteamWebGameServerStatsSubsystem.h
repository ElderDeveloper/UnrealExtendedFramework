// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebGameServerStatsSubsystem.generated.h"

/**
 * ISteamGameServerStats — stats gathered by game servers (publisher key required,
 * partner host — trusted-server use only).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebGameServerStatsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamGameServerStats/GetGameServerPlayerStatsForGame/v1 — per-player stats collected by
	 * game servers within a date range (publisher key). GameServerSteamId maps to the endpoint's
	 * "gameid" parameter — per the docs it is safe to pass the appid for non-mod games, so an
	 * empty value falls back to the resolved AppId. RangeStart/RangeEnd use the format
	 * "YYYY-MM-DD HH:MM:SS" in Seattle local time; MaxResults <= 0 is omitted (up to 1000).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameServerStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetGameServerPlayerStatsForGame(FString GameServerSteamId, int32 AppId, FString RangeStart, FString RangeEnd, int32 MaxResults, const FOnSteamWebResponse& OnResponse);
};

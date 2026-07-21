// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebUserStatsSubsystem.generated.h"

/**
 * ISteamUserStats — achievements, stats and player counts (public host).
 * Global/aggregate methods need no key; per-player methods need a user Web API key.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebUserStatsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamUserStats/GetGlobalAchievementPercentagesForApp/v2 — global unlock percentage
	 * of every achievement in the game (no key required).
	 * GameId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetGlobalAchievementPercentagesForApp(int32 GameId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/GetGlobalStatsForGame/v1 — aggregated global totals for the named stats (no key required).
	 * StatNames are sent as count=N plus the indexed name[0]..name[N-1] params the endpoint expects.
	 * StartDate/EndDate (unix time) restrict to daily history when > 0; both omitted returns lifetime totals.
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetGlobalStatsForGame(int32 AppId, const TArray<FString>& StatNames, int64 StartDate, int64 EndDate, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/GetNumberOfCurrentPlayers/v1 — players in game right now (no key required).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetNumberOfCurrentPlayers(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/GetPlayerAchievements/v1 — a player's achievement unlock states (user key).
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId;
	 * Language (e.g. "english") adds localized achievement names/descriptions and is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPlayerAchievements(FString SteamId, int32 AppId, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/GetSchemaForGame/v2 — the game's stat and achievement schema (user key).
	 * AppId <= 0 falls back to the configured AppId; Language is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetSchemaForGame(int32 AppId, FString Language, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/GetUserStatsForGame/v2 — a player's stats and achievements for the app (user key).
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUserStatsForGame(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUserStats/SetUserStatsForGame/v1 (POST) — writes a player's stats for the app (partner host, publisher key).
	 * StatNames/StatValues are sent as count=N plus the indexed name[0]..name[N-1] / value[0]..value[N-1] params;
	 * pass parallel arrays (StatValues[i] is the value for StatNames[i]).
	 * An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|UserStats", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetUserStatsForGame(FString SteamId, int32 AppId, const TArray<FString>& StatNames, const TArray<int32>& StatValues, const FOnSteamWebResponse& OnResponse);
};

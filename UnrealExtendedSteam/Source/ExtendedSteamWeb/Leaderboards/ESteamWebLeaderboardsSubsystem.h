// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebLeaderboardsSubsystem.generated.h"

/**
 * ISteamLeaderboards — server-side leaderboard management and score writes
 * (partner host, publisher Web API key required).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebLeaderboardsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/** ISteamLeaderboards/GetLeaderboardsForGame/v2 — every leaderboard defined for the app (ids, names, sort/display config). AppId <= 0 falls back to the configured AppId. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetLeaderboardsForGame(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamLeaderboards/GetLeaderboardEntries/v1 — a range of entries from a leaderboard.
	 * DataRequest: 0 = Global (RangeStart/RangeEnd are ranks), 1 = GlobalAroundUser (range is relative
	 * to the user's entry), 2 = Friends — modes 1 and 2 need SteamId (empty falls back to DevSteamId;
	 * omitted entirely for Global).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetLeaderboardEntries(int32 AppId, int32 LeaderboardId, int32 RangeStart, int32 RangeEnd, int32 DataRequest, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** ISteamLeaderboards/DeleteLeaderboard/v1 (POST) — permanently deletes the named leaderboard and all its entries. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void DeleteLeaderboard(int32 AppId, FString Name, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamLeaderboards/FindOrCreateLeaderboard/v2 (POST) — looks up a leaderboard by name,
	 * creating it when bCreateIfNotFound. SortMethod is "Ascending" or "Descending";
	 * DisplayType is "Numeric", "Seconds" or "MilliSeconds". bOnlyTrustedWrites blocks client-side
	 * score writes; bOnlyFriendsReads restricts reads to friends of the requesting user.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void FindOrCreateLeaderboard(int32 AppId, FString Name, FString SortMethod, FString DisplayType, bool bCreateIfNotFound, bool bOnlyTrustedWrites, bool bOnlyFriendsReads, const FOnSteamWebResponse& OnResponse);

	/** ISteamLeaderboards/ResetLeaderboard/v1 (POST) — wipes all entries from the leaderboard, keeping its definition. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void ResetLeaderboard(int32 AppId, int32 LeaderboardId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamLeaderboards/SetLeaderboardScore/v1 (POST) — writes a score for a user (trusted,
	 * server-authoritative path). ScoreMethod is "KeepBest" or "ForceUpdate"; DetailsJson is
	 * optional per-entry detail data (omitted when empty). Empty SteamId falls back to DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Leaderboards", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetLeaderboardScore(int32 AppId, int32 LeaderboardId, FString SteamId, int32 Score, FString ScoreMethod, FString DetailsJson, const FOnSteamWebResponse& OnResponse);
};

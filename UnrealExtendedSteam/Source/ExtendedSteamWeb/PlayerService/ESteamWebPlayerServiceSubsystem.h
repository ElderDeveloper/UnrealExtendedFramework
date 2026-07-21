// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebPlayerServiceSubsystem.generated.h"

/**
 * IPlayerService — playtime, ownership, Steam level and badge data (public host, user Web API key).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebPlayerServiceSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IPlayerService/GetRecentlyPlayedGames/v1 — games played in the last two weeks (user key).
	 * An empty SteamId falls back to the configured DevSteamId; Count <= 0 returns all recently played games.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetRecentlyPlayedGames(FString SteamId, int32 Count, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPlayerService/GetOwnedGames/v1 — the games a player owns, with playtime (user key).
	 * An empty SteamId falls back to the configured DevSteamId.
	 * FilterAppIds (when non-empty) is sent as the service-interface indexed array form
	 * appids_filter[0]=..&appids_filter[N]= rather than an input_json blob — same effect, plain params.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetOwnedGames(FString SteamId, bool bIncludeAppInfo, bool bIncludePlayedFreeGames, const TArray<int32>& FilterAppIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPlayerService/GetSteamLevel/v1 — the player's Steam community level (user key).
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetSteamLevel(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPlayerService/GetBadges/v1 — badges owned by the player, plus level/XP info (user key).
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetBadges(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPlayerService/GetCommunityBadgeProgress/v1 — quest progress toward a community badge (user key).
	 * An empty SteamId falls back to the configured DevSteamId; BadgeId <= 0 queries the current event badge.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetCommunityBadgeProgress(FString SteamId, int32 BadgeId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IPlayerService/IsPlayingSharedGame/v1 — returns the lender SteamID when the player is running
	 * AppIdPlaying via Family Sharing, or 0 when playing an owned copy (user key).
	 * An empty SteamId falls back to the configured DevSteamId; AppIdPlaying <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PlayerService", meta = (AutoCreateRefTerm = "OnResponse"))
	void IsPlayingSharedGame(FString SteamId, int32 AppIdPlaying, const FOnSteamWebResponse& OnResponse);
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebPublishedItemVotingSubsystem.generated.h"

/**
 * ISteamPublishedItemVoting — vote summaries for LEGACY published items (publisher key,
 * partner host). DEPRECATED/legacy: superseded by the vote data returned through
 * IPublishedFileService/QueryFiles; wrapped here for full interface coverage.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebPublishedItemVotingSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamPublishedItemVoting/ItemVoteSummary/v1 (POST) — aggregate vote data for the given
	 * published files (publisher key). PublishedFileIds expand to count + publishedfileid[N].
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemVoting", meta = (AutoCreateRefTerm = "PublishedFileIds,OnResponse"))
	void ItemVoteSummary(FString SteamId, int32 AppId, const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamPublishedItemVoting/UserVoteSummary/v1 (POST) — the user's own votes on the given
	 * published files (publisher key). PublishedFileIds expand to count + publishedfileid[N].
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemVoting", meta = (AutoCreateRefTerm = "PublishedFileIds,OnResponse"))
	void UserVoteSummary(FString SteamId, const TArray<FString>& PublishedFileIds, const FOnSteamWebResponse& OnResponse);
};

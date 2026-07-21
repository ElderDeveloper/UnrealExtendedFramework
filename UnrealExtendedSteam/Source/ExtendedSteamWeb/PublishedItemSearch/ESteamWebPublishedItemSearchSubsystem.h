// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebPublishedItemSearchSubsystem.generated.h"

/**
 * ISteamPublishedItemSearch — ranked searches over LEGACY published items (publisher key,
 * partner host). DEPRECATED/legacy: this predates the modern Workshop query API and has been
 * superseded by IPublishedFileService/QueryFiles; wrapped here for full interface coverage.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebPublishedItemSearchSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamPublishedItemSearch/RankedByPublicationOrder/v1 (POST) — published items ranked by
	 * publication date (publisher key). Tags/UserTags expand to tagcount + tag[N] and
	 * usertagcount + usertag[N]; FileType matches EPublishedFileInfoMatchingFileType and is
	 * omitted when <= 0 (0 = Items is the endpoint default).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemSearch", meta = (AutoCreateRefTerm = "Tags,UserTags,OnResponse"))
	void RankedByPublicationOrder(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamPublishedItemSearch/RankedByTrend/v1 (POST) — published items ranked by trend over
	 * Days (1-7; omitted when <= 0) (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemSearch", meta = (AutoCreateRefTerm = "Tags,UserTags,OnResponse"))
	void RankedByTrend(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, int32 Days, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamPublishedItemSearch/RankedByVote/v1 (POST) — published items ranked by vote score
	 * (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemSearch", meta = (AutoCreateRefTerm = "Tags,UserTags,OnResponse"))
	void RankedByVote(FString SteamId, int32 AppId, int32 StartIdx, int32 Count, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamPublishedItemSearch/ResultSetSummary/v1 (POST) — summary (total counts) for the
	 * result set a search would return (publisher key).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|PublishedItemSearch", meta = (AutoCreateRefTerm = "Tags,UserTags,OnResponse"))
	void ResultSetSummary(FString SteamId, int32 AppId, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType, const FOnSteamWebResponse& OnResponse);

private:
	/** Builds the request skeleton and the parameter block shared by all four methods. */
	FESteamWebRequest MakeSearchRequest(const FString& Method, const FString& SteamId, int32 AppId) const;
	static void AppendTagParams(FESteamWebRequest& Request, const TArray<FString>& Tags, const TArray<FString>& UserTags, bool bHasAppAdminAccess, int32 FileType);
};

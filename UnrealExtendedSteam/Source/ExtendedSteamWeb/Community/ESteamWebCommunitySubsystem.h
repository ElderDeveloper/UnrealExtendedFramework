// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebCommunitySubsystem.generated.h"

/**
 * ISteamCommunity — community moderation (publisher key required, partner host —
 * trusted-server use only, never callable from clients).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebCommunitySubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ISteamCommunity/ReportAbuse/v1 (POST) — files an abuse report against a user
	 * (publisher key). AbuseType matches EAbuseReportType, ContentType matches
	 * ECommunityContentType, and Gid (the id of the related record, meaning depends on the
	 * content type) is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Community", meta = (AutoCreateRefTerm = "OnResponse"))
	void ReportAbuse(FString SteamIdActor, FString SteamIdTarget, int32 AppId, int32 AbuseType, int32 ContentType, FString Description, FString Gid, const FOnSteamWebResponse& OnResponse);
};

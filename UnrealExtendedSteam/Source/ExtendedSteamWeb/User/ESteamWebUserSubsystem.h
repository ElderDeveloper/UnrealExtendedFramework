// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebUserSubsystem.generated.h"

/**
 * ISteamUser — profile, friends, bans and vanity URL lookups (all require a Web API key).
 * The ownership/publisher calls at the bottom (CheckAppOwnership, GetAppPriceInfo,
 * GetPublisherAppOwnership[Changes], GetUserGroupList, GrantPackage) require a PUBLISHER key and go
 * out on the partner host — trusted-server use only.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebUserSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/** ISteamUser/GetPlayerSummaries/v2 — profile data for up to 100 SteamID64s. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPlayerSummaries(const TArray<FString>& SteamIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GetFriendList/v1 — friend list of a public profile (relationship=friend).
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetFriendList(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** ISteamUser/GetPlayerBans/v1 — VAC/community/economy ban status for the given SteamID64s. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPlayerBans(const TArray<FString>& SteamIds, const FOnSteamWebResponse& OnResponse);

	/** ISteamUser/ResolveVanityURL/v1 — resolves a profile vanity name to a SteamID64. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void ResolveVanityUrl(FString VanityName, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/CheckAppOwnership/v4 — whether a user owns the app, with purchase/permanent-license
	 * detail (partner host, publisher key). Follows the current documented version v4 (the sketch said
	 * v2). An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the
	 * configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void CheckAppOwnership(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GetAppPriceInfo/v1 — price info for the given apps for the user's wallet (partner
	 * host, publisher key). AppIds are comma-joined into the single "appids" param. An empty SteamId
	 * falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAppPriceInfo(FString SteamId, const TArray<int32>& AppIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GetPublisherAppOwnership/v4 — the user's ownership across all of the publisher's apps
	 * (partner host, publisher key). Follows the current documented version v4 (the sketch said v3).
	 * An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPublisherAppOwnership(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GetPublisherAppOwnershipChanges/v1 — ownership deltas since the given row versions,
	 * for incremental syncing (partner host, publisher key). PackageRowVersion/CdKeyRowVersion are the
	 * cursors returned by a prior call ("0" for a full initial sync). This endpoint is not listed on
	 * the public ISteamUser doc page; params follow the plugin author's spec — verify against your
	 * partner account before shipping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPublisherAppOwnershipChanges(FString PackageRowVersion, FString CdKeyRowVersion, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GetUserGroupList/v1 — the Steam groups a user belongs to (partner host, publisher
	 * key). Also works with a standard user key on the public host; wired to the partner host here to
	 * match the other publisher ownership calls. An empty SteamId falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUserGroupList(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamUser/GrantPackage/v1 (POST) — grants a package (and its apps) to a user (partner host,
	 * publisher key). ThirdPartyKey, when the package is configured for third-party grants, is omitted
	 * when empty. This endpoint is not listed on the public ISteamUser doc page; params follow the
	 * plugin author's spec — verify against your partner account before shipping. An empty SteamId
	 * falls back to the configured DevSteamId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|User", meta = (AutoCreateRefTerm = "OnResponse"))
	void GrantPackage(FString SteamId, int32 PackageId, FString ThirdPartyKey, const FOnSteamWebResponse& OnResponse);
};

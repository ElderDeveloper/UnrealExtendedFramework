// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebAppsSubsystem.generated.h"

/**
 * ISteamApps — app list, version checks, builds/betas and game servers.
 * The first three methods are public (no key); the rest are publisher-key endpoints
 * on the partner host and must only be called from trusted (server/tooling) contexts.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebAppsSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/** ISteamApps/GetAppList/v2 — every public app id and name on Steam (no key required; large response). */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAppList(const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetServersAtAddress/v1 — all game servers hosted at an IP[:port] address (no key required).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetServersAtAddress(FString Address, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/UpToDateCheck/v1 — whether the given app version is current (no key required).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpToDateCheck(int32 AppId, int32 Version, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetAppBetas/v1 — the app's beta branches (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAppBetas(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetAppBuilds/v1 — build history for the app (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId; Count <= 0 uses the endpoint default (10).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAppBuilds(int32 AppId, int32 Count, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetAppDepotVersions/v1 — all depot manifests of the app (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAppDepotVersions(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetPartnerAppListForWebAPIKey/v2 — apps the publisher key may act on
	 * (partner host, publisher key). TypeFilter (e.g. "game,dlc") is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPartnerAppListForWebAPIKey(FString TypeFilter, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/SetAppBuildLive/v1 (POST) — sets a build live on a beta branch (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId; BetaKey "public" is the default branch;
	 * Description is an optional internal note, omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetAppBuildLive(int32 AppId, int32 BuildId, FString BetaKey, FString Description, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetPlayersBanned/v1 — players banned in the app (partner host, publisher key).
	 * AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetPlayersBanned(int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ISteamApps/GetServerList/v1 — game servers matching a query filter (partner host, publisher key).
	 * Filter uses the master-server query format (e.g. "\\appid\\480"); Limit <= 0 uses the endpoint default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Apps", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetServerList(FString Filter, int32 Limit, const FOnSteamWebResponse& OnResponse);
};

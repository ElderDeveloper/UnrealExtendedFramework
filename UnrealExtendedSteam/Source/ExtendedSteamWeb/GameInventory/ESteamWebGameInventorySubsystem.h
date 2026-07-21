// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebGameInventorySubsystem.generated.h"

/**
 * IGameInventory — item definition management for the LEGACY Steam asset/inventory API
 * (publisher key, partner host). This interface has been superseded by IInventoryService
 * (Steam Inventory Service); it is wrapped here for completeness with legacy titles.
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebGameInventorySubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * IGameInventory/GetItemDefArchive/v1 — downloads the item definition archive whose SHA1
	 * digest matches Digest (publisher key). The digest comes from the app's inventory config.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetItemDefArchive(int32 AppId, FString Digest, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameInventory/UpdateItemDefs/v1 (POST) — creates or updates item definitions from a JSON
	 * array (publisher key). ItemDefsJson is sent verbatim as the "itemdefs" parameter.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void UpdateItemDefs(int32 AppId, FString ItemDefsJson, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameInventory/GetUserHistory/v1 — a user's asset history in [StartTime, EndTime] (unix time)
	 * for the given inventory context (publisher key). The docs require a "contextid" the sketch
	 * omitted (added here). An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls
	 * back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetUserHistory(int32 AppId, FString SteamId, int32 ContextId, int32 StartTime, int32 EndTime, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameInventory/GetHistoryCommandDetails/v1 — details of a history Command run against a user's
	 * assets, with the Arguments originally supplied (publisher key). ContextId selects the inventory
	 * context. An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the
	 * configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetHistoryCommandDetails(int32 AppId, FString SteamId, FString Command, int32 ContextId, FString Arguments, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameInventory/HistoryExecuteCommands/v1 (POST) — executes queued history commands against a
	 * user's assets in the given context on behalf of ActorId (publisher key). The official docs list
	 * only appid/steamid/contextid/actorid — there is no command-payload parameter, so the sketch's
	 * "CommandsJson" is intentionally dropped. An empty SteamId falls back to the configured
	 * DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void HistoryExecuteCommands(int32 AppId, FString SteamId, int32 ContextId, int32 ActorId, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameInventory/SupportGetAssetHistory/v1 — support-facing history of a single asset (publisher
	 * key). The docs require a "contextid" the sketch omitted (added here). AppId <= 0 falls back to
	 * the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|GameInventory", meta = (AutoCreateRefTerm = "OnResponse"))
	void SupportGetAssetHistory(int32 AppId, FString AssetId, int32 ContextId, const FOnSteamWebResponse& OnResponse);
};

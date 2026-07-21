// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shared/ESteamTypes.h"
#include "ESteamBlueprintLibrary.generated.h"

/** Synchronous, pure Steam helpers for Blueprints. */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when the Steam client API is initialized and usable. */
	UFUNCTION(BlueprintPure, Category = "Steam")
	static bool IsSteamAvailable();

	/** True when a Steam client is running on this machine (regardless of initialization). */
	UFUNCTION(BlueprintPure, Category = "Steam")
	static bool IsSteamClientRunning();

	/** The Steam application id configured in Extended Steam project settings. */
	UFUNCTION(BlueprintPure, Category = "Steam")
	static int32 GetConfiguredAppId();

	/** Converts a Steam id to its decimal string form (e.g. for backend calls). */
	UFUNCTION(BlueprintPure, Category = "Steam|Id", meta = (DisplayName = "Steam Id To String"))
	static FString SteamIdToString(const FESteamId& SteamId);

	/** Parses a decimal Steam id string. Invalid input yields an invalid id. */
	UFUNCTION(BlueprintPure, Category = "Steam|Id", meta = (DisplayName = "Steam Id From String"))
	static FESteamId SteamIdFromString(const FString& SteamIdString);

	/** True when the Steam id is non-zero. */
	UFUNCTION(BlueprintPure, Category = "Steam|Id", meta = (DisplayName = "Is Valid Steam Id"))
	static bool IsValidSteamId(const FESteamId& SteamId);

	/**
	 * Returns a hex-encoded Steam session auth ticket (IOnlineIdentity::GetAuthToken).
	 * Suitable for PlayFab LoginWithSteam and similar backends that accept a sync session ticket.
	 * Prefer the async "Steam: Get Web API Auth Ticket" node for modern GetAuthTicketForWebApi flows.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Auth", meta = (Keywords = "PlayFab LoginWithSteam SessionTicket"))
	static FString GetSteamAuthTicket(int32 LocalUserNum = 0);
};

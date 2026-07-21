// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebGameServersSubsystem.generated.h"

/**
 * IGameServersService — persistent game server accounts (GSLT login tokens)
 * (partner host, publisher Web API key required; the key owner is the account owner).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebGameServersSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/** IGameServersService/GetAccountList/v1 — all game server accounts owned by the API key's user, with their login tokens. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAccountList(const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameServersService/CreateAccount/v1 (POST) — creates a persistent game server account for the app.
	 * Memo is a free-form note to identify the server. AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void CreateAccount(int32 AppId, FString Memo, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/SetMemo/v1 (POST) — replaces the memo on the server account with the given SteamID64. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetMemo(FString SteamId, FString Memo, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/ResetLoginToken/v1 (POST) — invalidates the old login token and generates a new one for the server account. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void ResetLoginToken(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/DeleteAccount/v1 (POST) — permanently deletes the persistent game server account. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void DeleteAccount(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/GetAccountPublicInfo/v1 — public info (appid, owner) for a game server account. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetAccountPublicInfo(FString SteamId, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/QueryLoginToken/v1 — status (banned/expired) of a login token; the token must be owned by the API key's user. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void QueryLoginToken(FString LoginToken, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/GetServerSteamIDsByIP/v1 — server SteamIDs for a list of server IPs (comma-joined into server_ips). */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetServerSteamIDsByIP(const TArray<FString>& ServerIps, const FOnSteamWebResponse& OnResponse);

	/** IGameServersService/GetServerIPsBySteamID/v1 — server IPs for a list of server SteamID64s (comma-joined into server_steamids). */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetServerIPsBySteamID(const TArray<FString>& ServerSteamIds, const FOnSteamWebResponse& OnResponse);

	/**
	 * IGameServersService/SetBanStatus/v1 (POST) — sets or clears the ban on a game server account (partner host, publisher key).
	 * bBanned toggles the ban; BanSeconds is the ban duration in seconds (0 = permanent) and is ignored when unbanning.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Game Servers", meta = (AutoCreateRefTerm = "OnResponse"))
	void SetBanStatus(FString SteamId, bool bBanned, int32 BanSeconds, const FOnSteamWebResponse& OnResponse);
};

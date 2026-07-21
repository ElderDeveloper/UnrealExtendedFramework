// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamMatchmakingServersSubsystem.generated.h"

/** A single game server entry from the server browser (filled from Steamworks gameserveritem_t). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamServerInfo
{
	GENERATED_BODY()

	/** "ip:queryport" address the entry was queried on. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString Address;

	/** Port that game clients connect to (distinct from the query port in Address). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 GamePort = 0;

	/** Current ping time in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 Ping = 0;

	/** Server name as shown in the server browser. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString Name;

	/** Current map. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString Map;

	/** Game description (ISteamGameServer::SetGameDescription). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString GameDescription;

	/** Game directory / mod dir (ISteamGameServer::SetModDir). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString GameDir;

	/** "Gametags" filter string exposed by the server (ISteamGameServer::SetGameTags). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FString Tags;

	/** Total players currently on the server (includes bots). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 Players = 0;

	/** Maximum players that can join. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 MaxPlayers = 0;

	/** Number of bots (simulated players). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 BotPlayers = 0;

	/** True when joining requires a password. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	bool bPassworded = false;

	/** True when the server is VAC-protected. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	bool bSecure = false;

	/** Server version as reported to Steam. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	int32 ServerVersion = 0;

	/** Steam id of the game server (invalid for old servers or servers not connected to Steam). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|ServerBrowser")
	FESteamId SteamId;
};

/** Fired once per server that responded during an active server list request. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamServerListServerResponded, const FESteamServerInfo&, Server);

/** Fired when a server list request finished (bSuccess false when the master server failed to respond). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamServerListComplete, bool, bSuccess, int32, ResponsiveServers);

/** Fired when a PingServer query completed. Server is default-initialized on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamServerPingResponded, bool, bSuccess, const FESteamServerInfo&, Server);

/** Fired when a ServerRules query completed; Keys and Values are parallel arrays (empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamServerRulesReceived, bool, bSuccess, const TArray<FString>&, Keys, const TArray<FString>&, Values);

/** Fired once per player that responded during an active RequestPlayerDetails query. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamServerPlayerResponded, const FString&, Name, int32, Score, float, TimePlayedSeconds);

/** Fired when a RequestPlayerDetails query finished (bSuccess false when the server failed to respond). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamServerPlayerListComplete, bool, bSuccess);

/**
 * Wraps ISteamMatchmakingServers: server browser lists (internet/LAN/friends/favorites/history)
 * and direct per-server ping/rules queries.
 *
 * Unlike most Steam interfaces this one uses response INTERFACES instead of callbacks; the
 * response objects live in the cpp and are owned by this subsystem so they always outlive the
 * requests (in-flight queries are cancelled on Steam shutdown and Deinitialize).
 *
 * Limitation: ONE in-flight request per operation kind (server list, ping, rules, player details).
 * Starting a new request of the same kind cancels the previous one first — its completion delegate
 * never fires. Servers/players are forwarded one-by-one on OnServerListServerResponded /
 * OnServerPlayerResponded as they respond, followed by a final completion broadcast.
 *
 * Filter keys follow the Steamworks filter operation codes ("map", "gametagsand", "notfull",
 * "hasplayers", "secure", ...); see isteammatchmaking.h for the full list.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamMatchmakingServersSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Server lists ----

	/** Requests the internet server list for this app. Returns false when the request could not be issued. */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestInternetServerList(const TMap<FString, FString>& Filters);

	/** Requests the LAN server list for this app (LAN discovery takes no filters). */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestLANServerList();

	/** Requests the list of servers friends are playing on. */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestFriendsServerList(const TMap<FString, FString>& Filters);

	/** Requests the local user's favorite servers. */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestFavoritesServerList(const TMap<FString, FString>& Filters);

	/** Requests the local user's server history. */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestHistoryServerList(const TMap<FString, FString>& Filters);

	/** Requests the spectator server list for a given app id (uses the shared single-list slot). */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestSpectatorServerList(int32 AppId, const TMap<FString, FString>& Filters);

	/** Cancels and releases the active server list request, if any. OnServerListComplete does not fire. */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	void CancelServerListRequest();

	// ---- Direct server queries ----

	/**
	 * Pings a single server ("a.b.c.d" + query port) for updated details.
	 * Result arrives on OnServerPingResponded. Returns false when the query could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool PingServer(const FString& Ip, int32 Port);

	/**
	 * Requests the rules (key/value pairs set via ISteamGameServer::SetKeyValue) of a single server.
	 * Result arrives on OnServerRulesReceived. Returns false when the query could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool ServerRules(const FString& Ip, int32 Port);

	/**
	 * Requests the player list (name/score/time played) of a single server. Each player arrives on
	 * OnServerPlayerResponded, followed by OnServerPlayerListComplete. Returns false when the query
	 * could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|ServerBrowser")
	bool RequestPlayerDetails(const FString& Ip, int32 Port);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerListServerResponded OnServerListServerResponded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerListComplete OnServerListComplete;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerPingResponded OnServerPingResponded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerRulesReceived OnServerRulesReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerPlayerResponded OnServerPlayerResponded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ServerBrowser")
	FOnSteamServerPlayerListComplete OnServerPlayerListComplete;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamServerBrowserQueries;
	TSharedPtr<class FESteamServerBrowserQueries> Queries;
};

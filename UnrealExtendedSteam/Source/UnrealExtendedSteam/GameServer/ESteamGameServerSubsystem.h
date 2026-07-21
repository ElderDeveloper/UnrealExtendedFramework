// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamGameServerSubsystem.generated.h"

/** Fired when the game server established its connection to the Steam servers (after LogOn/LogOnAnonymous). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamGameServerConnected);

/** Fired when a game server connection attempt failed. Steam keeps retrying while bStillRetrying is true. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGameServerConnectFailure, bool, bStillRetrying);

/** Fired when the game server lost its connection to the Steam servers. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamGameServerDisconnected);

/** Fired when the master server told the game server whether it may advertise itself as secure (VAC). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGameServerPolicyResponse, bool, bSecure);

/** Fired when an AssociateWithClan request completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGSAssociateWithClan, bool, bSuccess);

/**
 * Fired when a ComputeNewPlayerCompatibility request completes. The three counts describe
 * negative relationships between the candidate and players currently on the server (the SDK
 * only reports incompatibilities, not a positive "compatible" tally).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamGSPlayerCompatibility, bool, bSuccess, int32, PlayersThatDontLikeCandidate, int32, PlayersThatCandidateDoesntLike, int32, ClanPlayersThatDontLikeCandidate);

/**
 * Wraps ISteamGameServer: server identity/browser data, master-server advertising and
 * the game server login session.
 *
 * Availability: this subsystem wraps the Steam GAME SERVER API, which is never initialized
 * automatically — it only becomes usable after FExtendedSteamSharedModule::InitializeSteamGameServer
 * succeeded (the Extended Steam online subsystem orchestrates this for dedicated servers).
 * The UESteamSubsystem base-class hooks (HandleSteamClientInitialized/Shutdown) track the CLIENT
 * API only, so this subsystem binds to the shared module's OnSteamGameServerInitialized /
 * OnSteamGameServerShutdown delegates in Initialize/Deinitialize instead. All methods no-op
 * (or return their documented offline defaults) while the game server API is down.
 *
 * Client-side player authentication (session tickets, BeginAuthSession) lives in
 * UESteamUserSubsystem. Shared-socket query handling (ISteamGameServer::HandleIncomingPacket /
 * GetNextOutgoingPacket, "GameSocketShare" mode) is intentionally not wrapped here — it belongs
 * to the online-subsystem phase where the server socket is owned.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamGameServerSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- Identity / configuration (set before LogOn where noted by Steam; no-op while the game server API is down) ----

	/** Game product identifier used by the master server for version checking. Required, set before LogOn. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetProduct(const FString& Product);

	/** Game description shown in the server browser. Required, set before LogOn. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetGameDescription(const FString& GameDescription);

	/** Mod directory when the game is a mod; empty means the original game. Set before LogOn. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetModDir(const FString& ModDir);

	/** Whether this is a dedicated server. Default false. Set before LogOn. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetDedicatedServer(bool bDedicated);

	/** Max player count reported to the server browser and client queries. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetMaxPlayerCount(int32 MaxPlayers);

	/** Number of bots reported to the server browser. Default 0. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetBotPlayerCount(int32 BotPlayers);

	/** Server name as it appears in the server browser. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetServerName(const FString& ServerName);

	/** Map name reported in the server browser. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetMapName(const FString& MapName);

	/** Whether joining the server requires a password. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetPasswordProtected(bool bPasswordProtected);

	/** Spectator port to advertise; 0 (the default) means no spectator service. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetSpectatorPort(int32 SpectatorPort);

	/** Name of the spectator server (only used when the spectator port is non-zero). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetSpectatorServerName(const FString& SpectatorServerName);

	/** "Gametags" string used for server browser filtering (gametagsand/gametagsnor filters). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetGameTags(const FString& GameTags);

	/** "Gamedata" string used for server browser filtering (gamedataand/gamedataor/gamedatanor filters). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetGameData(const FString& GameData);

	/** Region identifier; empty (the default) means the "world" region. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetRegion(const FString& Region);

	/**
	 * Whether to list this server on the master server and respond to browser/LAN discovery.
	 * Starts false; set all relevant server parameters before enabling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetAdvertiseServerActive(bool bActive);

	/**
	 * Sets a custom key/value pair advertised to the server browser (returned in the "rules"
	 * query). Overwrites any previous value for the key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void SetKeyValue(const FString& Key, const FString& Value);

	/** Clears all custom key/value pairs previously set with SetKeyValue. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	void ClearAllKeyValues();

	/**
	 * Updates a player's name and score in the server-browser player list. Call once per player,
	 * keyed by their Steam id. Returns false while the game server API is down.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer")
	bool BUpdateUserData(FESteamId User, const FString& PlayerName, int32 Score);

	// ---- Login session ----

	/** Logs the server into a generic anonymous account. Result arrives on OnConnected / OnConnectFailure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	void LogOnAnonymous();

	/** Logs the server into a persistent game server account using its login token. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	void LogOn(const FString& Token);

	/** Begins logging the game server out of Steam. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	void LogOff();

	/** True while the game server is logged on to Steam. */
	UFUNCTION(BlueprintPure, Category = "Steam|GameServer|Session")
	bool IsLoggedOn() const;

	/** True when the server is VAC-secured (only meaningful after OnPolicyResponse). */
	UFUNCTION(BlueprintPure, Category = "Steam|GameServer|Session")
	bool IsSecure() const;

	/** The game server's Steam id (invalid while not logged on / unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|GameServer|Session")
	FESteamId GetServerSteamId() const;

	/**
	 * The server's public IPv4 address according to Steam as a dotted quad, useful behind NAT.
	 * Empty while unavailable, before logon, or when Steam reports a non-IPv4 address.
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|GameServer|Session")
	FString GetPublicIP() const;

	/**
	 * True when Steam requested that this server restart (e.g. it is out of date and Steam has
	 * queued a content update). Poll periodically and gracefully restart when it returns true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	bool WasRestartRequested() const;

	/**
	 * Associates this server with a Steam group/clan (for the "official server" badge and group
	 * server lists). The result arrives on OnAssociateWithClan.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	void AssociateWithClan(FESteamId ClanId);

	/**
	 * Asks Steam whether a candidate player is compatible with the players already on the server
	 * (based on block/avoid lists). The result arrives on OnPlayerCompatibility.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServer|Session")
	void ComputeNewPlayerCompatibility(FESteamId NewPlayer);

	// ---- Events (game-server pipe callbacks) ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGameServerConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGameServerConnectFailure OnConnectFailure;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGameServerDisconnected OnDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGameServerPolicyResponse OnPolicyResponse;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGSAssociateWithClan OnAssociateWithClan;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServer|Session")
	FOnSteamGSPlayerCompatibility OnPlayerCompatibility;

private:
	/** Called when the shared module finished initializing the game server API (or immediately when already up). */
	void HandleGameServerInitialized();

	/** Called when the game server API shuts down. */
	void HandleGameServerShutdown();

	/** True when the game server API is initialized and the ISteamGameServer interface is reachable. */
	bool IsGameServerAvailable() const;

	friend class FESteamGameServerCallbacks;
	TSharedPtr<class FESteamGameServerCallbacks> Callbacks;

	FDelegateHandle GameServerInitializedHandle;
	FDelegateHandle GameServerShutdownHandle;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamMatchmakingSubsystem.generated.h"

/** Visibility of a lobby (mirrors Steamworks ELobbyType). */
UENUM(BlueprintType)
enum class EESteamLobbyType : uint8
{
	/** Only joinable by invite; never returned by lobby searches. */
	Private,
	/** Visible to friends and invitees, but not in public lobby searches. */
	FriendsOnly,
	/** Visible to friends and returned by public lobby searches. */
	Public,
	/** Returned by searches but not visible to friends; a user can be in up to two invisible lobbies. */
	Invisible
};

/** Geographic reach of a lobby search (mirrors ELobbyDistanceFilter). Results sort closest first. */
UENUM(BlueprintType)
enum class EESteamLobbyDistanceFilter : uint8
{
	/** Only lobbies in the same immediate region. */
	Close,
	/** Same region or nearby regions. */
	Default,
	/** Up to about half-way around the globe. */
	Far,
	/** No distance filtering (expect high latencies). */
	Worldwide
};

/** Comparison operator for lobby search filters (mirrors ELobbyComparison). */
UENUM(BlueprintType)
enum class EESteamLobbyComparison : uint8
{
	EqualToOrLessThan,
	LessThan,
	Equal,
	GreaterThan,
	EqualToOrGreaterThan,
	NotEqual
};

/**
 * How a lobby member's state changed (mirrors the Steamworks EChatMemberStateChange bitflags).
 * The raw Steam value is a bitmask; the wrapper reports the single highest-priority set bit
 * (Banned > Kicked > Disconnected > Left > Entered).
 */
UENUM(BlueprintType)
enum class EESteamChatMemberStateChange : uint8
{
	/** The user joined the lobby. */
	Entered,
	/** The user left the lobby cleanly. */
	Left,
	/** The user disconnected without leaving. */
	Disconnected,
	/** The user was kicked by an admin. */
	Kicked,
	/** The user was kicked and permanently banned by an admin. */
	Banned
};

/** Fired when a CreateLobby request completed. LobbyId is invalid on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyCreated, bool, bSuccess, FESteamId, LobbyId);

/** Fired when a JoinLobby request completed (lobby creation completes on OnLobbyCreated instead). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyEntered, bool, bSuccess, FESteamId, LobbyId);

/** Fired when a RequestLobbyList search completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyListReceived, bool, bSuccess, const TArray<FESteamId>&, Lobbies);

/** Fired when a chat message arrives in a lobby the local user is a member of. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLobbyChatMessage, FESteamId, LobbyId, FESteamId, SenderId, const FString&, Message);

/**
 * Fired when lobby metadata changed. MemberId equals LobbyId when the lobby's own
 * data changed, otherwise it is the member whose per-user data changed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLobbyDataUpdated, FESteamId, LobbyId, FESteamId, MemberId, bool, bSuccess);

/**
 * Fired when a user entered (bEntered true) or left/disconnected/was kicked from a lobby.
 * StateChange gives the specific reason (highest-priority set bit of the Steam bitmask).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamLobbyMemberStateChanged, FESteamId, LobbyId, FESteamId, UserId, bool, bEntered, EESteamChatMemberStateChange, StateChange);

/** Fired when a friend invites the local user to a lobby. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyInviteReceived, FESteamId, FriendId, FESteamId, LobbyId);

/**
 * Fired when the lobby owner set a game server for the lobby via SetLobbyGameServer, telling members
 * where to connect. ServerIp/ServerPort are 0/empty when only a server Steam id was provided.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamLobbyGameCreated, FESteamId, LobbyId, FESteamId, ServerSteamId, const FString&, ServerIp, int32, ServerPort);

/** Fired on the local user when an admin kicks them from a lobby (or a disconnect is detected). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLobbyKicked, FESteamId, LobbyId, FESteamId, AdminId, bool, bKickedDueToDisconnect);

/**
 * Wraps ISteamMatchmaking: lobby lifecycle, search, metadata, membership and chat.
 *
 * Concurrency: same-type async requests are serialized via an internal per-operation FIFO
 * queue (create lobby, join lobby, lobby list each have their own). They complete in order
 * and none are dropped — issuing several before earlier ones finish is safe.
 *
 * Search filters (AddRequestLobbyList*) accumulate on the Steam side and are consumed/cleared
 * by the next RequestLobbyList call; the wrapper does not capture them per request. A queued
 * RequestLobbyList therefore re-runs with whatever filters are set on the Steam side at the
 * moment it is actually issued (typically none, since the first call already consumed them).
 * Add the filters for a search immediately before its RequestLobbyList call and avoid
 * overlapping filtered searches.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamMatchmakingSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Lobby lifecycle ----

	/**
	 * Creates a lobby on the Steam servers. Result arrives on OnLobbyCreated (the
	 * local user has already joined the lobby at that point). Returns false when the
	 * request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking")
	bool CreateLobby(EESteamLobbyType LobbyType, int32 MaxMembers);

	/**
	 * Joins an existing lobby. Result arrives on OnLobbyEntered; lobby metadata is
	 * usable immediately when it fires. Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking")
	bool JoinLobby(FESteamId LobbyId);

	/** Leaves a lobby. Takes effect immediately on the client side. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking")
	void LeaveLobby(FESteamId LobbyId);

	/**
	 * Invites a user to a lobby the local user is a member of. Returns true when the
	 * invite was sent (regardless of the target's response).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking")
	bool InviteUserToLobby(FESteamId LobbyId, FESteamId User);

	// ---- Lobby search ----

	/** Adds a string comparison filter for the next RequestLobbyList call. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListStringFilter(const FString& Key, const FString& Value, EESteamLobbyComparison Comparison);

	/** Adds a numerical comparison filter for the next RequestLobbyList call. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListNumericalFilter(const FString& Key, int32 Value, EESteamLobbyComparison Comparison);

	/** Sets the geographic reach of the next RequestLobbyList call. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListDistanceFilter(EESteamLobbyDistanceFilter DistanceFilter);

	/** Caps how many lobbies the next RequestLobbyList call returns (lower is faster). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListResultCountFilter(int32 MaxResults);

	/** Sorts the next search's results by how close a numeric key is to Value (closest first). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListNearValueFilter(const FString& Key, int32 Value);

	/** Restricts the next search to lobbies with at least SlotsAvailable open member slots. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListFilterSlotsAvailable(int32 SlotsAvailable);

	/** Restricts the next search to lobbies compatible with the members of the given lobby. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	void AddRequestLobbyListCompatibleMembersFilter(FESteamId LobbyId);

	/**
	 * Searches for lobbies matching the filters added since the last search (full
	 * lobbies are never returned). Result arrives on OnLobbyListReceived. Returns
	 * false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Search")
	bool RequestLobbyList();

	// ---- Lobby data ----

	/**
	 * Requests the current metadata of a lobby the local user is not a member of (e.g. a lobby from
	 * a search result). The data arrives via OnLobbyDataUpdated. Returns false when the request could
	 * not be issued; also returns false (harmlessly) when the data is already available locally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	bool RequestLobbyData(FESteamId LobbyId);

	/**
	 * Sets a key/value pair in the lobby metadata (owner only). Broadcast to all
	 * members via OnLobbyDataUpdated. Set an empty value to reset a key.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	bool SetLobbyData(FESteamId LobbyId, const FString& Key, const FString& Value);

	/** Reads lobby metadata. Empty when the key is unset or the lobby is invalid. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	FString GetLobbyData(FESteamId LobbyId, const FString& Key) const;

	/** Reads all metadata key/value pairs of a lobby. Returns false when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	bool GetAllLobbyData(FESteamId LobbyId, TMap<FString, FString>& OutData) const;

	/** Removes a metadata key from the lobby (owner only). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	bool DeleteLobbyData(FESteamId LobbyId, const FString& Key);

	/** Sets per-user metadata for the local user in a lobby. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	void SetLobbyMemberData(FESteamId LobbyId, const FString& Key, const FString& Value);

	/** Reads per-user metadata of a lobby member. Empty when unset or unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Data")
	FString GetLobbyMemberData(FESteamId LobbyId, FESteamId User, const FString& Key) const;

	// ---- Members and ownership ----

	/** Number of users in a lobby (0 when not a member or unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	int32 GetNumLobbyMembers(FESteamId LobbyId) const;

	/** Member at Index in [0, GetNumLobbyMembers). Requires local membership in the lobby. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	FESteamId GetLobbyMemberByIndex(FESteamId LobbyId, int32 Index) const;

	/** Current lobby owner (invalid when not a member or unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	FESteamId GetLobbyOwner(FESteamId LobbyId) const;

	/** Transfers ownership to another member (current owner only). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	bool SetLobbyOwner(FESteamId LobbyId, FESteamId NewOwner);

	/** Changes the lobby type (owner only). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	bool SetLobbyType(FESteamId LobbyId, EESteamLobbyType LobbyType);

	/** Sets whether users may join the lobby (owner only; defaults to true for new lobbies). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	bool SetLobbyJoinable(FESteamId LobbyId, bool bJoinable);

	/** Sets the member limit of the lobby (owner only). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	bool SetLobbyMemberLimit(FESteamId LobbyId, int32 MaxMembers);

	/** Current member limit of the lobby (0 when no limit is defined or unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Members")
	int32 GetLobbyMemberLimit(FESteamId LobbyId) const;

	// ---- Chat ----

	/**
	 * Broadcasts a chat message (up to 4k UTF-8 bytes) to all lobby members. Everyone,
	 * including the local user, receives it on OnLobbyChatMessage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Chat")
	bool SendLobbyChatMessage(FESteamId LobbyId, const FString& Message);

	// ---- Game server / linked lobbies ----

	/**
	 * Associates a game server with the lobby (owner only), telling members where to connect.
	 * Provide either a dotted IPv4 + port, a server Steam id, or both. Fires OnLobbyGameCreated on
	 * every member (including the owner).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|GameServer")
	void SetLobbyGameServer(FESteamId LobbyId, const FString& ServerIp, int32 ServerPort, FESteamId ServerSteamId);

	/**
	 * Reads the game server set on a lobby. OutIp is empty when only a Steam id was set. Returns
	 * false when no server is set or the lobby is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|GameServer")
	bool GetLobbyGameServer(FESteamId LobbyId, FString& OutIp, int32& OutPort, FESteamId& OutServerSteamId) const;

	/**
	 * Links two lobbies so that searches on the parent also return the dependent lobby (owner only).
	 * Returns false when the link could not be set or the lobbies are invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|GameServer")
	bool SetLinkedLobby(FESteamId LobbyId, FESteamId LobbyDependent);

	// ---- Favorite / history servers ----

	/** Number of servers in the local user's favorites + history list; 0 when Steam is unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Favorites")
	int32 GetFavoriteGameCount() const;

	/**
	 * Reads a favorite/history entry. Flags is the Steamworks favorite flag bitmask (1 = favorite,
	 * 2 = history); LastPlayed is a Unix timestamp. Returns false when Index is out of range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Favorites")
	bool GetFavoriteGame(int32 Index, int32& OutAppId, FString& OutIp, int32& OutConnPort, int32& OutQueryPort, int32& OutFlags, int32& OutLastPlayed) const;

	/**
	 * Adds a server to the local user's favorites/history list. Flags is the Steamworks favorite flag
	 * bitmask (1 = favorite, 2 = history); LastPlayed is a Unix timestamp (0 = never). Returns the
	 * new list index, or -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Favorites")
	int32 AddFavoriteGame(int32 AppId, const FString& Ip, int32 ConnPort, int32 QueryPort, int32 Flags, int32 LastPlayed);

	/** Removes a server from the local user's favorites/history list. Returns false when not found. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Matchmaking|Favorites")
	bool RemoveFavoriteGame(int32 AppId, const FString& Ip, int32 ConnPort, int32 QueryPort, int32 Flags);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking")
	FOnSteamLobbyCreated OnLobbyCreated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking")
	FOnSteamLobbyEntered OnLobbyEntered;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking|Search")
	FOnSteamLobbyListReceived OnLobbyListReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking|Chat")
	FOnSteamLobbyChatMessage OnLobbyChatMessage;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking|Data")
	FOnSteamLobbyDataUpdated OnLobbyDataUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking|Members")
	FOnSteamLobbyMemberStateChanged OnLobbyMemberStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking")
	FOnSteamLobbyInviteReceived OnLobbyInviteReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking|GameServer")
	FOnSteamLobbyGameCreated OnLobbyGameCreated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Matchmaking")
	FOnSteamLobbyKicked OnLobbyKicked;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamMatchmakingCallbacks;
	TSharedPtr<class FESteamMatchmakingCallbacks> Callbacks;
};

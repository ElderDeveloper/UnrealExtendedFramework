// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;
class FExtendedSteamSessionCallbacks;

/**
 * Steam-lobby-backed session interface for the "EXTENDEDSTEAM" online subsystem.
 *
 * Every session maps to one Steam lobby. Session/lobby ids are FUniqueNetIdExtendedSteam
 * (decimal SteamID64 string form), so CreateSessionIdFromString parses a decimal lobby id.
 *
 * Lobby data key conventions (all reserved keys use the "ESTEAM_" prefix):
 *  - "ESTEAM_SESSION_NAME"  : the FName the host created the session under.
 *  - "ESTEAM_BUILD_ID"      : decimal FOnlineSessionSettings::BuildUniqueId. FindSessions adds an
 *                             equality filter on it so different builds never see each other.
 *  - "ESTEAM_OWNER_ID"      : decimal SteamID64 of the session owner.
 *  - "ESTEAM_OWNER_NAME"    : persona name of the session owner.
 *  - "ESTEAM_SESSION_STATE" : EOnlineSessionState string, updated by Start/EndSession (lobbies
 *                             have no native start/end; this key is purely informational).
 *  - "ESTEAM_SETTING_TYPES" : "Key=Type,Key=Type" list mapping advertised setting keys to their
 *                             FVariantData type so searchers can reconstruct typed values.
 *  - "ESTEAM_CONNECT"       : OPTIONAL connect-string override. Games publish it by adding an
 *                             advertised FString session setting named ESTEAM_CONNECT (typically
 *                             via UpdateSession once the host address is known). When absent,
 *                             GetResolvedConnectString returns "steam.<lobbyid64>" and the game
 *                             is expected to travel via the lobby id (e.g. a Steam socket URL).
 *
 * Every advertised custom setting (EOnlineDataAdvertisementType ViaOnlineService or
 * ViaOnlineServiceAndPing) is written under its own key with the RAW FVariantData::ToString value
 * (no type prefix), so Steam's server-side string/numerical lobby filters compare against the
 * real values. Setting keys must not start with "ESTEAM_" (reserved, skipped with a warning,
 * except the documented ESTEAM_CONNECT pass-through) and must not contain '=' or ','
 * (breaks the ESTEAM_SETTING_TYPES map).
 *
 * Without the Steamworks SDK (WITH_EXTENDEDSTEAM_SDK=0) or while the Steam client API is down,
 * every online operation fails honestly: returns false and fires its completion delegate with
 * failure. Steam callbacks arrive through the shared module's callback pump; this interface
 * needs no Tick.
 */
class FOnlineSessionExtendedSteam : public IOnlineSession
{
public:
	explicit FOnlineSessionExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineSessionExtendedSteam();

	//~ Begin IOnlineSession
	virtual FUniqueNetIdPtr CreateSessionIdFromString(const FString& SessionIdStr) override;
	virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;
	virtual void RemoveNamedSession(FName SessionName) override;
	virtual bool HasPresenceSession() override;
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray<FUniqueNetIdRef>& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool JoinSession(int32 LocalUserNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray<FUniqueNetIdRef>& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray<FUniqueNetIdRef>& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType = NAME_GamePort) override;
	virtual bool GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
	virtual bool RegisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasInvited = false) override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players) override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override;
	//~ End IOnlineSession

protected:
	//~ Begin IOnlineSession named-session bookkeeping
	virtual FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override;
	virtual FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override;
	//~ End IOnlineSession

private:
	friend class FExtendedSteamSessionCallbacks;

	/** Shared implementation of both CreateSession overloads. */
	bool CreateSessionInternal(int32 HostingPlayerNum, FUniqueNetIdPtr HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings);

	/** Shared implementation of both FindSessions overloads. */
	bool FindSessionsInternal(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings);

	/** Shared implementation of both JoinSession overloads. */
	bool JoinSessionInternal(int32 LocalUserNum, FUniqueNetIdPtr LocalUserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession);

	/** Local Steam user's unique net id via the identity interface; null while Steam is down. */
	FUniqueNetIdPtr GetLocalUserId() const;

	// ------------------------------------------------------------------------------------
	// Steam call-result / callback handlers. Invoked by the cpp-local FExtendedSteamSessionCallbacks
	// holder; raw uint64/int parameters keep SDK types out of this header.
	// ------------------------------------------------------------------------------------

	/** CreateLobby call result (LobbyCreated_t). */
	void HandleLobbyCreated(uint64 LobbyId, int32 SteamResultCode, bool bIOFailure);

	/** JoinLobby call result (LobbyEnter_t). */
	void HandleJoinLobbyEnter(uint64 LobbyId, uint32 ChatRoomEnterResponse, bool bIOFailure);

	/** RequestLobbyList call result (LobbyMatchList_t). */
	void HandleLobbyMatchList(int32 LobbyCount, bool bIOFailure);

	/** LobbyDataUpdate_t callback: completes FindSessionById and invite-accept resolution. */
	void HandleLobbyDataUpdate(uint64 LobbyId, bool bSuccess);

	/** GameLobbyJoinRequested_t callback: friend invite / join-via-overlay accepted. */
	void HandleGameLobbyJoinRequested(uint64 LobbyId);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Guards Sessions and the pending-operation state below. */
	mutable FCriticalSection SessionLock;

	/** Named sessions; shared refs so FNamedOnlineSession* stays stable while the array grows. */
	TArray<FNamedOnlineSessionRef> Sessions;

	/** Steam CCallResult/CCallback holder; created in the constructor when the SDK is compiled in. */
	TSharedPtr<FExtendedSteamSessionCallbacks> SteamCallbacks;

	/** Session name of the CreateSession waiting for LobbyCreated_t; NAME_None when idle. */
	FName PendingCreateSessionName;

	/** Session name of the JoinSession waiting for LobbyEnter_t; NAME_None when idle. */
	FName PendingJoinSessionName;

	/** The single search in flight (SearchState Pending); a second FindSessions fails. */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

	/** Player index that started CurrentSessionSearch. */
	int32 CurrentSearchingPlayerNum = INDEX_NONE;

	/** FindSessionById delegates waiting for LobbyDataUpdate_t, per lobby id. */
	TMap<uint64, TArray<FOnSingleSessionResultCompleteDelegate>> PendingLobbyDataRequests;

	/** Lobbies from GameLobbyJoinRequested_t waiting for data before OnSessionUserInviteAccepted. */
	TSet<uint64> PendingInviteLobbies;
};

typedef TSharedPtr<FOnlineSessionExtendedSteam, ESPMode::ThreadSafe> FOnlineSessionExtendedSteamPtr;

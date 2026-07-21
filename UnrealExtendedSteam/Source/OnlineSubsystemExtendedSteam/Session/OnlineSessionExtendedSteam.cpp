// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Session/OnlineSessionExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamSession
{
	// Reserved lobby data keys (see the class comment in OnlineSessionExtendedSteam.h).
	static const char* KeySessionName = "ESTEAM_SESSION_NAME";
	static const char* KeyBuildId = "ESTEAM_BUILD_ID";
	static const char* KeyOwnerId = "ESTEAM_OWNER_ID";
	static const char* KeyOwnerName = "ESTEAM_OWNER_NAME";
	static const char* KeySessionState = "ESTEAM_SESSION_STATE";
	static const char* KeySettingTypes = "ESTEAM_SETTING_TYPES";
	static const char* KeyConnect = "ESTEAM_CONNECT";

	/** Prefix that marks a lobby data key as reserved for this interface. */
	static const TCHAR* ReservedKeyPrefix = TEXT("ESTEAM_");

	/** Steam caps lobby member counts at 250. */
	static constexpr int32 MaxLobbyMembers = 250;

	/** True while the shared module has the Steam client API up. */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
#else
		return false;
#endif
	}

	/** Parses a decimal SteamID64 out of any unique net id; 0 on malformed input. */
	static uint64 ParseSteamId64(const FUniqueNetId& Id)
	{
		const FUniqueNetIdExtendedSteamPtr Parsed = FUniqueNetIdExtendedSteam::Create(Id.ToString());
		return Parsed.IsValid() ? Parsed->GetSteamId64() : 0;
	}

	/** Lobby id backing a session's SessionInfo; 0 when the info is missing/foreign/invalid. */
	static uint64 GetLobbyIdFromSession(const FOnlineSession& Session)
	{
		if (Session.SessionInfo.IsValid() && Session.SessionInfo->IsValid())
		{
			const FUniqueNetIdExtendedSteamPtr Parsed = FUniqueNetIdExtendedSteam::Create(Session.SessionInfo->GetSessionId().ToString());
			return Parsed.IsValid() ? Parsed->GetSteamId64() : 0;
		}
		return 0;
	}

	/** Builds an empty FVariantData of the given type so FromString can parse into it. */
	static FVariantData MakeVariantOfType(EOnlineKeyValuePairDataType::Type Type)
	{
		FVariantData Data;
		switch (Type)
		{
		case EOnlineKeyValuePairDataType::Int32:  Data.SetValue(static_cast<int32>(0)); break;
		case EOnlineKeyValuePairDataType::UInt32: Data.SetValue(static_cast<uint32>(0)); break;
		case EOnlineKeyValuePairDataType::Int64:  Data.SetValue(static_cast<int64>(0)); break;
		case EOnlineKeyValuePairDataType::UInt64: Data.SetValue(static_cast<uint64>(0)); break;
		case EOnlineKeyValuePairDataType::Double: Data.SetValue(0.0); break;
		case EOnlineKeyValuePairDataType::Float:  Data.SetValue(0.0f); break;
		case EOnlineKeyValuePairDataType::Bool:   Data.SetValue(false); break;
		case EOnlineKeyValuePairDataType::String:
		default:
			Data.SetValue(FString());
			break;
		}
		return Data;
	}

	/** True when the setting is advertised to the online service and safe as a lobby data key. */
	static bool IsAdvertisableSetting(const FName& Key, const FOnlineSessionSetting& Setting)
	{
		if (Setting.AdvertisementType != EOnlineDataAdvertisementType::ViaOnlineService
			&& Setting.AdvertisementType != EOnlineDataAdvertisementType::ViaOnlineServiceAndPing)
		{
			return false;
		}

		const FString KeyStr = Key.ToString();
		if (KeyStr.StartsWith(ReservedKeyPrefix) && KeyStr != UTF8_TO_TCHAR(KeyConnect))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("Session setting '%s' uses the reserved ESTEAM_ key prefix and is not advertised"), *KeyStr);
			return false;
		}
		if (KeyStr.Contains(TEXT("=")) || KeyStr.Contains(TEXT(",")))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("Session setting '%s' contains '=' or ',' and is not advertised"), *KeyStr);
			return false;
		}
		return true;
	}
}

/**
 * Session info for a lobby-backed Extended Steam session: the session id IS the lobby's
 * SteamID64, wrapped in FUniqueNetIdExtendedSteam. Valid while the lobby id is nonzero.
 */
class FOnlineSessionInfoExtendedSteam : public FOnlineSessionInfo
{
public:
	explicit FOnlineSessionInfoExtendedSteam(uint64 InLobbyId)
		: LobbyId(FUniqueNetIdExtendedSteam::Create(InLobbyId))
	{
	}

	//~ Begin IOnlinePlatformData
	virtual const uint8* GetBytes() const override
	{
		return LobbyId->GetBytes();
	}

	virtual int32 GetSize() const override
	{
		return LobbyId->GetSize();
	}

	virtual bool IsValid() const override
	{
		return LobbyId->IsValid();
	}

	virtual FString ToString() const override
	{
		return LobbyId->ToString();
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("ExtendedSteamLobby:%s"), *LobbyId->ToString());
	}
	//~ End IOnlinePlatformData

	//~ Begin FOnlineSessionInfo
	virtual const FUniqueNetId& GetSessionId() const override
	{
		return *LobbyId;
	}
	//~ End FOnlineSessionInfo

private:
	FUniqueNetIdExtendedSteamRef LobbyId;
};

/**
 * Holder for the Steam CCallResult/CCallback registrations, owned by the session interface.
 * Defined in both SDK configurations so the TUniquePtr in the header always has a complete
 * type; without the SDK it is an empty shell that is never exercised.
 */
class FExtendedSteamSessionCallbacks
{
public:
	explicit FExtendedSteamSessionCallbacks(FOnlineSessionExtendedSteam& InOwner)
		: Owner(InOwner)
#if WITH_EXTENDEDSTEAM_SDK
		, LobbyDataUpdateCallback(this, &FExtendedSteamSessionCallbacks::OnLobbyDataUpdate)
		, GameLobbyJoinRequestedCallback(this, &FExtendedSteamSessionCallbacks::OnGameLobbyJoinRequested)
#endif
	{
	}

	FExtendedSteamSessionCallbacks(const FExtendedSteamSessionCallbacks&) = delete;
	FExtendedSteamSessionCallbacks& operator=(const FExtendedSteamSessionCallbacks&) = delete;

	FOnlineSessionExtendedSteam& Owner;

#if WITH_EXTENDEDSTEAM_SDK
	CCallResult<FExtendedSteamSessionCallbacks, LobbyCreated_t> LobbyCreatedCallResult;
	CCallResult<FExtendedSteamSessionCallbacks, LobbyEnter_t> LobbyEnterCallResult;
	CCallResult<FExtendedSteamSessionCallbacks, LobbyMatchList_t> LobbyMatchListCallResult;

	void OnLobbyCreated(LobbyCreated_t* Result, bool bIOFailure)
	{
		Owner.HandleLobbyCreated(
			Result != nullptr ? Result->m_ulSteamIDLobby : 0,
			Result != nullptr ? static_cast<int32>(Result->m_eResult) : 0,
			bIOFailure || Result == nullptr);
	}

	void OnLobbyEnter(LobbyEnter_t* Result, bool bIOFailure)
	{
		Owner.HandleJoinLobbyEnter(
			Result != nullptr ? Result->m_ulSteamIDLobby : 0,
			Result != nullptr ? Result->m_EChatRoomEnterResponse : 0,
			bIOFailure || Result == nullptr);
	}

	void OnLobbyMatchList(LobbyMatchList_t* Result, bool bIOFailure)
	{
		Owner.HandleLobbyMatchList(
			Result != nullptr ? static_cast<int32>(Result->m_nLobbiesMatching) : 0,
			bIOFailure || Result == nullptr);
	}

	/** Lobby-level data updates only (member updates carry a member id != lobby id). */
	void OnLobbyDataUpdate(LobbyDataUpdate_t* Param)
	{
		if (Param != nullptr && Param->m_ulSteamIDLobby == Param->m_ulSteamIDMember)
		{
			Owner.HandleLobbyDataUpdate(Param->m_ulSteamIDLobby, Param->m_bSuccess != 0);
		}
	}

	void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* Param)
	{
		if (Param != nullptr && Param->m_steamIDLobby.IsValid())
		{
			Owner.HandleGameLobbyJoinRequested(Param->m_steamIDLobby.ConvertToUint64());
		}
	}

	CCallback<FExtendedSteamSessionCallbacks, LobbyDataUpdate_t> LobbyDataUpdateCallback;
	CCallback<FExtendedSteamSessionCallbacks, GameLobbyJoinRequested_t> GameLobbyJoinRequestedCallback;
#endif // WITH_EXTENDEDSTEAM_SDK
};

#if WITH_EXTENDEDSTEAM_SDK
namespace ExtendedSteamSession
{
	/** SteamMatchmaking when the client API is up; nullptr otherwise. */
	static ISteamMatchmaking* GetMatchmaking()
	{
		return IsSteamClientUp() ? SteamMatchmaking() : nullptr;
	}

	/**
	 * Writes a named session's advertised state into its lobby's data (owner only).
	 * Raw FVariantData::ToString values keep Steam's server-side lobby filters usable;
	 * the types of custom settings travel in the ESTEAM_SETTING_TYPES map.
	 */
	static void WriteSessionToLobbyData(ISteamMatchmaking& Matchmaking, const CSteamID& Lobby, const FNamedOnlineSession& Session, int32 EffectiveBuildId)
	{
		const FOnlineSessionSettings& Settings = Session.SessionSettings;

		Matchmaking.SetLobbyData(Lobby, KeySessionName, TCHAR_TO_UTF8(*Session.SessionName.ToString()));
		// Advertise the same build id FindSessions filters on (GetBuildUniqueId), not the caller's raw
		// Settings.BuildUniqueId: the search side always filters on GetBuildUniqueId(), so a caller that
		// left BuildUniqueId at its 0 default would otherwise advertise 0 and never match its own searchers.
		Matchmaking.SetLobbyData(Lobby, KeyBuildId, TCHAR_TO_UTF8(*LexToString(EffectiveBuildId)));
		Matchmaking.SetLobbyData(Lobby, KeySessionState, TCHAR_TO_UTF8(EOnlineSessionState::ToString(Session.SessionState)));
		if (Session.OwningUserId.IsValid())
		{
			Matchmaking.SetLobbyData(Lobby, KeyOwnerId, TCHAR_TO_UTF8(*Session.OwningUserId->ToString()));
		}
		if (!Session.OwningUserName.IsEmpty())
		{
			Matchmaking.SetLobbyData(Lobby, KeyOwnerName, TCHAR_TO_UTF8(*Session.OwningUserName));
		}

		FString TypesList;
		for (const TPair<FName, FOnlineSessionSetting>& Pair : Settings.Settings)
		{
			if (!IsAdvertisableSetting(Pair.Key, Pair.Value))
			{
				continue;
			}

			const FString KeyStr = Pair.Key.ToString();
			Matchmaking.SetLobbyData(Lobby, TCHAR_TO_UTF8(*KeyStr), TCHAR_TO_UTF8(*Pair.Value.Data.ToString()));
			if (KeyStr != UTF8_TO_TCHAR(KeyConnect))
			{
				TypesList += FString::Printf(TEXT("%s=%s,"), *KeyStr, Pair.Value.Data.GetTypeString());
			}
		}
		Matchmaking.SetLobbyData(Lobby, KeySettingTypes, TCHAR_TO_UTF8(*TypesList));
	}

	/**
	 * Rebuilds an FOnlineSession from a lobby's data (search results, invites, FindSessionById).
	 * Returns false when the lobby exposes no data at all (unknown/expired lobby).
	 */
	static bool FillSessionFromLobby(ISteamMatchmaking& Matchmaking, const CSteamID& Lobby, FOnlineSession& OutSession)
	{
		const int32 DataCount = Matchmaking.GetLobbyDataCount(Lobby);
		if (DataCount <= 0)
		{
			return false;
		}

		// Raw key/value snapshot first: ESTEAM_SETTING_TYPES must be parsed before the settings.
		TMap<FString, FString> RawData;
		RawData.Reserve(DataCount);
		{
			char Key[k_nMaxLobbyKeyLength] = {};
			char Value[k_cubChatMetadataMax] = {};
			for (int32 Index = 0; Index < DataCount; ++Index)
			{
				if (Matchmaking.GetLobbyDataByIndex(Lobby, Index, Key, sizeof(Key), Value, sizeof(Value)))
				{
					RawData.Add(FString(UTF8_TO_TCHAR(Key)), FString(UTF8_TO_TCHAR(Value)));
				}
			}
		}

		// Setting-name -> FVariantData type map ("Key=Type,Key=Type").
		TMap<FString, EOnlineKeyValuePairDataType::Type> SettingTypes;
		if (const FString* TypesList = RawData.Find(UTF8_TO_TCHAR(KeySettingTypes)))
		{
			TArray<FString> Entries;
			TypesList->ParseIntoArray(Entries, TEXT(","), /*CullEmpty*/ true);
			for (const FString& Entry : Entries)
			{
				FString EntryKey, EntryType;
				if (Entry.Split(TEXT("="), &EntryKey, &EntryType))
				{
					SettingTypes.Add(EntryKey, EOnlineKeyValuePairDataType::FromString(EntryType));
				}
			}
		}

		FOnlineSessionSettings& Settings = OutSession.SessionSettings;

		// Sessions found through the service are by definition advertised lobby sessions.
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bUsesPresence = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.bUseLobbiesIfAvailable = true;

		for (const TPair<FString, FString>& Pair : RawData)
		{
			if (Pair.Key == UTF8_TO_TCHAR(KeyBuildId))
			{
				LexFromString(Settings.BuildUniqueId, *Pair.Value);
			}
			else if (Pair.Key == UTF8_TO_TCHAR(KeyOwnerId))
			{
				OutSession.OwningUserId = FUniqueNetIdExtendedSteam::Create(Pair.Value);
			}
			else if (Pair.Key == UTF8_TO_TCHAR(KeyOwnerName))
			{
				OutSession.OwningUserName = Pair.Value;
			}
			else if (Pair.Key == UTF8_TO_TCHAR(KeyConnect))
			{
				// Kept as a session setting so GetResolvedConnectString works on plain search results.
				Settings.Set(FName(UTF8_TO_TCHAR(KeyConnect)), Pair.Value, EOnlineDataAdvertisementType::ViaOnlineService);
			}
			else if (Pair.Key.StartsWith(ReservedKeyPrefix))
			{
				// Remaining reserved keys (session name/state/types) carry no session-object state.
			}
			else
			{
				const EOnlineKeyValuePairDataType::Type* KnownType = SettingTypes.Find(Pair.Key);
				FVariantData Data = MakeVariantOfType(KnownType != nullptr ? *KnownType : EOnlineKeyValuePairDataType::String);
				if (!Data.FromString(Pair.Value))
				{
					Data.SetValue(Pair.Value);
				}
				Settings.Set(FName(*Pair.Key), FOnlineSessionSetting(MoveTemp(Data), EOnlineDataAdvertisementType::ViaOnlineService));
			}
		}

		// Owner fallback: live lobby owner (resolvable while we are in the lobby).
		if (!OutSession.OwningUserId.IsValid())
		{
			const CSteamID LobbyOwner = Matchmaking.GetLobbyOwner(Lobby);
			if (LobbyOwner.IsValid())
			{
				OutSession.OwningUserId = FUniqueNetIdExtendedSteam::Create(LobbyOwner.ConvertToUint64());
			}
		}

		const int32 MemberLimit = Matchmaking.GetLobbyMemberLimit(Lobby);
		const int32 MemberCount = Matchmaking.GetNumLobbyMembers(Lobby);
		Settings.NumPublicConnections = MemberLimit;
		OutSession.NumOpenPublicConnections = FMath::Max(0, MemberLimit - MemberCount);
		OutSession.NumOpenPrivateConnections = 0;

		OutSession.SessionInfo = MakeShared<FOnlineSessionInfoExtendedSteam>(Lobby.ConvertToUint64());
		return true;
	}

	/**
	 * Builds a search result for the session a friend is currently in, if it is joinable through this
	 * subsystem. Reads the friend's game/lobby info with GetFriendGamePlayed and, when the friend is
	 * in a lobby of THIS app, maps that lobby to a result (reusing FillSessionFromLobby). Steam
	 * pre-caches friends' lobby data, so the fill usually already has data; when it does not yet, a
	 * minimal result carrying the lobby id + owner is returned so JoinSession (which travels by lobby
	 * id) still works. Returns false when the friend is not in a joinable game/lobby of this app.
	 */
	static bool BuildFriendSessionResult(ISteamMatchmaking& Matchmaking, ISteamFriends& Friends, const CSteamID& Friend, FOnlineSessionSearchResult& OutResult)
	{
		FriendGameInfo_t GameInfo;
		if (!Friends.GetFriendGamePlayed(Friend, &GameInfo))
		{
			// Friend is not currently in any game.
			return false;
		}

		// Only this app's own sessions are joinable through this subsystem.
		if (SteamUtils() != nullptr && GameInfo.m_gameID.AppID() != SteamUtils()->GetAppID())
		{
			return false;
		}

		if (!GameInfo.m_steamIDLobby.IsValid())
		{
			// In the game, but not in a joinable lobby (server-only or private); nothing to join here.
			return false;
		}

		// Nudge a refresh of the friend's lobby metadata; the fill below reads whatever is cached.
		Matchmaking.RequestLobbyData(GameInfo.m_steamIDLobby);

		OutResult.PingInMs = 0;
		if (!FillSessionFromLobby(Matchmaking, GameInfo.m_steamIDLobby, OutResult.Session))
		{
			// No cached lobby data yet: return a still-joinable result with the lobby id and owner.
			FOnlineSession& Session = OutResult.Session;
			Session.SessionInfo = MakeShared<FOnlineSessionInfoExtendedSteam>(GameInfo.m_steamIDLobby.ConvertToUint64());
			Session.OwningUserId = FUniqueNetIdExtendedSteam::Create(Friend.ConvertToUint64());
			Session.OwningUserName = FString(UTF8_TO_TCHAR(Friends.GetFriendPersonaName(Friend)));
			Session.SessionSettings.bShouldAdvertise = true;
			Session.SessionSettings.bAllowJoinInProgress = true;
			Session.SessionSettings.bUsesPresence = true;
			Session.SessionSettings.bAllowJoinViaPresence = true;
			Session.SessionSettings.bUseLobbiesIfAvailable = true;
		}
		return true;
	}
}
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineSessionExtendedSteam::FOnlineSessionExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
	SteamCallbacks = MakeShared<FExtendedSteamSessionCallbacks>(*this);
}

FOnlineSessionExtendedSteam::~FOnlineSessionExtendedSteam() = default;

FUniqueNetIdPtr FOnlineSessionExtendedSteam::GetLocalUserId() const
{
	const IOnlineIdentityPtr Identity = Subsystem != nullptr ? Subsystem->GetIdentityInterface() : nullptr;
	return Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
}

// -------------------------------------------------------------------------------------------
// Named-session bookkeeping
// -------------------------------------------------------------------------------------------

FNamedOnlineSession* FOnlineSessionExtendedSteam::AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings)
{
	FScopeLock Lock(&SessionLock);
	return &Sessions.Add_GetRef(MakeShared<FNamedOnlineSession, ESPMode::ThreadSafe>(SessionName, SessionSettings)).Get();
}

FNamedOnlineSession* FOnlineSessionExtendedSteam::AddNamedSession(FName SessionName, const FOnlineSession& Session)
{
	FScopeLock Lock(&SessionLock);
	return &Sessions.Add_GetRef(MakeShared<FNamedOnlineSession, ESPMode::ThreadSafe>(SessionName, Session)).Get();
}

FNamedOnlineSession* FOnlineSessionExtendedSteam::GetNamedSession(FName SessionName)
{
	FScopeLock Lock(&SessionLock);
	for (const FNamedOnlineSessionRef& Session : Sessions)
	{
		if (Session->SessionName == SessionName)
		{
			return &Session.Get();
		}
	}
	return nullptr;
}

void FOnlineSessionExtendedSteam::RemoveNamedSession(FName SessionName)
{
	FScopeLock Lock(&SessionLock);
	Sessions.RemoveAll([SessionName](const FNamedOnlineSessionRef& Session)
	{
		return Session->SessionName == SessionName;
	});
}

bool FOnlineSessionExtendedSteam::HasPresenceSession()
{
	FScopeLock Lock(&SessionLock);
	for (const FNamedOnlineSessionRef& Session : Sessions)
	{
		if (Session->SessionSettings.bUsesPresence)
		{
			return true;
		}
	}
	return false;
}

EOnlineSessionState::Type FOnlineSessionExtendedSteam::GetSessionState(FName SessionName) const
{
	FScopeLock Lock(&SessionLock);
	for (const FNamedOnlineSessionRef& Session : Sessions)
	{
		if (Session->SessionName == SessionName)
		{
			return Session->SessionState;
		}
	}
	return EOnlineSessionState::NoSession;
}

int32 FOnlineSessionExtendedSteam::GetNumSessions()
{
	FScopeLock Lock(&SessionLock);
	return Sessions.Num();
}

void FOnlineSessionExtendedSteam::DumpSessionState()
{
	FScopeLock Lock(&SessionLock);
	UE_LOG(LogExtendedSteam, Log, TEXT("Extended Steam session state: %d named session(s)"), Sessions.Num());
	for (const FNamedOnlineSessionRef& Session : Sessions)
	{
		DumpNamedSession(&Session.Get());
	}
}

FUniqueNetIdPtr FOnlineSessionExtendedSteam::CreateSessionIdFromString(const FString& SessionIdStr)
{
	// Session ids are lobby SteamID64s in decimal string form.
	return FUniqueNetIdExtendedSteam::Create(SessionIdStr);
}

FOnlineSessionSettings* FOnlineSessionExtendedSteam::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	return Session != nullptr ? &Session->SessionSettings : nullptr;
}

// -------------------------------------------------------------------------------------------
// Create / start / update / end / destroy
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	return CreateSessionInternal(HostingPlayerNum, GetLocalUserId(), SessionName, NewSessionSettings);
}

bool FOnlineSessionExtendedSteam::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	return CreateSessionInternal(0, HostingPlayerId.AsShared(), SessionName, NewSessionSettings);
}

bool FOnlineSessionExtendedSteam::CreateSessionInternal(int32 HostingPlayerNum, FUniqueNetIdPtr HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	if (GetNamedSession(SessionName) != nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("CreateSession: session '%s' already exists"), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		{
			FScopeLock Lock(&SessionLock);
			if (!PendingCreateSessionName.IsNone())
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("CreateSession: another create ('%s') is already in flight"), *PendingCreateSessionName.ToString());
				Lock.Unlock();
				TriggerOnCreateSessionCompleteDelegates(SessionName, false);
				return false;
			}
		}

		// Lobby visibility from the session's joinability flags. Invisible lobbies are unused.
		ELobbyType LobbyType = k_ELobbyTypePrivate;
		if (NewSessionSettings.bShouldAdvertise && NewSessionSettings.bAllowJoinViaPresence)
		{
			LobbyType = k_ELobbyTypePublic;
		}
		else if (NewSessionSettings.bAllowJoinViaPresenceFriendsOnly)
		{
			LobbyType = k_ELobbyTypeFriendsOnly;
		}

		const int32 MaxMembers = FMath::Clamp(NewSessionSettings.NumPublicConnections, 1, ExtendedSteamSession::MaxLobbyMembers);

		const SteamAPICall_t ApiCall = Matchmaking->CreateLobby(LobbyType, MaxMembers);
		if (ApiCall == k_uAPICallInvalid)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("CreateSession: CreateLobby refused the call for session '%s'"), *SessionName.ToString());
			TriggerOnCreateSessionCompleteDelegates(SessionName, false);
			return false;
		}

		FNamedOnlineSession* Session = AddNamedSession(SessionName, NewSessionSettings);
		Session->SessionState = EOnlineSessionState::Creating;
		Session->bHosting = true;
		Session->HostingPlayerNum = HostingPlayerNum;
		Session->LocalOwnerId = HostingPlayerId;
		Session->OwningUserId = HostingPlayerId;
		Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;
		Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
		if (const IOnlineIdentityPtr Identity = Subsystem != nullptr ? Subsystem->GetIdentityInterface() : nullptr)
		{
			Session->OwningUserName = Identity->GetPlayerNickname(0);
		}

		{
			FScopeLock Lock(&SessionLock);
			PendingCreateSessionName = SessionName;
		}
		SteamCallbacks->LobbyCreatedCallResult.Set(ApiCall, SteamCallbacks.Get(), &FExtendedSteamSessionCallbacks::OnLobbyCreated);

		UE_LOG(LogExtendedSteam, Log, TEXT("CreateSession: creating %s lobby for session '%s' (%d members)"),
			LobbyType == k_ELobbyTypePublic ? TEXT("public") : (LobbyType == k_ELobbyTypeFriendsOnly ? TEXT("friends-only") : TEXT("private")),
			*SessionName.ToString(), MaxMembers);
		return true;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("CreateSession: Steam matchmaking unavailable; session '%s' not created"), *SessionName.ToString());
	TriggerOnCreateSessionCompleteDelegates(SessionName, false);
	return false;
}

void FOnlineSessionExtendedSteam::HandleLobbyCreated(uint64 LobbyId, int32 SteamResultCode, bool bIOFailure)
{
#if WITH_EXTENDEDSTEAM_SDK
	FName SessionName;
	{
		FScopeLock Lock(&SessionLock);
		SessionName = PendingCreateSessionName;
		PendingCreateSessionName = NAME_None;
	}

	ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();

	if (SessionName.IsNone())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("HandleLobbyCreated: no create in flight; leaving orphan lobby %llu"), LobbyId);
		if (LobbyId != 0 && Matchmaking != nullptr)
		{
			Matchmaking->LeaveLobby(CSteamID(LobbyId));
		}
		return;
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		// Session was torn down while the lobby was being created (e.g. DestroySession raced us).
		if (LobbyId != 0 && Matchmaking != nullptr)
		{
			Matchmaking->LeaveLobby(CSteamID(LobbyId));
		}
		return;
	}

	const bool bSuccess = !bIOFailure && SteamResultCode == static_cast<int32>(k_EResultOK) && LobbyId != 0 && Matchmaking != nullptr;
	if (!bSuccess)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("HandleLobbyCreated: lobby creation for session '%s' failed (result %d, io failure %s)"),
			*SessionName.ToString(), SteamResultCode, bIOFailure ? TEXT("yes") : TEXT("no"));
		RemoveNamedSession(SessionName);
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return;
	}

	// As lobby creator we are already inside the lobby (Steam also sends the matching
	// LobbyEnter_t); publish the advertised session state as lobby data right away.
	Session->SessionInfo = MakeShared<FOnlineSessionInfoExtendedSteam>(LobbyId);
	Session->SessionState = EOnlineSessionState::Pending;
	ExtendedSteamSession::WriteSessionToLobbyData(*Matchmaking, CSteamID(LobbyId), *Session, GetBuildUniqueId());

	UE_LOG(LogExtendedSteam, Log, TEXT("HandleLobbyCreated: session '%s' backed by lobby %llu"), *SessionName.ToString(), LobbyId);
	TriggerOnCreateSessionCompleteDelegates(SessionName, true);
#endif
}

bool FOnlineSessionExtendedSteam::StartSession(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartSession: session '%s' does not exist"), *SessionName.ToString());
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState != EOnlineSessionState::Pending && Session->SessionState != EOnlineSessionState::Ended)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("StartSession: session '%s' cannot start from state %s"),
			*SessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnStartSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Lobbies have no native start/end; the transition is local, mirrored into lobby data.
	Session->SessionState = EOnlineSessionState::InProgress;
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(*Session);
		if (Session->bHosting && LobbyId != 0)
		{
			Matchmaking->SetLobbyData(CSteamID(LobbyId), ExtendedSteamSession::KeySessionState, TCHAR_TO_UTF8(EOnlineSessionState::ToString(Session->SessionState)));
		}
	}
#endif
	TriggerOnStartSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionExtendedSteam::EndSession(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("EndSession: session '%s' does not exist"), *SessionName.ToString());
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState != EOnlineSessionState::InProgress)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("EndSession: session '%s' cannot end from state %s"),
			*SessionName.ToString(), EOnlineSessionState::ToString(Session->SessionState));
		TriggerOnEndSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Ended;
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(*Session);
		if (Session->bHosting && LobbyId != 0)
		{
			Matchmaking->SetLobbyData(CSteamID(LobbyId), ExtendedSteamSession::KeySessionState, TCHAR_TO_UTF8(EOnlineSessionState::ToString(Session->SessionState)));
		}
	}
#endif
	TriggerOnEndSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionExtendedSteam::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UpdateSession: session '%s' does not exist"), *SessionName.ToString());
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (bShouldRefreshOnlineData)
	{
#if WITH_EXTENDEDSTEAM_SDK
		ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();
		const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(*Session);
		if (Session->bHosting && Matchmaking != nullptr && LobbyId != 0)
		{
			const CSteamID Lobby(LobbyId);

			// Delete advertised keys that no longer exist in the updated settings.
			for (const TPair<FName, FOnlineSessionSetting>& OldPair : Session->SessionSettings.Settings)
			{
				if (ExtendedSteamSession::IsAdvertisableSetting(OldPair.Key, OldPair.Value)
					&& UpdatedSessionSettings.Settings.Find(OldPair.Key) == nullptr)
				{
					Matchmaking->DeleteLobbyData(Lobby, TCHAR_TO_UTF8(*OldPair.Key.ToString()));
				}
			}

			Session->SessionSettings = UpdatedSessionSettings;
			ExtendedSteamSession::WriteSessionToLobbyData(*Matchmaking, Lobby, *Session, GetBuildUniqueId());
			TriggerOnUpdateSessionCompleteDelegates(SessionName, true);
			return true;
		}
#endif
		// Non-owners cannot rewrite lobby data; keep the local copy in sync and report honestly.
		UE_LOG(LogExtendedSteam, Warning, TEXT("UpdateSession: cannot refresh online data for session '%s' (not owner or Steam unavailable); local settings updated only"),
			*SessionName.ToString());
		Session->SessionSettings = UpdatedSessionSettings;
		TriggerOnUpdateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionSettings = UpdatedSessionSettings;
	TriggerOnUpdateSessionCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineSessionExtendedSteam::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DestroySession: session '%s' does not exist"), *SessionName.ToString());
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DestroySession: session '%s' is already being destroyed"), *SessionName.ToString());
		CompletionDelegate.ExecuteIfBound(SessionName, false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, false);
		return false;
	}

	Session->SessionState = EOnlineSessionState::Destroying;

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(*Session);
		if (LobbyId != 0)
		{
			// Leaving is enough for host and client alike: Steam destroys empty lobbies itself.
			Matchmaking->LeaveLobby(CSteamID(LobbyId));
		}
	}
#endif

	RemoveNamedSession(SessionName);
	UE_LOG(LogExtendedSteam, Log, TEXT("DestroySession: session '%s' destroyed"), *SessionName.ToString());
	CompletionDelegate.ExecuteIfBound(SessionName, true);
	TriggerOnDestroySessionCompleteDelegates(SessionName, true);
	return true;
}

// -------------------------------------------------------------------------------------------
// Search
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	return FindSessionsInternal(SearchingPlayerNum, SearchSettings);
}

bool FOnlineSessionExtendedSteam::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	return FindSessionsInternal(0, SearchSettings);
}

bool FOnlineSessionExtendedSteam::FindSessionsInternal(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		{
			FScopeLock Lock(&SessionLock);
			if (CurrentSessionSearch.IsValid())
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessions: a lobby search is already in flight; only one search at a time is supported"));
				SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
				return false;
			}
			CurrentSessionSearch = SearchSettings;
			CurrentSearchingPlayerNum = SearchingPlayerNum;
		}

		SearchSettings->SearchResults.Empty();
		SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;

		// Translate the query settings into Steam lobby list filters (Equals only; lobby data
		// stores raw values, so the filters compare against exactly what the host advertised).
		for (const TPair<FName, FOnlineSessionSearchParam>& Param : SearchSettings->QuerySettings.SearchParams)
		{
			// Engine meta keys that do not correspond to advertised lobby data. These are search
			// directives the engine sets on the search object (PRESENCE is set by the standard
			// "Find Sessions (presence)" path; SEARCH_USER/SEARCH_KEYWORDS by friend/keyword searches).
			// Turning them into lobby-data filters matches no lobby and silently excludes every result.
			// Note: SEARCH_PRESENCE was removed from OnlineSessionNames.h in UE 5.8; keep the FName
			// literal so older callers that still inject "PRESENCE" are skipped correctly.
			static const FName SearchPresenceKey(TEXT("PRESENCE"));
			if (Param.Key == SEARCH_LOBBIES || Param.Key == SEARCH_DEDICATED_ONLY
				|| Param.Key == SEARCH_EMPTY_SERVERS_ONLY || Param.Key == SEARCH_SECURE_SERVERS_ONLY
				|| Param.Key == SearchPresenceKey || Param.Key == SEARCH_USER || Param.Key == SEARCH_KEYWORDS)
			{
				continue;
			}

			if (Param.Key == SEARCH_MINSLOTSAVAILABLE)
			{
				int32 MinSlots = 0;
				Param.Value.Data.GetValue(MinSlots);
				if (MinSlots > 0)
				{
					Matchmaking->AddRequestLobbyListFilterSlotsAvailable(MinSlots);
				}
				continue;
			}

			if (Param.Value.ComparisonOp != EOnlineComparisonOp::Equals)
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessions: search param '%s' skipped (only Equals comparisons are supported)"), *Param.Key.ToString());
				continue;
			}

			const FTCHARToUTF8 KeyUtf8(*Param.Key.ToString());
			switch (Param.Value.Data.GetType())
			{
			case EOnlineKeyValuePairDataType::String:
			{
				FString Value;
				Param.Value.Data.GetValue(Value);
				Matchmaking->AddRequestLobbyListStringFilter(KeyUtf8.Get(), TCHAR_TO_UTF8(*Value), k_ELobbyComparisonEqual);
				break;
			}
			case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value = 0;
				Param.Value.Data.GetValue(Value);
				Matchmaking->AddRequestLobbyListNumericalFilter(KeyUtf8.Get(), Value, k_ELobbyComparisonEqual);
				break;
			}
			case EOnlineKeyValuePairDataType::Bool:
			{
				bool bValue = false;
				Param.Value.Data.GetValue(bValue);
				// Bools serialize as "true"/"false" through FVariantData::ToString.
				Matchmaking->AddRequestLobbyListStringFilter(KeyUtf8.Get(), bValue ? "true" : "false", k_ELobbyComparisonEqual);
				break;
			}
			default:
				UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessions: search param '%s' skipped (unsupported value type %s)"),
					*Param.Key.ToString(), Param.Value.Data.GetTypeString());
				break;
			}
		}

		// Different builds never see each other (BuildUniqueId purpose in the OSS contract).
		Matchmaking->AddRequestLobbyListNumericalFilter(ExtendedSteamSession::KeyBuildId, GetBuildUniqueId(), k_ELobbyComparisonEqual);

		if (SearchSettings->MaxSearchResults > 0)
		{
			Matchmaking->AddRequestLobbyListResultCountFilter(SearchSettings->MaxSearchResults);
		}
		Matchmaking->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);

		const SteamAPICall_t ApiCall = Matchmaking->RequestLobbyList();
		if (ApiCall == k_uAPICallInvalid)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessions: RequestLobbyList refused the call"));
			{
				FScopeLock Lock(&SessionLock);
				CurrentSessionSearch.Reset();
				CurrentSearchingPlayerNum = INDEX_NONE;
			}
			SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
			TriggerOnFindSessionsCompleteDelegates(false);
			return false;
		}

		SteamCallbacks->LobbyMatchListCallResult.Set(ApiCall, SteamCallbacks.Get(), &FExtendedSteamSessionCallbacks::OnLobbyMatchList);
		return true;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessions: Steam matchmaking unavailable"));
	SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
	TriggerOnFindSessionsCompleteDelegates(false);
	return false;
}

void FOnlineSessionExtendedSteam::HandleLobbyMatchList(int32 LobbyCount, bool bIOFailure)
{
#if WITH_EXTENDEDSTEAM_SDK
	TSharedPtr<FOnlineSessionSearch> Search;
	{
		FScopeLock Lock(&SessionLock);
		Search = CurrentSessionSearch;
		CurrentSessionSearch.Reset();
		CurrentSearchingPlayerNum = INDEX_NONE;
	}

	if (!Search.IsValid())
	{
		// Search was cancelled while the request was in flight.
		return;
	}

	ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();
	if (bIOFailure || Matchmaking == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("HandleLobbyMatchList: lobby list request failed"));
		Search->SearchState = EOnlineAsyncTaskState::Failed;
		TriggerOnFindSessionsCompleteDelegates(false);
		return;
	}

	for (int32 Index = 0; Index < LobbyCount; ++Index)
	{
		const CSteamID Lobby = Matchmaking->GetLobbyByIndex(Index);
		if (!Lobby.IsValid())
		{
			continue;
		}

		FOnlineSessionSearchResult Result;
		Result.PingInMs = 0; // Lobbies expose no ping; searches are region-filtered by Steam.
		if (ExtendedSteamSession::FillSessionFromLobby(*Matchmaking, Lobby, Result.Session))
		{
			Search->SearchResults.Add(MoveTemp(Result));
		}
	}

	UE_LOG(LogExtendedSteam, Log, TEXT("HandleLobbyMatchList: %d lobby(ies) matched, %d result(s) built"), LobbyCount, Search->SearchResults.Num());
	Search->SearchState = EOnlineAsyncTaskState::Done;
	TriggerOnFindSessionsCompleteDelegates(true);
#endif
}

bool FOnlineSessionExtendedSteam::CancelFindSessions()
{
	TSharedPtr<FOnlineSessionSearch> Search;
	{
		FScopeLock Lock(&SessionLock);
		Search = CurrentSessionSearch;
		CurrentSessionSearch.Reset();
		CurrentSearchingPlayerNum = INDEX_NONE;
	}

	if (!Search.IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("CancelFindSessions: no search in flight"));
		TriggerOnCancelFindSessionsCompleteDelegates(false);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	// Steam cannot abort RequestLobbyList; dropping the call result makes the reply a no-op.
	SteamCallbacks->LobbyMatchListCallResult.Cancel();
#endif
	Search->SearchState = EOnlineAsyncTaskState::Failed;
	TriggerOnCancelFindSessionsCompleteDelegates(true);
	return true;
}

bool FOnlineSessionExtendedSteam::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		const uint64 LobbyId = ExtendedSteamSession::ParseSteamId64(SessionId);
		if (LobbyId == 0)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessionById: '%s' is not a valid lobby id"), *SessionId.ToDebugString());
			CompletionDelegate.ExecuteIfBound(0, false, FOnlineSessionSearchResult());
			return false;
		}

		{
			FScopeLock Lock(&SessionLock);
			PendingLobbyDataRequests.FindOrAdd(LobbyId).Add(CompletionDelegate);
		}

		if (!Matchmaking->RequestLobbyData(CSteamID(LobbyId)))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessionById: RequestLobbyData refused for lobby %llu"), LobbyId);
			{
				FScopeLock Lock(&SessionLock);
				if (TArray<FOnSingleSessionResultCompleteDelegate>* Pending = PendingLobbyDataRequests.Find(LobbyId))
				{
					Pending->Pop();
					if (Pending->IsEmpty())
					{
						PendingLobbyDataRequests.Remove(LobbyId);
					}
				}
			}
			CompletionDelegate.ExecuteIfBound(0, false, FOnlineSessionSearchResult());
			return false;
		}
		return true;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("FindSessionById: Steam matchmaking unavailable"));
	CompletionDelegate.ExecuteIfBound(0, false, FOnlineSessionSearchResult());
	return false;
}

void FOnlineSessionExtendedSteam::HandleLobbyDataUpdate(uint64 LobbyId, bool bSuccess)
{
#if WITH_EXTENDEDSTEAM_SDK
	TArray<FOnSingleSessionResultCompleteDelegate> Delegates;
	bool bInviteLobby = false;
	{
		FScopeLock Lock(&SessionLock);
		PendingLobbyDataRequests.RemoveAndCopyValue(LobbyId, Delegates);
		bInviteLobby = PendingInviteLobbies.Remove(LobbyId) > 0;
	}

	if (Delegates.IsEmpty() && !bInviteLobby)
	{
		// Data update for a lobby we are simply a member of; nothing pending on it.
		return;
	}

	FOnlineSessionSearchResult Result;
	bool bResultBuilt = false;
	if (bSuccess)
	{
		if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
		{
			bResultBuilt = ExtendedSteamSession::FillSessionFromLobby(*Matchmaking, CSteamID(LobbyId), Result.Session);
			Result.PingInMs = 0;
		}
	}

	for (const FOnSingleSessionResultCompleteDelegate& Delegate : Delegates)
	{
		Delegate.ExecuteIfBound(0, bResultBuilt, Result);
	}

	if (bInviteLobby)
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("Session invite for lobby %llu %s"), LobbyId, bResultBuilt ? TEXT("resolved") : TEXT("failed to resolve"));
		TriggerOnSessionUserInviteAcceptedDelegates(bResultBuilt, 0, GetLocalUserId(), Result);
	}
#endif
}

void FOnlineSessionExtendedSteam::HandleGameLobbyJoinRequested(uint64 LobbyId)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();
	if (Matchmaking == nullptr || LobbyId == 0)
	{
		return;
	}

	UE_LOG(LogExtendedSteam, Log, TEXT("HandleGameLobbyJoinRequested: resolving invited lobby %llu"), LobbyId);
	{
		FScopeLock Lock(&SessionLock);
		PendingInviteLobbies.Add(LobbyId);
	}

	if (!Matchmaking->RequestLobbyData(CSteamID(LobbyId)))
	{
		{
			FScopeLock Lock(&SessionLock);
			PendingInviteLobbies.Remove(LobbyId);
		}
		UE_LOG(LogExtendedSteam, Warning, TEXT("HandleGameLobbyJoinRequested: could not request data for lobby %llu"), LobbyId);
		TriggerOnSessionUserInviteAcceptedDelegates(false, 0, GetLocalUserId(), FOnlineSessionSearchResult());
	}
#endif
}

bool FOnlineSessionExtendedSteam::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	// Lobbies carry no QoS endpoint to ping.
	UE_LOG(LogExtendedSteam, Warning, TEXT("PingSearchResults: not supported for Steam lobby sessions"));
	TriggerOnPingSearchResultsCompleteDelegates(false);
	return false;
}

// -------------------------------------------------------------------------------------------
// Join
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::JoinSession(int32 LocalUserNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	return JoinSessionInternal(LocalUserNum, GetLocalUserId(), SessionName, DesiredSession);
}

bool FOnlineSessionExtendedSteam::JoinSession(const FUniqueNetId& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	return JoinSessionInternal(0, LocalUserId.AsShared(), SessionName, DesiredSession);
}

bool FOnlineSessionExtendedSteam::JoinSessionInternal(int32 LocalUserNum, FUniqueNetIdPtr LocalUserId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	if (GetNamedSession(SessionName) != nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinSession: session '%s' already exists"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
		return false;
	}

	const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(DesiredSession.Session);
	if (LobbyId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinSession: desired session for '%s' has no valid lobby id"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::SessionDoesNotExist);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		{
			FScopeLock Lock(&SessionLock);
			if (!PendingJoinSessionName.IsNone())
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("JoinSession: another join ('%s') is already in flight"), *PendingJoinSessionName.ToString());
				Lock.Unlock();
				TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
				return false;
			}
		}

		const SteamAPICall_t ApiCall = Matchmaking->JoinLobby(CSteamID(LobbyId));
		if (ApiCall == k_uAPICallInvalid)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("JoinSession: JoinLobby refused the call for lobby %llu"), LobbyId);
			TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
			return false;
		}

		// Named session built from the desired search result; finalized in HandleJoinLobbyEnter.
		FNamedOnlineSession* Session = AddNamedSession(SessionName, DesiredSession.Session);
		Session->SessionState = EOnlineSessionState::Creating;
		Session->bHosting = false;
		Session->HostingPlayerNum = LocalUserNum;
		Session->LocalOwnerId = LocalUserId;

		{
			FScopeLock Lock(&SessionLock);
			PendingJoinSessionName = SessionName;
		}
		SteamCallbacks->LobbyEnterCallResult.Set(ApiCall, SteamCallbacks.Get(), &FExtendedSteamSessionCallbacks::OnLobbyEnter);

		UE_LOG(LogExtendedSteam, Log, TEXT("JoinSession: joining lobby %llu as session '%s'"), LobbyId, *SessionName.ToString());
		return true;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("JoinSession: Steam matchmaking unavailable"));
	TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
	return false;
}

void FOnlineSessionExtendedSteam::HandleJoinLobbyEnter(uint64 LobbyId, uint32 ChatRoomEnterResponse, bool bIOFailure)
{
#if WITH_EXTENDEDSTEAM_SDK
	FName SessionName;
	{
		FScopeLock Lock(&SessionLock);
		SessionName = PendingJoinSessionName;
		PendingJoinSessionName = NAME_None;
	}

	if (SessionName.IsNone())
	{
		return;
	}

	EOnJoinSessionCompleteResult::Type JoinResult = EOnJoinSessionCompleteResult::UnknownError;
	if (!bIOFailure)
	{
		switch (ChatRoomEnterResponse)
		{
		case k_EChatRoomEnterResponseSuccess:
			JoinResult = EOnJoinSessionCompleteResult::Success;
			break;
		case k_EChatRoomEnterResponseFull:
			JoinResult = EOnJoinSessionCompleteResult::SessionIsFull;
			break;
		case k_EChatRoomEnterResponseDoesntExist:
			JoinResult = EOnJoinSessionCompleteResult::SessionDoesNotExist;
			break;
		default:
			JoinResult = EOnJoinSessionCompleteResult::UnknownError;
			break;
		}
	}

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		// Session was torn down while joining; back out of the lobby if we made it in.
		if (JoinResult == EOnJoinSessionCompleteResult::Success && LobbyId != 0)
		{
			if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
			{
				Matchmaking->LeaveLobby(CSteamID(LobbyId));
			}
		}
		return;
	}

	if (JoinResult == EOnJoinSessionCompleteResult::Success)
	{
		if (ExtendedSteamSession::GetLobbyIdFromSession(*Session) == 0 && LobbyId != 0)
		{
			Session->SessionInfo = MakeShared<FOnlineSessionInfoExtendedSteam>(LobbyId);
		}
		Session->SessionState = EOnlineSessionState::Pending;
		UE_LOG(LogExtendedSteam, Log, TEXT("HandleJoinLobbyEnter: joined lobby %llu for session '%s'"), LobbyId, *SessionName.ToString());
	}
	else
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("HandleJoinLobbyEnter: join for session '%s' failed (%s)"),
			*SessionName.ToString(), LexToString(JoinResult));
		RemoveNamedSession(SessionName);
	}

	TriggerOnJoinSessionCompleteDelegates(SessionName, JoinResult);
#endif
}

// -------------------------------------------------------------------------------------------
// Connect string
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	// PortType is not applicable: lobby sessions resolve to a lobby id or a game-published
	// connect string ("ESTEAM_CONNECT"), never to an ip:port pair this interface owns.
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetResolvedConnectString: session '%s' does not exist"), *SessionName.ToString());
		return false;
	}

	const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(*Session);
	if (LobbyId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetResolvedConnectString: session '%s' has no lobby yet"), *SessionName.ToString());
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	// Live lobby data first: the host may have published the connect string after we joined.
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		const char* LiveConnect = Matchmaking->GetLobbyData(CSteamID(LobbyId), ExtendedSteamSession::KeyConnect);
		if (LiveConnect != nullptr && LiveConnect[0] != '\0')
		{
			ConnectInfo = FString(UTF8_TO_TCHAR(LiveConnect));
			return true;
		}
	}
#endif

	FString StoredConnect;
	if (Session->SessionSettings.Get(FName(UTF8_TO_TCHAR(ExtendedSteamSession::KeyConnect)), StoredConnect) && !StoredConnect.IsEmpty())
	{
		ConnectInfo = StoredConnect;
		return true;
	}

	ConnectInfo = FString::Printf(TEXT("steam.%llu"), LobbyId);
	return true;
}

bool FOnlineSessionExtendedSteam::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	const uint64 LobbyId = ExtendedSteamSession::GetLobbyIdFromSession(SearchResult.Session);
	if (LobbyId == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetResolvedConnectString: search result has no valid lobby id"));
		return false;
	}

	FString StoredConnect;
	if (SearchResult.Session.SessionSettings.Get(FName(UTF8_TO_TCHAR(ExtendedSteamSession::KeyConnect)), StoredConnect) && !StoredConnect.IsEmpty())
	{
		ConnectInfo = StoredConnect;
		return true;
	}

	ConnectInfo = FString::Printf(TEXT("steam.%llu"), LobbyId);
	return true;
}

// -------------------------------------------------------------------------------------------
// Invites
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	return SendSessionInviteToFriends(LocalUserNum, SessionName, { Friend.AsShared() });
}

bool FOnlineSessionExtendedSteam::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	return SendSessionInviteToFriends(0, SessionName, { Friend.AsShared() });
}

bool FOnlineSessionExtendedSteam::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray<FUniqueNetIdRef>& Friends)
{
	return SendSessionInviteToFriends(0, SessionName, Friends);
}

bool FOnlineSessionExtendedSteam::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray<FUniqueNetIdRef>& Friends)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking())
	{
		FNamedOnlineSession* Session = GetNamedSession(SessionName);
		const uint64 LobbyId = Session != nullptr ? ExtendedSteamSession::GetLobbyIdFromSession(*Session) : 0;
		if (LobbyId == 0)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("SendSessionInviteToFriends: session '%s' has no lobby to invite to"), *SessionName.ToString());
			return false;
		}

		bool bAllSent = !Friends.IsEmpty();
		for (const FUniqueNetIdRef& Friend : Friends)
		{
			const uint64 FriendId = ExtendedSteamSession::ParseSteamId64(*Friend);
			if (FriendId == 0 || !Matchmaking->InviteUserToLobby(CSteamID(LobbyId), CSteamID(FriendId)))
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("SendSessionInviteToFriends: could not invite '%s' to session '%s'"),
					*Friend->ToDebugString(), *SessionName.ToString());
				bAllSent = false;
			}
		}
		return bAllSent;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("SendSessionInviteToFriends: Steam matchmaking unavailable"));
	return false;
}

// -------------------------------------------------------------------------------------------
// Player registration
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	FScopeLock Lock(&SessionLock);
	for (const FNamedOnlineSessionRef& Session : Sessions)
	{
		if (Session->SessionName == SessionName)
		{
			for (const FUniqueNetIdRef& Player : Session->RegisteredPlayers)
			{
				if (*Player == UniqueId)
				{
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

bool FOnlineSessionExtendedSteam::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	return RegisterPlayers(SessionName, { PlayerId.AsShared() }, bWasInvited);
}

bool FOnlineSessionExtendedSteam::RegisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players, bool bWasInvited)
{
	// Pure bookkeeping: Steam lobby membership is authoritative on Valve's side; auth handshakes
	// (BeginAuthSession) belong to the game server flow, not the lobby session interface.
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RegisterPlayers: session '%s' does not exist"), *SessionName.ToString());
		TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	for (const FUniqueNetIdRef& Player : Players)
	{
		const bool bAlreadyRegistered = Session->RegisteredPlayers.ContainsByPredicate([&Player](const FUniqueNetIdRef& Existing)
		{
			return *Existing == *Player;
		});
		if (!bAlreadyRegistered)
		{
			Session->RegisteredPlayers.Add(Player);
			Session->NumOpenPublicConnections = FMath::Max(0, Session->NumOpenPublicConnections - 1);
			TriggerOnSessionParticipantJoinedDelegates(SessionName, *Player);
		}
	}

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

bool FOnlineSessionExtendedSteam::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	return UnregisterPlayers(SessionName, { PlayerId.AsShared() });
}

bool FOnlineSessionExtendedSteam::UnregisterPlayers(FName SessionName, const TArray<FUniqueNetIdRef>& Players)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UnregisterPlayers: session '%s' does not exist"), *SessionName.ToString());
		TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, false);
		return false;
	}

	for (const FUniqueNetIdRef& Player : Players)
	{
		const int32 NumRemoved = Session->RegisteredPlayers.RemoveAll([&Player](const FUniqueNetIdRef& Existing)
		{
			return *Existing == *Player;
		});
		if (NumRemoved > 0)
		{
			Session->NumOpenPublicConnections = FMath::Min(Session->SessionSettings.NumPublicConnections, Session->NumOpenPublicConnections + 1);
			TriggerOnSessionParticipantLeftDelegates(SessionName, *Player, EOnSessionParticipantLeftReason::Left);
		}
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, true);
	return true;
}

void FOnlineSessionExtendedSteam::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	// Joining the lobby already made the local player a member; nothing extra to do.
	const bool bSessionExists = GetNamedSession(SessionName) != nullptr;
	Delegate.ExecuteIfBound(PlayerId, bSessionExists ? EOnJoinSessionCompleteResult::Success : EOnJoinSessionCompleteResult::SessionDoesNotExist);
}

void FOnlineSessionExtendedSteam::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, GetNamedSession(SessionName) != nullptr);
}

// -------------------------------------------------------------------------------------------
// Unsupported operations (honest failures)
// -------------------------------------------------------------------------------------------

bool FOnlineSessionExtendedSteam::StartMatchmaking(const TArray<FUniqueNetIdRef>& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	// Steam has no cloud matchmaking for lobbies; use CreateSession/FindSessions/JoinSession.
	UE_LOG(LogExtendedSteam, Warning, TEXT("StartMatchmaking: not supported for Steam lobby sessions"));
	SearchSettings->SearchState = EOnlineAsyncTaskState::Failed;
	TriggerOnMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionExtendedSteam::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	UE_LOG(LogExtendedSteam, Warning, TEXT("CancelMatchmaking: not supported for Steam lobby sessions"));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionExtendedSteam::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	return CancelMatchmaking(0, SessionName);
}

bool FOnlineSessionExtendedSteam::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();
	ISteamFriends* Friends = ExtendedSteamSession::IsSteamClientUp() ? SteamFriends() : nullptr;
	const uint64 FriendId64 = ExtendedSteamSession::ParseSteamId64(Friend);

	if (Matchmaking != nullptr && Friends != nullptr && FriendId64 != 0)
	{
		FOnlineSessionSearchResult Result;
		if (ExtendedSteamSession::BuildFriendSessionResult(*Matchmaking, *Friends, CSteamID(FriendId64), Result))
		{
			TArray<FOnlineSessionSearchResult> Results;
			Results.Add(MoveTemp(Result));
			TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, true, Results);
			return true;
		}

		UE_LOG(LogExtendedSteam, Log, TEXT("FindFriendSession: friend %s is not in a joinable session"), *Friend.ToDebugString());
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());
		return false;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("FindFriendSession: Steam unavailable or invalid friend id"));
	TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, TArray<FOnlineSessionSearchResult>());
	return false;
}

bool FOnlineSessionExtendedSteam::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	return FindFriendSession(0, Friend);
}

bool FOnlineSessionExtendedSteam::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& FriendList)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamMatchmaking* Matchmaking = ExtendedSteamSession::GetMatchmaking();
	ISteamFriends* Friends = ExtendedSteamSession::IsSteamClientUp() ? SteamFriends() : nullptr;

	if (Matchmaking != nullptr && Friends != nullptr)
	{
		TArray<FOnlineSessionSearchResult> Results;
		for (const FUniqueNetIdRef& FriendId : FriendList)
		{
			const uint64 FriendId64 = ExtendedSteamSession::ParseSteamId64(*FriendId);
			if (FriendId64 == 0)
			{
				continue;
			}

			FOnlineSessionSearchResult Result;
			if (ExtendedSteamSession::BuildFriendSessionResult(*Matchmaking, *Friends, CSteamID(FriendId64), Result))
			{
				Results.Add(MoveTemp(Result));
			}
		}

		// Success when at least one friend was in a joinable session; the delegate carries them all.
		const bool bAnyFound = Results.Num() > 0;
		UE_LOG(LogExtendedSteam, Log, TEXT("FindFriendSession: %d of %d friend(s) in a joinable session"), Results.Num(), FriendList.Num());
		TriggerOnFindFriendSessionCompleteDelegates(0, bAnyFound, Results);
		return bAnyFound;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("FindFriendSession: Steam unavailable"));
	TriggerOnFindFriendSessionCompleteDelegates(0, false, TArray<FOnlineSessionSearchResult>());
	return false;
}

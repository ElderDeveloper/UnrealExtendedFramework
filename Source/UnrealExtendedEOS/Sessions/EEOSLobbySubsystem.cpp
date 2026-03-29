// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLobbySubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "UnrealExtendedEOS.h"

static const FName LOBBY_SESSION_NAME = TEXT("EOS_Lobby");

void UEEOSLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSLobbySubsystem::Deinitialize()
{
	CurrentLobbyId.Empty();
	CachedLobbyAttributes.Empty();
	LobbySearch.Reset();
	bInLobby = false;
	Super::Deinitialize();
}

// ── Create / Join / Leave ────────────────────────────────────────────────────

void UEEOSLobbySubsystem::CreateLobby(int32 MaxMembers, bool bIsPublic, bool bUseVoiceChat)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateLobby"));
		OnLobbyCreated.Broadcast(false, TEXT(""));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::CreateLobby — Session interface not available"));
		OnLobbyCreated.Broadcast(false, TEXT(""));
		return;
	}

	// Destroy existing lobby first if one exists
	auto ExistingSession = SessionInterface->GetNamedSession(LOBBY_SESSION_NAME);
	if (ExistingSession != nullptr)
	{
		SessionInterface->DestroySession(LOBBY_SESSION_NAME);
	}

	SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxMembers;
	Settings.bIsLANMatch = false;
	Settings.bShouldAdvertise = bIsPublic;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = bUseVoiceChat;

	SessionInterface->CreateSession(0, LOBBY_SESSION_NAME, Settings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::CreateLobby — Creating lobby with %d max members (Voice=%d)"), MaxMembers, bUseVoiceChat);
}

void UEEOSLobbySubsystem::FindLobbies(int32 MaxResults)
{
	FindLobbiesFiltered(MaxResults, TMap<FString, FString>());
}

void UEEOSLobbySubsystem::FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindLobbies"));
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return;
	}

	SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UEEOSLobbySubsystem::HandleFindSessionsComplete);

	LobbySearch = MakeShareable(new FOnlineSessionSearch());
	LobbySearch->MaxSearchResults = MaxResults;
	LobbySearch->bIsLanQuery = false;
	LobbySearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);

	for (const auto& Filter : SearchFilters)
	{
		LobbySearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	}

	SessionInterface->FindSessions(0, LobbySearch.ToSharedRef());
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::FindLobbies — Searching for lobbies (max %d)..."), MaxResults);
}

void UEEOSLobbySubsystem::JoinLobby(int32 SearchResultIndex)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("JoinLobby"));
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return;
	}

	if (!LobbySearch.IsValid() || !LobbySearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::JoinLobby — Invalid search result index: %d"), SearchResultIndex);
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return;
	}

	SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UEEOSLobbySubsystem::HandleJoinSessionComplete);
	SessionInterface->JoinSession(0, LOBBY_SESSION_NAME, LobbySearch->SearchResults[SearchResultIndex]);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::JoinLobby — Joining lobby at index %d..."), SearchResultIndex);
}

void UEEOSLobbySubsystem::LeaveLobby()
{
	if (!bInLobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::LeaveLobby — Not in a lobby"));
		return;
	}

	if (!IsEOSAvailable()) return;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->DestroySession(LOBBY_SESSION_NAME);
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::LeaveLobby — Leaving lobby '%s'"), *CurrentLobbyId);
	bInLobby = false;
	CurrentLobbyId.Empty();
	CachedLobbyAttributes.Empty();
}

void UEEOSLobbySubsystem::DestroyLobby()
{
	if (!bInLobby) return;

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DestroyLobby"));
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return;
	}

	SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UEEOSLobbySubsystem::HandleDestroySessionComplete);
	SessionInterface->DestroySession(LOBBY_SESSION_NAME);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::DestroyLobby — Destroying lobby '%s'"), *CurrentLobbyId);
}

// ── Lobby Attributes ─────────────────────────────────────────────────────────

void UEEOSLobbySubsystem::SetLobbyAttribute(const FString& Key, const FString& Value)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetLobbyAttribute"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		Settings->Set(FName(*Key), Value, EOnlineDataAdvertisementType::ViaOnlineService);
		SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
		CachedLobbyAttributes.Add(Key, Value);
		OnLobbyAttributeChanged.Broadcast(Key, Value);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Set attribute '%s' = '%s'"), *Key, *Value);
	}
}

FString UEEOSLobbySubsystem::GetLobbyAttribute(const FString& Key) const
{
	if (const FString* Value = CachedLobbyAttributes.Find(Key))
	{
		return *Value;
	}
	return FString();
}

TMap<FString, FString> UEEOSLobbySubsystem::GetAllLobbyAttributes() const
{
	return CachedLobbyAttributes;
}

// ── Member Attributes ────────────────────────────────────────────────────────

void UEEOSLobbySubsystem::SetMemberAttribute(const FString& Key, const FString& Value)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetMemberAttribute"));
		return;
	}

	// Member attributes are set as session settings with a member prefix
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		// Get local userId to scope the attribute per-member
		FString LocalId;
		IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
		if (IdentityInterface.IsValid())
		{
			FUniqueNetIdPtr LocalUserId = IdentityInterface->GetUniquePlayerId(0);
			if (LocalUserId.IsValid())
			{
				LocalId = LocalUserId->ToString();
			}
		}

		// Use member-scoped key: "MEMBER_<MemberId>_<Key>"
		FString MemberKey = FString::Printf(TEXT("MEMBER_%s_%s"), *LocalId, *Key);
		Settings->Set(FName(*MemberKey), Value, EOnlineDataAdvertisementType::ViaOnlineService);
		SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Set member attribute '%s' = '%s'"), *Key, *Value);
	}
}

FString UEEOSLobbySubsystem::GetMemberAttribute(const FString& MemberId, const FString& Key) const
{
	if (!IsEOSAvailable() || !bInLobby) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return FString();

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		// Member attributes are stored with "MEMBER_<MemberId>_<Key>" scoping
		FString MemberKey = FString::Printf(TEXT("MEMBER_%s_%s"), *MemberId, *Key);
		FOnlineSessionSetting* FoundSetting = Settings->Settings.Find(FName(*MemberKey));
		if (FoundSetting)
		{
			return FoundSetting->Data.ToString();
		}
	}

	return FString();
}

// ── Member Management ────────────────────────────────────────────────────────

TArray<FString> UEEOSLobbySubsystem::GetLobbyMembers() const
{
	TArray<FString> Members;
	if (!IsEOSAvailable() || !bInLobby) return Members;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return Members;

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(LOBBY_SESSION_NAME);
	if (Session)
	{
		for (const auto& Player : Session->RegisteredPlayers)
		{
			Members.Add(Player->ToString());
		}
	}
	return Members;
}

int32 UEEOSLobbySubsystem::GetLobbyMemberCount() const
{
	return GetLobbyMembers().Num();
}

FString UEEOSLobbySubsystem::GetLobbyOwner() const
{
	if (!IsEOSAvailable() || !bInLobby) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return FString();

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(LOBBY_SESSION_NAME);
	if (Session && Session->OwningUserId.IsValid())
	{
		return Session->OwningUserId->ToString();
	}
	return FString();
}

bool UEEOSLobbySubsystem::IsLobbyOwner() const
{
	if (!IsEOSAvailable() || !bInLobby) return false;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return false;

	FUniqueNetIdPtr LocalId = IdentityInterface->GetUniquePlayerId(0);
	if (!LocalId.IsValid()) return false;

	return GetLobbyOwner() == LocalId->ToString();
}

void UEEOSLobbySubsystem::KickMember(const FString& MemberId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("KickMember"));
		return;
	}

	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Only the lobby owner can kick members"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	// Create the unique ID for the member to unregister
	FUniqueNetIdPtr KickId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(MemberId);
	if (!KickId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Invalid MemberId '%s'"), *MemberId);
		return;
	}

	// Unregister the player from the session — this triggers a member-left notification for all participants
	if (SessionInterface->UnregisterPlayer(LOBBY_SESSION_NAME, *KickId))
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Kicked member '%s' from lobby"), *MemberId);
		OnLobbyMemberLeft.Broadcast(MemberId);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Failed to unregister '%s'"), *MemberId);
	}
}

void UEEOSLobbySubsystem::PromoteMember(const FString& MemberId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("PromoteMember"));
		return;
	}

	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Only the lobby owner can promote members"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		// Set the new owner via a session attribute — the EOS lobby backend will process the ownership transfer
		Settings->Set(FName(TEXT("LOBBY_OWNER")), MemberId, EOnlineDataAdvertisementType::ViaOnlineService);
		SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Promoted '%s' to lobby owner"), *MemberId);
		OnLobbyOwnerChanged.Broadcast(MemberId);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Failed to get session settings"));
	}
}

// ── Lobby Settings ───────────────────────────────────────────────────────────

void UEEOSLobbySubsystem::SetLobbyJoinable(bool bIsPublic)
{
	if (!IsEOSAvailable()) return;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		Settings->bShouldAdvertise = bIsPublic;
		SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Lobby joinability set to %s"), bIsPublic ? TEXT("Public") : TEXT("Private"));
	}
}

void UEEOSLobbySubsystem::InviteToLobby(const FString& UserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("InviteToLobby"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FUniqueNetIdRef InviteeId = FUniqueNetIdString::Create(UserId, EOS_SUBSYSTEM);
	SessionInterface->SendSessionInviteToFriend(0, LOBBY_SESSION_NAME, *InviteeId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::InviteToLobby — Invited '%s'"), *UserId);
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::IsInLobby() const
{
	return bInLobby;
}

FString UEEOSLobbySubsystem::GetCurrentLobbyId() const
{
	return CurrentLobbyId;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSLobbySubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	bInLobby = bWasSuccessful;
	if (bWasSuccessful)
	{
		CurrentLobbyId = InSessionName.ToString();
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Create lobby %s"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnLobbyCreated.Broadcast(bWasSuccessful, CurrentLobbyId);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnCreateSessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSLobbySubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	TArray<FEEOSSessionSearchResult> Results;

	if (bWasSuccessful && LobbySearch.IsValid())
	{
		for (const auto& SearchResult : LobbySearch->SearchResults)
		{
			FEEOSSessionSearchResult Result;
			Result.SessionId = SearchResult.GetSessionIdStr();
			Result.CurrentPlayers = SearchResult.Session.SessionSettings.NumPublicConnections - SearchResult.Session.NumOpenPublicConnections;
			Result.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			Result.Ping = SearchResult.PingInMs;
			Results.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Found %d lobbies"), Results.Num());
	OnLobbiesFound.Broadcast(Results);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSLobbySubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	bInLobby = bSuccess;
	if (bSuccess)
	{
		CurrentLobbyId = InSessionName.ToString();
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join lobby %s"), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	OnLobbyJoined.Broadcast(bSuccess, CurrentLobbyId);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}
}

void UEEOSLobbySubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	FString DestroyedId = CurrentLobbyId;
	if (bWasSuccessful)
	{
		bInLobby = false;
		CurrentLobbyId.Empty();
		CachedLobbyAttributes.Empty();
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Destroy lobby %s"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnLobbyDestroyed.Broadcast(bWasSuccessful, DestroyedId);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		EOSSub->GetSessionInterface()->OnDestroySessionCompleteDelegates.RemoveAll(this);
	}
}

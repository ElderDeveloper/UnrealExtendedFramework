// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLobbySubsystem.h"
#include "EEOSSearchCoordinator.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "UnrealExtendedEOS.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Async/Async.h"

#include "eos_sdk.h"
#include "eos_lobby.h"

static const FName LOBBY_SESSION_NAME = TEXT("EOS_Lobby");

/** Owner tag this subsystem uses with the shared UEEOSSearchCoordinator. */
static const FName LobbySearchOwner(TEXT("EEOSLobbySubsystem"));

void UEEOSLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Subsystem-lifetime notifications: member join/leave, remote lobby-data updates, and
	// remote lobby destruction. The OSS may not be loaded yet at Initialize time (the Shared
	// base documents this race) — if registration fails, retry on a 1 Hz ticker instead of
	// silently never registering (member events/attribute sync would be dead all session).
	if (!TryRegisterLifetimeNotifications())
	{
		NotificationRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UEEOSLobbySubsystem::TickRetryRegisterNotifications), 1.0f);
	}
}

void UEEOSLobbySubsystem::Deinitialize()
{
	if (NotificationRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NotificationRetryTickerHandle);
		NotificationRetryTickerHandle.Reset();
	}

	// Clear any still-pending per-operation handles (and the lifetime notifications) so late
	// completions can't reach a dead subsystem.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (CreateLobbyCompleteHandle.IsValid())	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
			if (DestroyForCreateLobbyHandle.IsValid())	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateLobbyHandle);
			if (FindLobbiesCompleteHandle.IsValid())	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
			if (JoinLobbyCompleteHandle.IsValid())		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
			if (DestroyLobbyCompleteHandle.IsValid())	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
			if (UpdateLobbyCompleteHandle.IsValid())	SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteHandle);

			if (ParticipantJoinedHandle.IsValid())		SessionInterface->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantJoinedHandle);
			if (ParticipantLeftHandle.IsValid())		SessionInterface->ClearOnSessionParticipantLeftDelegate_Handle(ParticipantLeftHandle);
			if (SessionSettingsUpdatedHandle.IsValid())	SessionInterface->ClearOnSessionSettingsUpdatedDelegate_Handle(SessionSettingsUpdatedHandle);
			if (LifetimeDestroyHandle.IsValid())		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(LifetimeDestroyHandle);
		}
	}

	CreateLobbyCompleteHandle.Reset();
	DestroyForCreateLobbyHandle.Reset();
	FindLobbiesCompleteHandle.Reset();
	JoinLobbyCompleteHandle.Reset();
	DestroyLobbyCompleteHandle.Reset();
	UpdateLobbyCompleteHandle.Reset();
	ParticipantJoinedHandle.Reset();
	ParticipantLeftHandle.Reset();
	SessionSettingsUpdatedHandle.Reset();
	LifetimeDestroyHandle.Reset();

	// If a lobby search of ours was still in flight, free the cross-subsystem search slot.
	ReleaseSearchSlot();

	PendingUpdateKind = EPendingLobbyUpdate::None;
	PendingAttributeKey.Empty();
	PendingAttributeValue.Empty();
	PendingKickedPuids.Empty();
	InFlightKickPuids.Empty();

	CurrentLobbyId.Empty();
	CachedLobbyAttributes.Empty();
	LobbySearch.Reset();
	bInLobby = false;
	Super::Deinitialize();
}

// ── Lifetime notifications ───────────────────────────────────────────────────

bool UEEOSLobbySubsystem::TryRegisterLifetimeNotifications()
{
	if (ParticipantJoinedHandle.IsValid())
	{
		return true;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			// The EOS OSS raises these from its lobby notifications (OnMemberStatusReceived →
			// TriggerOnSessionParticipantJoined/Left, OnLobbyUpdateReceived →
			// TriggerOnSessionSettingsUpdated). Each handler filters on LOBBY_SESSION_NAME
			// because the delegate lists are interface-wide (game sessions raise them too).
			ParticipantJoinedHandle = SessionInterface->AddOnSessionParticipantJoinedDelegate_Handle(
				FOnSessionParticipantJoinedDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleSessionParticipantJoined));
			ParticipantLeftHandle = SessionInterface->AddOnSessionParticipantLeftDelegate_Handle(
				FOnSessionParticipantLeftDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleSessionParticipantLeft));
			SessionSettingsUpdatedHandle = SessionInterface->AddOnSessionSettingsUpdatedDelegate_Handle(
				FOnSessionSettingsUpdatedDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleSessionSettingsUpdated));

			// Lifetime destroy listener: a REMOTE lobby closure (owner destroyed it, backend
			// closed it) makes the engine destroy the named lobby session without any local
			// operation in flight — without this listener bInLobby would wedge true forever.
			LifetimeDestroyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
				FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleLifetimeSessionDestroyed));

			UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSLobbySubsystem — Lifetime lobby notifications registered"));
			return true;
		}
	}
	return false;
}

bool UEEOSLobbySubsystem::TickRetryRegisterNotifications(float /*DeltaTime*/)
{
	if (TryRegisterLifetimeNotifications())
	{
		NotificationRetryTickerHandle.Reset();
		return false; // stop ticking
	}
	// EOS platform creation is permanently exhausted (invalid credentials/config) — every
	// further retry would just re-boot the SDK into the same failure. Give up for this session.
	if (IsEOSCreationExhausted())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem — EOS unavailable for this session; stopping notification registration retries."));
		NotificationRetryTickerHandle.Reset();
		return false;
	}
	return true;
}

// ── Search coordination ──────────────────────────────────────────────────────

UEEOSSearchCoordinator* UEEOSLobbySubsystem::GetSearchCoordinator() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UEEOSSearchCoordinator>() : nullptr;
}

bool UEEOSLobbySubsystem::TryAcquireSearchSlot()
{
	UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator();
	// No coordinator only happens during GameInstance teardown — nothing else can be
	// searching then, so proceed rather than deadlock.
	return Coordinator ? Coordinator->TryAcquire(LobbySearchOwner) : true;
}

void UEEOSLobbySubsystem::ReleaseSearchSlot()
{
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator())
	{
		if (Coordinator->GetCurrentOwner() == LobbySearchOwner)
		{
			Coordinator->Release(LobbySearchOwner);
		}
	}
}

FString UEEOSLobbySubsystem::ResetLobbyState()
{
	FString PreviousLobbyId = CurrentLobbyId;
	CurrentLobbyId.Empty();
	bInLobby = false;
	CachedLobbyAttributes.Empty();
	PendingKickedPuids.Empty();
	InFlightKickPuids.Empty();
	return PreviousLobbyId;
}

// ── Create / Join / Leave ────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::CreateLobby(int32 MaxMembers, bool bIsPublic, bool bUseVoiceChat)
{
	// In-flight rejections come FIRST and never broadcast: the legitimate in-flight caller is
	// waiting on the same delegate, and a failure broadcast here would be misreported as its
	// completion. Reject while a create (either leg of the destroy-then-create chain) or a
	// leave/destroy is in flight — the lobby session's state is mid-transition either way.
	if (CreateLobbyCompleteHandle.IsValid() || DestroyForCreateLobbyHandle.IsValid() || DestroyLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::CreateLobby — A lobby create/destroy operation is already in flight; rejecting new call (no delegate will fire)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateLobby"));
		OnLobbyCreated.Broadcast(false, TEXT(""));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::CreateLobby — Session interface not available"));
		OnLobbyCreated.Broadcast(false, TEXT(""));
		return false;
	}

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
	PendingCreateLobbySettings = Settings;

	// If a lobby session already exists, DestroySession is async — an immediate CreateSession
	// would be rejected by the engine with "session already exists". Chain the create inside
	// the destroy completion instead (handle-scoped, name-filtered, self-clearing).
	if (SessionInterface->GetNamedSession(LOBBY_SESSION_NAME) != nullptr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::CreateLobby — Lobby session already exists, destroying first"));

		DestroyForCreateLobbyHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleDestroyThenCreateLobbyComplete));

		SessionInterface->DestroySession(LOBBY_SESSION_NAME);
		return true;
	}

	CreateLobbyCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete));

	SessionInterface->CreateSession(0, LOBBY_SESSION_NAME, PendingCreateLobbySettings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::CreateLobby — Creating lobby with %d max members (Voice=%d)"), MaxMembers, bUseVoiceChat);
	return true;
}

bool UEEOSLobbySubsystem::FindLobbies(int32 MaxResults)
{
	return FindLobbiesFiltered(MaxResults, TMap<FString, FString>());
}

bool UEEOSLobbySubsystem::FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	// In-flight rejection: never broadcast (the legitimate search's waiters listen on the
	// same OnLobbiesFound and would consume an empty result as their completion).
	if (FindLobbiesCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::FindLobbies — A lobby search is already in flight; rejecting new search (no delegate will fire)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindLobbies"));
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	// The engine cannot run concurrent searches (see UEEOSSearchCoordinator) — a sibling
	// subsystem's search in flight means ours must be rejected, with in-flight semantics
	// (no broadcast).
	if (!TryAcquireSearchSlot())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::FindLobbies — Another session/lobby search is in flight; rejecting (no delegate will fire)"));
		return false;
	}

	FindLobbiesCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleFindSessionsComplete));

	LobbySearch = MakeShareable(new FOnlineSessionSearch());
	LobbySearch->MaxSearchResults = MaxResults;
	LobbySearch->bIsLanQuery = false;
	LobbySearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);

	for (const auto& Filter : SearchFilters)
	{
		LobbySearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	}

	// A synchronous false return means the engine fires NO delegate at all (unique to the
	// find path) — clean up and fail here or the handle wedges forever. Broadcasting is safe:
	// we hold the coordinator slot, so no sibling search is in flight.
	if (!SessionInterface->FindSessions(0, LobbySearch.ToSharedRef()))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::FindLobbies — FindSessions failed to start (synchronous failure; no delegate will fire from the engine)"));
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
		FindLobbiesCompleteHandle.Reset();
		LobbySearch.Reset();
		ReleaseSearchSlot();
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::FindLobbies — Searching for lobbies (max %d)..."), MaxResults);
	return true;
}

bool UEEOSLobbySubsystem::JoinLobby(int32 SearchResultIndex)
{
	// In-flight rejection: never broadcast.
	if (JoinLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::JoinLobby — A join-lobby operation is already in flight; rejecting new call (no delegate will fire)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("JoinLobby"));
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return false;
	}

	if (!LobbySearch.IsValid() || !LobbySearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::JoinLobby — Invalid search result index: %d"), SearchResultIndex);
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbyJoined.Broadcast(false, TEXT(""));
		return false;
	}

	JoinLobbyCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleJoinSessionComplete));

	// Engine-documented requirement (OnlineSessionEOS::CopyLobbyData): "bUsesPresence will be
	// set to false by default in search results, and it should be set by the game side before
	// calling JoinSession." The engine forwards it verbatim to EOS_Lobby_JoinLobby's
	// bPresenceEnabled — without this every non-host member joins presence-less (no invites,
	// no join-via-presence). Modify a local copy so the cached search results stay pristine.
	FOnlineSessionSearchResult SearchResultCopy = LobbySearch->SearchResults[SearchResultIndex];
	SearchResultCopy.Session.SessionSettings.bUsesPresence = true;
	SearchResultCopy.Session.SessionSettings.bUseLobbiesIfAvailable = true;

	SessionInterface->JoinSession(0, LOBBY_SESSION_NAME, SearchResultCopy);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::JoinLobby — Joining lobby at index %d..."), SearchResultIndex);
	return true;
}

bool UEEOSLobbySubsystem::LeaveLobby()
{
	// In-flight rejections come first and never broadcast. Also covers CreateLobby's
	// destroy-then-create chain — the lobby session is mid-transition and a second
	// DestroySession would race the chain.
	if (DestroyLobbyCompleteHandle.IsValid() || DestroyForCreateLobbyHandle.IsValid() || CreateLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::LeaveLobby — A lobby create/destroy operation is already in flight; rejecting new call (no delegate will fire)"));
		return false;
	}

	if (!bInLobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::LeaveLobby — Not in a lobby"));
		OnLobbyDestroyed.Broadcast(false, TEXT(""));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LeaveLobby"));
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	// For lobby-backed sessions DestroySession maps to EOS_Lobby_LeaveLobby (the backend uses
	// the host-migration setting to decide whether an owner's leave destroys the lobby), so
	// this is the correct "leave" for any member. Local state is cleared ONLY in the
	// completion — a failed leave must not desync bInLobby from the engine's named session.
	DestroyLobbyCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleDestroySessionComplete));

	SessionInterface->DestroySession(LOBBY_SESSION_NAME);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::LeaveLobby — Leaving lobby '%s'..."), *CurrentLobbyId);
	return true;
}

bool UEEOSLobbySubsystem::DestroyLobby()
{
	// In-flight rejections come first and never broadcast (see LeaveLobby).
	if (DestroyLobbyCompleteHandle.IsValid() || DestroyForCreateLobbyHandle.IsValid() || CreateLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::DestroyLobby — A lobby create/destroy operation is already in flight; rejecting new call (no delegate will fire)"));
		return false;
	}

	if (!bInLobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::DestroyLobby — Not in a lobby"));
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DestroyLobby"));
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	// Documented owner-only: non-owners leave, they don't destroy.
	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::DestroyLobby — Only the lobby owner can destroy the lobby; use LeaveLobby instead"));
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}

	DestroyLobbyCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleDestroySessionComplete));

	SessionInterface->DestroySession(LOBBY_SESSION_NAME);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::DestroyLobby — Destroying lobby '%s'"), *CurrentLobbyId);
	return true;
}

// ── Lobby Attributes ─────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyAttribute(const FString& Key, const FString& Value)
{
	// In-flight rejection first: a single UpdateSession may be pending at a time (the engine's
	// update completion carries only the session name, so a second update would corrupt the
	// first one's correlation).
	if (UpdateLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — A lobby update is already in flight; rejecting key '%s'"), *Key);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetLobbyAttribute"));
		return false;
	}

	// Engine-enforced owner restriction (UpdateLobbySession): only the owner's UpdateSession
	// applies lobby-level attributes; a non-owner update silently publishes member settings
	// only. Fail loudly here instead of appearing to succeed.
	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — Only the lobby owner can set lobby attributes (key '%s')"), *Key);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — Session interface not available"));
		return false;
	}

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — No lobby session settings found"));
		return false;
	}

	Settings->Set(FName(*Key), Value, EOnlineDataAdvertisementType::ViaOnlineService);

	// Cache write and OnLobbyAttributeChanged broadcast happen in the completion (success
	// only) — not optimistically here.
	PendingUpdateKind = EPendingLobbyUpdate::LobbyAttribute;
	PendingAttributeKey = Key;
	PendingAttributeValue = Value;
	UpdateLobbyCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete));

	SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — Updating attribute '%s' = '%s'..."), *Key, *Value);
	return true;
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

bool UEEOSLobbySubsystem::SetMemberAttribute(const FString& Key, const FString& Value)
{
	// In-flight rejection first (see SetLobbyAttribute).
	if (UpdateLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetMemberAttribute — A lobby update is already in flight; rejecting key '%s'"), *Key);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetMemberAttribute"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetMemberAttribute — Session interface not available"));
		return false;
	}

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetMemberAttribute — No lobby session settings found"));
		return false;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	FUniqueNetIdPtr LocalUserId = IdentityInterface.IsValid() ? IdentityInterface->GetUniquePlayerId(0) : nullptr;
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetMemberAttribute — No local user id (not logged in?)"));
		return false;
	}

	// OSS-native member attributes: the EOS OSS publishes FOnlineSessionSettings::MemberSettings
	// for the LOCAL member on every UpdateSession — for owners and non-owners alike — via
	// EOS_LobbyModification_AddMemberAttribute (UpdateLobbySession/SetLobbyMemberAttributes).
	// The attribute must be advertised ViaOnlineService or the engine skips it.
	FSessionSettings& LocalMemberSettings = Settings->MemberSettings.FindOrAdd(LocalUserId.ToSharedRef());
	LocalMemberSettings.Add(FName(*Key), FOnlineSessionSetting(Value, EOnlineDataAdvertisementType::ViaOnlineService));

	PendingUpdateKind = EPendingLobbyUpdate::MemberAttribute;
	PendingAttributeKey = Key;
	PendingAttributeValue = Value;
	UpdateLobbyCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete));

	SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::SetMemberAttribute — Updating member attribute '%s' = '%s'..."), *Key, *Value);
	return true;
}

FString UEEOSLobbySubsystem::GetMemberAttribute(const FString& MemberId, const FString& Key) const
{
	if (!IsEOSAvailable() || !bInLobby) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return FString();

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings) return FString();

	// Member attributes live in MemberSettings, keyed by the member's net id (the EOS OSS
	// populates them for every member from the lobby snapshot). Accept the full composite
	// net-id string or a bare Product User ID for MemberId.
	const FString QueryPuid = UEEOSBlueprintLibrary::ExtractProductUserId(MemberId);
	for (const TPair<FUniqueNetIdRef, FSessionSettings>& MemberPair : Settings->MemberSettings)
	{
		const FString EntryIdStr = MemberPair.Key->ToString();
		const bool bMatches = (EntryIdStr == MemberId) ||
			(!QueryPuid.IsEmpty() && UEEOSBlueprintLibrary::ExtractProductUserId(EntryIdStr) == QueryPuid);
		if (!bMatches)
		{
			continue;
		}

		if (const FOnlineSessionSetting* FoundSetting = MemberPair.Value.Find(FName(*Key)))
		{
			return FoundSetting->Data.ToString();
		}
		return FString();
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

	// Enumerate the engine's MemberSettings — the EOS lobby flow tracks members there (it
	// never populates FNamedOnlineSession::RegisteredPlayers, which stays empty for lobbies).
	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (Settings)
	{
		for (const TPair<FUniqueNetIdRef, FSessionSettings>& MemberPair : Settings->MemberSettings)
		{
			Members.Add(MemberPair.Key->ToString());
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

bool UEEOSLobbySubsystem::KickMember(const FString& MemberId)
{
	// The OSS path (UnregisterPlayer) never reaches the backend for lobby-backed sessions, so
	// a real kick requires the raw SDK: EOS_Lobby_KickMember (owner-only per SDK docs).
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("KickMember"));
		return false;
	}

	if (!bInLobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Not in a lobby"));
		return false;
	}

	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — Only the lobby owner can kick members (EOS_Lobby_KickMember is owner-only)"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HLobby LobbyHandle = PlatformHandle ? EOS_Platform_GetLobbyInterface(PlatformHandle) : nullptr;
	if (!LobbyHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — EOS Lobby interface not available"));
		return false;
	}

	if (CurrentLobbyId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — No lobby id cached"));
		return false;
	}

	// Local user: bare PUID half of the composite identity net id. EOS_ProductUserId_FromString
	// performs NO validation, so guard on the extracted strings instead.
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	FUniqueNetIdPtr LocalUserId = IdentityInterface.IsValid() ? IdentityInterface->GetUniquePlayerId(0) : nullptr;
	const FString LocalPUIDStr = LocalUserId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString()) : FString();
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — Local user has no Product User ID (no Connect session?)"));
		return false;
	}

	const FString TargetPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(MemberId);
	if (TargetPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — Target '%s' has no Product User ID"), *MemberId);
		return false;
	}

	// Per-target in-flight guard: double-kicking the same member would double-broadcast
	// OnLobbyMemberLeft (once per SDK completion). In-flight rejection: log-only, no delegate.
	if (InFlightKickPuids.Contains(TargetPUIDStr))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — A kick for '%s' is already in flight; rejecting duplicate (no delegate will fire)"), *MemberId);
		return false;
	}

	const FTCHARToUTF8 Utf8LobbyId(*CurrentLobbyId);

	EOS_Lobby_KickMemberOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_KICKMEMBER_API_LATEST;
	Options.LobbyId = (EOS_LobbyId)Utf8LobbyId.Get();
	Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));
	Options.TargetUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetPUIDStr));

	// Mark before the async call: the engine will also raise participant-left (Kicked) for this
	// member, and the SDK completion below must be the single OnLobbyMemberLeft source.
	PendingKickedPuids.Add(TargetPUIDStr);
	InFlightKickPuids.Add(TargetPUIDStr);

	struct FKickContext
	{
		TWeakObjectPtr<UEEOSLobbySubsystem> Self;
		FString MemberId;
		FString TargetPuid;
	};
	FKickContext* Context = new FKickContext{ this, MemberId, TargetPUIDStr };

	EOS_Lobby_KickMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_KickMemberCallbackInfo* Data)
		{
			TUniquePtr<FKickContext> Ctx(static_cast<FKickContext*>(Data->ClientData));
			if (!Ctx) return;

			AsyncTask(ENamedThreads::GameThread,
				[WeakSelf = Ctx->Self, KickedMemberId = MoveTemp(Ctx->MemberId), TargetPuid = MoveTemp(Ctx->TargetPuid), ResultCode = Data->ResultCode]()
				{
					UEEOSLobbySubsystem* Self = WeakSelf.Get();
					if (!Self) return;

					Self->InFlightKickPuids.Remove(TargetPuid);

					if (ResultCode == EOS_EResult::EOS_Success)
					{
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::KickMember — Kicked '%s' from lobby"), *KickedMemberId);
						// The PendingKickedPuids entry stays: it suppresses the duplicate
						// engine participant-left (Kicked) notification, which removes it.
						Self->OnLobbyMemberLeft.Broadcast(KickedMemberId);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::KickMember — EOS_Lobby_KickMember for '%s' failed: %s"),
							*KickedMemberId, ANSI_TO_TCHAR(EOS_EResult_ToString(ResultCode)));
						// Remove the dedupe marker only if it is still OURS to remove. If it is
						// already gone, a participant-left (Kicked) was suppressed against this
						// kick (or the member rejoined / the lobby was torn down) — never
						// "un-consume" someone else's suppression.
						if (Self->PendingKickedPuids.Remove(TargetPuid) == 0)
						{
							UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Dedupe marker for '%s' was already consumed while the failed kick was in flight"), *KickedMemberId);
						}
					}
				});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::KickMember — Kicking '%s'..."), *MemberId);
	return true;
}

bool UEEOSLobbySubsystem::PromoteMember(const FString& MemberId)
{
	// The old "LOBBY_OWNER" session-attribute write was fiction — ownership transfer requires
	// the raw SDK: EOS_Lobby_PromoteMember (owner-only per SDK docs). The engine consumes the
	// resulting EOS_LMS_PROMOTED notification internally and re-points the named session's
	// OwningUserId, so GetLobbyOwner()/IsLobbyOwner() stay consistent everywhere.
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("PromoteMember"));
		return false;
	}

	if (!bInLobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Not in a lobby"));
		return false;
	}

	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — Only the lobby owner can promote members (EOS_Lobby_PromoteMember is owner-only)"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HLobby LobbyHandle = PlatformHandle ? EOS_Platform_GetLobbyInterface(PlatformHandle) : nullptr;
	if (!LobbyHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — EOS Lobby interface not available"));
		return false;
	}

	if (CurrentLobbyId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — No lobby id cached"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	FUniqueNetIdPtr LocalUserId = IdentityInterface.IsValid() ? IdentityInterface->GetUniquePlayerId(0) : nullptr;
	const FString LocalPUIDStr = LocalUserId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString()) : FString();
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — Local user has no Product User ID (no Connect session?)"));
		return false;
	}

	const FString TargetPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(MemberId);
	if (TargetPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — Target '%s' has no Product User ID"), *MemberId);
		return false;
	}

	const FTCHARToUTF8 Utf8LobbyId(*CurrentLobbyId);

	EOS_Lobby_PromoteMemberOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_PROMOTEMEMBER_API_LATEST;
	Options.LobbyId = (EOS_LobbyId)Utf8LobbyId.Get();
	Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));
	Options.TargetUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetPUIDStr));

	struct FPromoteContext
	{
		TWeakObjectPtr<UEEOSLobbySubsystem> Self;
		FString MemberId;
	};
	FPromoteContext* Context = new FPromoteContext{ this, MemberId };

	EOS_Lobby_PromoteMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_PromoteMemberCallbackInfo* Data)
		{
			TUniquePtr<FPromoteContext> Ctx(static_cast<FPromoteContext*>(Data->ClientData));
			if (!Ctx) return;

			AsyncTask(ENamedThreads::GameThread,
				[WeakSelf = Ctx->Self, PromotedMemberId = MoveTemp(Ctx->MemberId), ResultCode = Data->ResultCode]()
				{
					UEEOSLobbySubsystem* Self = WeakSelf.Get();
					if (!Self) return;

					if (ResultCode == EOS_EResult::EOS_Success)
					{
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::PromoteMember — Promoted '%s' to lobby owner"), *PromotedMemberId);
						Self->OnLobbyOwnerChanged.Broadcast(PromotedMemberId);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::PromoteMember — EOS_Lobby_PromoteMember for '%s' failed: %s"),
							*PromotedMemberId, ANSI_TO_TCHAR(EOS_EResult_ToString(ResultCode)));
					}
				});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::PromoteMember — Promoting '%s'..."), *MemberId);
	return true;
}

// ── Lobby Settings ───────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyJoinable(bool bIsPublic)
{
	// Routed through UpdateSession like the attribute setters, so it must share their single
	// in-flight update slot: the engine's update completion carries only the session name, and
	// a second concurrent update would be consumed as the first one's result.
	if (UpdateLobbyCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetLobbyJoinable — A lobby update is already in flight; rejecting joinability change"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SetLobbyJoinable"));
		return false;
	}

	// Joinability is a lobby-level setting: the engine applies it only for the owner (a
	// non-owner update silently publishes member settings only).
	if (!IsLobbyOwner())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetLobbyJoinable — Only the lobby owner can change joinability"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return false;

	FOnlineSessionSettings* Settings = SessionInterface->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::SetLobbyJoinable — No lobby session settings found"));
		return false;
	}

	Settings->bShouldAdvertise = bIsPublic;

	PendingUpdateKind = EPendingLobbyUpdate::Joinability;
	PendingAttributeKey.Empty();
	PendingAttributeValue = bIsPublic ? TEXT("Public") : TEXT("Private");
	UpdateLobbyCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete));

	SessionInterface->UpdateSession(LOBBY_SESSION_NAME, *Settings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::SetLobbyJoinable — Updating lobby joinability to %s..."), *PendingAttributeValue);
	return true;
}

bool UEEOSLobbySubsystem::InviteToLobby(const FString& UserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("InviteToLobby"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return false;

	// Must be constructed by the identity interface: the EOS OSS downcasts incoming ids to
	// FUniqueNetIdEOS, so a generic FUniqueNetIdString here is undefined behavior.
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return false;

	const FUniqueNetIdPtr InviteeId = IdentityInterface->CreateUniquePlayerId(UserId);
	if (!InviteeId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::InviteToLobby — Could not parse user id '%s'"), *UserId);
		return false;
	}
	SessionInterface->SendSessionInviteToFriend(0, LOBBY_SESSION_NAME, *InviteeId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::InviteToLobby — Invited '%s'"), *UserId);
	return true;
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
	// Interface-wide delegate: ignore completions for sessions that aren't our lobby
	// (e.g. the Sessions subsystem's game session) — without clearing our handle or broadcasting.
	if (InSessionName != LOBBY_SESSION_NAME) return;

	IOnlineSessionPtr SessionInterface;
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		SessionInterface = EOSSub->GetSessionInterface();
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
	}
	CreateLobbyCompleteHandle.Reset();

	bInLobby = bWasSuccessful;
	CurrentLobbyId.Empty();
	if (bWasSuccessful && SessionInterface.IsValid())
	{
		// Store the real backend id: for a lobby-backed session GetSessionIdStr() is the EOS
		// lobby id (the engine feeds this exact string to EOS_Lobby_* calls as EOS_LobbyId).
		if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(LOBBY_SESSION_NAME))
		{
			CurrentLobbyId = Session->GetSessionIdStr();
			RefreshCachedLobbyAttributes(Session->SessionSettings, /*bBroadcastChanges*/ false);
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Create succeeded but named lobby session not found; lobby id unavailable"));
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Create lobby %s (id '%s')"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), *CurrentLobbyId);
	OnLobbyCreated.Broadcast(bWasSuccessful, CurrentLobbyId);
}

void UEEOSLobbySubsystem::HandleDestroyThenCreateLobbyComplete(FName InSessionName, bool bWasSuccessful)
{
	// One-shot continuation of CreateLobby's destroy-then-create chain. Ignore destroys of
	// other sessions (e.g. the Sessions subsystem's game session) without clearing or broadcasting.
	if (InSessionName != LOBBY_SESSION_NAME) return;

	IOnlineSessionPtr SessionInterface;
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		SessionInterface = EOSSub->GetSessionInterface();
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateLobbyHandle);
	}
	DestroyForCreateLobbyHandle.Reset();

	if (!bWasSuccessful || !SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::CreateLobby — Failed to destroy the existing lobby before re-creating"));
		OnLobbyCreated.Broadcast(false, TEXT(""));
		return;
	}

	// The old lobby is gone — reset local lobby state before creating the new one.
	ResetLobbyState();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::CreateLobby — Old lobby destroyed, now creating the new lobby"));
	CreateLobbyCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete));
	SessionInterface->CreateSession(0, LOBBY_SESSION_NAME, PendingCreateLobbySettings);
}

void UEEOSLobbySubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	// This handler is only bound while OUR lobby search is in flight, and the search
	// coordinator guarantees no sibling subsystem (Sessions/Matchmaking) search overlaps
	// ours — so ANY trigger here is OUR search's terminal event. Do NOT gate on the search
	// object's SearchState: the engine's session zero-result path fires this delegate
	// without ever setting it (OnlineSessionEOS.cpp:2675-2679); an "InProgress means not
	// ours" check would wedge the handle (and the search slot) forever.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
		}
	}
	FindLobbiesCompleteHandle.Reset();
	ReleaseSearchSlot();

	TArray<FEEOSSessionSearchResult> Results;

	// Read results from OUR search object; the trigger's payload carries success. Empty
	// results with bWasSuccessful == true is a legitimate successful (empty) search.
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

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Found %d lobbies (search %s)"), Results.Num(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnLobbiesFound.Broadcast(Results);
}

void UEEOSLobbySubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	IOnlineSessionPtr SessionInterface;
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		SessionInterface = EOSSub->GetSessionInterface();
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
	}
	JoinLobbyCompleteHandle.Reset();

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	bInLobby = bSuccess;
	CurrentLobbyId.Empty();
	if (bSuccess && SessionInterface.IsValid())
	{
		// Real backend id (see HandleCreateSessionComplete) + initial attribute snapshot so
		// non-host members don't read empty attributes until the first remote update.
		if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(LOBBY_SESSION_NAME))
		{
			CurrentLobbyId = Session->GetSessionIdStr();
			RefreshCachedLobbyAttributes(Session->SessionSettings, /*bBroadcastChanges*/ false);
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join succeeded but named lobby session not found; lobby id unavailable"));
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join lobby %s (id '%s')"), bSuccess ? TEXT("succeeded") : TEXT("failed"), *CurrentLobbyId);
	OnLobbyJoined.Broadcast(bSuccess, CurrentLobbyId);
}

void UEEOSLobbySubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
		}
	}
	DestroyLobbyCompleteHandle.Reset();

	FString DestroyedId = CurrentLobbyId;
	if (bWasSuccessful)
	{
		DestroyedId = ResetLobbyState();
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Leave/destroy lobby %s"), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnLobbyDestroyed.Broadcast(bWasSuccessful, DestroyedId);
}

void UEEOSLobbySubsystem::HandleLifetimeSessionDestroyed(FName InSessionName, bool bWasSuccessful)
{
	// Subsystem-lifetime destroy listener: catches the engine tearing down the lobby session
	// when the owner destroyed it remotely (or the backend closed it). Our own operations are
	// consumed by their handle-scoped listeners, which take precedence.
	if (InSessionName != LOBBY_SESSION_NAME) return;

	if (DestroyLobbyCompleteHandle.IsValid() || DestroyForCreateLobbyHandle.IsValid())
	{
		return; // an own leave/destroy op is in flight — its handler owns this completion
	}

	if (!bInLobby || !bWasSuccessful)
	{
		return; // nothing to reset (already reset elsewhere), or the session isn't actually gone
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Lobby session destroyed outside any local operation (remote closure) — resetting lobby state"));
	const FString DestroyedId = ResetLobbyState();
	OnLobbyDestroyed.Broadcast(true, DestroyedId);
}

void UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	// Interface-wide delegate: ignore update completions for other named sessions. Note the
	// payload carries only the session name, so an engine-internal lobby update completing
	// while ours is in flight is indistinguishable from ours (documented Phase 2 residual).
	if (InSessionName != LOBBY_SESSION_NAME) return;

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteHandle);
		}
	}
	UpdateLobbyCompleteHandle.Reset();

	const EPendingLobbyUpdate CompletedKind = PendingUpdateKind;
	const FString Key = PendingAttributeKey;
	const FString Value = PendingAttributeValue;
	PendingUpdateKind = EPendingLobbyUpdate::None;
	PendingAttributeKey.Empty();
	PendingAttributeValue.Empty();

	switch (CompletedKind)
	{
	case EPendingLobbyUpdate::LobbyAttribute:
		if (bWasSuccessful)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Set attribute '%s' = '%s'"), *Key, *Value);
			// Cache + broadcast only on a real transition: the lobby-update notification
			// (HandleSessionSettingsUpdated) may have already delivered this change —
			// whichever observes the transition first broadcasts, the other stays silent.
			const FString* Existing = CachedLobbyAttributes.Find(Key);
			if (!Existing || *Existing != Value)
			{
				CachedLobbyAttributes.Add(Key, Value);
				OnLobbyAttributeChanged.Broadcast(Key, Value);
			}
		}
		else
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetLobbyAttribute — Update for '%s' failed; attribute NOT synced"), *Key);
		}
		break;

	case EPendingLobbyUpdate::MemberAttribute:
		if (bWasSuccessful)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Set member attribute '%s' = '%s'"), *Key, *Value);
		}
		else
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetMemberAttribute — Update for '%s' failed; attribute NOT synced"), *Key);
		}
		break;

	case EPendingLobbyUpdate::Joinability:
		if (bWasSuccessful)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Lobby joinability set to %s"), *Value);
		}
		else
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLobbySubsystem::SetLobbyJoinable — Update to %s failed; joinability NOT changed"), *Value);
		}
		break;

	default:
		break;
	}
}

void UEEOSLobbySubsystem::HandleSessionParticipantJoined(FName InSessionName, const FUniqueNetId& UniqueId)
{
	// Subsystem-lifetime notification; the engine raises it for every named session.
	if (InSessionName != LOBBY_SESSION_NAME) return;

	const FString MemberIdStr = UniqueId.ToString();

	// A previously-kicked member rejoining invalidates any stale kick-suppression entry
	// (e.g. when the engine never delivered the participant-left for our own kick).
	const FString MemberPuid = UEEOSBlueprintLibrary::ExtractProductUserId(MemberIdStr);
	if (!MemberPuid.IsEmpty())
	{
		PendingKickedPuids.Remove(MemberPuid);
	}

	OnLobbyMemberJoined.Broadcast(MemberIdStr);
}

void UEEOSLobbySubsystem::HandleSessionParticipantLeft(FName InSessionName, const FUniqueNetId& UniqueId, EOnSessionParticipantLeftReason Reason)
{
	if (InSessionName != LOBBY_SESSION_NAME) return;

	const FString MemberIdStr = UniqueId.ToString();
	const FString MemberPuid = UEEOSBlueprintLibrary::ExtractProductUserId(MemberIdStr);

	// The LOCAL player leaving involuntarily (kicked, or the lobby closed under us) never
	// runs our own leave/destroy path, so the lobby state must be reset here or bInLobby
	// wedges true forever. Skipped while an own leave/destroy op is in flight — that op's
	// completion owns the state transition.
	if (bInLobby && !DestroyLobbyCompleteHandle.IsValid() && !DestroyForCreateLobbyHandle.IsValid())
	{
		bool bIsLocalPlayer = false;
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
			FUniqueNetIdPtr LocalId = IdentityInterface.IsValid() ? IdentityInterface->GetUniquePlayerId(0) : nullptr;
			const FString LocalPuid = LocalId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalId->ToString()) : FString();
			bIsLocalPlayer = !LocalPuid.IsEmpty() && LocalPuid == MemberPuid;
		}

		if (bIsLocalPlayer)
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: LOCAL player removed from lobby (%s) — resetting lobby state"),
				Reason == EOnSessionParticipantLeftReason::Kicked ? TEXT("kicked") : TEXT("left/closed"));
			const FString DestroyedId = ResetLobbyState();
			OnLobbyMemberLeft.Broadcast(MemberIdStr);
			OnLobbyDestroyed.Broadcast(true, DestroyedId);
			return;
		}
	}

	// Departures caused by our own EOS_Lobby_KickMember are broadcast from its SDK completion;
	// suppress ONLY the engine's Kicked-reason duplicate (order-independent — whichever of the
	// two fires second consumes the pending entry). A voluntary leave racing a kick keeps its
	// broadcast: only a Kicked reason may consume the suppression marker.
	if (Reason == EOnSessionParticipantLeftReason::Kicked &&
		!MemberPuid.IsEmpty() && PendingKickedPuids.Remove(MemberPuid) > 0)
	{
		return;
	}

	OnLobbyMemberLeft.Broadcast(MemberIdStr);
}

void UEEOSLobbySubsystem::HandleSessionSettingsUpdated(FName InSessionName, const FOnlineSessionSettings& UpdatedSettings)
{
	// The EOS OSS raises this after refreshing the named session from a remote lobby update
	// (OnLobbyUpdateReceived → CopyLobbyData), so non-host members see owner-side attribute
	// changes here.
	if (InSessionName != LOBBY_SESSION_NAME) return;

	RefreshCachedLobbyAttributes(UpdatedSettings, /*bBroadcastChanges*/ true);
}

void UEEOSLobbySubsystem::RefreshCachedLobbyAttributes(const FOnlineSessionSettings& InSettings, bool bBroadcastChanges)
{
	TMap<FString, FString> NewAttributes;
	for (FSessionSettings::TConstIterator It(InSettings.Settings); It; ++It)
	{
		NewAttributes.Add(It.Key().ToString(), It.Value().Data.ToString());
	}

	// Collect transitions before swapping the cache so broadcast listeners reading
	// GetLobbyAttribute() during the broadcast see the new values.
	TArray<TPair<FString, FString>> ChangedAttributes;
	if (bBroadcastChanges)
	{
		for (const auto& Pair : NewAttributes)
		{
			const FString* OldValue = CachedLobbyAttributes.Find(Pair.Key);
			if (!OldValue || *OldValue != Pair.Value)
			{
				ChangedAttributes.Emplace(Pair.Key, Pair.Value);
			}
		}
	}

	CachedLobbyAttributes = MoveTemp(NewAttributes);

	for (const TPair<FString, FString>& Changed : ChangedAttributes)
	{
		OnLobbyAttributeChanged.Broadcast(Changed.Key, Changed.Value);
	}
}

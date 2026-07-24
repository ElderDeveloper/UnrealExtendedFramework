// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSessionSubsystem.h"
#include "EEOSSearchCoordinator.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "UnrealExtendedEOS.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

/** Owner tag this subsystem uses with the shared UEEOSSearchCoordinator. */
static const FName SessionsSearchOwner(TEXT("EEOSSessionSubsystem"));

void UEEOSSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Listen for session invite acceptance. The OSS may not be loaded yet (the Shared base
	// documents that its lookup retries for exactly this reason) — if registration fails,
	// retry on a 1 Hz ticker instead of silently never registering, which would leave
	// invite notifications dead for the whole session.
	if (!TryRegisterLifetimeNotifications())
	{
		NotificationRetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UEEOSSessionSubsystem::TickRetryRegisterNotifications), 1.0f);
	}
}

void UEEOSSessionSubsystem::Deinitialize()
{
	if (NotificationRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NotificationRetryTickerHandle);
		NotificationRetryTickerHandle.Reset();
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			if (SessionInviteAcceptedHandle.IsValid())	SessionInterface->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedHandle);

			// Clear any still-pending per-operation handles so late completions can't
			// reach a dead subsystem.
			if (CreateSessionCompleteHandle.IsValid())	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
			if (DestroyForCreateHandle.IsValid())		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateHandle);
			if (FindSessionsCompleteHandle.IsValid())	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
			if (JoinSessionCompleteHandle.IsValid())	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
			if (DestroySessionCompleteHandle.IsValid())	SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
			if (StartSessionCompleteHandle.IsValid())	SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
			if (EndSessionCompleteHandle.IsValid())		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
		}
	}

	SessionInviteAcceptedHandle.Reset();
	CreateSessionCompleteHandle.Reset();
	DestroyForCreateHandle.Reset();
	FindSessionsCompleteHandle.Reset();
	JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset();
	StartSessionCompleteHandle.Reset();
	EndSessionCompleteHandle.Reset();

	// If a search of ours was still in flight, free the cross-subsystem search slot.
	ReleaseSearchSlot();

	PendingCreateSessionName = NAME_None;
	PendingJoinSessionName = NAME_None;
	PendingDestroySessionName = NAME_None;
	PendingStartSessionName = NAME_None;
	PendingEndSessionName = NAME_None;

	CachedSearchResults.Empty();
	SessionSearch.Reset();
	Super::Deinitialize();
}

// ── Lifetime notifications ───────────────────────────────────────────────────

bool UEEOSSessionSubsystem::TryRegisterLifetimeNotifications()
{
	if (SessionInviteAcceptedHandle.IsValid())
	{
		return true;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInviteAcceptedHandle = SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(
				FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleSessionInviteAccepted));
			UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSSessionSubsystem — Lifetime session notifications registered"));
			return true;
		}
	}
	return false;
}

bool UEEOSSessionSubsystem::TickRetryRegisterNotifications(float /*DeltaTime*/)
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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem — EOS unavailable for this session; stopping notification registration retries."));
		NotificationRetryTickerHandle.Reset();
		return false;
	}
	return true;
}

// ── Search coordination ──────────────────────────────────────────────────────

UEEOSSearchCoordinator* UEEOSSessionSubsystem::GetSearchCoordinator() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UEEOSSearchCoordinator>() : nullptr;
}

bool UEEOSSessionSubsystem::TryAcquireSearchSlot()
{
	UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator();
	// No coordinator only happens during GameInstance teardown — nothing else can be
	// searching then, so proceed rather than deadlock.
	return Coordinator ? Coordinator->TryAcquire(SessionsSearchOwner) : true;
}

void UEEOSSessionSubsystem::ReleaseSearchSlot()
{
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator())
	{
		if (Coordinator->GetCurrentOwner() == SessionsSearchOwner)
		{
			Coordinator->Release(SessionsSearchOwner);
		}
	}
}

/** Stable, portable string for the advertised "REGION" session attribute (empty for NoSelection).
 *  Deliberately NOT UEnum::GetValueAsString — these values are wire data other builds filter on,
 *  so they must not change if the enum type is renamed or reflected differently. */
static FString UEEOSSessionSubsystem_RegionToString(EEOSRegionInfo Region)
{
	switch (Region)
	{
	case EEOSRegionInfo::Asia:          return TEXT("Asia");
	case EEOSRegionInfo::NorthAmerica:  return TEXT("NorthAmerica");
	case EEOSRegionInfo::SouthAmerica:  return TEXT("SouthAmerica");
	case EEOSRegionInfo::Africa:        return TEXT("Africa");
	case EEOSRegionInfo::Europe:        return TEXT("Europe");
	case EEOSRegionInfo::Australia:     return TEXT("Australia");
	case EEOSRegionInfo::NoSelection:
	default:                            return FString();
	}
}

static FOnlineSessionSettings UEEOSSessionSubsystem_BuildNativeSettings(const FEEOSSessionSettings& Settings)
{
	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = Settings.MaxPlayers;
	SessionSettings.NumPrivateConnections = Settings.NumPrivateConnections;
	SessionSettings.bIsDedicated = Settings.bIsDedicatedServer;
	SessionSettings.bIsLANMatch = Settings.bIsLANMatch;
	SessionSettings.bShouldAdvertise = Settings.bShouldAdvertise;
	SessionSettings.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
	SessionSettings.bUsesPresence = Settings.bUsesPresence;
	SessionSettings.bAllowJoinViaPresence = Settings.bAllowJoinViaPresence;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = Settings.bAllowJoinViaPresenceFriendsOnly;
	SessionSettings.bAllowInvites = Settings.bAllowInvites;
	SessionSettings.bUseLobbiesIfAvailable = Settings.bUseLobbiesIfAvailable;
	SessionSettings.bUseLobbiesVoiceChatIfAvailable = Settings.bUseLobbiesVoiceChatIfAvailable;
	SessionSettings.bUsesStats = Settings.bUsesStats;

	// Apply custom attributes (advertised for search filtering)
	for (const auto& Pair : Settings.CustomSettings)
	{
		SessionSettings.Set(FName(*Pair.Key), Pair.Value, EOnlineDataAdvertisementType::ViaOnlineService);
	}

	// Advertise the region as a custom "REGION" attribute — EOS sessions have no native
	// region field. Set after CustomSettings so the typed Region field wins over a manually
	// supplied "REGION" custom key. Searches are NOT filtered by region automatically;
	// callers opt in by passing "REGION" in FindSessionsFiltered's filter map.
	if (Settings.Region != EEOSRegionInfo::NoSelection)
	{
		SessionSettings.Set(FName(TEXT("REGION")), UEEOSSessionSubsystem_RegionToString(Settings.Region), EOnlineDataAdvertisementType::ViaOnlineService);
	}

	return SessionSettings;
}

bool UEEOSSessionSubsystem::CreateSession(int32 MaxPlayers, bool bIsLAN, bool bIsPresence, const FString& SessionName)
{
	// In-flight rejections come FIRST and never broadcast: the legitimate in-flight caller
	// is waiting on the same delegate, and a failure broadcast here would be misreported as
	// its completion.
	if (CreateSessionCompleteHandle.IsValid() || DestroyForCreateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSession — A create-session operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingCreateSessionName.ToString(), *SessionName);
		return false;
	}
	// Symmetric guard: a destroy in flight owns the named-session state a create would race
	// (the engine silently skips overlapping ops and the chain can recreate the session).
	if (DestroySessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSession — A destroy operation is in flight ('%s'); rejecting create of '%s' (no delegate will fire)"), *PendingDestroySessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateSession"));
		OnSessionCreated.Broadcast(false, SessionName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::CreateSession — Session interface not available"));
		OnSessionCreated.Broadcast(false, SessionName);
		return false;
	}

	PendingCreateSessionName = FName(*SessionName);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bIsLANMatch = bIsLAN;
	Settings.bShouldAdvertise = true;
	Settings.bUsesPresence = bIsPresence;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowJoinViaPresence = bIsPresence;
	PendingCreateSettings = MoveTemp(Settings);

	// Same existing-session handling as CreateSessionAdvanced: destroy first, then create
	// inside HandleDestroyThenCreateComplete (handle-scoped and name-filtered so an unrelated
	// destroy — e.g. the lobby's — can't trigger the create).
	if (SessionInterface->GetNamedSession(PendingCreateSessionName) != nullptr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSession — Session '%s' already exists, destroying first"), *SessionName);

		DestroyForCreateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleDestroyThenCreateComplete));

		SessionInterface->DestroySession(PendingCreateSessionName);
		return true;
	}

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete));

	SessionInterface->CreateSession(0, PendingCreateSessionName, PendingCreateSettings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::CreateSession — Creating session '%s' with %d max players"), *SessionName, MaxPlayers);
	return true;
}

bool UEEOSSessionSubsystem::CreateSessionAdvanced(const FEEOSSessionSettings& Settings, const FString& SessionName)
{
	// See CreateSession: in-flight rejections never broadcast.
	if (CreateSessionCompleteHandle.IsValid() || DestroyForCreateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — A create-session operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingCreateSessionName.ToString(), *SessionName);
		return false;
	}
	if (DestroySessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — A destroy operation is in flight ('%s'); rejecting create of '%s' (no delegate will fire)"), *PendingDestroySessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateSessionAdvanced"));
		OnSessionCreated.Broadcast(false, SessionName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Session interface not available"));
		OnSessionCreated.Broadcast(false, SessionName);
		return false;
	}

	PendingCreateSessionName = FName(*SessionName);
	PendingCreateSettings = UEEOSSessionSubsystem_BuildNativeSettings(Settings);

	// Destroy existing session first if one exists, then create in the completion handler
	auto ExistingSession = SessionInterface->GetNamedSession(PendingCreateSessionName);
	if (ExistingSession != nullptr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Session '%s' already exists, destroying first"), *SessionName);

		// Chain: destroy → create inside HandleDestroyThenCreateComplete. Handle-scoped and
		// name-filtered so an unrelated destroy (e.g. the lobby's) can't trigger the create.
		DestroyForCreateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleDestroyThenCreateComplete));

		SessionInterface->DestroySession(PendingCreateSessionName);
		return true;
	}

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete));

	SessionInterface->CreateSession(0, PendingCreateSessionName, PendingCreateSettings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::CreateSessionAdvanced — Creating advanced session '%s' with %d public + %d private slots"),
		*SessionName, Settings.MaxPlayers, Settings.NumPrivateConnections);
	return true;
}

bool UEEOSSessionSubsystem::FindSessions(int32 MaxResults)
{
	// In-flight rejection: never broadcast (the legitimate search's waiters listen on the
	// same OnSessionsFound and would consume an empty result as their completion).
	if (FindSessionsCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::FindSessions — A session search is already in flight; rejecting new search (no delegate will fire)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindSessions"));
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	// The engine cannot run concurrent searches (see UEEOSSearchCoordinator) — a sibling
	// subsystem's search in flight means ours must be rejected, with in-flight semantics
	// (no broadcast).
	if (!TryAcquireSearchSlot())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::FindSessions — Another session/lobby search is in flight; rejecting (no delegate will fire)"));
		return false;
	}

	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleFindSessionsComplete));

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxResults;
	SessionSearch->bIsLanQuery = false;
	// Deliberately NO query attributes here. The 5.8 EOS OSS forwards every QuerySettings key
	// (outside a small skip-list) verbatim as an EOS attribute filter, so a key like the old
	// "PRESENCESEARCH" — which no session ever advertises — made every plain search return
	// 0 results. The engine adds its own bucket-id and NumPublicConnections >= 1 filters.

	// A synchronous false return means the engine fires NO delegate at all (unique to the
	// find path) — clean up and fail here or the handle wedges forever. Broadcasting is safe:
	// we hold the coordinator slot, so no sibling search is in flight.
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::FindSessions — FindSessions failed to start (synchronous failure; no delegate will fire from the engine)"));
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		SessionSearch.Reset();
		ReleaseSearchSlot();
		CachedSearchResults.Empty();
		OnSessionsFound.Broadcast(CachedSearchResults);
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::FindSessions — Searching for sessions (max %d)..."), MaxResults);
	return true;
}

bool UEEOSSessionSubsystem::FindSessionsFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	// See FindSessions: in-flight rejections never broadcast.
	if (FindSessionsCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::FindSessionsFiltered — A session search is already in flight; rejecting new search (no delegate will fire)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("FindSessionsFiltered"));
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionsFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		return false;
	}

	if (!TryAcquireSearchSlot())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::FindSessionsFiltered — Another session/lobby search is in flight; rejecting (no delegate will fire)"));
		return false;
	}

	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleFindSessionsComplete));

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = MaxResults;
	SessionSearch->bIsLanQuery = false;

	// Apply custom search filters
	for (const auto& Filter : SearchFilters)
	{
		SessionSearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	}

	// See FindSessions: a synchronous false fires no engine delegate.
	if (!SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::FindSessionsFiltered — FindSessions failed to start (synchronous failure; no delegate will fire from the engine)"));
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
		SessionSearch.Reset();
		ReleaseSearchSlot();
		CachedSearchResults.Empty();
		OnSessionsFound.Broadcast(CachedSearchResults);
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::FindSessionsFiltered — Searching with %d filters (max %d results)..."), SearchFilters.Num(), MaxResults);
	return true;
}

bool UEEOSSessionSubsystem::JoinSession(int32 SearchResultIndex, const FString& SessionName)
{
	// In-flight rejection: never broadcast (would be misreported as the pending join's result).
	if (JoinSessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::JoinSession — A join operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingJoinSessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("JoinSession"));
		OnSessionJoined.Broadcast(false, SessionName);
		return false;
	}

	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::JoinSession — Invalid search result index: %d"), SearchResultIndex);
		OnSessionJoined.Broadcast(false, SessionName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionJoined.Broadcast(false, SessionName);
		return false;
	}

	PendingJoinSessionName = FName(*SessionName);
	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleJoinSessionComplete));

	// Engine-documented requirement (same class as the lobby join fix): bUsesPresence is
	// false by default in search results and must be set game-side before JoinSession, or
	// the joiner loses presence/invites. Game sessions are NOT lobbies, so force
	// bUseLobbiesIfAvailable off to keep the join on the sessions path. Modify a local copy
	// so the cached search results stay pristine.
	FOnlineSessionSearchResult SearchResultCopy = SessionSearch->SearchResults[SearchResultIndex];
	SearchResultCopy.Session.SessionSettings.bUsesPresence = true;
	SearchResultCopy.Session.SessionSettings.bUseLobbiesIfAvailable = false;

	SessionInterface->JoinSession(0, PendingJoinSessionName, SearchResultCopy);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::JoinSession — Joining session at index %d..."), SearchResultIndex);
	return true;
}

bool UEEOSSessionSubsystem::DestroySession(const FString& SessionName)
{
	// In-flight rejections: never broadcast. This includes the create chain — the engine
	// silently skips a destroy issued during a create, and the chain would then recreate
	// the session the caller thought was gone.
	if (DestroySessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::DestroySession — A destroy operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingDestroySessionName.ToString(), *SessionName);
		return false;
	}
	if (CreateSessionCompleteHandle.IsValid() || DestroyForCreateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::DestroySession — A create-session operation is in flight ('%s'); rejecting destroy of '%s' (no delegate will fire)"), *PendingCreateSessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DestroySession"));
		OnSessionDestroyed.Broadcast(false, SessionName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionDestroyed.Broadcast(false, SessionName);
		return false;
	}

	PendingDestroySessionName = FName(*SessionName);
	DestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleDestroySessionComplete));

	SessionInterface->DestroySession(PendingDestroySessionName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::DestroySession — Destroying session '%s'..."), *SessionName);
	return true;
}

bool UEEOSSessionSubsystem::StartSession(const FString& SessionName)
{
	// In-flight rejection: never broadcast.
	if (StartSessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::StartSession — A start operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingStartSessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("StartSession"));
		OnSessionStarted.Broadcast(false);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionStarted.Broadcast(false);
		return false;
	}

	PendingStartSessionName = FName(*SessionName);
	StartSessionCompleteHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleStartSessionComplete));

	SessionInterface->StartSession(PendingStartSessionName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::StartSession — Starting session '%s'..."), *SessionName);
	return true;
}

bool UEEOSSessionSubsystem::EndSession(const FString& SessionName)
{
	// In-flight rejection: never broadcast.
	if (EndSessionCompleteHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::EndSession — An end operation is already in flight ('%s'); rejecting '%s' (no delegate will fire)"), *PendingEndSessionName.ToString(), *SessionName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("EndSession"));
		OnSessionEnded.Broadcast(false, SessionName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OnSessionEnded.Broadcast(false, SessionName);
		return false;
	}

	PendingEndSessionName = FName(*SessionName);
	EndSessionCompleteHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(
		FOnEndSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleEndSessionComplete));

	SessionInterface->EndSession(PendingEndSessionName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::EndSession — Ending session '%s'"), *SessionName);
	return true;
}

bool UEEOSSessionSubsystem::RegisterPlayer(const FString& SessionName, const FString& PlayerId, bool bWasInvited)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RegisterPlayer"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return false;

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return false;

	FUniqueNetIdPtr UniqueId = IdentityInterface->CreateUniquePlayerId(PlayerId);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::RegisterPlayer — Could not parse player id '%s'"), *PlayerId);
		return false;
	}

	SessionInterface->RegisterPlayer(FName(*SessionName), *UniqueId, bWasInvited);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::RegisterPlayer — Registered '%s' in session '%s'"), *PlayerId, *SessionName);
	return true;
}

bool UEEOSSessionSubsystem::UnRegisterPlayer(const FString& SessionName, const FString& PlayerId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnRegisterPlayer"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return false;

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return false;

	FUniqueNetIdPtr UniqueId = IdentityInterface->CreateUniquePlayerId(PlayerId);
	if (!UniqueId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::UnRegisterPlayer — Could not parse player id '%s'"), *PlayerId);
		return false;
	}

	SessionInterface->UnregisterPlayer(FName(*SessionName), *UniqueId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::UnRegisterPlayer — Unregistered '%s' from session '%s'"), *PlayerId, *SessionName);
	return true;
}

bool UEEOSSessionSubsystem::ServerTravel(const UObject* WorldContextObject, const FString& MapPath)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ServerTravel — Invalid world context"));
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	const FString TravelURL = MapPath + TEXT("?listen");
	World->ServerTravel(TravelURL);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::ServerTravel — Travelling to '%s'"), *TravelURL);
	return true;
}

bool UEEOSSessionSubsystem::ClientTravel(const UObject* WorldContextObject, const FString& SessionName)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ClientTravel — Invalid world context"));
		return false;
	}

	FString ConnectionInfo = GetResolvedConnectString(SessionName);
	if (ConnectionInfo.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem::ClientTravel — Could not resolve connection string for session '%s'"), *SessionName);
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC)
	{
		return false;
	}

	PC->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::ClientTravel — Travelling to '%s'"), *ConnectionInfo);
	return true;
}

FString UEEOSSessionSubsystem::GetResolvedConnectString(const FString& SessionName) const
{
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return FString();

	FString ConnectionInfo;
	SessionInterface->GetResolvedConnectString(FName(*SessionName), ConnectionInfo);
	return ConnectionInfo;
}

TArray<FEEOSSessionSearchResult> UEEOSSessionSubsystem::GetSearchResults() const
{
	return CachedSearchResults;
}

bool UEEOSSessionSubsystem::IsInSession() const
{
	return bInSession;
}

EEOSSessionState UEEOSSessionSubsystem::GetSessionState(const FString& SessionName) const
{
	if (!IsEOSAvailable()) return EEOSSessionState::NoSession;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
	if (!SessionInterface.IsValid()) return EEOSSessionState::NoSession;

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(FName(*SessionName));
	if (!Session) return EEOSSessionState::NoSession;

	switch (Session->SessionState)
	{
	case EOnlineSessionState::Creating:		return EEOSSessionState::Creating;
	case EOnlineSessionState::Pending:		return EEOSSessionState::Pending;
	case EOnlineSessionState::Starting:		return EEOSSessionState::Starting;
	case EOnlineSessionState::InProgress:	return EEOSSessionState::InProgress;
	case EOnlineSessionState::Ending:		return EEOSSessionState::Ending;
	case EOnlineSessionState::Ended:		return EEOSSessionState::Ended;
	case EOnlineSessionState::Destroying:	return EEOSSessionState::Destroying;
	default:								return EEOSSessionState::NoSession;
	}
}

FString UEEOSSessionSubsystem::GenerateSessionCode(int32 CodeLength)
{
	CodeLength = FMath::Clamp(CodeLength, 1, 20);
	const FString Chars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString Code;
	Code.Reserve(CodeLength);
	for (int32 i = 0; i < CodeLength; ++i)
	{
		Code.AppendChar(Chars[FMath::RandRange(0, Chars.Len() - 1)]);
	}
	return Code;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSSessionSubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	// Interface-wide delegate: ignore completions that belong to another operation/subsystem
	// (e.g. the lobby's create) — without clearing our handle or broadcasting.
	if (InSessionName != PendingCreateSessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		}
	}
	CreateSessionCompleteHandle.Reset();
	PendingCreateSessionName = NAME_None;

	bInSession = bWasSuccessful;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Create session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionCreated.Broadcast(bWasSuccessful, InSessionName.ToString());
}

void UEEOSSessionSubsystem::HandleDestroyThenCreateComplete(FName InSessionName, bool bWasSuccessful)
{
	// One-shot continuation of the destroy-then-create chain shared by CreateSession and
	// CreateSessionAdvanced (both stage their settings in PendingCreateSettings).
	// Ignore destroys of other sessions (e.g. the lobby's) without clearing or broadcasting.
	if (InSessionName != PendingCreateSessionName)
	{
		return;
	}

	IOnlineSessionPtr SessionInterface;
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		SessionInterface = EOSSub->GetSessionInterface();
	}
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateHandle);
	}
	DestroyForCreateHandle.Reset();

	if (!bWasSuccessful || !SessionInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSSessionSubsystem: Create-session chain — Failed to destroy existing session '%s'"), *InSessionName.ToString());
		PendingCreateSessionName = NAME_None;
		OnSessionCreated.Broadcast(false, InSessionName.ToString());
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Create-session chain — Old session destroyed, now creating '%s'"), *InSessionName.ToString());
	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete));
	SessionInterface->CreateSession(0, PendingCreateSessionName, PendingCreateSettings);
}

void UEEOSSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	// This handler is only bound while OUR search is in flight, and the search coordinator
	// guarantees no sibling subsystem (Lobby/Matchmaking) search overlaps ours — so ANY
	// trigger here is OUR search's terminal event. Do NOT gate on the search object's
	// SearchState: the engine's zero-result path fires this delegate without ever setting
	// it (OnlineSessionEOS.cpp:2675-2679) — an "InProgress means not ours" check would
	// ignore our own completion and wedge the handle (and the search slot) forever.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		}
	}
	FindSessionsCompleteHandle.Reset();
	ReleaseSearchSlot();

	CachedSearchResults.Empty();

	// Read results from OUR search object; the trigger's payload carries success. Empty
	// results with bWasSuccessful == true is a legitimate successful (empty) search.
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (const auto& SearchResult : SessionSearch->SearchResults)
		{
			FEEOSSessionSearchResult Result;
			Result.SessionId = SearchResult.GetSessionIdStr();
			Result.OwnerName = SearchResult.Session.OwningUserName;

			// 5.8 OnlineSessionEOS::AddSearchResult puts the open-slot count in
			// Session.NumOpenPrivateConnections and never writes NumOpenPublicConnections
			// (it stays 0), while SessionSettings.NumPublicConnections is restored from the
			// advertised "NumPublicConnections" attribute in CopyAttributes.
			const int32 MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			Result.MaxPlayers = MaxPlayers;
			Result.CurrentPlayers = FMath::Clamp(MaxPlayers - SearchResult.Session.NumOpenPrivateConnections, 0, MaxPlayers);

			Result.Ping = SearchResult.PingInMs;
			Result.bIsDedicatedServer = SearchResult.Session.SessionSettings.bIsDedicated;

			// Surface the advertised custom attributes (e.g. "REGION") as strings. Engine-known
			// keys (NumPublicConnections, bIsDedicated, ...) are folded into typed fields by
			// CopyAttributes and never appear in this map.
			for (const auto& SettingPair : SearchResult.Session.SessionSettings.Settings)
			{
				Result.Settings.Add(SettingPair.Key, SettingPair.Value.Data.ToString());
			}

			CachedSearchResults.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Found %d sessions (search %s)"), CachedSearchResults.Num(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionsFound.Broadcast(CachedSearchResults);
}

void UEEOSSessionSubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (InSessionName != PendingJoinSessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		}
	}
	JoinSessionCompleteHandle.Reset();
	PendingJoinSessionName = NAME_None;

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	bInSession = bSuccess;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Join session '%s' %s"), *InSessionName.ToString(), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	OnSessionJoined.Broadcast(bSuccess, InSessionName.ToString());
}

void UEEOSSessionSubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != PendingDestroySessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		}
	}
	DestroySessionCompleteHandle.Reset();
	PendingDestroySessionName = NAME_None;

	if (bWasSuccessful) bInSession = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Destroy session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionDestroyed.Broadcast(bWasSuccessful, InSessionName.ToString());
}

void UEEOSSessionSubsystem::HandleStartSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != PendingStartSessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
		}
	}
	StartSessionCompleteHandle.Reset();
	PendingStartSessionName = NAME_None;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Start session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionStarted.Broadcast(bWasSuccessful);
}

void UEEOSSessionSubsystem::HandleEndSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != PendingEndSessionName)
	{
		return;
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
		}
	}
	EndSessionCompleteHandle.Reset();
	PendingEndSessionName = NAME_None;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: End session '%s' %s"), *InSessionName.ToString(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnSessionEnded.Broadcast(bWasSuccessful, InSessionName.ToString());
}

void UEEOSSessionSubsystem::HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	FString SessionId = InviteResult.GetSessionIdStr();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Session invite accepted for session '%s' — %s"), *SessionId, bWasSuccessful ? TEXT("success") : TEXT("failed"));
	OnSessionInviteAccepted.Broadcast(bWasSuccessful, SessionId);
}

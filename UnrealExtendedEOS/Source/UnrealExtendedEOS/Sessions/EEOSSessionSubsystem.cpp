// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSessionSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "EEOSCapacity.h"
#include "eos_sessions_types.h"
#include "EEOSRetiredSessionOperation.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "eos_lobby_types.h"
#include "EEOSSearchCoordinator.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

/** Owner tag this subsystem uses with the shared UEEOSSearchCoordinator. */
static const FName SessionsSearchOwner(TEXT("EEOSSessionSubsystem"));

void UEEOSSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MembershipIdentityGeneration = CaptureEOSContext().Generation;
	MembershipWatcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSSessionSubsystem::TickMembership), 0.5f);

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
	BeginEOSShutdown(); bShuttingDown = true;
	if (MembershipWatcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(MembershipWatcher);
	MembershipWatcher.Reset();
	if (NotificationSessions.IsValid()) NotificationSessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedHandle);
	NotificationSessions.Reset();
	if (SessionLease.IsValid())
	{
		using Kind = FEEOSRetiredSessionOperation::EKind;
		const Kind PendingKind = CreateSessionCompleteHandle.IsValid() ? Kind::Create : JoinSessionCompleteHandle.IsValid() ? Kind::Join
			: StartSessionCompleteHandle.IsValid() ? Kind::Start : EndSessionCompleteHandle.IsValid() ? Kind::End : Kind::Destroy;
		const FName Name = CreateSessionCompleteHandle.IsValid() || DestroyForCreateHandle.IsValid() ? PendingCreateSessionName
			: JoinSessionCompleteHandle.IsValid() ? PendingJoinSessionName : StartSessionCompleteHandle.IsValid() ? PendingStartSessionName
			: EndSessionCompleteHandle.IsValid() ? PendingEndSessionName : PendingDestroySessionName;
		FEEOSRetiredSessionOperation::Hold(OperationSessions, SessionLease, GetOwningEOSInstanceName(), Name, PendingKind, true);
	}
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator()) Coordinator->Retire(SessionsSearchOwner);
	if (NotificationRetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(NotificationRetryTickerHandle);
		NotificationRetryTickerHandle.Reset();
	}


	SessionInviteAcceptedHandle.Reset();

	// If a search of ours was still in flight, free the cross-subsystem search slot.
	ReleaseSearchSlot();

	PendingCreateSessionName = NAME_None;
	PendingJoinSessionName = NAME_None;
	PendingDestroySessionName = NAME_None;
	PendingStartSessionName = NAME_None;
	PendingEndSessionName = NAME_None;

	CachedSearchResults.Empty();
	SessionSearch.Reset();
	if (OperationSessions.IsValid())
	{
		OperationSessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		OperationSessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateHandle);
		OperationSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OperationSessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		OperationSessions->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
		OperationSessions->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
	}
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	CreateSessionCompleteHandle.Reset();
	DestroyForCreateHandle.Reset();
	FindSessionsCompleteHandle.Reset();
	JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset();
	StartSessionCompleteHandle.Reset();
	EndSessionCompleteHandle.Reset();
	SessionLease.Reset(); OperationSessions.Reset(); SearchSessions.Reset(); AcceptedInvite = FOnlineSessionSearchResult(); AcceptedInviteDescriptor = FEEOSSessionInvite(); MembershipNames.Empty();
	Super::Deinitialize();
}

// ── Lifetime notifications ───────────────────────────────────────────────────

bool UEEOSSessionSubsystem::TryRegisterLifetimeNotifications()
{
	if (SessionInviteAcceptedHandle.IsValid())
	{
		IOnlineSubsystem* Existing = GetExistingEOSOnlineSubsystem();
		if (Existing && Existing->GetSessionInterface() == NotificationSessions) return true;
		if (NotificationSessions.IsValid()) NotificationSessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(SessionInviteAcceptedHandle);
		SessionInviteAcceptedHandle.Reset(); NotificationSessions.Reset();
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			NotificationSessions = SessionInterface;
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
	if (IsOwningEOSCreationExhausted())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem — EOS unavailable for this session; stopping notification registration retries."));
		NotificationRetryTickerHandle.Reset();
		return false;
	}
	return true;
}

bool UEEOSSessionSubsystem::UseLobbiesByDefault() const
{
	const UEEOSSettings* Settings = GetEOSSettings();
	return Settings ? Settings->bUseLobbiesByDefault : false;
}

/** Query key the EOS OSS reads to search the lobby backend instead of the sessions backend —
 *  the same literal UEEOSLobbySubsystem::FindLobbiesFiltered uses. */
static const FName GEEOSLobbySearchKey(TEXT("LOBBYSEARCH"));

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
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	return Coordinator && OSS && Coordinator->TryAcquire(SessionsSearchOwner, OSS->GetSessionInterface(), this, TEXT("FindSessions"));
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
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxPlayers; Settings.bIsLANMatch = bIsLAN;
	Settings.bShouldAdvertise = !GetEOSSettings() || GetEOSSettings()->bPublicSessionsByDefault;
	Settings.bUsesPresence = bIsPresence; Settings.bAllowJoinViaPresence = bIsPresence;
	Settings.bAllowJoinInProgress = true; Settings.bUseLobbiesIfAvailable = !bIsLAN && UseLobbiesByDefault();
	if (Settings.bUseLobbiesIfAvailable) Settings.Set(SETTING_HOST_MIGRATION, GetEOSSettings()->bAllowLobbyHostMigration, EOnlineDataAdvertisementType::DontAdvertise);
	return BeginCreateSession(Settings, SessionName);
}

bool UEEOSSessionSubsystem::CreateSessionAdvanced(const FEEOSSessionSettings& Settings, const FString& SessionName)
{
	FOnlineSessionSettings Native = UEEOSSessionSubsystem_BuildNativeSettings(Settings);
	if (Native.bUseLobbiesIfAvailable) Native.Set(SETTING_HOST_MIGRATION,
		Settings.bOverrideLobbyHostMigration ? Settings.bAllowLobbyHostMigration : GetEOSSettings()->bAllowLobbyHostMigration, EOnlineDataAdvertisementType::DontAdvertise);
	return BeginCreateSession(Native, SessionName);
}

bool UEEOSSessionSubsystem::FindSessions(int32 MaxResults)
{
	return FindSessionsFiltered(MaxResults, TMap<FString, FString>());
}

bool UEEOSSessionSubsystem::FindSessionsFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	return FindSessionsForBackend(EEOSSessionBackend::Unknown, MaxResults, SearchFilters);
}

bool UEEOSSessionSubsystem::FindSessionsForBackend(EEOSSessionBackend Backend, int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	if (bShuttingDown || FindSessionsCompleteHandle.IsValid())
	{
		RejectOperation(TEXT("FindSessions"), EEOSOperationCode::Busy, TEXT("Session search is already pending or shutting down.")); return false;
	}
	const auto Fail = [this](EEOSOperationCode Code, const FString& Message)
	{
		CachedSearchResults.Empty(); SessionSearch.Reset();
		const auto Outcome = CompleteOperation(TEXT("FindSessions"), false, Code, Message);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnSessionsFound.Broadcast(CachedSearchResults); OnOperationCompleted.Broadcast(Outcome); return false;
	};
	if (MaxResults < 1 || MaxResults > EOS_LOBBY_MAX_SEARCH_RESULTS)
		return Fail(EEOSOperationCode::InvalidInput, TEXT("Search limit is outside the SDK range."));
	for (const auto& Filter : SearchFilters)
		if (Filter.Key.IsEmpty() || FName(*Filter.Key) == FName(TEXT("LOBBYSEARCH")) || FName(*Filter.Key) == FName(TEXT("PRESENCESEARCH")) || FTCHARToUTF8(*Filter.Key).Length() > EOS_LOBBYMODIFICATION_MAX_ATTRIBUTE_LENGTH)
			return Fail(EEOSOperationCode::InvalidInput, TEXT("Invalid search attribute name."));
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	if (!Sessions.IsValid()) return Fail(EEOSOperationCode::UnsupportedCapability, TEXT("EOS session interface is unavailable."));
	if (!TryAcquireSearchSlot())
	{
		RejectOperation(TEXT("FindSessions"), EEOSOperationCode::Busy, TEXT("Another native search is pending.")); return false;
	}
	SearchGeneration = BeginOperation(TEXT("FindSessions"), FString(), GetSearchCoordinator()->GetRequestId()); const int64 Token = SearchGeneration;
	SearchSessions = Sessions; SearchContext = CaptureEOSContext(); SearchBackend = Backend == EEOSSessionBackend::Unknown
		? (UseLobbiesByDefault() ? EEOSSessionBackend::Lobby : EEOSSessionBackend::Session) : Backend;
	SessionSearch = MakeShared<FOnlineSessionSearch>(); SessionSearch->MaxSearchResults = MaxResults;
	SessionSearch->bIsLanQuery = SearchBackend == EEOSSessionBackend::LAN;
	if (SearchBackend == EEOSSessionBackend::Lobby) SessionSearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
	for (const auto& Filter : SearchFilters) SessionSearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleFindSessionsComplete));
	const auto Submitted = SessionSearch.ToSharedRef();
	const bool bStarted = Sessions->FindSessions(0, Submitted);
	if (!bStarted && FindSessionsCompleteHandle.IsValid() && SearchGeneration == Token)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset(); SearchSessions.Reset(); ReleaseSearchSlot();
		return Fail(EEOSOperationCode::NativeStartRejected, TEXT("Native search refused submission."));
	}
	return bStarted;
}

bool UEEOSSessionSubsystem::JoinSession(int32 SearchResultIndex, const FString& SessionName)
{
	if (bShuttingDown || SessionLease.IsValid())
	{
		RejectOperation(TEXT("JoinSession"), EEOSOperationCode::Busy, TEXT("A named session operation is pending."), SessionName); return false;
	}
	if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		FinishSessionOperation(TEXT("JoinSession"), FName(*SessionName), false, EEOSOperationCode::InvalidTarget, TEXT("Invalid search result index.")); return false;
	}
	FOnlineSessionSearchResult Target = SessionSearch->SearchResults[SearchResultIndex];
	Target.Session.SessionSettings.bUseLobbiesIfAvailable = SearchBackend == EEOSSessionBackend::Lobby;
	if (SearchBackend == EEOSSessionBackend::Lobby) Target.Session.SessionSettings.bUsesPresence = true;
	return BeginJoinSession(Target, SessionName);
}

FEEOSSessionInvite UEEOSSessionSubsystem::GetAcceptedInvite() const
{
	auto Invite = AcceptedInviteDescriptor;
	if (Invite.RequestId) Invite.Target.SnapshotAgeSeconds = (FDateTime::UtcNow() - Invite.ReceivedAtUtc).GetTotalSeconds();
	return Invite;
}

bool UEEOSSessionSubsystem::JoinAcceptedInvite(const FString& SessionName)
{
	if (bShuttingDown || SessionLease.IsValid())
	{
		RejectOperation(TEXT("JoinSession"), EEOSOperationCode::Busy, TEXT("A named session operation is pending."), SessionName); return false;
	}
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (!AcceptedInviteDescriptor.bValid || !Local.IsValid() || !Local->IsValid()
		|| Local->ToString() != AcceptedInviteDescriptor.RecipientId)
	{
		FinishSessionOperation(TEXT("JoinSession"), FName(*SessionName), false, EEOSOperationCode::StaleResult, TEXT("No valid invite for the current local identity is retained.")); return false;
	}
	return BeginJoinSession(AcceptedInvite, SessionName);
}

bool UEEOSSessionSubsystem::JoinSessionResult(const FOnlineSessionSearchResult& Result, const FString& SessionName)
{
	return BeginJoinSession(Result, SessionName);
}

bool UEEOSSessionSubsystem::DestroySession(const FString& SessionName)
{
	const FName Name(*SessionName);
	if (!AdmitSessionOperation(TEXT("DestroySession"), Name)) return false;
	const auto Sessions = OperationSessions; const int64 Token = SessionLease.GetRequestId();
	const auto* Native = Sessions->GetNamedSession(Name);
	if (!Native || Native->SessionState == EOnlineSessionState::Creating || Native->SessionState == EOnlineSessionState::Destroying)
	{
		FinishSessionOperation(TEXT("DestroySession"), Name, false, EEOSOperationCode::InvalidTarget, TEXT("Named session is absent or transitioning.")); return false;
	}
	PendingDestroySessionName = Name;
	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleDestroySessionComplete));
	const bool bStarted = Sessions->DestroySession(Name);
	if (!bStarted && SessionLease.GetRequestId() == Token)
		FinishSessionOperation(TEXT("DestroySession"), Name, false, EEOSOperationCode::NativeStartRejected, TEXT("Native destroy refused submission."));
	return bStarted;
}

bool UEEOSSessionSubsystem::StartSession(const FString& SessionName)
{
	const FName Name(*SessionName);
	if (!AdmitSessionOperation(TEXT("StartSession"), Name)) return false;
	const auto Sessions = OperationSessions; const int64 Token = SessionLease.GetRequestId();
	const auto* Native = Sessions->GetNamedSession(Name);
	if (!Native || Native->SessionState == EOnlineSessionState::Creating || Native->SessionState == EOnlineSessionState::Destroying)
	{
		FinishSessionOperation(TEXT("StartSession"), Name, false, EEOSOperationCode::InvalidTarget, TEXT("Named session is absent or transitioning.")); return false;
	}
	PendingStartSessionName = Name;
	StartSessionCompleteHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleStartSessionComplete));
	const bool bStarted = Sessions->StartSession(Name);
	if (!bStarted && SessionLease.GetRequestId() == Token)
		FinishSessionOperation(TEXT("StartSession"), Name, false, EEOSOperationCode::NativeStartRejected, TEXT("Native start refused submission."));
	return bStarted;
}

bool UEEOSSessionSubsystem::EndSession(const FString& SessionName)
{
	const FName Name(*SessionName);
	if (!AdmitSessionOperation(TEXT("EndSession"), Name)) return false;
	const auto Sessions = OperationSessions; const int64 Token = SessionLease.GetRequestId();
	const auto* Native = Sessions->GetNamedSession(Name);
	if (!Native || Native->SessionState == EOnlineSessionState::Creating || Native->SessionState == EOnlineSessionState::Destroying)
	{
		FinishSessionOperation(TEXT("EndSession"), Name, false, EEOSOperationCode::InvalidTarget, TEXT("Named session is absent or transitioning.")); return false;
	}
	PendingEndSessionName = Name;
	EndSessionCompleteHandle = Sessions->AddOnEndSessionCompleteDelegate_Handle(
		FOnEndSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleEndSessionComplete));
	const bool bStarted = Sessions->EndSession(Name);
	if (!bStarted && SessionLease.GetRequestId() == Token)
		FinishSessionOperation(TEXT("EndSession"), Name, false, EEOSOperationCode::NativeStartRejected, TEXT("Native end refused submission."));
	return bStarted;
}

bool UEEOSSessionSubsystem::RegisterPlayer(const FString& SessionName, const FString& PlayerId, bool bWasInvited)
{
	if (bShuttingDown || FName(*SessionName) == FName(TEXT("EOS_Lobby")) || SessionLease.IsValid()) return false;
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
	if (!UniqueId.IsValid() || !UniqueId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::RegisterPlayer — Could not parse player id '%s'"), *FEEOSNativeOperationLease::SafeField(PlayerId));
		return false;
	}

	const bool bSubmitted = SessionInterface->RegisterPlayer(FName(*SessionName), *UniqueId, bWasInvited);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::RegisterPlayer — Player=%s Session=%s Submitted=%d"), *FEEOSNativeOperationLease::SafeField(PlayerId), *FEEOSNativeOperationLease::SafeField(SessionName), bSubmitted);
	return bSubmitted;
}

bool UEEOSSessionSubsystem::UnRegisterPlayer(const FString& SessionName, const FString& PlayerId)
{
	if (bShuttingDown || FName(*SessionName) == FName(TEXT("EOS_Lobby")) || SessionLease.IsValid()) return false;
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
	if (!UniqueId.IsValid() || !UniqueId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::UnRegisterPlayer — Could not parse player id '%s'"), *FEEOSNativeOperationLease::SafeField(PlayerId));
		return false;
	}

	const bool bSubmitted = SessionInterface->UnregisterPlayer(FName(*SessionName), *UniqueId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem::UnRegisterPlayer — Player=%s Session=%s Submitted=%d"), *FEEOSNativeOperationLease::SafeField(PlayerId), *FEEOSNativeOperationLease::SafeField(SessionName), bSubmitted);
	return bSubmitted;
}

bool UEEOSSessionSubsystem::ServerTravel(const UObject* WorldContextObject, const FString& MapPath)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (bShuttingDown || !World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_Client || MapPath.IsEmpty()) return false;
	const FString URL = MapPath.Contains(TEXT("?listen")) ? MapPath : MapPath + TEXT("?listen");
	const bool bStarted = World->ServerTravel(URL);
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionTravel Phase=ServerTravelRequested Accepted=%d"), bStarted);
	return bStarted;
}

bool UEEOSSessionSubsystem::ClientTravel(const UObject* WorldContextObject, const FString& SessionName)
{
	if (bShuttingDown || !WorldContextObject || !WorldContextObject->GetWorld() || WorldContextObject->GetWorld()->GetGameInstance() != GetGameInstance())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::ClientTravel — Invalid world context"));
		return false;
	}

	FString ConnectionInfo = GetResolvedConnectString(SessionName);
	if (ConnectionInfo.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSessionSubsystem::ClientTravel — Could not resolve connection string for session '%s'"), *FEEOSNativeOperationLease::SafeField(SessionName));
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC)
	{
		return false;
	}

	PC->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
		const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
		const auto* Native = Sessions.IsValid() ? Sessions->GetNamedSession(FName(*SessionName)) : nullptr;
		const auto Joined = GetLastOperationOutcome(TEXT("JoinSession"));
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionTravel ParentRequest=%lld Name=%s Backend=%s Driver=%s Destination=ResolvedNativeSession ClientTravelSubmitted=1 GameplayConnectionObserved=0"),
			Joined.RequestId, *FEEOSNativeOperationLease::SafeField(SessionName), Native ? Native->SessionSettings.bUseLobbiesIfAvailable ? TEXT("Lobby") : Native->SessionSettings.bIsLANMatch ? TEXT("LAN") : TEXT("Session") : TEXT("Unknown"),
			*GetNameSafe(WorldContextObject->GetWorld()->GetNetDriver()));
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
	auto Results = CachedSearchResults;
	for (auto& Result : Results) Result.SnapshotAgeSeconds = SearchCompletedAtSeconds > 0 ? FPlatformTime::Seconds() - SearchCompletedAtSeconds : -1;
	for (auto& Result : Results) Result.Capacity.SnapshotAgeSeconds = Result.SnapshotAgeSeconds;
	return Results;
}

bool UEEOSSessionSubsystem::IsInSession() const
{
	for (FName Name : MembershipNames) if (IsInNamedSession(Name.ToString())) return true;
	return false;
}

bool UEEOSSessionSubsystem::IsInNamedSession(const FString& SessionName) const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	const auto* Native = Sessions.IsValid() ? Sessions->GetNamedSession(FName(*SessionName)) : nullptr;
	if (!GetEOSReadiness().bNativeLoggedIn && !(Native && Native->SessionSettings.bIsLANMatch)) return false;
	return Native && Native->SessionInfo.IsValid() && Native->SessionInfo->IsValid()
		&& Native->SessionState != EOnlineSessionState::NoSession && Native->SessionState != EOnlineSessionState::Creating
		&& Native->SessionState != EOnlineSessionState::Destroying;
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
	if (!CreateSessionCompleteHandle.IsValid() || InSessionName != PendingCreateSessionName)
	{ LogCallbackDisposition(TEXT("HandleCreateSessionComplete"), SessionLease.GetRequestId(), InSessionName != PendingCreateSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleCreateSessionComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	FinishSessionOperation(TEXT("CreateSession"), InSessionName, bWasSuccessful,
		bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bWasSuccessful ? TEXT("Session create completed.") : TEXT("Native session create failed."),
		bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
}

void UEEOSSessionSubsystem::HandleDestroyThenCreateComplete(FName InSessionName, bool bWasSuccessful)
{
	if (!DestroyForCreateHandle.IsValid() || InSessionName != PendingCreateSessionName)
	{ LogCallbackDisposition(TEXT("HandleDestroyThenCreateComplete"), SessionLease.GetRequestId(), InSessionName != PendingCreateSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleDestroyThenCreateComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	const auto Sessions = OperationSessions;
	if (Sessions.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateHandle);
	DestroyForCreateHandle.Reset();
	if (bShuttingDown || !IsEOSContextCurrent(SessionOperationContext) || !bWasSuccessful || !Sessions.IsValid() || Sessions->GetNamedSession(InSessionName))
	{
		FinishSessionOperation(TEXT("CreateSession"), InSessionName, false, EEOSOperationCode::ExistingLobbyCloseFailed,
			TEXT("Existing session close failed or has not drained; creation was not started.")); return;
	}
	SubmitSessionCreation();
}

void UEEOSSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!FindSessionsCompleteHandle.IsValid()) { LogCallbackDisposition(TEXT("FindSessions"), SearchGeneration, TEXT("Duplicate"), SearchContext.Generation); return; }
	LogCallbackDisposition(TEXT("FindSessions"), SearchGeneration, bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SearchContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SearchContext.Generation);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	SearchSessions.Reset();
	FindSessionsCompleteHandle.Reset();
	ReleaseSearchSlot();

	CachedSearchResults.Empty();
	SearchCompletedAtSeconds = FPlatformTime::Seconds();

	// Read results from OUR search object; the trigger's payload carries success. Empty
	// results with bWasSuccessful == true is a legitimate successful (empty) search.
	const bool bNativeSuccess = bWasSuccessful;
	const bool bContextCurrent = IsEOSContextCurrent(SearchContext);
	bWasSuccessful = bWasSuccessful && bContextCurrent;
	const int32 NativeSeen = SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
	int32 InvalidTarget = 0, InvalidDetails = 0, OwnerUnknown = 0;
	if (SessionSearch.IsValid())
		SessionSearch->SearchResults.RemoveAll([&](const FOnlineSessionSearchResult& Native)
		{
			if (!Native.IsValid()) { ++InvalidTarget; return true; }
			if (!Native.Session.SessionInfo.IsValid() || !Native.Session.SessionInfo->IsValid()) { ++InvalidDetails; return true; }
			if (!Native.Session.OwningUserId.IsValid() || !Native.Session.OwningUserId->IsValid()) ++OwnerUnknown;
			return false;
		});
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (const auto& SearchResult : SessionSearch->SearchResults)
		{
			FEEOSSessionSearchResult Result = EEOSCapacity::Describe(SearchResult, SearchBackend, SearchGeneration);
			UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSSessionSearch Request=%lld Target=%s Backend=%d OwnerKnown=%d CapacityKnown=%d Max=%d Members=%d Slots=%d OpenPublic=%d OpenPrivate=%d Source=%s Advertise=%d Invites=%d PresenceJoin=%d JoinInProgress=%d"),
				SearchGeneration, *FEEOSNativeOperationLease::SafeField(Result.SessionId), int32(SearchBackend), Result.bOwnerKnown, Result.Capacity.bKnown, Result.MaxPlayers, Result.CurrentPlayers,
				Result.Capacity.AvailableSlots, Result.Capacity.RawOpenPublic, Result.Capacity.RawOpenPrivate, *FEEOSNativeOperationLease::SafeField(Result.Capacity.Source),
				Result.bShouldAdvertise, Result.bAllowInvites, Result.bAllowJoinViaPresence, Result.bAllowJoinInProgress);

			CachedSearchResults.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionSearch Request=%lld NativeSeen=%d ValidEmitted=%d DroppedInvalidTarget=%d DroppedInvalidDetails=%d OwnerUnknownObserved=%d HiddenNativeResultsKnown=0 ContextCurrent=%d"), SearchGeneration, NativeSeen, CachedSearchResults.Num(), InvalidTarget, InvalidDetails, OwnerUnknown, bContextCurrent);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSSessionSubsystem: Found %d sessions (search %s)"), CachedSearchResults.Num(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	const auto Outcome = CompleteOperation(TEXT("FindSessions"), bWasSuccessful, !bContextCurrent ? EEOSOperationCode::Canceled : bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		!bContextCurrent ? TEXT("Session search context retired before completion.") : bWasSuccessful ? TEXT("Session search succeeded.") : TEXT("Native session search failed."), FString(), bNativeSuccess ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	const auto Results = CachedSearchResults;
	if (!bShuttingDown) { OnSessionsFound.Broadcast(Results); OnOperationCompleted.Broadcast(Outcome); }
}

void UEEOSSessionSubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!JoinSessionCompleteHandle.IsValid() || InSessionName != PendingJoinSessionName)
	{ LogCallbackDisposition(TEXT("HandleJoinSessionComplete"), SessionLease.GetRequestId(), InSessionName != PendingJoinSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleJoinSessionComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	FinishSessionOperation(TEXT("JoinSession"), InSessionName, Result == EOnJoinSessionCompleteResult::Success,
		Result == EOnJoinSessionCompleteResult::Success ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		FString::Printf(TEXT("Native join returned %s; the SDK result is unavailable through this callback."), LexToString(Result)),
		LexToString(Result), EEOSResultSource::NativeCallback);
}

void UEEOSSessionSubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (!DestroySessionCompleteHandle.IsValid() || InSessionName != PendingDestroySessionName)
	{ LogCallbackDisposition(TEXT("HandleDestroySessionComplete"), SessionLease.GetRequestId(), InSessionName != PendingDestroySessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleDestroySessionComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	FinishSessionOperation(TEXT("DestroySession"), InSessionName, bWasSuccessful,
		bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bWasSuccessful ? TEXT("Session destroy completed.") : TEXT("Native session destroy failed."),
		bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
}

void UEEOSSessionSubsystem::HandleStartSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (!StartSessionCompleteHandle.IsValid() || InSessionName != PendingStartSessionName)
	{ LogCallbackDisposition(TEXT("HandleStartSessionComplete"), SessionLease.GetRequestId(), InSessionName != PendingStartSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleStartSessionComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	FinishSessionOperation(TEXT("StartSession"), InSessionName, bWasSuccessful,
		bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bWasSuccessful ? TEXT("Session start completed.") : TEXT("Native session start failed."),
		bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
}

void UEEOSSessionSubsystem::HandleEndSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (!EndSessionCompleteHandle.IsValid() || InSessionName != PendingEndSessionName)
	{ LogCallbackDisposition(TEXT("HandleEndSessionComplete"), SessionLease.GetRequestId(), InSessionName != PendingEndSessionName ? TEXT("DifferentOwner") : TEXT("Duplicate"), SessionOperationContext.Generation); return; }
	LogCallbackDisposition(TEXT("HandleEndSessionComplete"), SessionLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SessionOperationContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SessionOperationContext.Generation);
	FinishSessionOperation(TEXT("EndSession"), InSessionName, bWasSuccessful,
		bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bWasSuccessful ? TEXT("Session end completed.") : TEXT("Native session end failed."),
		bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
}

void UEEOSSessionSubsystem::HandleSessionInviteAccepted(const bool bWasSuccessful, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult)
{
	if (bShuttingDown || ControllerId != 0) return;
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (!OSS || OSS->GetSessionInterface() != NotificationSessions) { LogCallbackDisposition(TEXT("SessionInvite"), 0, TEXT("DifferentOwner")); return; }
	const bool bValid = GetEOSReadiness().bNativeLoggedIn && bWasSuccessful && InviteResult.IsValid() && UserId.IsValid() && UserId->IsValid()
		&& Local.IsValid() && Local->IsValid() && *Local == *UserId;
	AcceptedInvite = bValid ? InviteResult : FOnlineSessionSearchResult();
	AcceptedInviteDescriptor = FEEOSSessionInvite();
	AcceptedInviteDescriptor.RequestId = FEEOSNativeOperationLease::NextRequestId();
	AcceptedInviteDescriptor.bValid = bValid; AcceptedInviteDescriptor.LocalUserNum = ControllerId;
	AcceptedInviteDescriptor.RecipientId = UserId.IsValid() && UserId->IsValid() ? UserId->ToString() : FString();
	AcceptedInviteDescriptor.ReceivedAtUtc = FDateTime::UtcNow();
	const auto Backend = InviteResult.Session.SessionSettings.bIsLANMatch ? EEOSSessionBackend::LAN
		: InviteResult.Session.SessionSettings.bUseLobbiesIfAvailable ? EEOSSessionBackend::Lobby : EEOSSessionBackend::Session;
	if (bValid) AcceptedInviteDescriptor.Target = EEOSCapacity::Describe(InviteResult, Backend, 0);
	const auto Delivered = AcceptedInviteDescriptor;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionInvite Request=%lld Valid=%d Target=%s Backend=%d LocalUser=%d"),
		Delivered.RequestId, bValid, *FEEOSNativeOperationLease::SafeField(Delivered.Target.SessionId), int32(Backend), ControllerId);
	OnSessionInviteAccepted.Broadcast(bValid, Delivered.Target.SessionId);
	if (!bShuttingDown) OnSessionInviteDetailed.Broadcast(Delivered);
}


bool UEEOSSessionSubsystem::AdmitSessionOperation(FName Operation, FName SessionName)
{
	if (bShuttingDown || SessionLease.IsValid())
	{
		RejectOperation(Operation, EEOSOperationCode::Busy, TEXT("A named session operation is already pending or shutting down."), SessionName.ToString()); return false;
	}
	if (SessionName.IsNone() || SessionName == FName(TEXT("EOS_Lobby")))
	{
		FinishSessionOperation(Operation, SessionName, false, EEOSOperationCode::InvalidInput, TEXT("Session name is empty or reserved for the dedicated lobby subsystem.")); return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (!Sessions.IsValid() || !Local.IsValid() || !Local->IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		FinishSessionOperation(Operation, SessionName, false, EEOSOperationCode::IdentityUnavailable, TEXT("EOS requires a valid logged-in local identity and session interface.")); return false;
	}
	if (!SessionLease.TryAcquire(Sessions.Get(), SessionName, this, Operation))
	{
		RejectOperation(Operation, EEOSOperationCode::Busy, TEXT("Another plugin operation owns the named native session."), SessionName.ToString()); return false;
	}
	OperationSessions = Sessions; SessionOperationContext = CaptureEOSContext();
	BeginOperation(Operation, SessionName.ToString(), SessionLease.GetRequestId());
	const auto* Existing = Sessions->GetNamedSession(SessionName);
	TagOperationContext(Operation, Existing && Existing->SessionInfo.IsValid() ? Existing->GetSessionIdStr() : FString()); return true;
}

bool UEEOSSessionSubsystem::BeginCreateSession(const FOnlineSessionSettings& Settings, const FString& SessionName)
{
	if (bShuttingDown || SessionLease.IsValid())
	{
		RejectOperation(TEXT("CreateSession"), EEOSOperationCode::Busy, TEXT("A named session operation is pending."), SessionName); return false;
	}
	int32 AdvertisedAttributes = 0;
	for (const auto& Attribute : Settings.Settings)
	{
		if (Attribute.Value.AdvertisementType == EOnlineDataAdvertisementType::DontAdvertise) continue;
		++AdvertisedAttributes;
		if (EEOSCapacity::IsReservedAttribute(Attribute.Key) || Attribute.Key.IsNone()
			|| FTCHARToUTF8(*Attribute.Key.ToString()).Length() > (Settings.bUseLobbiesIfAvailable ? EOS_LOBBYMODIFICATION_MAX_ATTRIBUTE_LENGTH : EOS_SESSIONMODIFICATION_MAX_SESSION_ATTRIBUTE_LENGTH))
		{
			FinishSessionOperation(TEXT("CreateSession"), FName(*SessionName), false, EEOSOperationCode::InvalidInput,
				TEXT("A custom attribute name is empty, reserved, or exceeds the SDK limit.")); return false;
		}
	}
	const int32 NativeReservedAttributes = 6; // Six built-in fields in both EOS native adapters.
	const int32 AttributeLimit = Settings.bUseLobbiesIfAvailable ? EOS_LOBBYMODIFICATION_MAX_ATTRIBUTES : EOS_SESSIONMODIFICATION_MAX_SESSION_ATTRIBUTES;
	if (!Settings.bIsLANMatch && AdvertisedAttributes + NativeReservedAttributes > AttributeLimit)
	{ FinishSessionOperation(TEXT("CreateSession"), FName(*SessionName), false, EEOSOperationCode::InvalidInput, TEXT("Advertised attributes exceed the backend limit including native fields.")); return false; }
	const int64 Total = int64(Settings.NumPublicConnections) + Settings.NumPrivateConnections;
	if (Settings.NumPublicConnections < 0 || Settings.NumPrivateConnections < 0 || Total <= 0 || Total > MAX_int32
		|| (Settings.bUseLobbiesIfAvailable && Total > EOS_LOBBY_MAX_LOBBY_MEMBERS))
	{
		FinishSessionOperation(TEXT("CreateSession"), FName(*SessionName), false, EEOSOperationCode::InvalidInput, FString::Printf(TEXT("Requested public=%d private=%d total=%lld; allowed total=1..%lld for this backend."), Settings.NumPublicConnections, Settings.NumPrivateConnections, Total, Settings.bUseLobbiesIfAvailable ? int64(EOS_LOBBY_MAX_LOBBY_MEMBERS) : int64(MAX_int32))); return false;
	}
	if (!AdmitSessionOperation(TEXT("CreateSession"), FName(*SessionName))) return false;
	PendingCreateSessionName = FName(*SessionName); PendingCreateSettings = Settings;
	const auto Sessions = OperationSessions; const int64 Token = SessionLease.GetRequestId();
	if (const auto* Existing = Sessions->GetNamedSession(PendingCreateSessionName))
	{
		if (Existing->SessionState == EOnlineSessionState::Creating || Existing->SessionState == EOnlineSessionState::Destroying)
		{
			FinishSessionOperation(TEXT("CreateSession"), PendingCreateSessionName, false, EEOSOperationCode::Busy, TEXT("Existing native session is creating or destroying.")); return false;
		}
		SetOperationPhase(TEXT("CreateSession"), TEXT("ClosingExisting"));
		DestroyForCreateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleDestroyThenCreateComplete));
		const bool bStarted = Sessions->DestroySession(PendingCreateSessionName);
		if (!bStarted && DestroyForCreateHandle.IsValid() && SessionLease.GetRequestId() == Token)
			FinishSessionOperation(TEXT("CreateSession"), PendingCreateSessionName, false, EEOSOperationCode::NativeStartRejected, TEXT("Existing session close refused submission."));
		return bStarted;
	}
	return SubmitSessionCreation();
}

bool UEEOSSessionSubsystem::SubmitSessionCreation()
{
	const auto Sessions = OperationSessions; const FName Name = PendingCreateSessionName;
	const int64 Token = SessionLease.GetRequestId();
	if (bShuttingDown || !IsEOSContextCurrent(SessionOperationContext) || !Sessions.IsValid() || Sessions->GetNamedSession(Name))
	{
		FinishSessionOperation(TEXT("CreateSession"), Name, false, EEOSOperationCode::InvalidTarget, TEXT("Named session is not available for creation.")); return false;
	}
	SetOperationPhase(TEXT("CreateSession"), TEXT("Creating"));
	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleCreateSessionComplete));
	const FOnlineSessionSettings Settings = PendingCreateSettings;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionCreate Request=%lld Name=%s Public=%d Private=%d LobbyBackend=%d LAN=%d Voice=%d Advertise=%d"),
		Token, *FEEOSNativeOperationLease::SafeField(Name.ToString()), Settings.NumPublicConnections, Settings.NumPrivateConnections, Settings.bUseLobbiesIfAvailable,
		Settings.bIsLANMatch, Settings.bUseLobbiesVoiceChatIfAvailable, Settings.bShouldAdvertise);
	const bool bStarted = Sessions->CreateSession(0, Name, Settings);
	if (!bStarted && CreateSessionCompleteHandle.IsValid() && SessionLease.GetRequestId() == Token)
		FinishSessionOperation(TEXT("CreateSession"), Name, false, EEOSOperationCode::NativeStartRejected, TEXT("Native create refused submission."));
	return bStarted;
}

bool UEEOSSessionSubsystem::BeginJoinSession(const FOnlineSessionSearchResult& Result, const FString& SessionName)
{
	if (bShuttingDown || SessionLease.IsValid())
	{
		RejectOperation(TEXT("JoinSession"), EEOSOperationCode::Busy, TEXT("A named session operation is pending."), SessionName); return false;
	}
	if (!Result.IsValid() || !Result.Session.SessionInfo.IsValid())
	{
		FinishSessionOperation(TEXT("JoinSession"), FName(*SessionName), false, EEOSOperationCode::InvalidTarget, TEXT("Native search or invite result is invalid.")); return false;
	}
	if (!AdmitSessionOperation(TEXT("JoinSession"), FName(*SessionName))) return false;
	TagOperationContext(TEXT("JoinSession"), FString(), 0, SessionSearch.IsValid() && SessionSearch->SearchResults.ContainsByPredicate([&](const FOnlineSessionSearchResult& Entry) { return Entry.IsValid() && Entry.GetSessionIdStr() == Result.GetSessionIdStr(); }) ? SearchGeneration : 0);
	const auto Sessions = OperationSessions; const FName Name(*SessionName); const int64 Token = SessionLease.GetRequestId();
	if (Sessions->GetNamedSession(Name))
	{
		FinishSessionOperation(TEXT("JoinSession"), Name, false, EEOSOperationCode::InvalidTarget, TEXT("Named session already exists; close it before joining.")); return false;
	}
	PendingJoinSessionName = Name;
	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSSessionSubsystem::HandleJoinSessionComplete));
	const FOnlineSessionSearchResult Target = Result;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionJoin Request=%lld Name=%s Target=%s LobbyBackend=%d"), Token, *FEEOSNativeOperationLease::SafeField(Name.ToString()), *FEEOSNativeOperationLease::SafeField(Target.GetSessionIdStr()), Target.Session.SessionSettings.bUseLobbiesIfAvailable);
	const bool bStarted = Sessions->JoinSession(0, Name, Target);
	if (!bStarted && JoinSessionCompleteHandle.IsValid() && SessionLease.GetRequestId() == Token)
		FinishSessionOperation(TEXT("JoinSession"), Name, false, EEOSOperationCode::NativeStartRejected, TEXT("Native join refused submission."));
	return bStarted;
}

void UEEOSSessionSubsystem::FinishSessionOperation(FName Operation, FName Name, bool bSuccess, EEOSOperationCode Code,
	const FString& Message, const FString& NativeResult, EEOSResultSource Source)
{
	const auto Sessions = OperationSessions;
	const auto* Native = Sessions.IsValid() ? Sessions->GetNamedSession(Name) : nullptr;
	const bool bContextRetired = SessionLease.IsValid() && !IsEOSContextCurrent(SessionOperationContext);
	if (bContextRetired) { bSuccess = false; Code = EEOSOperationCode::Canceled; }
	const bool bUsable = Native && Native->SessionInfo.IsValid() && Native->SessionInfo->IsValid()
		&& Native->SessionState != EOnlineSessionState::Creating && Native->SessionState != EOnlineSessionState::Destroying;
	const FString CurrentId = bUsable ? Native->GetSessionIdStr() : FString();
	if (bUsable && (Operation == TEXT("CreateSession") || Operation == TEXT("JoinSession")))
	{
		const auto Capacity = EEOSCapacity::Read(*Native, Native->SessionSettings.bIsLANMatch ? EEOSSessionBackend::LAN : Native->SessionSettings.bUseLobbiesIfAvailable ? EEOSSessionBackend::Lobby : EEOSSessionBackend::Session);
		bool bMigration = GetEOSSettings()->bAllowLobbyHostMigration; Native->SessionSettings.Get(SETTING_HOST_MIGRATION, bMigration);
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSSessionMembership Request=%lld Name=%s NewId=%s Known=%d Max=%d Members=%d Slots=%d PrivateRequested=%d PrivatePolicy=NativePermissionMapping Voice=%d Presence=%d Migration=%d"),
			SessionLease.GetRequestId(), *FEEOSNativeOperationLease::SafeField(Name.ToString()), *FEEOSNativeOperationLease::SafeField(CurrentId), Capacity.bKnown, Capacity.Maximum, Capacity.Members, Capacity.AvailableSlots,
			Native->SessionSettings.NumPrivateConnections, Native->SessionSettings.bUseLobbiesVoiceChatIfAvailable, Native->SessionSettings.bUsesPresence, bMigration);
	}
	if (bUsable && !bContextRetired) MembershipNames.Add(Name); else MembershipNames.Remove(Name);
	if (Operation == TEXT("CreateSession") || Operation == TEXT("JoinSession")) bSuccess = bSuccess && bUsable;
	if (Operation == TEXT("DestroySession")) bSuccess = bSuccess && !Native;
	if (!bSuccess && Code == EEOSOperationCode::Succeeded) Code = EEOSOperationCode::NativeFailure;
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyForCreateHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
		Sessions->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
	}
	CreateSessionCompleteHandle.Reset(); DestroyForCreateHandle.Reset(); JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset(); StartSessionCompleteHandle.Reset(); EndSessionCompleteHandle.Reset();
	PendingCreateSessionName = PendingJoinSessionName = PendingDestroySessionName = PendingStartSessionName = PendingEndSessionName = NAME_None;
	SessionLease.Reset(); OperationSessions.Reset(); bInSession = IsInSession();
	const auto Outcome = CompleteOperation(Operation, bSuccess, Code, bContextRetired ? TEXT("Original identity/platform context retired.") : Message, CurrentId, NativeResult, Source);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (bShuttingDown) return;
	if (Operation == TEXT("CreateSession")) OnSessionCreated.Broadcast(bSuccess, Name.ToString());
	else if (Operation == TEXT("JoinSession")) OnSessionJoined.Broadcast(bSuccess, Name.ToString());
	else if (Operation == TEXT("DestroySession")) OnSessionDestroyed.Broadcast(bSuccess, Name.ToString());
	else if (Operation == TEXT("StartSession")) OnSessionStarted.Broadcast(bSuccess);
	else if (Operation == TEXT("EndSession")) OnSessionEnded.Broadcast(bSuccess, Name.ToString());
	OnOperationCompleted.Broadcast(Outcome);
}

bool UEEOSSessionSubsystem::TickMembership(float)
{
	if (bShuttingDown) return false;
	TryRegisterLifetimeNotifications();
	const auto Context = CaptureEOSContext();
	if (MembershipIdentityGeneration != Context.Generation)
	{
		MembershipIdentityGeneration = Context.Generation;
		AcceptedInvite = FOnlineSessionSearchResult(); AcceptedInviteDescriptor = FEEOSSessionInvite();
	}
	const auto Names = MembershipNames.Array();
	for (FName Name : Names) if (!IsInNamedSession(Name.ToString())) MembershipNames.Remove(Name);
	bInSession = IsInSession(); return true;
}

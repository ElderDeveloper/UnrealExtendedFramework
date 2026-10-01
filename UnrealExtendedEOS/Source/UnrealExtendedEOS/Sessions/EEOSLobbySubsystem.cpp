// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLobbySubsystem.h"
#include "EEOSSearchCoordinator.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Async/Async.h"

#include "eos_sdk.h"
#include "eos_lobby.h"

static const FName LOBBY_SESSION_NAME = TEXT("EOS_Lobby");

/** Owner tag this subsystem uses with the shared UEEOSSearchCoordinator. */
static const FName LobbySearchOwner(TEXT("EEOSLobbySubsystem"));

namespace
{
// EOS ticks on the game thread normally. Inline completion is required while shutdown pumps it.
template<typename F> void OnLobbyGameThread(F&& Completion)
{
	if (IsInGameThread()) Completion();
	else AsyncTask(ENamedThreads::GameThread, Forward<F>(Completion));
}

void CleanupNamedLobby(const IOnlineSessionPtr& Sessions, const FString& LobbyId)
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (Session && Session->SessionInfo.IsValid() && Session->GetSessionIdStr() == LobbyId
		&& Session->SessionState != EOnlineSessionState::Destroying)
	{
		Sessions->DestroySession(LOBBY_SESSION_NAME); // also removes the lobby RTC room/analytics
	}
}

/** A timed-out create/join can finish after the GameInstance died in a persistent editor OSS.
	* Keep only interface/identity state, never the dead subsystem, and close that exact late lobby. */
struct FEEOSLateLobbyCleanup : TSharedFromThis<FEEOSLateLobbyCleanup>
{
	IOnlineSessionPtr Sessions;
	EOS_HPlatform Platform = nullptr;
	FUniqueNetIdPtr LocalUser;
	FDelegateHandle CreateHandle, JoinHandle;
	void Arm(bool bCreatePending, bool bJoinPending)
	{
		const TSharedRef<FEEOSLateLobbyCleanup> Self = AsShared();
		if (bCreatePending) CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateLambda([Self](FName Name, bool) { if (Name == LOBBY_SESSION_NAME) Self->Complete(); }));
		if (bJoinPending) JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateLambda([Self](FName Name, EOnJoinSessionCompleteResult::Type) { if (Name == LOBBY_SESSION_NAME) Self->Complete(); }));
	}
	void Complete()
	{
		// Hold Self while removing the delegates that own this cleanup object.
		const TSharedRef<FEEOSLateLobbyCleanup> Self = AsShared();
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		const FNamedOnlineSession* Session = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
		if (!Session || !Session->SessionInfo.IsValid() || !Session->SessionInfo->IsValid()) return;
		const FString LobbyId = Session->GetSessionIdStr();
		const FString Puid = LocalUser.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalUser->ToString()) : FString();
		const bool bOwner = LocalUser.IsValid() && Session->OwningUserId.IsValid() && *LocalUser == *Session->OwningUserId;
		EOS_HLobby Lobby = Platform ? EOS_Platform_GetLobbyInterface(Platform) : nullptr;
		if (!bOwner || !Lobby || Puid.IsEmpty()) { CleanupNamedLobby(Sessions, LobbyId); return; }
		const FTCHARToUTF8 Utf8Id(*LobbyId);
		EOS_Lobby_DestroyLobbyOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
		Options.LobbyId = Utf8Id.Get();
		Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
		struct FContext { IOnlineSessionPtr Sessions; FString LobbyId; };
		EOS_Lobby_DestroyLobby(Lobby, &Options, new FContext{Sessions, LobbyId},
			[](const EOS_Lobby_DestroyLobbyCallbackInfo* Data)
			{
				// Retrying callbacks retain ClientData until the SDK reports a terminal result.
				if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
				TUniquePtr<FContext> Context(static_cast<FContext*>(Data->ClientData));
				OnLobbyGameThread([Sessions = Context->Sessions, Id = Context->LobbyId]() { CleanupNamedLobby(Sessions, Id); });
			});
	}
};
}

void UEEOSLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bShuttingDown = false;
	bShutdownFlushed = false;
	bDeinitialized = false;
	LobbyOwnerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEEOSLobbySubsystem::TickLobbyOwner), 0.25f);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UEEOSLobbySubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UEEOSLobbySubsystem::HandleTravelFailure);
	}

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

bool UEEOSLobbySubsystem::ShutdownLobby(float MaxSeconds)
{
	bShuttingDown = true; // No create/join continuation or gameplay broadcast may escape the drain.
	if (bShutdownFlushed) return !IsMembershipOperationInFlight() && !bInLobby;
	bShutdownFlushed = true;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	EOS_HPlatform Platform = GetPlatformHandle();
	if (!Sessions.IsValid()) return !IsMembershipOperationInFlight() && !bInLobby;
	bool bExitAttempted = ExitRequest.IsActive();
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.0f, MaxSeconds);
	do
	{
		const FNamedOnlineSession* Session = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
		RefreshLobbyState(Session);
		if (!IsMembershipOperationInFlight() && Session && !bExitAttempted)
		{
			bExitAttempted = true;
			StartLobbyExit(Sessions, IsLobbyOwner(), FEEOSLobbyExitRequest::EContinuation::None);
		}
		if (!IsMembershipOperationInFlight() && !Sessions->GetNamedSession(LOBBY_SESSION_NAME)) return true;
		if (!Platform || FPlatformTime::Seconds() >= Deadline) break;
		EOS_Platform_Tick(Platform);
		// EOS callbacks also schedule native completions for the OSS's next tick. Pump its queue,
		// without re-entering the editor/core ticker or other worlds.
		static_cast<FOnlineSubsystemImpl*>(EOSSub)->FOnlineSubsystemImpl::Tick(0.01f);
		FPlatformProcess::Sleep(0.01f);
	} while (FPlatformTime::Seconds() < Deadline);
	RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	const bool bComplete = !IsMembershipOperationInFlight() && !Sessions->GetNamedSession(LOBBY_SESSION_NAME);
	if (!bComplete)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Lobby shutdown timed out; late create/join cleanup retained (create=%d, join=%d, exit=%d, promotion=%d)."),
			CreateLobbyCompleteHandle.IsValid(), JoinLobbyCompleteHandle.IsValid(), ExitRequest.IsActive(), bPromotionPending);
		if (CreateLobbyCompleteHandle.IsValid() || JoinLobbyCompleteHandle.IsValid())
		{
			TSharedRef<FEEOSLateLobbyCleanup> Cleanup = MakeShared<FEEOSLateLobbyCleanup>();
			Cleanup->Sessions = Sessions;
			Cleanup->Platform = Platform;
			const IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
			Cleanup->LocalUser = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
			Cleanup->Arm(CreateLobbyCompleteHandle.IsValid(), JoinLobbyCompleteHandle.IsValid());
		}
	}
	return bComplete;
}

void UEEOSLobbySubsystem::Deinitialize()
{
	bShuttingDown = true;
	ShutdownLobby(2.0f);
	if (LobbyOwnerTickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(LobbyOwnerTickerHandle);
	LobbyOwnerTickerHandle.Reset();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();

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
	FindLobbiesCompleteHandle.Reset();
	JoinLobbyCompleteHandle.Reset();
	JoinRequest.Reset();
	RemoveExitCloseNotification();
	ExitRequest.Reset();
	bPromotionPending = false;
	++PromotionToken;
	PromotionLobbyId.Empty();
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
	bDeinitialized = true;
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
	CachedLobbyOwnerId.Empty();
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
	// join/leave/destroy is in flight — the named session cannot be changed concurrently.
	if (IsMembershipOperationInFlight())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::CreateLobby — A lobby membership operation is already in flight; rejecting new call (no delegate will fire)"));
		return false;
	}

	if (bShuttingDown || !IsEOSAvailable())
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

	// OnlineSubsystemEOS reads SETTING_HOST_MIGRATION at create and passes its
	// inverse to EOS_Lobby_CreateLobby as bDisableHostMigration (it defaults to on).
	const UEEOSSettings* EOSSettings = GetEOSSettings();
	const bool bAllowHostMigration = !EOSSettings || EOSSettings->bAllowLobbyHostMigration;
	Settings.Set(SETTING_HOST_MIGRATION, bAllowHostMigration, EOnlineDataAdvertisementType::DontAdvertise);

	PendingCreateLobbySettings = Settings;

	if (SessionInterface->GetNamedSession(LOBBY_SESSION_NAME))
	{
		RefreshLobbyState(SessionInterface->GetNamedSession(LOBBY_SESSION_NAME));
		if (!StartLobbyExit(SessionInterface, IsLobbyOwner(), FEEOSLobbyExitRequest::EContinuation::Create))
		{
			OnLobbyCreated.Broadcast(false, CurrentLobbyId);
			return false;
		}
		return true;
	}

	CreateLobbyCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete));

	SessionInterface->CreateSession(0, LOBBY_SESSION_NAME, PendingCreateLobbySettings);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::CreateLobby — Creating lobby with %d max members (Voice=%d, HostMigration=%d)"), MaxMembers, bUseVoiceChat, bAllowHostMigration);
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

bool UEEOSLobbySubsystem::IsMembershipOperationInFlight() const
{
	return CreateLobbyCompleteHandle.IsValid() || ExitRequest.IsActive() || bPromotionPending || JoinRequest.IsActive();
}

bool UEEOSLobbySubsystem::JoinLobby(int32 SearchResultIndex)
{
	if (IsMembershipOperationInFlight())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::JoinLobby: membership operation already in flight; rejected without completion"));
		return false;
	}
	if (!LobbySearch.IsValid() || !LobbySearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		LastLobbyJoinError = FString::Printf(TEXT("Invalid lobby search result index: %d"), SearchResultIndex);
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: %s"), *LastLobbyJoinError);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId);
		return false;
	}
	return BeginJoinLobby(LobbySearch->SearchResults[SearchResultIndex]);
}

bool UEEOSLobbySubsystem::JoinLobbyResult(const FOnlineSessionSearchResult& SearchResult)
{
	if (IsMembershipOperationInFlight())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::JoinLobbyResult: membership operation already in flight; rejected without completion"));
		return false;
	}
	return BeginJoinLobby(SearchResult);
}

bool UEEOSLobbySubsystem::BeginJoinLobby(const FOnlineSessionSearchResult& SearchResult)
{
	LastLobbyJoinError.Empty();
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (bShuttingDown || !Sessions.IsValid() || !SearchResult.IsValid())
	{
		LastLobbyJoinError = bShuttingDown ? TEXT("Lobby subsystem is shutting down.")
			: !Sessions.IsValid() ? TEXT("EOS session interface is unavailable.") : TEXT("Lobby search result is invalid.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *LastLobbyJoinError);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId);
		return false;
	}

	const FNamedOnlineSession* Existing = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
	RefreshLobbyState(Existing);
	if (Existing && (Existing->SessionState == EOnlineSessionState::Creating || Existing->SessionState == EOnlineSessionState::Destroying))
	{
		LastLobbyJoinError = TEXT("Existing lobby session is still creating or leaving; retry after it completes.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *LastLobbyJoinError);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId);
		return false;
	}
	if (Existing && Existing->SessionInfo.IsValid() && Existing->SessionInfo->GetSessionId() == SearchResult.Session.SessionInfo->GetSessionId())
	{
		LastLobbyJoinError = TEXT("Already in the requested lobby.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *LastLobbyJoinError);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId);
		return false;
	}

	FOnlineSessionSearchResult Target = SearchResult;
	Target.Session.SessionSettings.bUsesPresence = true;
	Target.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	JoinRequest.Begin(Target, Existing != nullptr);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join requested target='%s', existing='%s', state=%d, cachedMember=%d"),
		*Target.GetSessionIdStr(), *CurrentLobbyId, Existing ? int32(Existing->SessionState) : int32(EOnlineSessionState::NoSession), bInLobby);
	if (!Existing) return StartJoiningLobby();

	if (!StartLobbyExit(Sessions, IsLobbyOwner(), FEEOSLobbyExitRequest::EContinuation::Join))
	{
		FinishJoiningLobby(false, TEXT("Could not start closing the existing lobby."));
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::StartJoiningLobby()
{
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (bShuttingDown || !Sessions.IsValid())
	{
		FinishJoiningLobby(false, TEXT("EOS became unavailable before joining the lobby."));
		return false;
	}
	JoinLobbyCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleJoinSessionComplete));
	// A synchronous callback may reset JoinRequest. Keep the argument alive independently.
	const FOnlineSessionSearchResult Target = JoinRequest.GetResult();
	if (!Sessions->JoinSession(0, LOBBY_SESSION_NAME, Target) && JoinRequest.IsJoining())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
		JoinLobbyCompleteHandle.Reset();
		FinishJoiningLobby(false, TEXT("EOS refused to start the lobby join."));
		return false;
	}
	return true;
}

void UEEOSLobbySubsystem::RefreshLobbyState(const FNamedOnlineSession* Session)
{
	if (!Session || !Session->SessionInfo.IsValid() || !Session->SessionInfo->IsValid()
		|| Session->SessionState == EOnlineSessionState::NoSession || Session->SessionState == EOnlineSessionState::Creating)
	{
		ResetLobbyState();
		return;
	}
	const bool bNewLobby = CurrentLobbyId != Session->GetSessionIdStr();
	bInLobby = true;
	CurrentLobbyId = Session->GetSessionIdStr();
	if (bNewLobby) CachedLobbyOwnerId = Session->OwningUserId.IsValid() ? Session->OwningUserId->ToString() : FString();
	RefreshCachedLobbyAttributes(Session->SessionSettings, false);
}

void UEEOSLobbySubsystem::FinishJoiningLobby(bool bSuccess, const FString& Error)
{
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	const FString TargetId = JoinRequest.GetResult().IsValid() ? JoinRequest.GetResult().GetSessionIdStr() : FString();
	if (Sessions.IsValid()) RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	// The success callback must still correspond to a usable named lobby.
	bSuccess = bSuccess && Sessions.IsValid() && bInLobby;
	JoinRequest.Reset();
	LastLobbyJoinError = !bSuccess && Error.IsEmpty() ? TEXT("Join completed without a usable named lobby session.") : Error;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join %s target='%s', current='%s', member=%d, error='%s'"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"), *TargetId, *CurrentLobbyId, bInLobby, *LastLobbyJoinError);
	OnLobbyJoined.Broadcast(bSuccess, CurrentLobbyId);
}

bool UEEOSLobbySubsystem::ShouldLeaveAfterConnectionFailure(const UGameInstance* FailureGameInstance) const
{
	const UEEOSSettings* Settings = GetEOSSettings();
	return !bShuttingDown && FailureGameInstance && FailureGameInstance == GetGameInstance()
		&& (!Settings || Settings->bLeaveLobbyOnConnectionFailure) && !IsMembershipOperationInFlight();
}

void UEEOSLobbySubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (!World || !ShouldLeaveAfterConnectionFailure(World->GetGameInstance()) || !NetDriver
		|| NetDriver->GetNetDriverDefinition() != FName(TEXT("GameNetDriver"))) return;
	switch (FailureType)
	{
	case ENetworkFailure::PendingConnectionFailure:
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Game network failure: %s"), ENetworkFailure::ToString(FailureType));
		HandleTravelFailure(World, ETravelFailure::ClientTravelFailure, Error);
		break;
	default:
		break;
	}
}

void UEEOSLobbySubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (!World || !ShouldLeaveAfterConnectionFailure(World->GetGameInstance())) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	// Hosting a lobby is independent of an unrelated travel failure; recover clients only.
	if (!Session || World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer) return;
	RefreshLobbyState(Session);
	LastLobbyJoinError = FString::Printf(TEXT("Game connection failed (%s): %s"), ETravelFailure::ToString(FailureType), *Error);
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: %s; leaving lobby '%s' so another join can start"), *LastLobbyJoinError, *CurrentLobbyId);
	LeaveLobby();
}

bool UEEOSLobbySubsystem::LeaveLobby()
{
	if (IsMembershipOperationInFlight() || bShuttingDown)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::LeaveLobby: membership operation pending or shutting down; rejected without completion"));
		return false;
	}
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (!Sessions.IsValid() || !StartLobbyExit(Sessions, false, FEEOSLobbyExitRequest::EContinuation::None))
	{
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::DestroyLobby()
{
	if (IsMembershipOperationInFlight() || bShuttingDown)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::DestroyLobby: membership operation pending or shutting down; rejected without completion"));
		return false;
	}
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	if (!Sessions.IsValid() || !IsLobbyOwner() || !StartLobbyExit(Sessions, true, FEEOSLobbyExitRequest::EContinuation::None))
	{
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId);
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::StartLobbyExit(const IOnlineSessionPtr& Sessions, bool bDeleteBackend, FEEOSLobbyExitRequest::EContinuation Continuation)
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	RefreshLobbyState(Session);
	if (!Session || !bInLobby || ExitRequest.IsActive() || Session->SessionState == EOnlineSessionState::Creating) return false;
	EOS_HLobby Lobby = nullptr;
	EOS_ProductUserId LocalPuid = nullptr;
	if (bDeleteBackend)
	{
		const EOS_HPlatform Platform = GetPlatformHandle();
		Lobby = Platform ? EOS_Platform_GetLobbyInterface(Platform) : nullptr;
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		const IOnlineIdentityPtr Identity = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
		const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
		const FString Puid = LocalId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalId->ToString()) : FString();
		LocalPuid = Puid.IsEmpty() ? nullptr : EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
		if (!Lobby || !LocalPuid || !EOS_ProductUserId_IsValid(LocalPuid)) return false;
	}
	ExitRequest.Begin(CurrentLobbyId, bDeleteBackend, Continuation);
	ExpectedNativeExitCompletions = 0;
	NativeExitCompletions = 0;
	if (bDeleteBackend)
	{
		ExitPlatform = GetPlatformHandle();
		EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions NotifyOptions = {};
		NotifyOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
		ExitClosedNotificationId = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(Lobby, &NotifyOptions, this,
			[](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data)
			{
				if (Data->CurrentStatus != EOS_ELobbyMemberStatus::EOS_LMS_CLOSED) return;
				const TWeakObjectPtr<UEEOSLobbySubsystem> Weak(static_cast<UEEOSLobbySubsystem*>(Data->ClientData));
				const FString Id = UTF8_TO_TCHAR(Data->LobbyId);
				OnLobbyGameThread([Weak, Id]() { if (UEEOSLobbySubsystem* Self = Weak.Get()) Self->ObserveBackendLobbyClosed(Id); });
			});
		if (ExitClosedNotificationId == EOS_INVALID_NOTIFICATIONID)
		{
			ExitRequest.Reset();
			ExitPlatform = nullptr;
			return false;
		}
	}
	// Bind BEFORE deleting: EOS_LMS_CLOSED may cause native cleanup before the SDK result arrives.
	DestroyLobbyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleDestroySessionComplete));
	if (!bDeleteBackend)
	{
		StartNativeLobbyCleanup(Sessions);
		return true;
	}
	struct FDeleteContext { TWeakObjectPtr<UEEOSLobbySubsystem> Self; IOnlineSessionPtr Sessions; FString LobbyId; uint64 Token; };
	FDeleteContext* Context = new FDeleteContext{this, Sessions, CurrentLobbyId, ExitRequest.GetToken()};
	const FTCHARToUTF8 Utf8Id(*CurrentLobbyId);
	EOS_Lobby_DestroyLobbyOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
	Options.LobbyId = Utf8Id.Get();
	Options.LocalUserId = LocalPuid;
	EOS_Lobby_DestroyLobby(Lobby, &Options, Context, [](const EOS_Lobby_DestroyLobbyCallbackInfo* Data)
	{
		// Retrying callbacks retain ClientData until the SDK reports a terminal result.
		if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
		TUniquePtr<FDeleteContext> Ctx(static_cast<FDeleteContext*>(Data->ClientData));
		const bool bDeleted = Data->ResultCode == EOS_EResult::EOS_Success || Data->ResultCode == EOS_EResult::EOS_NotFound;
		if (!bDeleted) UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Backend lobby delete failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
		OnLobbyGameThread([Weak = Ctx->Self, Sessions = Ctx->Sessions, Id = Ctx->LobbyId, Token = Ctx->Token, bDeleted]()
		{
			UEEOSLobbySubsystem* Self = Weak.Get();
			if (Self && Self->ExitRequest.Matches(Token, Id)) Self->HandleBackendLobbyDeleted(Token, Id, bDeleted);
			else if (bDeleted || !Self || Self->bShuttingDown) CleanupNamedLobby(Sessions, Id); // shutdown timed out / subsystem gone
		});
	});
	return true;
}

void UEEOSLobbySubsystem::HandleBackendLobbyDeleted(uint64 Token, const FString& LobbyId, bool bDeleted)
{
	if (!ExitRequest.CompleteBackend(Token, LobbyId, bDeleted)) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (!bDeleted)
	{
		// Even when backend deletion fails, shutdown must release local membership/RTC.
		if (bShuttingDown && Session)
		{
			StartNativeLobbyCleanup(Sessions);
			return;
		}
		FinishLobbyExit(false);
		return;
	}
	if (!Session || !Sessions.IsValid()) { FinishLobbyExit(false); return; }
	if (!Session->SessionInfo.IsValid() || Session->GetSessionIdStr() != LobbyId) { FinishLobbyExit(false); return; }
	// CLOSED can precede our SDK result. Its native cleanup owns that completion.
	if (Session->SessionState == EOnlineSessionState::Destroying || ExpectedNativeExitCompletions > 0) return;
	StartNativeLobbyCleanup(Sessions);
}

void UEEOSLobbySubsystem::ObserveBackendLobbyClosed(const FString& LobbyId)
{
	if (!ExitRequest.IsActive() || ExitRequest.GetLobbyId() != LobbyId) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	// The native CLOSED handler issues a leave for each notification while this lobby
	// remains named. Wait for ALL those callbacks before reusing EOS_Lobby for a target.
	if (Session && Session->SessionInfo.IsValid() && Session->GetSessionIdStr() == LobbyId) ++ExpectedNativeExitCompletions;
}

void UEEOSLobbySubsystem::RemoveExitCloseNotification()
{
	if (ExitPlatform && ExitClosedNotificationId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(EOS_Platform_GetLobbyInterface(ExitPlatform), ExitClosedNotificationId);
	}
	ExitClosedNotificationId = EOS_INVALID_NOTIFICATIONID;
	ExitPlatform = nullptr;
	ExpectedNativeExitCompletions = 0;
	NativeExitCompletions = 0;
}

void UEEOSLobbySubsystem::StartNativeLobbyCleanup(const IOnlineSessionPtr& Sessions)
{
	const FNamedOnlineSession* Session = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
	if (!Session) { FinishLobbyExit(false); return; }
	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// An externally started leave has no CLOSED notification to account for it.
		ExpectedNativeExitCompletions = FMath::Max(ExpectedNativeExitCompletions, 1);
		return;
	}
	++ExpectedNativeExitCompletions;
	if (!Sessions->DestroySession(LOBBY_SESSION_NAME))
	{
		--ExpectedNativeExitCompletions;
		FinishLobbyExit(false);
	}
}

void UEEOSLobbySubsystem::FinishLobbyExit(bool bNativeSuccess)
{
	if (!ExitRequest.IsActive() || ExitRequest.IsBackendPending() || NativeExitCompletions < ExpectedNativeExitCompletions) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
	DestroyLobbyCompleteHandle.Reset();
	const auto Continuation = ExitRequest.GetContinuation();
	const FString ClosedId = ExitRequest.GetLobbyId();
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	const bool bSuccess = Sessions.IsValid() && ExitRequest.Succeeded(bNativeSuccess, Session != nullptr);
	RefreshLobbyState(Session);
	RemoveExitCloseNotification();
	ExitRequest.Reset();
	if (bShuttingDown) { JoinRequest.Reset(); return; }
	if (Continuation == FEEOSLobbyExitRequest::EContinuation::Join)
	{
		if (!JoinRequest.CompleteLeave(bSuccess, Session != nullptr)) FinishJoiningLobby(false, TEXT("Existing lobby close failed; the new join was not started."));
		else StartJoiningLobby();
	}
	else if (Continuation == FEEOSLobbyExitRequest::EContinuation::Create)
	{
		if (!bSuccess) { OnLobbyCreated.Broadcast(false, CurrentLobbyId); return; }
		CreateLobbyCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete));
		Sessions->CreateSession(0, LOBBY_SESSION_NAME, PendingCreateLobbySettings);
	}
	else OnLobbyDestroyed.Broadcast(bSuccess, ClosedId);
}

// ── Lobby Attributes ─────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyAttribute(const FString& Key, const FString& Value)
{
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
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
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
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
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
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
		FString LobbyId;
	};
	FKickContext* Context = new FKickContext{ this, MemberId, TargetPUIDStr, CurrentLobbyId };

	EOS_Lobby_KickMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_KickMemberCallbackInfo* Data)
		{
			// Retrying callbacks retain ClientData until the SDK reports a terminal result.
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FKickContext> Ctx(static_cast<FKickContext*>(Data->ClientData));
			if (!Ctx) return;

			AsyncTask(ENamedThreads::GameThread,
				[WeakSelf = Ctx->Self, KickedMemberId = MoveTemp(Ctx->MemberId), TargetPuid = MoveTemp(Ctx->TargetPuid), LobbyId = MoveTemp(Ctx->LobbyId), ResultCode = Data->ResultCode]()
				{
					UEEOSLobbySubsystem* Self = WeakSelf.Get();
					if (!Self || !Self->bInLobby || Self->CurrentLobbyId != LobbyId) return;

					Self->InFlightKickPuids.Remove(TargetPuid);

					if (ResultCode == EOS_EResult::EOS_Success)
					{
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::KickMember — Kicked '%s' from lobby"), *KickedMemberId);
						// The PendingKickedPuids entry stays: it suppresses the duplicate
						// engine participant-left (Kicked) notification, which removes it.
						if (!Self->bShuttingDown) Self->OnLobbyMemberLeft.Broadcast(KickedMemberId);
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
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
	if (const UEEOSSettings* Settings = GetEOSSettings(); Settings && !Settings->bAllowLobbyOwnerTransfer)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Manual lobby ownership transfer is disabled; gameplay server handoff is required."));
		return false;
	}
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

	bPromotionPending = true;
	PromotionLobbyId = CurrentLobbyId;
	const uint64 Token = ++PromotionToken;
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
		FString LobbyId;
		uint64 Token;
		IOnlineSessionPtr Sessions;
	};
	FPromoteContext* Context = new FPromoteContext{ this, MemberId, CurrentLobbyId, Token, EOSSub->GetSessionInterface() };

	EOS_Lobby_PromoteMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_PromoteMemberCallbackInfo* Data)
		{
			// Retrying callbacks retain ClientData until the SDK reports a terminal result.
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FPromoteContext> Ctx(static_cast<FPromoteContext*>(Data->ClientData));
			if (!Ctx) return;

			if (Data->ResultCode != EOS_EResult::EOS_Success)
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Lobby promotion failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			OnLobbyGameThread([Weak = Ctx->Self, Sessions = Ctx->Sessions, Member = Ctx->MemberId, Id = Ctx->LobbyId, Token = Ctx->Token, Result = Data->ResultCode]()
			{
				UEEOSLobbySubsystem* Self = Weak.Get();
				if (!Self || Self->bDeinitialized) CleanupNamedLobby(Sessions, Id);
				else Self->HandlePromotionComplete(Token, Id, Member, Result == EOS_EResult::EOS_Success);
			});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::PromoteMember — Promoting '%s'..."), *MemberId);
	return true;
}

// ── Lobby Settings ───────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyJoinable(bool bIsPublic)
{
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
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
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
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

FName UEEOSLobbySubsystem::GetLobbySessionName()
{
	return LOBBY_SESSION_NAME;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSLobbySubsystem::HandleCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME || !CreateLobbyCompleteHandle.IsValid()) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
	CreateLobbyCompleteHandle.Reset();
	RefreshLobbyState(Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr);
	if (!bShuttingDown) OnLobbyCreated.Broadcast(bWasSuccessful && bInLobby, CurrentLobbyId);
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
			Result.OwnerName = SearchResult.Session.OwningUserName;
			Result.CurrentPlayers = SearchResult.Session.SessionSettings.NumPublicConnections - SearchResult.Session.NumOpenPublicConnections;
			Result.MaxPlayers = SearchResult.Session.SessionSettings.NumPublicConnections;
			Result.Ping = SearchResult.PingInMs;

			// Attributes set through SetLobbyAttribute are advertised ViaOnlineService and
			// come back on the search result's settings. Without copying them out here the
			// game sees anonymous rows: the lobby's name, join code and build tag are all
			// carried as attributes and would be dropped on the floor.
			for (const TPair<FName, FOnlineSessionSetting>& Setting : SearchResult.Session.SessionSettings.Settings)
			{
				Result.Settings.Add(Setting.Key, Setting.Value.Data.ToString());
			}

			Results.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Found %d lobbies (search %s)"), Results.Num(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnLobbiesFound.Broadcast(Results);
}

void UEEOSLobbySubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (InSessionName != LOBBY_SESSION_NAME || !JoinRequest.IsJoining()) return;
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		const IOnlineSessionPtr Sessions = EOSSub->GetSessionInterface();
		if (Sessions.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
	}
	JoinLobbyCompleteHandle.Reset();
	if (bShuttingDown)
	{
		JoinRequest.Reset();
		return;
	}
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success;
	FinishJoiningLobby(bSuccess, bSuccess ? FString() : FString::Printf(TEXT("EOS lobby join failed: %s. See LogOnlineSession for the SDK result."), LexToString(Result)));
}

void UEEOSLobbySubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME || !ExitRequest.IsActive()) return;
	++NativeExitCompletions;
	// The delete result may follow native EOS_LMS_CLOSED cleanup. Keep the request/handle
	// until both stages finish; a failed leave of an already deleted lobby is expected.
	FinishLobbyExit(bWasSuccessful);
}

void UEEOSLobbySubsystem::HandleLifetimeSessionDestroyed(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME || ExitRequest.IsActive() || JoinRequest.IsActive() || CreateLobbyCompleteHandle.IsValid()) return;
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) ReconcileRemoteLobbyExit(Sessions->GetNamedSession(LOBBY_SESSION_NAME), bWasSuccessful);
}

void UEEOSLobbySubsystem::ReconcileRemoteLobbyExit(const FNamedOnlineSession* Session, bool bWasSuccessful)
{
	if (ExitRequest.IsActive() || JoinRequest.IsActive() || CreateLobbyCompleteHandle.IsValid()) return;
	const bool bHadMembership = bInLobby;
	const FString ClosedId = CurrentLobbyId;
	RefreshLobbyState(Session); // Unreal removes the local named lobby even on an EOS leave error.
	if (bHadMembership && !bInLobby && !bShuttingDown) OnLobbyDestroyed.Broadcast(bWasSuccessful, ClosedId);
}

bool UEEOSLobbySubsystem::ShouldHandleLocalMemberRemoval() const
{
	return bInLobby && !ExitRequest.IsActive() && !JoinRequest.IsActive() && !CreateLobbyCompleteHandle.IsValid();
}

bool UEEOSLobbySubsystem::UpdateCachedLobbyOwner(const FString& OwnerId)
{
	if (OwnerId.IsEmpty() || CachedLobbyOwnerId == OwnerId) return false;
	const bool bChanged = !CachedLobbyOwnerId.IsEmpty();
	CachedLobbyOwnerId = OwnerId;
	return bChanged;
}

void UEEOSLobbySubsystem::RefreshLobbyOwner()
{
	if (bInLobby && !bShuttingDown)
	{
		const FString OwnerId = GetLobbyOwner();
		if (UpdateCachedLobbyOwner(OwnerId)) OnLobbyOwnerChanged.Broadcast(OwnerId);
	}
}

bool UEEOSLobbySubsystem::TickLobbyOwner(float)
{
	if (!bShuttingDown && bInLobby && !ExitRequest.IsActive() && !JoinRequest.IsActive()) RefreshLobbyOwner();
	return !bShuttingDown;
}

void UEEOSLobbySubsystem::HandlePromotionComplete(uint64 Token, const FString& LobbyId, const FString& MemberId, bool bSuccess)
{
	if (!bPromotionPending || PromotionToken != Token || PromotionLobbyId != LobbyId) return;
	bPromotionPending = false;
	PromotionLobbyId.Empty();
	// Shutdown may drain this operation, but must not run game/UI listeners or transfer again.
	if (bShuttingDown || CurrentLobbyId != LobbyId || !bInLobby) return;
	RefreshLobbyOwner();
	OnLobbyPromotionComplete.Broadcast(bSuccess, MemberId);
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
	if (ShouldHandleLocalMemberRemoval())
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

	RefreshLobbyOwner();
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

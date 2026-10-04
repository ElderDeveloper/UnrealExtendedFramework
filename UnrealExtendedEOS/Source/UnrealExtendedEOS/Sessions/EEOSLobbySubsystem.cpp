// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLobbySubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "EEOSCapacity.h"
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

#include "IEOSSDKManager.h"
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

bool IsWritableLobbyAttribute(const FString& Key)
{
	if (Key.IsEmpty() || FTCHARToUTF8(*Key).Length() > EOS_LOBBYMODIFICATION_MAX_ATTRIBUTE_LENGTH) return false;
	static const TSet<FName> Reserved = {TEXT("NumPublicConnections"), TEXT("NumPrivateConnections"),
		TEXT("bAntiCheatProtected"), TEXT("bUsesStats"), TEXT("bIsDedicated"), TEXT("BuildUniqueId"), TEXT("LOBBYSEARCH")};
	return !Reserved.Contains(FName(*Key));
}

}

/** Retains the exact native operation and platform after the GameInstance stops listening. */
struct FEEOSLateLobbyCleanup : TSharedFromThis<FEEOSLateLobbyCleanup>
{
	IOnlineSessionPtr Sessions;
	TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> PlatformOwner;
	EOS_HPlatform Platform = nullptr;
	FUniqueNetIdPtr LocalUser;
	FEEOSNativeOperationLease Lease;
	FName Instance;
	FString LobbyId;
	FDelegateHandle CreateHandle, JoinHandle, UpdateHandle, DestroyHandle;
	FTSTicker::FDelegateHandle Watcher;
	EOS_NotificationId ClosedNotify = EOS_INVALID_NOTIFICATIONID;
	int32 ExpectedNative = 0, NativeCompleted = 0;
	bool bMembershipPending = false, bUpdatePending = false, bBackendPending = false;
	bool bArmed = false, bCleanupStarted = false, bFinished = false, bWarned = false;
	double Started = 0;

	void Arm(bool bCreate, bool bJoin, bool bUpdate, bool bExit, bool bBackend, int32 Expected, int32 Completed)
	{
		if (bArmed || bFinished) return;
		bArmed = true; bMembershipPending = bCreate || bJoin; bUpdatePending = bUpdate;
		bBackendPending = bBackend; bCleanupStarted = bExit;
		ExpectedNative = Expected; NativeCompleted = Completed; Started = FPlatformTime::Seconds();
		const auto Self = AsShared();
		if (bCreate) CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateLambda([Self](FName Name, bool) { if (Name == LOBBY_SESSION_NAME) Self->MembershipComplete(); }));
		if (bJoin) JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateLambda([Self](FName Name, EOnJoinSessionCompleteResult::Type) { if (Name == LOBBY_SESSION_NAME) Self->MembershipComplete(); }));
		if (bUpdate) UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
			FOnUpdateSessionCompleteDelegate::CreateLambda([Self](FName Name, bool)
			{
				if (Name != LOBBY_SESSION_NAME) return;
				Self->Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(Self->UpdateHandle);
				Self->UpdateHandle.Reset(); Self->bUpdatePending = false; Self->BeginCleanup();
			}));
		DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateLambda([Self](FName Name, bool)
			{
				if (Name != LOBBY_SESSION_NAME || !Self->bCleanupStarted) return;
				++Self->NativeCompleted; Self->TryFinish();
			}));
		if (Platform)
		{
			EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions Options = {};
			Options.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
			ClosedNotify = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(EOS_Platform_GetLobbyInterface(Platform), &Options, this,
				[](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data)
				{
					if (Data->CurrentStatus != EOS_ELobbyMemberStatus::EOS_LMS_CLOSED) return;
					auto* Drain = static_cast<FEEOSLateLobbyCleanup*>(Data->ClientData);
					const auto* Native = Drain->Sessions->GetNamedSession(LOBBY_SESSION_NAME);
					if (Drain->bCleanupStarted && Native && Native->SessionInfo.IsValid()
						&& Native->GetSessionIdStr() == UTF8_TO_TCHAR(Data->LobbyId) && Native->GetSessionIdStr() == Drain->LobbyId)
						++Drain->ExpectedNative;
				});
		}
		Watcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
		{
			IOnlineSubsystem* OSS = IOnlineSubsystem::DoesInstanceExist(Self->Instance) ? IOnlineSubsystem::Get(Self->Instance) : nullptr;
			if (!OSS || OSS->GetSessionInterface() != Self->Sessions) { Self->Finish(); return false; }
			if (!Self->bWarned && FPlatformTime::Seconds() - Self->Started > 15)
			{
				Self->bWarned = true;
				UE_LOG(LogExtendedEOS, Warning, TEXT("EOSLobbyDrain Request=%lld Lobby=%s BackendPending=%d NativeCompleted=%d NativeExpected=%d"),
					Self->Lease.GetRequestId(), *FEEOSNativeOperationLease::SafeField(Self->LobbyId), Self->bBackendPending, Self->NativeCompleted, Self->ExpectedNative);
			}
			Self->TryFinish(); return !Self->bFinished;
		}), 1.0f);
		if (!bMembershipPending && !bUpdatePending && !bExit) BeginCleanup();
		else if (bExit && !bBackendPending) BeginLocalCleanup();
	}
	void MembershipComplete()
	{
		const auto Self = AsShared();
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		CreateHandle.Reset(); JoinHandle.Reset(); bMembershipPending = false;
		BeginCleanup();
	}
	void BeginCleanup()
	{
		if (bFinished || bMembershipPending || bUpdatePending) return;
		const auto* Native = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
		bCleanupStarted = true;
		if (!Native || !Native->SessionInfo.IsValid()) { TryFinish(); return; }
		if (!LobbyId.IsEmpty() && LobbyId != Native->GetSessionIdStr()) { Finish(); return; }
		LobbyId = Native->GetSessionIdStr();
		const FString Puid = LocalUser.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalUser->ToString()) : FString();
		EOS_HLobby Lobby = Platform ? EOS_Platform_GetLobbyInterface(Platform) : nullptr;
		const bool bOwner = LocalUser.IsValid() && Native->OwningUserId.IsValid() && *LocalUser == *Native->OwningUserId;
		if (!bOwner || !Lobby || Puid.IsEmpty()) { BeginLocalCleanup(); return; }
		bBackendPending = true;
		EOS_Lobby_DestroyLobbyOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
		const FTCHARToUTF8 Id(*LobbyId); Options.LobbyId = Id.Get();
		Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
		using FContext = TSharedRef<FEEOSLateLobbyCleanup>;
		EOS_Lobby_DestroyLobby(Lobby, &Options, new FContext(AsShared()), [](const EOS_Lobby_DestroyLobbyCallbackInfo* Data)
		{
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FContext> Context(static_cast<FContext*>(Data->ClientData));
			const auto Self = *Context;
			OnLobbyGameThread([Self]() { Self->BackendComplete(); });
		});
	}
	void BackendComplete()
	{
		bBackendPending = false;
		if (bArmed && !bFinished) BeginLocalCleanup();
	}
	void BeginLocalCleanup()
	{
		if (bFinished) return;
		const auto* Native = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
		if (!Native) { TryFinish(); return; }
		if (!Native->SessionInfo.IsValid() || Native->GetSessionIdStr() != LobbyId) { Finish(); return; }
		if (Native->SessionState == EOnlineSessionState::Destroying)
		{
			ExpectedNative = FMath::Max(ExpectedNative, NativeCompleted + 1); return;
		}
		++ExpectedNative; const int32 Before = NativeCompleted;
		if (!Sessions->DestroySession(LOBBY_SESSION_NAME) && !bFinished && NativeCompleted == Before)
		{
			--ExpectedNative;
			// A refused cleanup must keep the lease until native state disappears or its OSS retires.
			UE_LOG(LogExtendedEOS, Warning, TEXT("EOSLobbyDrain Request=%lld Lobby=%s NativeCleanupRejected=1"), Lease.GetRequestId(), *FEEOSNativeOperationLease::SafeField(LobbyId));
		}
		TryFinish();
	}
	void TryFinish()
	{
		if (!bArmed || bFinished || bMembershipPending || bUpdatePending || bBackendPending || !bCleanupStarted
			|| NativeCompleted < ExpectedNative) return;
		if (!Sessions->GetNamedSession(LOBBY_SESSION_NAME)) Finish();
	}
	void Finish()
	{
		if (bFinished) return;
		const auto Self = AsShared(); bFinished = true;
		if (Sessions.IsValid())
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		}
		if (Platform && ClosedNotify != EOS_INVALID_NOTIFICATIONID)
			EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(EOS_Platform_GetLobbyInterface(Platform), ClosedNotify);
		ClosedNotify = EOS_INVALID_NOTIFICATIONID;
		if (Watcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Watcher);
		Watcher.Reset(); Lease.Reset(); Sessions.Reset(); Platform = nullptr; PlatformOwner.Reset();
	}
};


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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Lobby shutdown timed out; late create/join cleanup retained (create=%d, join=%d, exit=%d, promotion=%d, update=%d, followers=%d, queued=%d)."),
			CreateLobbyCompleteHandle.IsValid(), JoinLobbyCompleteHandle.IsValid(), ExitRequest.IsActive(), bPromotionPending, UpdateLobbyCompleteHandle.IsValid(), UpdateFollowers.Num(), QueuedUpdates.Num());
		if (!ExitDrain.IsValid())
		{
			ExitSDKResult.Empty();
	ExitDrain = MakeShared<FEEOSLateLobbyCleanup>();
			ExitDrain->Sessions = Sessions; ExitDrain->PlatformOwner = GetOwningEOSPlatform(); ExitDrain->Platform = Platform;
			ExitDrain->Instance = GetOwningEOSInstanceName(); ExitDrain->LobbyId = CurrentLobbyId;
			ExitDrain->Lease = MembershipLease.IsValid() ? MembershipLease : UpdateLease;
			const auto Identity = EOSSub->GetIdentityInterface();
			ExitDrain->LocalUser = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
		}
		ExitDrain->Arm(CreateLobbyCompleteHandle.IsValid(), JoinLobbyCompleteHandle.IsValid(), UpdateLobbyCompleteHandle.IsValid(),
			ExitRequest.IsActive(), ExitRequest.IsBackendPending(), ExpectedNativeExitCompletions, NativeExitCompletions);
	}
	return bComplete;
}

void UEEOSLobbySubsystem::Deinitialize()
{
	BeginEOSShutdown();
	if (UEEOSSearchCoordinator* Coordinator = GetSearchCoordinator()) Coordinator->Retire(LobbySearchOwner);
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

	if (OperationSessions.IsValid())
	{
		OperationSessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
		OperationSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
		OperationSessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
	}
	if (UpdateSessions.IsValid()) UpdateSessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteHandle);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
	UnbindLifetimeNotifications();
	RetireActiveLobbyUpdate(TEXT("Shutdown recipient retired; native result unobserved and cleanup lease retained."));

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

	CancelQueuedLobbyUpdates();
	if (OperationSessions.IsValid())
	{
		OperationSessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
		OperationSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
		OperationSessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
	}
	if (UpdateSessions.IsValid()) UpdateSessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteHandle);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
	MembershipLease.Reset(); UpdateLease.Reset(); OperationSessions.Reset(); UpdateSessions.Reset(); SearchSessions.Reset(); ExitDrain.Reset();
	CurrentLobbyId.Empty();
	CachedLobbyAttributes.Empty();
	LobbySearch.Reset();
	bInLobby = false;
	bDeinitialized = true;
	Super::Deinitialize();
}

// ── Lifetime notifications ───────────────────────────────────────────────────

void UEEOSLobbySubsystem::UnbindLifetimeNotifications()
{
	if (NotificationSessions.IsValid())
	{
		NotificationSessions->ClearOnSessionParticipantJoinedDelegate_Handle(ParticipantJoinedHandle);
		NotificationSessions->ClearOnSessionParticipantLeftDelegate_Handle(ParticipantLeftHandle);
		NotificationSessions->ClearOnSessionSettingsUpdatedDelegate_Handle(SessionSettingsUpdatedHandle);
		NotificationSessions->ClearOnDestroySessionCompleteDelegate_Handle(LifetimeDestroyHandle);
	}
	ParticipantJoinedHandle.Reset(); ParticipantLeftHandle.Reset(); SessionSettingsUpdatedHandle.Reset(); LifetimeDestroyHandle.Reset();
	NotificationSessions.Reset();
}
bool UEEOSLobbySubsystem::CanDeliverLobbyNotification(FName SessionName, bool bAllowRemoval) const
{
	if (bShuttingDown) { LogCallbackDisposition(TEXT("LobbyNotification"), 0, TEXT("ShutdownInternalOnly"), int64(LobbyGeneration)); return false; }
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	if (SessionName != LOBBY_SESSION_NAME || !OSS || OSS->GetSessionInterface() != NotificationSessions)
	{
		LogCallbackDisposition(TEXT("LobbyNotification"), 0, TEXT("DifferentOwner"), int64(LobbyGeneration)); return false;
	}
	const auto* Session = NotificationSessions.IsValid() ? NotificationSessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (!bInLobby || !IsEOSContextCurrent(LobbyContext) || (Session && (!Session->SessionInfo.IsValid() || Session->GetSessionIdStr() != CurrentLobbyId))
		|| (!Session && !bAllowRemoval))
	{
		LogCallbackDisposition(TEXT("LobbyNotification"), 0, TEXT("StaleGeneration"), int64(LobbyGeneration)); return false;
	}
	LogCallbackDisposition(TEXT("LobbyNotification"), 0, TEXT("Consumed"), int64(LobbyGeneration)); return true;
}
void UEEOSLobbySubsystem::RetireActiveLobbyUpdate(const FString& Reason)
{
	if (!PendingUpdateRequestId || UpdateOutcomeHistory.Contains(PendingUpdateRequestId)) return;
	auto Outcome = CompleteOperation(TEXT("UpdateLobby"), false, EEOSOperationCode::Canceled, Reason, CurrentLobbyId);
	Outcome.RequestId = PendingUpdateRequestId;
	Outcome.MembershipGeneration = int64(PendingUpdateGeneration);
	const auto Followers = MoveTemp(UpdateFollowers);
	PublishLobbyUpdateOutcome(Outcome);
	for (int64 Id : Followers) { auto Follower = Outcome; Follower.RequestId = Id; PublishLobbyUpdateOutcome(Follower); }
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyUpdate Request=%lld Phase=RecipientRetired Followers=%d NativeCompletionObserved=0 NativeLeaseRetained=%d Shutdown=%d"),
		Outcome.RequestId, Followers.Num(), UpdateLease.IsValid(), bShuttingDown);
	if (!bShuttingDown) { FEEOSOutcomeDispatchScope Dispatch(this, Outcome); OnOperationCompleted.Broadcast(Outcome); }
}
bool UEEOSLobbySubsystem::TryRegisterLifetimeNotifications()
{
	if (ParticipantJoinedHandle.IsValid())
	{
		IOnlineSubsystem* Existing = GetExistingEOSOnlineSubsystem();
		if (Existing && Existing->GetSessionInterface() == NotificationSessions) return true;
		UnbindLifetimeNotifications();
	}

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineSessionPtr SessionInterface = EOSSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			NotificationSessions = SessionInterface;
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
	if (IsOwningEOSCreationExhausted())
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
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	return Coordinator && OSS && Coordinator->TryAcquire(LobbySearchOwner, OSS->GetSessionInterface(), this, TEXT("FindLobbies"));
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
	if (!PreviousLobbyId.IsEmpty()) ++LobbyGeneration;
	CurrentLobbyId.Empty();
	bInLobby = false;
	JoinedLobbyAwaitingMemberList.Empty();
	ConfirmedVisibility = FEEOSLobbyVisibilitySnapshot();
	bPromotionPending = false; ++PromotionToken; PromotionLobbyId.Empty();
	CachedLobbyAttributes.Empty();
	CachedLobbyOwnerId.Empty();
	PendingKickedPuids.Empty();
	InFlightKickPuids.Empty();
	RetireActiveLobbyUpdate(TEXT("Lobby generation retired before native update completion."));
	CancelQueuedLobbyUpdates();
	return PreviousLobbyId;
}

// ── Create / Join / Leave ────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::CreateLobby(int32 MaxMembers, bool bIsPublic, bool bUseVoiceChat)
{
	if (IsMembershipOperationInFlight() || bShuttingDown)
	{
		RejectOperation(TEXT("CreateLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is in flight."));
		return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (MaxMembers < 1 || MaxMembers > EOS_LOBBY_MAX_LOBBY_MEMBERS || !Local.IsValid() || !Local->IsValid()
		|| Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn
		|| UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()).IsEmpty())
	{
		const auto Outcome = CompleteOperation(TEXT("CreateLobby"), false,
			MaxMembers < 1 || MaxMembers > EOS_LOBBY_MAX_LOBBY_MEMBERS ? EEOSOperationCode::InvalidInput : EEOSOperationCode::IdentityUnavailable,
			TEXT("Lobby creation requires valid capacity and a logged-in Product User ID."), CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyCreated.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}
	if (!AcquireLobbyMembership(TEXT("CreateLobby"))) return false;
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = MaxMembers;
	Settings.bShouldAdvertise = bIsPublic; Settings.bUsesPresence = true;
	Settings.bAllowJoinInProgress = true; Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true; Settings.bUseLobbiesIfAvailable = true;
	Settings.bUseLobbiesVoiceChatIfAvailable = bUseVoiceChat;
	const UEEOSSettings* Defaults = GetEOSSettings();
	Settings.Set(SETTING_HOST_MIGRATION, !Defaults || Defaults->bAllowLobbyHostMigration, EOnlineDataAdvertisementType::DontAdvertise);
	PendingCreateLobbySettings = Settings;
	if (OperationSessions->GetNamedSession(LOBBY_SESSION_NAME))
	{
		RefreshLobbyState(OperationSessions->GetNamedSession(LOBBY_SESSION_NAME));
		SetOperationPhase(TEXT("CreateLobby"), TEXT("ClosingExisting"));
		if (!StartLobbyExit(OperationSessions, IsLobbyOwner(), FEEOSLobbyExitRequest::EContinuation::Create))
		{
			FinishLobbyCreation(false, EEOSOperationCode::ExistingLobbyCloseFailed, TEXT("Could not close the existing lobby."));
			return false;
		}
		return true;
	}
	return SubmitLobbyCreation();
}

bool UEEOSLobbySubsystem::FindLobbies(int32 MaxResults)
{
	return FindLobbiesFiltered(MaxResults, TMap<FString, FString>());
}

bool UEEOSLobbySubsystem::FindLobbiesFiltered(int32 MaxResults, const TMap<FString, FString>& SearchFilters)
{
	if (bShuttingDown || FindLobbiesCompleteHandle.IsValid())
	{
		RejectOperation(TEXT("FindLobbies"), EEOSOperationCode::Busy, TEXT("Lobby search is already pending or shutting down."));
		return false;
	}
	const auto Fail = [this](EEOSOperationCode Code, const FString& Message)
	{
		const auto Outcome = CompleteOperation(TEXT("FindLobbies"), false, Code, Message);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbiesFound.Broadcast(TArray<FEEOSSessionSearchResult>());
		OnOperationCompleted.Broadcast(Outcome);
		return false;
	};
	if (MaxResults < 1 || MaxResults > EOS_LOBBY_MAX_SEARCH_RESULTS)
		return Fail(EEOSOperationCode::InvalidInput, TEXT("Lobby search limit is outside the SDK range."));
	for (const auto& Filter : SearchFilters)
		if (Filter.Key.IsEmpty() || FName(*Filter.Key) == FName(TEXT("LOBBYSEARCH")) || FName(*Filter.Key) == FName(TEXT("PRESENCESEARCH")) || FTCHARToUTF8(*Filter.Key).Length() > EOS_LOBBYMODIFICATION_MAX_ATTRIBUTE_LENGTH)
			return Fail(EEOSOperationCode::InvalidInput, TEXT("A search attribute name is empty or exceeds the SDK limit."));
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	if (!Sessions.IsValid()) return Fail(EEOSOperationCode::UnsupportedCapability, TEXT("EOS session search interface is unavailable."));
	if (!TryAcquireSearchSlot())
	{
		RejectOperation(TEXT("FindLobbies"), EEOSOperationCode::Busy, TEXT("The native search interface has another pending search."));
		return false;
	}
	const int64 Token = BeginOperation(TEXT("FindLobbies"), FString(), GetSearchCoordinator()->GetRequestId());
	SearchSessions = Sessions; SearchContext = CaptureEOSContext();
	LobbySearchGeneration = Token;
	LobbySearchTime = FPlatformTime::Seconds();
	LobbySearch = MakeShared<FOnlineSessionSearch>();
	LobbySearch->MaxSearchResults = MaxResults;
	LobbySearch->bIsLanQuery = false;
	LobbySearch->QuerySettings.Set(FName(TEXT("LOBBYSEARCH")), true, EOnlineComparisonOp::Equals);
	for (const auto& Filter : SearchFilters)
		LobbySearch->QuerySettings.Set(FName(*Filter.Key), Filter.Value, EOnlineComparisonOp::Equals);
	FindLobbiesCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleFindSessionsComplete));
	const auto SubmittedSearch = LobbySearch.ToSharedRef();
	const bool bStarted = Sessions->FindSessions(0, SubmittedSearch);
	if (!bStarted && FindLobbiesCompleteHandle.IsValid() && LobbySearchGeneration == Token)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
		FindLobbiesCompleteHandle.Reset(); SearchSessions.Reset(); LobbySearch.Reset(); ReleaseSearchSlot();
		return Fail(EEOSOperationCode::NativeStartRejected, TEXT("Native lobby search refused submission."));
	}
	return bStarted;
}

bool UEEOSLobbySubsystem::IsMembershipOperationInFlight() const
{
	return CreateLobbyCompleteHandle.IsValid() || ExitRequest.IsActive() || bPromotionPending || JoinRequest.IsActive() || UpdateLobbyCompleteHandle.IsValid();
}

bool UEEOSLobbySubsystem::JoinLobby(int32 SearchResultIndex)
{
	if (bShuttingDown || IsMembershipOperationInFlight())
	{
		RejectOperation(TEXT("JoinLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is pending."));
		return false;
	}
	if (!LobbySearch.IsValid() || !LobbySearch->SearchResults.IsValidIndex(SearchResultIndex))
	{
		LastLobbyJoinError = FString::Printf(TEXT("Invalid lobby search result index: %d"), SearchResultIndex);
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: %s"), *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError));
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::InvalidTarget, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}
	bJoinTargetAuthorizedDetails = false;
	return BeginJoinLobby(LobbySearch->SearchResults[SearchResultIndex]);
}

bool UEEOSLobbySubsystem::JoinLobbyById(const FString& LobbyId)
{
	if (bShuttingDown || IsMembershipOperationInFlight())
	{
		RejectOperation(TEXT("JoinLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is pending."));
		return false;
	}
	const int32 Index = LobbySearch.IsValid() && !LobbyId.IsEmpty()
		? LobbySearch->SearchResults.IndexOfByPredicate([&LobbyId](const FOnlineSessionSearchResult& Result)
			{ return Result.IsValid() && Result.GetSessionIdStr() == LobbyId; })
		: INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		LastLobbyJoinError = TEXT("That lobby is not in the last search results; search again.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: %s Target=%s"), *LastLobbyJoinError, *FEEOSNativeOperationLease::SafeField(LobbyId));
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::StaleResult, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}
	return JoinLobby(Index);
}

bool UEEOSLobbySubsystem::JoinLobbyResult(const FOnlineSessionSearchResult& SearchResult)
{
	if (bShuttingDown || IsMembershipOperationInFlight())
	{
		RejectOperation(TEXT("JoinLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is pending."));
		return false;
	}
	bJoinTargetAuthorizedDetails = true; // Caller retained native invite/details; native backend still enforces authorization.
	return BeginJoinLobby(SearchResult);
}

bool UEEOSLobbySubsystem::BeginJoinLobby(const FOnlineSessionSearchResult& SearchResult)
{
	LastLobbyJoinError.Empty();
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = EOSSub ? EOSSub->GetSessionInterface() : IOnlineSessionPtr();
	if (bShuttingDown || !Sessions.IsValid() || !SearchResult.IsValid() || !SearchResult.Session.SessionInfo.IsValid())
	{
		LastLobbyJoinError = bShuttingDown ? TEXT("Lobby subsystem is shutting down.")
			: !Sessions.IsValid() ? TEXT("EOS session interface is unavailable.") : TEXT("Lobby search result is invalid.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError));
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::InvalidTarget, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		MembershipLease.Reset(); OperationSessions.Reset();
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}

	const IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	const FUniqueNetIdPtr Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	const FEEOSCapacitySnapshot Capacity = EEOSCapacity::Read(SearchResult.Session, EEOSSessionBackend::Lobby);
	const bool bFromSearch = LobbySearch.IsValid() && LobbySearch->SearchResults.ContainsByPredicate([&](const auto& Item)
		{ return Item.Session.SessionInfo == SearchResult.Session.SessionInfo; });
	const double Age = bFromSearch ? FPlatformTime::Seconds() - LobbySearchTime : -1;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyJoin Target=%s CapacityKnown=%d Max=%d Members=%d Slots=%d Source=%s SearchAgeSeconds=%.2f"),
		*FEEOSNativeOperationLease::SafeField(SearchResult.GetSessionIdStr()), Capacity.bKnown, Capacity.Maximum, Capacity.Members, Capacity.AvailableSlots, *FEEOSNativeOperationLease::SafeField(Capacity.Source), Age);
	if (!Local.IsValid() || !Local->IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn
		|| UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()).IsEmpty())
	{
		LastLobbyJoinError = TEXT("A logged-in Product User ID is required.");
		BeginOperation(TEXT("JoinLobby"), SearchResult.GetSessionIdStr());
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::IdentityUnavailable, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}
	if (!AcquireLobbyMembership(TEXT("JoinLobby"), SearchResult.GetSessionIdStr())) return false;
	const FNamedOnlineSession* Existing = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
	RefreshLobbyState(Existing);
	TagOperationContext(TEXT("JoinLobby"), CurrentLobbyId, int64(LobbyGeneration), bFromSearch ? LobbySearchGeneration : 0);
	SetOperationPhase(TEXT("JoinLobby"), TEXT("Preflight"));
	if (Existing && (Existing->SessionState == EOnlineSessionState::Creating || Existing->SessionState == EOnlineSessionState::Destroying))
	{
		LastLobbyJoinError = TEXT("Existing lobby session is still creating or leaving; retry after it completes.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError));
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::InvalidTarget, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		MembershipLease.Reset(); OperationSessions.Reset();
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}
	if (Existing && Existing->SessionInfo.IsValid() && Existing->SessionInfo->GetSessionId() == SearchResult.Session.SessionInfo->GetSessionId())
	{
		LastLobbyJoinError = TEXT("Already in the requested lobby.");
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join rejected: %s"), *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError));
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), false, EEOSOperationCode::AlreadyInLobby, LastLobbyJoinError, CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		MembershipLease.Reset(); OperationSessions.Reset();
		OnLobbyJoined.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		return false;
	}

	if (bFromSearch && Capacity.bKnown && Capacity.AvailableSlots == 0 && Age <= 5)
	{
		FinishJoiningLobby(false, TEXT("The recent target capacity snapshot reports no available member slots; current membership was preserved."), EEOSOperationCode::LobbyFull);
		return false;
	}

	FOnlineSessionSearchResult Target = SearchResult;
	Target.Session.SessionSettings.bUsesPresence = true;
	Target.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	JoinRequest.Begin(Target, Existing != nullptr);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join requested target='%s', existing='%s', state=%d, cachedMember=%d"),
		*FEEOSNativeOperationLease::SafeField(Target.GetSessionIdStr()), *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), Existing ? int32(Existing->SessionState) : int32(EOnlineSessionState::NoSession), bInLobby);
	return RefreshJoinTargetCapacity(Existing ? CurrentLobbyId : FString());
}

bool UEEOSLobbySubsystem::StartJoiningLobby()
{
	const IOnlineSessionPtr Sessions = OperationSessions;
	if (bShuttingDown || !Sessions.IsValid() || !IsEOSContextCurrent(MembershipContext))
	{
		FinishJoiningLobby(false, TEXT("Original identity/platform retired before joining the lobby."), EEOSOperationCode::Canceled);
		return false;
	}
	JoinLobbyCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleJoinSessionComplete));
	// A synchronous callback may reset JoinRequest. Keep the argument alive independently.
	const FOnlineSessionSearchResult Target = JoinRequest.GetResult();
	const int64 Token = MembershipLease.GetRequestId();
	SetOperationPhase(TEXT("JoinLobby"), TEXT("JoiningTarget"));
	if (!Sessions->JoinSession(0, LOBBY_SESSION_NAME, Target) && JoinRequest.IsJoining() && MembershipLease.GetRequestId() == Token)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
		JoinLobbyCompleteHandle.Reset();
		FinishJoiningLobby(false, TEXT("EOS refused to start the lobby join."), EEOSOperationCode::NativeStartRejected);
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::IsLocalLobbyMember(const FNamedOnlineSession* Session, const FUniqueNetIdPtr& Local) const
{
	if (!Session || !Local.IsValid() || !Local->IsValid()) return false;
	if (Session->OwningUserId.IsValid() && *Session->OwningUserId == *Local) return true;
	if (Session->SessionSettings.MemberSettings.Contains(Local.ToSharedRef())) return true;
	return !JoinedLobbyAwaitingMemberList.IsEmpty() && Session->SessionInfo.IsValid()
		&& Session->GetSessionIdStr() == JoinedLobbyAwaitingMemberList;
}

void UEEOSLobbySubsystem::RefreshLobbyState(const FNamedOnlineSession* Session)
{
	const auto Context = CaptureEOSContext();
	const auto Local = Context.Identity.IsValid() ? Context.Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const bool bLocalMember = !Context.LocalId.IsEmpty() && IsLocalLobbyMember(Session, Local);
	// Once the member list has caught up with the join, or the joined lobby is gone, the
	// ordinary rule applies again.
	if (!JoinedLobbyAwaitingMemberList.IsEmpty() && (!Session || !Session->SessionInfo.IsValid()
		|| Session->GetSessionIdStr() != JoinedLobbyAwaitingMemberList
		|| (Local.IsValid() && Session->SessionSettings.MemberSettings.Contains(Local.ToSharedRef()))))
	{
		JoinedLobbyAwaitingMemberList.Empty();
	}
	const bool bNativeReady = (bLocalMember || bShuttingDown) && Session && Session->SessionInfo.IsValid() && Session->SessionInfo->IsValid()
		&& Session->SessionState != EOnlineSessionState::NoSession && Session->SessionState != EOnlineSessionState::Creating;
	const FString NativeId = bNativeReady ? Session->GetSessionIdStr() : FString();
	if (bInLobby != bNativeReady || CurrentLobbyId != NativeId)
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyReconcile Generation=%llu PreviousCachedMember=%d NativeMember=%d PreviousId=%s NativeId=%s NativeState=%d"),
			LobbyGeneration, bInLobby, bNativeReady, *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), *FEEOSNativeOperationLease::SafeField(NativeId), Session ? int32(Session->SessionState) : int32(EOnlineSessionState::NoSession));
	if (!bNativeReady) { ResetLobbyState(); return; }
	const bool bNewLobby = CurrentLobbyId != NativeId;
	if (bNewLobby) { ++LobbyGeneration; LobbyContext = CaptureEOSContext(); }
	bInLobby = true; CurrentLobbyId = NativeId;
	if (bNewLobby) CachedLobbyOwnerId = Session->OwningUserId.IsValid() ? Session->OwningUserId->ToString() : FString();
	if (bNewLobby)
	{
		ConfirmedVisibility.bKnown = true; ConfirmedVisibility.bShouldAdvertise = Session->SessionSettings.bShouldAdvertise;
		ConfirmedVisibility.LobbyId = CurrentLobbyId; ConfirmedVisibility.Generation = int64(LobbyGeneration);
		RefreshCachedLobbyAttributes(Session->SessionSettings, false);
	}
}

void UEEOSLobbySubsystem::FinishJoiningLobby(bool bSuccess, const FString& Error, EEOSOperationCode Code, const FString& NativeResult, EEOSResultSource Source, const FString& SDKResult)
{
	const IOnlineSessionPtr Sessions = OperationSessions;
	const FString TargetId = JoinRequest.GetResult().IsValid() ? JoinRequest.GetResult().GetSessionIdStr() : FString();
	if (Sessions.IsValid()) RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	// The success callback must still correspond to a usable named lobby.
	const bool bContextCurrent = bShuttingDown || IsEOSContextCurrent(MembershipContext);
	if (!bContextCurrent) Code = EEOSOperationCode::Canceled;
	bSuccess = bSuccess && bContextCurrent && Sessions.IsValid() && bInLobby && CurrentLobbyId == TargetId;
	JoinRequest.Reset();
	LastLobbyJoinError = !bContextCurrent ? TEXT("Original identity/platform retired before join completion.") : !bSuccess && Error.IsEmpty() ? TEXT("Join completed without a usable named lobby session.") : Error;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Join %s target='%s', current='%s', member=%d, error='%s'"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"), *FEEOSNativeOperationLease::SafeField(TargetId), *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), bInLobby, *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError));
	MembershipLease.Reset(); OperationSessions.Reset();
	{
		const auto Outcome = CompleteOperation(TEXT("JoinLobby"), bSuccess, bSuccess ? EEOSOperationCode::Succeeded : Code, LastLobbyJoinError, CurrentLobbyId, NativeResult, Source, SDKResult);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		if (!bShuttingDown) { OnLobbyJoined.Broadcast(bSuccess, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome); }
	}
	// A join reported as failed must not leave the player inside the lobby it targeted: every
	// retry would then be refused as "Already in the requested lobby". Skipped when a listener
	// has already started other membership work.
	if (!bSuccess && bContextCurrent && !bShuttingDown && bInLobby && !TargetId.IsEmpty() && CurrentLobbyId == TargetId
		&& !IsMembershipOperationInFlight())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Join of '%s' failed but left this player in it; leaving so another join can start."),
			*FEEOSNativeOperationLease::SafeField(TargetId));
		LeaveLobby();
	}
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
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: %s; leaving lobby '%s' so another join can start"), *FEEOSNativeOperationLease::SafeField(LastLobbyJoinError), *FEEOSNativeOperationLease::SafeField(CurrentLobbyId));
	LeaveLobby();
}

bool UEEOSLobbySubsystem::LeaveLobby()
{
	if (IsMembershipOperationInFlight() || bShuttingDown)
	{
		RejectOperation(TEXT("LeaveLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is pending.")); return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	if (!Sessions.IsValid() || !bInLobby)
	{
		const auto Outcome = CompleteOperation(TEXT("LeaveLobby"), false, EEOSOperationCode::InvalidTarget, TEXT("A usable native lobby is required."), CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome); return false;
	}
	if (!AcquireLobbyMembership(TEXT("LeaveLobby"), CurrentLobbyId)) return false;
	const int64 Token = MembershipLease.GetRequestId();
	if (!StartLobbyExit(Sessions, false, FEEOSLobbyExitRequest::EContinuation::None))
	{
		if (MembershipLease.GetRequestId() == Token)
		{
			MembershipLease.Reset(); OperationSessions.Reset();
			const auto Outcome = CompleteOperation(TEXT("LeaveLobby"), false, EEOSOperationCode::NativeStartRejected, TEXT("Lobby exit could not start."), CurrentLobbyId);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			OnLobbyDestroyed.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		}
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::DestroyLobby()
{
	if (IsMembershipOperationInFlight() || bShuttingDown)
	{
		RejectOperation(TEXT("DestroyLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership/update work is pending.")); return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	if (Sessions.IsValid()) RefreshLobbyState(Sessions->GetNamedSession(LOBBY_SESSION_NAME));
	if (!Sessions.IsValid() || !bInLobby || !IsLobbyOwner())
	{
		const auto Outcome = CompleteOperation(TEXT("DestroyLobby"), false, EEOSOperationCode::InvalidTarget, TEXT("A usable owned native lobby is required."), CurrentLobbyId);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyDestroyed.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome); return false;
	}
	if (!AcquireLobbyMembership(TEXT("DestroyLobby"), CurrentLobbyId)) return false;
	const int64 Token = MembershipLease.GetRequestId();
	if (!StartLobbyExit(Sessions, true, FEEOSLobbyExitRequest::EContinuation::None))
	{
		if (MembershipLease.GetRequestId() == Token)
		{
			MembershipLease.Reset(); OperationSessions.Reset();
			const auto Outcome = CompleteOperation(TEXT("DestroyLobby"), false, EEOSOperationCode::NativeStartRejected, TEXT("Lobby exit could not start."), CurrentLobbyId);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			OnLobbyDestroyed.Broadcast(false, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome);
		}
		return false;
	}
	return true;
}

bool UEEOSLobbySubsystem::StartLobbyExit(const IOnlineSessionPtr& Sessions, bool bDeleteBackend, FEEOSLobbyExitRequest::EContinuation Continuation)
{
	if (!bShuttingDown && !IsEOSContextCurrent(MembershipContext)) return false;
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
	if (!MembershipLease.IsValid() && !AcquireLobbyMembership(bShuttingDown ? TEXT("ShutdownLobby") : TEXT("DestroyLobby"))) return false;
	ExitRequest.Begin(CurrentLobbyId, bDeleteBackend, Continuation);
	ExitChildRequestId = FEEOSNativeOperationLease::NextRequestId();
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyExit Request=%lld ParentRequest=%lld Generation=%llu Lobby=%s BackendDelete=%d Continuation=%d"),
		ExitChildRequestId, MembershipLease.GetRequestId(), LobbyGeneration, *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), bDeleteBackend, int32(Continuation));
	CancelQueuedLobbyUpdates();
	SetOperationPhase(MembershipLease.GetOperation(), bDeleteBackend ? FName(TEXT("ClosingExisting")) : FName(TEXT("NativeCleanup")));
	ExpectedNativeExitCompletions = 0;
	NativeExitCompletions = 0;
	if (bDeleteBackend)
	{
		ExitPlatformOwner = GetOwningEOSPlatform();
		ExitPlatform = ExitPlatformOwner.IsValid() ? static_cast<EOS_HPlatform>(*ExitPlatformOwner) : nullptr;
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
			ExitPlatform = nullptr; ExitPlatformOwner.Reset();
			return false;
		}
	}
	ExitSDKResult.Empty();
	ExitDrain = MakeShared<FEEOSLateLobbyCleanup>();
	ExitDrain->Sessions = Sessions; ExitDrain->Lease = MembershipLease; ExitDrain->Instance = GetOwningEOSInstanceName();
	ExitDrain->PlatformOwner = GetOwningEOSPlatform(); ExitDrain->Platform = GetPlatformHandle(); ExitDrain->LobbyId = CurrentLobbyId;
	if (IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem())
	{
		const auto Identity = OSS->GetIdentityInterface();
		ExitDrain->LocalUser = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	}
	// Bind BEFORE deleting: EOS_LMS_CLOSED may cause native cleanup before the SDK result arrives.
	DestroyLobbyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleDestroySessionComplete));
	if (!bDeleteBackend)
	{
		StartNativeLobbyCleanup(Sessions);
		return true;
	}
	struct FDeleteContext { TWeakObjectPtr<UEEOSLobbySubsystem> Self; TSharedRef<FEEOSLateLobbyCleanup> Drain; FString LobbyId; uint64 Token; };
	FDeleteContext* Context = new FDeleteContext{this, ExitDrain.ToSharedRef(), CurrentLobbyId, ExitRequest.GetToken()};
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
		OnLobbyGameThread([Weak = Ctx->Self, Drain = Ctx->Drain, Id = Ctx->LobbyId, Token = Ctx->Token, bDeleted, SDKResult = FString(ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)))]()
		{
			UEEOSLobbySubsystem* Self = Weak.Get();
			if (Self && !Self->bDeinitialized && !Drain->bArmed && Self->ExitRequest.Matches(Token, Id)) Self->HandleBackendLobbyDeleted(Token, Id, bDeleted, SDKResult);
			else Drain->BackendComplete();
		});
	});
	return true;
}

void UEEOSLobbySubsystem::HandleBackendLobbyDeleted(uint64 Token, const FString& LobbyId, bool bDeleted, const FString& SDKResult)
{
	if (!ExitRequest.CompleteBackend(Token, LobbyId, bDeleted)) return;
	ExitSDKResult = SDKResult;
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyExit Request=%lld Lobby=%s BackendDeleted=%d SDK=%s NativeCallbacks=%d/%d"),
		MembershipLease.GetRequestId(), *FEEOSNativeOperationLease::SafeField(LobbyId), bDeleted, *FEEOSNativeOperationLease::SafeField(SDKResult), NativeExitCompletions, ExpectedNativeExitCompletions);
	const IOnlineSessionPtr Sessions = OperationSessions;
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
	const IOnlineSessionPtr Sessions = OperationSessions;
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
	ExitPlatformOwner.Reset();
	ExpectedNativeExitCompletions = 0;
	NativeExitCompletions = 0;
}

void UEEOSLobbySubsystem::StartNativeLobbyCleanup(const IOnlineSessionPtr& Sessions)
{
	SetOperationPhase(MembershipLease.GetOperation(), TEXT("NativeCleanup"));
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSLobbyCleanup Request=%lld ParentRequest=%lld Phase=NativeCleanup ExpectedCallbacks=%d ReceivedCallbacks=%d"), ExitChildRequestId, MembershipLease.GetRequestId(), ExpectedNativeExitCompletions, NativeExitCompletions);
	const FNamedOnlineSession* Session = Sessions->GetNamedSession(LOBBY_SESSION_NAME);
	if (!Session) { FinishLobbyExit(false); return; }
	if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// An externally started leave has no CLOSED notification to account for it.
		ExpectedNativeExitCompletions = FMath::Max(ExpectedNativeExitCompletions, 1);
		return;
	}
	++ExpectedNativeExitCompletions;
	const uint64 Token = ExitRequest.GetToken();
	const FString Id = ExitRequest.GetLobbyId();
	const int32 BeforeCallbacks = NativeExitCompletions;
	if (!Sessions->DestroySession(LOBBY_SESSION_NAME) && ExitRequest.Matches(Token, Id) && NativeExitCompletions == BeforeCallbacks)
	{
		--ExpectedNativeExitCompletions;
		FinishLobbyExit(false);
	}
}

void UEEOSLobbySubsystem::FinishLobbyExit(bool bNativeSuccess)
{
	if (!ExitRequest.IsActive() || ExitRequest.IsBackendPending() || NativeExitCompletions < ExpectedNativeExitCompletions) return;
	const IOnlineSessionPtr Sessions = OperationSessions;
	if (Sessions.IsValid()) Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyLobbyCompleteHandle);
	DestroyLobbyCompleteHandle.Reset();
	const auto Continuation = ExitRequest.GetContinuation();
	const FString ClosedId = ExitRequest.GetLobbyId();
	const FString SDKResult = MoveTemp(ExitSDKResult);
	const FString NativeResult = NativeExitCompletions > 0 ? (bNativeSuccess ? TEXT("Success") : TEXT("Failure")) : FString();
	const EEOSResultSource Source = !SDKResult.IsEmpty() ? EEOSResultSource::SDKCallback : NativeExitCompletions > 0 ? EEOSResultSource::NativeCallback : EEOSResultSource::Plugin;
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	const bool bSuccess = Sessions.IsValid() && ExitRequest.Succeeded(bNativeSuccess, Session != nullptr);
	RefreshLobbyState(Session);
	RemoveExitCloseNotification();
	ExitRequest.Reset();
	if (ExitDrain.IsValid()) ExitDrain->Finish();
	ExitDrain.Reset();
	if (bShuttingDown)
	{
		CompleteOperation(MembershipLease.GetOperation(), bSuccess, bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::Canceled, TEXT("Shutdown lobby cleanup retired."));
		JoinRequest.Reset(); MembershipLease.Reset(); OperationSessions.Reset(); return;
	}
	if (Continuation == FEEOSLobbyExitRequest::EContinuation::Join)
	{
		if (!JoinRequest.CompleteLeave(bSuccess, Session != nullptr)) FinishJoiningLobby(false, TEXT("Existing lobby close failed; the new join was not started."));
		else StartJoiningLobby();
	}
	else if (Continuation == FEEOSLobbyExitRequest::EContinuation::Create)
	{
		if (!bSuccess) { FinishLobbyCreation(false, EEOSOperationCode::ExistingLobbyCloseFailed, TEXT("Existing lobby close failed.")); return; }
		SubmitLobbyCreation();
	}
	else
	{
		const FName Operation = MembershipLease.GetOperation();
		MembershipLease.Reset(); OperationSessions.Reset();
		const auto Outcome = CompleteOperation(Operation, bSuccess, bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure, bSuccess ? TEXT("Lobby exit completed.") : TEXT("Lobby exit failed; native membership was reconciled."), CurrentLobbyId, NativeResult, Source, SDKResult);
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnLobbyDestroyed.Broadcast(bSuccess, ClosedId); OnOperationCompleted.Broadcast(Outcome);
	}
}

// ── Lobby Attributes ─────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyAttribute(const FString& Key, const FString& Value)
{
	return StartLobbyUpdate(EPendingLobbyUpdate::LobbyAttribute, Key, Value);
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
	return StartLobbyUpdate(EPendingLobbyUpdate::MemberAttribute, Key, Value);
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
	const auto Capacity = GetLobbyCapacity();
	return Capacity.bKnown ? Capacity.Members : -1;
}

FEEOSCapacitySnapshot UEEOSLobbySubsystem::GetLobbyCapacity() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	const auto* Native = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	FEEOSCapacitySnapshot Result; Result.Backend = EEOSSessionBackend::Lobby;
	if (!IsInLobby() || !Native) { Result.Source = TEXT("NativeLobbyUnavailable"); Result.UnknownReason = TEXT("NoUsableLocalMembership"); return Result; }
	Result = EEOSCapacity::Read(*Native, EEOSSessionBackend::Lobby);
	const auto Platform = GetOwningEOSPlatform();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const FString Puid = Local.IsValid() && Local->IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()) : FString();
	if (!Platform.IsValid() || Puid.IsEmpty()) return Result;
	EOS_Lobby_CopyLobbyDetailsHandleOptions Copy = {};
	Copy.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
	const FTCHARToUTF8 Id(*Native->GetSessionIdStr()); Copy.LobbyId = Id.Get();
	Copy.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
	EOS_HLobbyDetails Details = nullptr;
	const auto Copied = EOS_Lobby_CopyLobbyDetailsHandle(EOS_Platform_GetLobbyInterface(*Platform), &Copy, &Details);
	if (Copied == EOS_EResult::EOS_Success && Details)
	{
		EOS_LobbyDetails_CopyInfoOptions Options = {}; Options.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
		EOS_LobbyDetails_Info* Info = nullptr;
		if (EOS_LobbyDetails_CopyInfo(Details, &Options, &Info) == EOS_EResult::EOS_Success && Info)
		{
			Result.Source = TEXT("EOSLobbyCachedInfo");
			Result.Permission = EEOSCapacity::ReadPermission(Info->PermissionLevel);
			Result.bConsistent = Info->MaxMembers > 0 && Info->MaxMembers <= EOS_LOBBY_MAX_LOBBY_MEMBERS && Info->AvailableSlots <= Info->MaxMembers;
			Result.bKnown = Result.bConsistent;
			Result.UnknownReason = Result.bKnown ? FString() : TEXT("SDK maximum/available slot data is inconsistent.");
			Result.Maximum = Result.bKnown ? int32(Info->MaxMembers) : -1;
			Result.AvailableSlots = Result.bKnown ? int32(Info->AvailableSlots) : -1;
			Result.Members = Result.bKnown ? Result.Maximum - Result.AvailableSlots : -1;
		}
		if (Info) EOS_LobbyDetails_Info_Release(Info);
	}
	if (Details) EOS_LobbyDetails_Release(Details);
	return Result;
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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Only the lobby owner can kick members (EOS_Lobby_KickMember is owner-only)"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HLobby LobbyHandle = PlatformHandle ? EOS_Platform_GetLobbyInterface(PlatformHandle) : nullptr;
	if (!LobbyHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("KickMember"), TEXT("CapabilityUnavailable"));
		return false;
	}

	if (CurrentLobbyId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — No lobby id cached"));
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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Local user has no Product User ID (no Connect session?)"));
		return false;
	}

	const FString TargetPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(MemberId);
	if (TargetPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Target '%s' has no Product User ID"), *FEEOSNativeOperationLease::SafeField(MemberId));
		return false;
	}

	// Per-target in-flight guard: double-kicking the same member would double-broadcast
	// OnLobbyMemberLeft (once per SDK completion). In-flight rejection: detailed record only, no legacy completion.
	if (InFlightKickPuids.Contains(TargetPUIDStr))
	{
		RejectOperation(TEXT("KickMember"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	const FTCHARToUTF8 Utf8LobbyId(*CurrentLobbyId);

	EOS_Lobby_KickMemberOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_KICKMEMBER_API_LATEST;
	Options.LobbyId = (EOS_LobbyId)Utf8LobbyId.Get();
	Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));
	Options.TargetUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*TargetPUIDStr));
	if (EOS_ProductUserId_IsValid(Options.LocalUserId) != EOS_TRUE || EOS_ProductUserId_IsValid(Options.TargetUserId) != EOS_TRUE)
	{ RejectOperation(TEXT("KickMember"), EEOSOperationCode::InvalidTarget, TEXT("Kick requires valid local and target Product User IDs.")); return false; }

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
		uint64 Generation;
		FEEOSRequestContext Context;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
	};
	FKickContext* Context = new FKickContext{ this, MemberId, TargetPUIDStr, CurrentLobbyId, LobbyGeneration, CaptureEOSContext(), GetOwningEOSPlatform() };

	EOS_Lobby_KickMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_KickMemberCallbackInfo* Data)
		{
			// Retrying callbacks retain ClientData until the SDK reports a terminal result.
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FKickContext> Ctx(static_cast<FKickContext*>(Data->ClientData));
			if (!Ctx) return;

			AsyncTask(ENamedThreads::GameThread,
				[WeakSelf = Ctx->Self, KickedMemberId = MoveTemp(Ctx->MemberId), TargetPuid = MoveTemp(Ctx->TargetPuid), LobbyId = MoveTemp(Ctx->LobbyId), Generation = Ctx->Generation, Context = Ctx->Context, ResultCode = Data->ResultCode]()
				{
					UEEOSLobbySubsystem* Self = WeakSelf.Get();
					if (!Self) return;
					if (Self->bShuttingDown || !Self->bInLobby || Self->CurrentLobbyId != LobbyId || Self->LobbyGeneration != Generation || !Self->IsEOSContextCurrent(Context))
					{ Self->LogCallbackDisposition(TEXT("KickLobbyMember"), 0, Self->bShuttingDown ? TEXT("ShutdownInternalOnly") : TEXT("StaleGeneration"), int64(Generation)); return; }
					Self->LogCallbackDisposition(TEXT("KickLobbyMember"), 0, TEXT("Consumed"), int64(Generation));

					Self->InFlightKickPuids.Remove(TargetPuid);

					if (ResultCode == EOS_EResult::EOS_Success)
					{
						const auto Capacity = Self->GetLobbyCapacity();
						UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyMembers Lobby=%s Generation=%llu Change=SDKKickAccepted Known=%d Members=%d Max=%d Slots=%d Source=%s UnknownReason=%s"),
							*FEEOSNativeOperationLease::SafeField(LobbyId), Generation, Capacity.bKnown, Capacity.Members, Capacity.Maximum, Capacity.AvailableSlots,
							*FEEOSNativeOperationLease::SafeField(Capacity.Source), *FEEOSNativeOperationLease::SafeField(Capacity.UnknownReason));
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::KickMember — Kicked '%s' from lobby"), *FEEOSNativeOperationLease::SafeField(KickedMemberId));
						// The PendingKickedPuids entry stays: it suppresses the duplicate
						// engine participant-left (Kicked) notification, which removes it.
						if (!Self->bShuttingDown) Self->OnLobbyMemberLeft.Broadcast(KickedMemberId);
					}
					else
					{
						UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — EOS_Lobby_KickMember for '%s' failed: %s"),
							*FEEOSNativeOperationLease::SafeField(KickedMemberId), ANSI_TO_TCHAR(EOS_EResult_ToString(ResultCode)));
						// Remove the dedupe marker only if it is still OURS to remove. If it is
						// already gone, a participant-left (Kicked) was suppressed against this
						// kick (or the member rejoined / the lobby was torn down) — never
						// "un-consume" someone else's suppression.
						if (Self->PendingKickedPuids.Remove(TargetPuid) == 0)
						{
							UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::KickMember — Dedupe marker for '%s' was already consumed while the failed kick was in flight"), *FEEOSNativeOperationLease::SafeField(KickedMemberId));
						}
					}
				});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::KickMember — Kicking '%s'..."), *FEEOSNativeOperationLease::SafeField(MemberId));
	return true;
}

bool UEEOSLobbySubsystem::PromoteMember(const FString& MemberId)
{
	if (bShuttingDown || IsMembershipOperationInFlight()) return false;
	if (const UEEOSSettings* Settings = GetEOSSettings(); Settings && !Settings->bAllowLobbyOwnerTransfer)
	{
		RejectOperation(TEXT("PromoteMember"), EEOSOperationCode::UnsupportedCapability, TEXT("Lobby ownership transfer is disabled until gameplay server handoff exists."));
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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Only the lobby owner can promote members (EOS_Lobby_PromoteMember is owner-only)"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HLobby LobbyHandle = PlatformHandle ? EOS_Platform_GetLobbyInterface(PlatformHandle) : nullptr;
	if (!LobbyHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("PromoteMember"), TEXT("CapabilityUnavailable"));
		return false;
	}

	if (CurrentLobbyId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — No lobby id cached"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	FUniqueNetIdPtr LocalUserId = IdentityInterface.IsValid() ? IdentityInterface->GetUniquePlayerId(0) : nullptr;
	const FString LocalPUIDStr = LocalUserId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString()) : FString();
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Local user has no Product User ID (no Connect session?)"));
		return false;
	}

	const FString TargetPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(MemberId);
	if (TargetPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::PromoteMember — Target '%s' has no Product User ID"), *FEEOSNativeOperationLease::SafeField(MemberId));
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
		uint64 Generation;
		FEEOSRequestContext Context;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
		IOnlineSessionPtr Sessions;
	};
	FPromoteContext* Context = new FPromoteContext{ this, MemberId, CurrentLobbyId, Token, LobbyGeneration, CaptureEOSContext(), GetOwningEOSPlatform(), EOSSub->GetSessionInterface() };

	EOS_Lobby_PromoteMember(LobbyHandle, &Options, Context,
		[](const EOS_Lobby_PromoteMemberCallbackInfo* Data)
		{
			// Retrying callbacks retain ClientData until the SDK reports a terminal result.
			if (!EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FPromoteContext> Ctx(static_cast<FPromoteContext*>(Data->ClientData));
			if (!Ctx) return;

			if (Data->ResultCode != EOS_EResult::EOS_Success)
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem: Lobby promotion failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			OnLobbyGameThread([Weak = Ctx->Self, Member = Ctx->MemberId, Id = Ctx->LobbyId, Token = Ctx->Token, Generation = Ctx->Generation, Context = Ctx->Context, Result = Data->ResultCode]()
			{
				UEEOSLobbySubsystem* Self = Weak.Get();
				if (!Self) return;
				if (Self->bDeinitialized || !Self->IsEOSContextCurrent(Context))
				{ Self->LogCallbackDisposition(TEXT("PromoteLobbyMember"), int64(Token), Self->bDeinitialized ? TEXT("ShutdownInternalOnly") : TEXT("StaleGeneration"), int64(Generation)); return; }
				Self->HandlePromotionComplete(Token, Generation, Id, Member, Result == EOS_EResult::EOS_Success);
			});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::PromoteMember — Promoting '%s'..."), *FEEOSNativeOperationLease::SafeField(MemberId));
	return true;
}

// ── Lobby Settings ───────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::SetLobbyJoinable(bool bIsPublic)
{
	return StartLobbyUpdate(EPendingLobbyUpdate::Joinability, FString(), bIsPublic ? TEXT("Public") : TEXT("Private"));
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
	if (!InviteeId.IsValid() || !InviteeId->IsValid() || !EOS_ProductUserId_IsValid(EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*UEEOSBlueprintLibrary::ExtractProductUserId(InviteeId->ToString())))))
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLobbySubsystem::InviteToLobby — Could not parse user id '%s'"), *FEEOSNativeOperationLease::SafeField(UserId));
		return false;
	}
	const bool bSubmitted = SessionInterface->SendSessionInviteToFriend(0, LOBBY_SESSION_NAME, *InviteeId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem::InviteToLobby — Target=%s Submitted=%d"), *FEEOSNativeOperationLease::SafeField(UserId), bSubmitted);
	return bSubmitted;
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSLobbySubsystem::IsInLobby() const
{
	if (bShuttingDown || !GetEOSReadiness().bNativeLoggedIn) return false;
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	const auto* Native = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	const auto Context = CaptureEOSContext();
	const auto Local = Context.Identity.IsValid() ? Context.Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const bool bLocalMember = IsLocalLobbyMember(Native, Local);
	return bLocalMember && Native && Native->SessionInfo.IsValid() && Native->SessionInfo->IsValid()
		&& Native->SessionState != EOnlineSessionState::NoSession && Native->SessionState != EOnlineSessionState::Creating
		&& Native->SessionState != EOnlineSessionState::Destroying;
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
	if (InSessionName != LOBBY_SESSION_NAME || !CreateLobbyCompleteHandle.IsValid()) { LogCallbackDisposition(TEXT("HandleCreateSessionComplete"), MembershipLease.GetRequestId(), InSessionName != LOBBY_SESSION_NAME ? TEXT("DifferentOwner") : TEXT("Duplicate"), int64(LobbyGeneration)); return; }
	LogCallbackDisposition(TEXT("HandleCreateSessionComplete"), MembershipLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(MembershipContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), int64(LobbyGeneration));
	FinishLobbyCreation(bWasSuccessful, bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bWasSuccessful ? TEXT("Lobby creation completed.") : TEXT("Native lobby creation failed."),
		EEOSResultSource::NativeCallback, bWasSuccessful ? TEXT("Success") : TEXT("Failure"));
}

void UEEOSLobbySubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (!FindLobbiesCompleteHandle.IsValid()) { LogCallbackDisposition(TEXT("FindLobbies"), LobbySearchGeneration, TEXT("Duplicate"), SearchContext.Generation); return; }
	LogCallbackDisposition(TEXT("FindLobbies"), LobbySearchGeneration, bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(SearchContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), SearchContext.Generation);
	if (SearchSessions.IsValid()) SearchSessions->ClearOnFindSessionsCompleteDelegate_Handle(FindLobbiesCompleteHandle);
	SearchSessions.Reset();
	FindLobbiesCompleteHandle.Reset();
	ReleaseSearchSlot();

	TArray<FEEOSSessionSearchResult> Results;

	// Read results from OUR search object; the trigger's payload carries success. Empty
	// results with bWasSuccessful == true is a legitimate successful (empty) search.
	const bool bNativeSuccess = bWasSuccessful;
	const bool bContextCurrent = IsEOSContextCurrent(SearchContext);
	bWasSuccessful = bWasSuccessful && bContextCurrent;
	LobbySearchTime = FPlatformTime::Seconds();
	const int32 NativeSeen = LobbySearch.IsValid() ? LobbySearch->SearchResults.Num() : 0;
	int32 InvalidTarget = 0, InvalidDetails = 0, OwnerUnknown = 0;
	if (LobbySearch.IsValid())
		LobbySearch->SearchResults.RemoveAll([&](const FOnlineSessionSearchResult& Native)
		{
			if (!Native.IsValid()) { ++InvalidTarget; return true; }
			if (!Native.Session.SessionInfo.IsValid() || !Native.Session.SessionInfo->IsValid()) { ++InvalidDetails; return true; }
			if (!Native.Session.OwningUserId.IsValid() || !Native.Session.OwningUserId->IsValid()) ++OwnerUnknown;
			return false;
		});
	if (bWasSuccessful && LobbySearch.IsValid())
	{
		for (const auto& SearchResult : LobbySearch->SearchResults)
		{
			const FEEOSSessionSearchResult Result = EEOSCapacity::Describe(SearchResult, EEOSSessionBackend::Lobby, LobbySearchGeneration);
			UE_LOG(LogExtendedEOS, Verbose, TEXT("EOSLobbySearch Request=%lld Target=%s OwnerKnown=%d CapacityKnown=%d Max=%d Members=%d Slots=%d OpenPublic=%d OpenPrivate=%d Source=%s Advertise=%d Invites=%d PresenceJoin=%d JoinInProgress=%d"),
				LobbySearchGeneration, *FEEOSNativeOperationLease::SafeField(Result.SessionId), Result.bOwnerKnown, Result.Capacity.bKnown, Result.MaxPlayers, Result.CurrentPlayers,
				Result.Capacity.AvailableSlots, Result.Capacity.RawOpenPublic, Result.Capacity.RawOpenPrivate, *FEEOSNativeOperationLease::SafeField(Result.Capacity.Source),
				Result.bShouldAdvertise, Result.bAllowInvites, Result.bAllowJoinViaPresence, Result.bAllowJoinInProgress);

			Results.Add(Result);
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbySearch Request=%lld NativeSeen=%d ValidEmitted=%d DroppedInvalidTarget=%d DroppedInvalidDetails=%d OwnerUnknownObserved=%d HiddenNativeResultsKnown=0 ContextCurrent=%d"), LobbySearchGeneration, NativeSeen, Results.Num(), InvalidTarget, InvalidDetails, OwnerUnknown, bContextCurrent);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLobbySubsystem: Found %d lobbies (search %s)"), Results.Num(), bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	const auto Outcome = CompleteOperation(TEXT("FindLobbies"), bWasSuccessful, !bContextCurrent ? EEOSOperationCode::Canceled : bWasSuccessful ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure, !bContextCurrent ? TEXT("Lobby search context retired before completion.") : bWasSuccessful ? TEXT("Lobby search succeeded.") : TEXT("Native lobby search failed."), FString(), bNativeSuccess ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (!bShuttingDown) { OnLobbiesFound.Broadcast(Results); OnOperationCompleted.Broadcast(Outcome); }
}

void UEEOSLobbySubsystem::HandleJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (InSessionName != LOBBY_SESSION_NAME || !JoinRequest.IsJoining()) { LogCallbackDisposition(TEXT("HandleJoinSessionComplete"), MembershipLease.GetRequestId(), InSessionName != LOBBY_SESSION_NAME ? TEXT("DifferentOwner") : TEXT("Duplicate"), int64(LobbyGeneration)); return; }
	LogCallbackDisposition(TEXT("HandleJoinSessionComplete"), MembershipLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(MembershipContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), int64(LobbyGeneration));
	if (OperationSessions.IsValid()) OperationSessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinLobbyCompleteHandle);
	JoinLobbyCompleteHandle.Reset();
	const bool bSuccess = Result == EOnJoinSessionCompleteResult::Success;
	// FOnlineSessionEOS reports success before it has resolved the lobby's member list, so the
	// local user is not in MemberSettings yet. Count the joined lobby as membership until it is.
	if (bSuccess && JoinRequest.GetResult().IsValid() && IsEOSContextCurrent(MembershipContext))
	{
		JoinedLobbyAwaitingMemberList = JoinRequest.GetResult().GetSessionIdStr();
	}
	FinishJoiningLobby(bSuccess, bSuccess ? FString() : FString::Printf(TEXT("EOS lobby join failed: %s. SDK result is unavailable to the native callback; see the capacity snapshot and LogOnlineSession."), LexToString(Result)), EEOSOperationCode::NativeFailure, LexToString(Result), EEOSResultSource::NativeCallback);
}

void UEEOSLobbySubsystem::HandleDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME || !ExitRequest.IsActive()) { LogCallbackDisposition(TEXT("HandleDestroySessionComplete"), MembershipLease.GetRequestId(), InSessionName != LOBBY_SESSION_NAME ? TEXT("DifferentOwner") : TEXT("Duplicate"), int64(LobbyGeneration)); return; }
	LogCallbackDisposition(TEXT("HandleDestroySessionComplete"), MembershipLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(MembershipContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), int64(LobbyGeneration));
	++NativeExitCompletions;
	// The delete result may follow native EOS_LMS_CLOSED cleanup. Keep the request/handle
	// until both stages finish; a failed leave of an already deleted lobby is expected.
	FinishLobbyExit(bWasSuccessful);
}


void UEEOSLobbySubsystem::HandleLifetimeSessionDestroyed(FName InSessionName, bool bWasSuccessful)
{
	if (ExitRequest.IsActive() || JoinRequest.IsActive() || CreateLobbyCompleteHandle.IsValid())
	{ LogCallbackDisposition(TEXT("LifetimeLobbyDestroyed"), MembershipLease.GetRequestId(), TEXT("DifferentOwner"), int64(LobbyGeneration)); return; }
	if (!CanDeliverLobbyNotification(InSessionName, true)) return;
	const auto* Native = NotificationSessions.IsValid() ? NotificationSessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (!Native) { LastRemovedLobbyId = CurrentLobbyId; LastRemovalReason = TEXT("NativeRemovalCauseUnknown"); }
	ReconcileRemoteLobbyExit(Native, bWasSuccessful);
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
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyOwner Lobby=%s Generation=%llu PreviousOwner=%s NewOwner=%s"), *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, *FEEOSNativeOperationLease::SafeField(CachedLobbyOwnerId), *FEEOSNativeOperationLease::SafeField(OwnerId));
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
	const FName PendingOperation = MembershipLease.IsValid() ? MembershipLease.GetOperation() : UpdateLease.IsValid() ? FName(TEXT("UpdateLobby")) : NAME_None;
	const auto Active = GetActiveOperationOutcome(PendingOperation);
	if (!bShuttingDown && Active.RequestId && Active.RequestId != LastStallWarningRequest && Active.ElapsedMilliseconds >= 15000)
	{
		LastStallWarningRequest = Active.RequestId;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EOSLobbyStall Op=%s Request=%lld Phase=%s Current=%s Target=%s ElapsedMs=%.0f NativeCallbacks=%d/%d BackendPending=%d"),
			*FEEOSNativeOperationLease::SafeField(PendingOperation.ToString()), Active.RequestId, *FEEOSNativeOperationLease::SafeField(Active.Phase.ToString()), *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), *FEEOSNativeOperationLease::SafeField(Active.TargetId),
			Active.ElapsedMilliseconds, NativeExitCompletions, ExpectedNativeExitCompletions, ExitRequest.IsBackendPending());
	}
	if (!bShuttingDown) TryRegisterLifetimeNotifications();
	if (!bShuttingDown && bInLobby && !IsEOSContextCurrent(LobbyContext))
	{
		ResetLobbyState();
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyReconcile Phase=IdentityRetired Generation=%llu CachedMember=0 NativeRemovalNotAssumed=1"), LobbyGeneration);
	}
	if (!bShuttingDown && !IsMembershipOperationInFlight())
	{
		IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
		const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
		ReconcileRemoteLobbyExit(Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr, false);
		RefreshLobbyOwner();
		PumpLobbyUpdates();
	}
	return !bShuttingDown;
}

void UEEOSLobbySubsystem::HandlePromotionComplete(uint64 Token, uint64 Generation, const FString& LobbyId, const FString& MemberId, bool bSuccess)
{
	if (!bPromotionPending || PromotionToken != Token || PromotionLobbyId != LobbyId || LobbyGeneration != Generation)
	{ LogCallbackDisposition(TEXT("PromoteLobbyMember"), int64(Token), LobbyGeneration != Generation ? TEXT("StaleGeneration") : TEXT("Duplicate"), int64(Generation)); return; }
	bPromotionPending = false;
	PromotionLobbyId.Empty();
	// Shutdown may drain this operation, but must not run game/UI listeners or transfer again.
	if (bShuttingDown || CurrentLobbyId != LobbyId || !bInLobby) return;
	RefreshLobbyOwner();
	OnLobbyPromotionComplete.Broadcast(bSuccess, MemberId);
}

void UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete(FName InSessionName, bool bWasSuccessful)
{
	if (InSessionName != LOBBY_SESSION_NAME || !UpdateLobbyCompleteHandle.IsValid()) { LogCallbackDisposition(TEXT("HandleUpdateLobbySessionComplete"), PendingUpdateRequestId, InSessionName != LOBBY_SESSION_NAME ? TEXT("DifferentOwner") : TEXT("Duplicate"), int64(LobbyGeneration)); return; }
	LogCallbackDisposition(TEXT("HandleUpdateLobbySessionComplete"), PendingUpdateRequestId, bShuttingDown ? TEXT("ShutdownInternalOnly") : IsEOSContextCurrent(UpdateContext) ? TEXT("Consumed") : TEXT("StaleGeneration"), int64(LobbyGeneration));
	const FQueuedLobbyUpdate Completed{PendingUpdateRequestId, PendingUpdateLobbyId, PendingUpdateGeneration, PendingUpdateKind, PendingAttributeKey, PendingAttributeValue};
	const TArray<int64> Followers = MoveTemp(UpdateFollowers);
	const bool bCurrent = bInLobby && CurrentLobbyId == Completed.LobbyId && LobbyGeneration == Completed.Generation && (bShuttingDown || IsEOSContextCurrent(UpdateContext));
	const bool bAlreadyRetired = UpdateOutcomeHistory.Contains(Completed.RequestId);
	const bool bSubmissionRejected = bUpdateSubmissionRejected;
	bUpdateSubmissionRejected = false;
	if (!bWasSuccessful) RollbackStagedLobbyUpdate();
	if (UpdateSessions.IsValid()) UpdateSessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateLobbyCompleteHandle);
	UpdateLobbyCompleteHandle.Reset();
	PendingUpdateKind = EPendingLobbyUpdate::None;
	PendingUpdateRequestId = 0;
	PendingAttributeKey.Empty(); PendingAttributeValue.Empty(); PendingUpdateLobbyId.Empty();
	PendingUpdateMember.Reset(); PreviousUpdateSetting.Reset();
	UpdateSessions.Reset(); UpdateLease.Reset();
	const auto Outcome = bAlreadyRetired ? UpdateOutcomeHistory.FindChecked(Completed.RequestId) : CompleteOperation(TEXT("UpdateLobby"), bWasSuccessful && bCurrent,
		!bCurrent ? EEOSOperationCode::Canceled : bWasSuccessful ? EEOSOperationCode::Succeeded : bSubmissionRejected ? EEOSOperationCode::NativeStartRejected : EEOSOperationCode::NativeFailure,
		!bCurrent ? TEXT("Lobby generation retired before update completion.") : bWasSuccessful ? TEXT("Lobby update applied.") : bSubmissionRejected ? TEXT("Native lobby update refused submission.") : TEXT("Native lobby update failed."),
		CurrentLobbyId, bSubmissionRejected ? FString() : bWasSuccessful ? TEXT("Success") : TEXT("Failure"), bSubmissionRejected ? EEOSResultSource::Plugin : EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (bWasSuccessful && bCurrent && !bShuttingDown && Completed.Kind == EPendingLobbyUpdate::LobbyAttribute)
	{
		const FString* Previous = CachedLobbyAttributes.Find(Completed.Key);
		if (!Previous || *Previous != Completed.Value)
		{
			CachedLobbyAttributes.Add(Completed.Key, Completed.Value);
			OnLobbyAttributeChanged.Broadcast(Completed.Key, Completed.Value);
		}
	}
	if (bWasSuccessful && bCurrent && Completed.Kind == EPendingLobbyUpdate::Joinability)
	{
		ConfirmedVisibility.bKnown = true; ConfirmedVisibility.bShouldAdvertise = Completed.Value == TEXT("Public");
		ConfirmedVisibility.LobbyId = Completed.LobbyId; ConfirmedVisibility.Generation = int64(Completed.Generation);
	}
	PublishLobbyUpdateOutcome(Outcome);
	if (!bShuttingDown && !bAlreadyRetired) OnOperationCompleted.Broadcast(Outcome);
	for (int64 Id : Followers)
	{
		auto FollowerOutcome = Outcome;
		FollowerOutcome.RequestId = Id;
		PublishLobbyUpdateOutcome(FollowerOutcome);
	}
	if (bShuttingDown) CancelQueuedLobbyUpdates(); else PumpLobbyUpdates();
}

void UEEOSLobbySubsystem::HandleSessionParticipantJoined(FName InSessionName, const FUniqueNetId& UniqueId)
{
	// Subsystem-lifetime notification; the engine raises it for every named session.
	if (!CanDeliverLobbyNotification(InSessionName)) return;

	const FString MemberIdStr = UniqueId.ToString();

	// A previously-kicked member rejoining invalidates any stale kick-suppression entry
	// (e.g. when the engine never delivered the participant-left for our own kick).
	const FString MemberPuid = UEEOSBlueprintLibrary::ExtractProductUserId(MemberIdStr);
	if (!MemberPuid.IsEmpty())
	{
		PendingKickedPuids.Remove(MemberPuid);
	}

	const auto Capacity = GetLobbyCapacity();
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyMembers Lobby=%s Generation=%llu Change=Joined Known=%d Members=%d Max=%d Slots=%d Source=%s"),
		*FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, Capacity.bKnown, Capacity.Members, Capacity.Maximum, Capacity.AvailableSlots, *FEEOSNativeOperationLease::SafeField(Capacity.Source));
	OnLobbyMemberJoined.Broadcast(MemberIdStr);
}

void UEEOSLobbySubsystem::HandleSessionParticipantLeft(FName InSessionName, const FUniqueNetId& UniqueId, EOnSessionParticipantLeftReason Reason)
{
	if (!CanDeliverLobbyNotification(InSessionName, true)) return;

	const FString MemberIdStr = UniqueId.ToString();
	const FString MemberPuid = UEEOSBlueprintLibrary::ExtractProductUserId(MemberIdStr);

	// The LOCAL player leaving involuntarily (kicked, or the lobby closed under us) never
	// runs our own leave/destroy path, so the lobby state must be reset here or bInLobby
	// wedges true forever. Skipped while an own leave/destroy op is in flight — that op's
	// completion owns the state transition.
	if (ShouldHandleLocalMemberRemoval())
	{
		bool bIsLocalPlayer = false;
		if (IOnlineSubsystem* EOSSub = GetExistingEOSOnlineSubsystem())
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
			const FString RemovalReason = Reason == EOnSessionParticipantLeftReason::Kicked ? TEXT("Kicked") : TEXT("LeftOrClosed");
			LastRemovedLobbyId = CurrentLobbyId; LastRemovalReason = RemovalReason;
			const auto Capacity = GetLobbyCapacity();
			UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyMembers Lobby=%s Generation=%llu Change=LocalRemoved Reason=%s Known=%d Members=%d Max=%d Slots=%d Source=%s UnknownReason=%s"),
				*FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, *RemovalReason, Capacity.bKnown, Capacity.Members, Capacity.Maximum, Capacity.AvailableSlots,
				*FEEOSNativeOperationLease::SafeField(Capacity.Source), *FEEOSNativeOperationLease::SafeField(Capacity.UnknownReason));
			const FString DestroyedId = ResetLobbyState();
			const uint64 RemovedGeneration = LobbyGeneration;
			const auto Outcome = CompleteOperation(TEXT("RemoteLobbyRemoval"), false, EEOSOperationCode::Canceled,
				TEXT("Local lobby membership ended involuntarily."), FString(), RemovalReason, EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			OnLobbyMemberLeft.Broadcast(MemberIdStr);
			if (bShuttingDown || LobbyGeneration != RemovedGeneration) return;
			OnLobbyRemoved.Broadcast(DestroyedId, RemovalReason);
			if (bShuttingDown || LobbyGeneration != RemovedGeneration) return;
			OnLobbyDestroyed.Broadcast(true, DestroyedId); OnOperationCompleted.Broadcast(Outcome);
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
		LogCallbackDisposition(TEXT("KickParticipantLeft"), 0, TEXT("Duplicate"), int64(LobbyGeneration));
		return;
	}

	const auto Capacity = GetLobbyCapacity();
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyMembers Lobby=%s Generation=%llu Change=Left Reason=%d Known=%d Members=%d Max=%d Slots=%d"),
		*FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, int32(Reason), Capacity.bKnown, Capacity.Members, Capacity.Maximum, Capacity.AvailableSlots);
	OnLobbyMemberLeft.Broadcast(MemberIdStr);
}

void UEEOSLobbySubsystem::HandleSessionSettingsUpdated(FName InSessionName, const FOnlineSessionSettings& UpdatedSettings)
{
	// The EOS OSS raises this after refreshing the named session from a remote lobby update
	// (OnLobbyUpdateReceived → CopyLobbyData), so non-host members see owner-side attribute
	// changes here.
	if (!CanDeliverLobbyNotification(InSessionName)) return;
	const uint64 Generation = LobbyGeneration;
	RefreshLobbyOwner();
	if (bShuttingDown || LobbyGeneration != Generation) return;
	ConfirmedVisibility.bKnown = true; ConfirmedVisibility.bShouldAdvertise = UpdatedSettings.bShouldAdvertise;
	ConfirmedVisibility.LobbyId = CurrentLobbyId; ConfirmedVisibility.Generation = int64(Generation);
	RefreshCachedLobbyAttributes(UpdatedSettings, /*bBroadcastChanges*/ true);
}

void UEEOSLobbySubsystem::RefreshCachedLobbyAttributes(const FOnlineSessionSettings& InSettings, bool bBroadcastChanges)
{
	TMap<FString, FString> NewAttributes;
	for (FSessionSettings::TConstIterator It(InSettings.Settings); It; ++It)
	{
		NewAttributes.Add(It.Key().ToString(), It.Value().Data.ToString());
	}

	// A complete cached SDK snapshot also detects removals omitted by UE's CopyLobbyAttributes.
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const FString Puid = Local.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()) : FString();
	const EOS_HPlatform Platform = GetPlatformHandle();
	const EOS_HLobby Lobby = Platform ? EOS_Platform_GetLobbyInterface(Platform) : nullptr;
	if (Lobby && !Puid.IsEmpty() && !CurrentLobbyId.IsEmpty())
	{
		EOS_Lobby_CopyLobbyDetailsHandleOptions Options = {};
		Options.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
		const FTCHARToUTF8 Id(*CurrentLobbyId); Options.LobbyId = Id.Get();
		Options.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
		EOS_HLobbyDetails Details = nullptr;
		if (EOS_Lobby_CopyLobbyDetailsHandle(Lobby, &Options, &Details) == EOS_EResult::EOS_Success && Details)
		{
			TMap<FString, FString> Snapshot; bool bComplete = true;
			EOS_LobbyDetails_GetAttributeCountOptions CountOptions = {};
			CountOptions.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;
			const uint32 Count = EOS_LobbyDetails_GetAttributeCount(Details, &CountOptions);
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				EOS_LobbyDetails_CopyAttributeByIndexOptions Copy = {};
				Copy.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST; Copy.AttrIndex = Index;
				EOS_Lobby_Attribute* Attribute = nullptr;
				const auto Result = EOS_LobbyDetails_CopyAttributeByIndex(Details, &Copy, &Attribute);
				if (Result != EOS_EResult::EOS_Success || !Attribute || !Attribute->Data) bComplete = false;
				else
				{
					const FString Key = UTF8_TO_TCHAR(Attribute->Data->Key);
					FString Value;
					switch (Attribute->Data->ValueType)
					{
					case EOS_ESessionAttributeType::EOS_SAT_String: Value = Attribute->Data->Value.AsUtf8 ? UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8) : FString(); break;
					case EOS_ESessionAttributeType::EOS_SAT_Boolean: Value = Attribute->Data->Value.AsBool == EOS_TRUE ? TEXT("true") : TEXT("false"); break;
					case EOS_ESessionAttributeType::EOS_SAT_Int64: Value = FString::Printf(TEXT("%lld"), Attribute->Data->Value.AsInt64); break;
					case EOS_ESessionAttributeType::EOS_SAT_Double: Value = FString::SanitizeFloat(Attribute->Data->Value.AsDouble); break;
					default: bComplete = false; break;
					}
					if (IsWritableLobbyAttribute(Key)) Snapshot.Add(Key, Value);
				}
				if (Attribute) EOS_Lobby_Attribute_Release(Attribute);
			}
			EOS_LobbyDetails_Release(Details);
			if (bComplete) NewAttributes = MoveTemp(Snapshot);
		}
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

	TArray<FString> RemovedKeys;
	if (bBroadcastChanges)
		for (const auto& Pair : CachedLobbyAttributes) if (!NewAttributes.Contains(Pair.Key)) RemovedKeys.Add(Pair.Key);
	const uint64 Generation = LobbyGeneration;
	CachedLobbyAttributes = MoveTemp(NewAttributes);

	for (const TPair<FString, FString>& Changed : ChangedAttributes)
	{
		if (bShuttingDown || LobbyGeneration != Generation) return;
		OnLobbyAttributeChanged.Broadcast(Changed.Key, Changed.Value);
	}
	for (const FString& Key : RemovedKeys)
	{
		if (bShuttingDown || LobbyGeneration != Generation) return;
		OnLobbyAttributeRemoved.Broadcast(Key);
	}
}

int64 UEEOSLobbySubsystem::QueueLobbyAttribute(const FString& Key, const FString& Value)
{
	return EnqueueLobbyUpdate(EPendingLobbyUpdate::LobbyAttribute, Key, Value);
}
int64 UEEOSLobbySubsystem::QueueMemberAttribute(const FString& Key, const FString& Value)
{
	return EnqueueLobbyUpdate(EPendingLobbyUpdate::MemberAttribute, Key, Value);
}
int64 UEEOSLobbySubsystem::QueueLobbyVisibility(bool bIsPublic)
{
	return EnqueueLobbyUpdate(EPendingLobbyUpdate::Joinability, FString(), bIsPublic ? TEXT("Public") : TEXT("Private"));
}
void UEEOSLobbySubsystem::EmitQueuedUpdateOutcome(const FQueuedLobbyUpdate& Request, EEOSOperationCode Code, const FString& Message)
{
	FEEOSOperationOutcome Outcome;
	Outcome.RequestId = Request.RequestId;
	Outcome.Operation = TEXT("UpdateLobby"); Outcome.Code = Code;
	Outcome.TargetId = Request.LobbyId; Outcome.CurrentId = CurrentLobbyId;
	Outcome.Phase = TEXT("Complete"); Outcome.Message = Message;
	PublishLobbyUpdateOutcome(Outcome);
}
int64 UEEOSLobbySubsystem::QueueLobbyCapacity(int32 MaxMembers)
{
	const auto Capacity = GetLobbyCapacity();
	if (MaxMembers < 1 || MaxMembers > EOS_LOBBY_MAX_LOBBY_MEMBERS || !Capacity.bKnown || MaxMembers < Capacity.Members)
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Capacity must fit all current lobby members and the SDK limits."));
		return 0;
	}
	return EnqueueLobbyUpdate(EPendingLobbyUpdate::Capacity, FString(), FString::FromInt(MaxMembers));
}

int64 UEEOSLobbySubsystem::EnqueueLobbyUpdate(EPendingLobbyUpdate Kind, const FString& Key, const FString& Value)
{
	if (bShuttingDown || !bInLobby || !IsEOSContextCurrent(LobbyContext) || ExitRequest.IsActive() || JoinRequest.IsActive() || CreateLobbyCompleteHandle.IsValid() || bPromotionPending)
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::Busy, TEXT("Lobby membership is unavailable or changing."));
		return 0;
	}
	if (Kind != EPendingLobbyUpdate::MemberAttribute && !IsLobbyOwner())
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Only the owner may publish this update."));
		return 0;
	}
	if (Kind != EPendingLobbyUpdate::Joinability && Kind != EPendingLobbyUpdate::Capacity && !IsWritableLobbyAttribute(Key))
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Attribute name is empty, reserved, or exceeds the SDK limit.")); return 0;
	}
	const int64 Id = FEEOSNativeOperationLease::NextRequestId();
	const auto RememberAdmission = [&](int64 Parent)
	{
		FEEOSOperationOutcome Admission; Admission.RequestId = Id; Admission.Operation = TEXT("UpdateLobby");
		Admission.StartedUtc = Admission.LastTransitionUtc = FDateTime::UtcNow(); Admission.TargetId = CurrentLobbyId;
		Admission.ParentRequestId = Parent; Admission.ContextGeneration = CaptureEOSContext().Generation; Admission.MembershipGeneration = int64(LobbyGeneration);
		AcceptedUpdateMetadata.Add(Id, Admission); AcceptedUpdateStartSeconds.Add(Id, FPlatformTime::Seconds());
		if (GetEOSSettings()->bEnableOperationLogging) UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyUpdate Request=%lld ParentRequest=%lld Phase=Accepted Context=%lld Lobby=%s Generation=%llu Kind=%d Key=%s"),
			Id, Parent, Admission.ContextGeneration, *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, int32(Kind), *FEEOSNativeOperationLease::SafeField(Key));
	};
	if (UpdateLobbyCompleteHandle.IsValid() && PendingUpdateKind == Kind && FName(*PendingAttributeKey) == FName(*Key) && PendingAttributeValue == Value
		&& PendingUpdateGeneration == LobbyGeneration && UpdateFollowers.Num() < 32)
	{
		RememberAdmission(PendingUpdateRequestId); UpdateFollowers.Add(Id);
		return Id;
	}
	const int32 Index = QueuedUpdates.IndexOfByPredicate([&](const auto& Item) { return Item.Kind == Kind && FName(*Item.Key) == FName(*Key); });
	if (Index == INDEX_NONE && QueuedUpdates.Num() >= 32)
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::Busy, TEXT("Lobby update queue is full."));
		return 0;
	}
	RememberAdmission(0);
	const FQueuedLobbyUpdate Request{Id, CurrentLobbyId, LobbyGeneration, Kind, Key, Value};
	TOptional<FQueuedLobbyUpdate> Superseded;
	if (Index != INDEX_NONE) { Superseded = QueuedUpdates[Index]; QueuedUpdates[Index] = Request; }
	else QueuedUpdates.Add(Request);
	if (Superseded.IsSet()) EmitQueuedUpdateOutcome(Superseded.GetValue(), EEOSOperationCode::Superseded, TEXT("Replaced by a newer desired value."));
	PumpLobbyUpdates();
	return Id;
}
void UEEOSLobbySubsystem::PumpLobbyUpdates()
{
	if (bShuttingDown || IsMembershipOperationInFlight() || UpdateLobbyCompleteHandle.IsValid() || QueuedUpdates.IsEmpty()) return;
	const FQueuedLobbyUpdate Request = QueuedUpdates[0];
	QueuedUpdates.RemoveAt(0);
	if (!bInLobby || Request.LobbyId != CurrentLobbyId || Request.Generation != LobbyGeneration)
		EmitQueuedUpdateOutcome(Request, EEOSOperationCode::Canceled, TEXT("The target lobby generation has retired."));
	else if (!StartLobbyUpdate(Request.Kind, Request.Key, Request.Value, Request.RequestId) && !UpdateOutcomeHistory.Contains(Request.RequestId))
		EmitQueuedUpdateOutcome(Request, EEOSOperationCode::NativeStartRejected, TEXT("The queued update could not start."));
	// Native completions, or the owner ticker after a rejection, advance the remaining queue.
}
void UEEOSLobbySubsystem::CancelQueuedLobbyUpdates()
{
	const auto Canceled = MoveTemp(QueuedUpdates);
	QueuedUpdates.Reset();
	for (const auto& Request : Canceled) EmitQueuedUpdateOutcome(Request, EEOSOperationCode::Canceled, TEXT("Lobby membership/shutdown canceled the queued write."));
}
void UEEOSLobbySubsystem::RollbackStagedLobbyUpdate()
{
	if (!UpdateSessions.IsValid() || CurrentLobbyId != PendingUpdateLobbyId || LobbyGeneration != PendingUpdateGeneration) return;
	FOnlineSessionSettings* Settings = UpdateSessions->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings) return;
	if (PendingUpdateKind == EPendingLobbyUpdate::Capacity)
	{
		if (Settings->NumPublicConnections + Settings->NumPrivateConnections == FCString::Atoi(*PendingAttributeValue))
		{
			Settings->NumPublicConnections = PreviousPublicConnections;
			Settings->NumPrivateConnections = PreviousPrivateConnections;
		}
		return;
	}
	if (PendingUpdateKind == EPendingLobbyUpdate::Joinability)
	{
		const bool Desired = PendingAttributeValue == TEXT("Public");
		if (Settings->bShouldAdvertise == Desired) Settings->bShouldAdvertise = bPreviousVisibility;
		return;
	}
	FSessionSettings* Fields = &Settings->Settings;
	if (PendingUpdateKind == EPendingLobbyUpdate::MemberAttribute)
		Fields = PendingUpdateMember.IsValid() ? Settings->MemberSettings.Find(PendingUpdateMember.ToSharedRef()) : nullptr;
	if (!Fields) return;
	const FName Key(*PendingAttributeKey);
	const FOnlineSessionSetting* Staged = Fields->Find(Key);
	if (!Staged || Staged->Data.GetType() != EOnlineKeyValuePairDataType::String
		|| Staged->AdvertisementType != EOnlineDataAdvertisementType::ViaOnlineService
		|| Staged->Data.ToString() != PendingAttributeValue) return;
	if (PreviousUpdateSetting.IsSet()) Fields->Add(Key, PreviousUpdateSetting.GetValue());
	else Fields->Remove(Key);
	if (PendingUpdateKind == EPendingLobbyUpdate::MemberAttribute && bCreatedMemberEntry && Fields->IsEmpty())
		Settings->MemberSettings.Remove(PendingUpdateMember.ToSharedRef());
}
bool UEEOSLobbySubsystem::StartLobbyUpdate(EPendingLobbyUpdate Kind, const FString& Key, const FString& Value, int64 RequestId)
{
	if (bShuttingDown || IsMembershipOperationInFlight() || UpdateLobbyCompleteHandle.IsValid())
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::Busy, TEXT("A lobby membership/update operation is in flight."));
		return false;
	}
	if (Kind != EPendingLobbyUpdate::Joinability && Kind != EPendingLobbyUpdate::Capacity && !IsWritableLobbyAttribute(Key))
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Attribute name is empty, reserved, or exceeds the SDK limit."));
		return false;
	}
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const IOnlineSessionPtr Sessions = OSS ? OSS->GetSessionInterface() : nullptr;
	const IOnlineIdentityPtr Identity = OSS ? OSS->GetIdentityInterface() : nullptr;
	const FUniqueNetIdPtr LocalId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	const FNamedOnlineSession* Native = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (!Native || !bInLobby || !IsEOSContextCurrent(LobbyContext) || !LocalId.IsValid() || !LocalId->IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn
		|| Native->SessionState == EOnlineSessionState::Creating || Native->SessionState == EOnlineSessionState::Destroying
		|| (Kind != EPendingLobbyUpdate::MemberAttribute && !IsLobbyOwner()))
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidTarget, TEXT("A usable native lobby, logged-in identity, and publishing permission are required.")); return false;
	}
	FOnlineSessionSettings* Settings = Sessions->GetSessionSettings(LOBBY_SESSION_NAME);
	if (!Settings) return false;
	if (Kind == EPendingLobbyUpdate::Capacity)
	{
		const auto Capacity = GetLobbyCapacity();
		const int32 Desired = FCString::Atoi(*Value);
		if (!Capacity.bKnown || Desired < FMath::Max(1, Capacity.Members) || Desired > EOS_LOBBY_MAX_LOBBY_MEMBERS)
		{
			RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Lobby membership changed before capacity could be applied."));
			return false;
		}
	}
	const FSessionSettings* ExistingFields = Kind == EPendingLobbyUpdate::MemberAttribute ? Settings->MemberSettings.Find(LocalId.ToSharedRef()) : &Settings->Settings;
	const int32 NativeReservedAttributes = Kind == EPendingLobbyUpdate::MemberAttribute ? 0 : 6;
	int32 AdvertisedAttributes = NativeReservedAttributes;
	bool bAlreadyAdvertised = false;
	if (ExistingFields)
	{
		for (const auto& Field : *ExistingFields)
		{
			const auto Type = Field.Value.Data.GetType();
			const bool bSupported = Type == EOnlineKeyValuePairDataType::Int32 || Type == EOnlineKeyValuePairDataType::Int64
				|| Type == EOnlineKeyValuePairDataType::Double || Type == EOnlineKeyValuePairDataType::Bool || Type == EOnlineKeyValuePairDataType::String
				|| Type == EOnlineKeyValuePairDataType::UInt32 || Type == EOnlineKeyValuePairDataType::Float || Type == EOnlineKeyValuePairDataType::Json;
			if (!bSupported || Field.Value.AdvertisementType < EOnlineDataAdvertisementType::ViaOnlineService) continue;
			// The six typed native fields are already included in the reservation above.
			if (Kind == EPendingLobbyUpdate::MemberAttribute || !EEOSCapacity::IsReservedAttribute(Field.Key)) ++AdvertisedAttributes;
			if (Field.Key == FName(*Key)) bAlreadyAdvertised = true;
		}
	}
	if (Kind != EPendingLobbyUpdate::Joinability && Kind != EPendingLobbyUpdate::Capacity && !bAlreadyAdvertised
		&& AdvertisedAttributes >= EOS_LOBBYMODIFICATION_MAX_ATTRIBUTES)
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::InvalidInput, TEXT("Lobby attribute count would exceed the SDK limit.")); return false;
	}
	if (!UpdateLease.TryAcquire(Sessions.Get(), LOBBY_SESSION_NAME, this, TEXT("UpdateLobby"), RequestId))
	{
		RejectOperation(TEXT("UpdateLobby"), EEOSOperationCode::Busy, TEXT("Another plugin operation owns the native lobby.")); return false;
	}
	UpdateSessions = Sessions; UpdateContext = CaptureEOSContext();
	PendingUpdateRequestId = BeginOperation(TEXT("UpdateLobby"), CurrentLobbyId, RequestId ? RequestId : UpdateLease.GetRequestId());
	const int64 Token = PendingUpdateRequestId;
	PendingUpdateLobbyId = CurrentLobbyId; PendingUpdateGeneration = LobbyGeneration;
	TagOperationContext(TEXT("UpdateLobby"), PendingUpdateLobbyId, int64(PendingUpdateGeneration));
	PendingUpdateKind = Kind; PendingAttributeKey = Key; PendingAttributeValue = Value; bUpdateSubmissionRejected = false;
	PreviousUpdateSetting.Reset(); bCreatedMemberEntry = false; PendingUpdateMember.Reset();
	if (Kind == EPendingLobbyUpdate::Joinability)
	{
		bPreviousVisibility = Settings->bShouldAdvertise;
		Settings->bShouldAdvertise = Value == TEXT("Public");
	}
	else if (Kind == EPendingLobbyUpdate::Capacity)
	{
		PreviousPublicConnections = Settings->NumPublicConnections;
		PreviousPrivateConnections = Settings->NumPrivateConnections;
		if (Settings->bShouldAdvertise)
		{
			Settings->NumPublicConnections = FCString::Atoi(*Value);
			Settings->NumPrivateConnections = 0;
		}
		else
		{
			Settings->NumPrivateConnections = FCString::Atoi(*Value);
			Settings->NumPublicConnections = 0;
		}
	}
	else
	{
		FSessionSettings* Fields = &Settings->Settings;
		if (Kind == EPendingLobbyUpdate::MemberAttribute)
		{
			PendingUpdateMember = LocalId;
			bCreatedMemberEntry = !Settings->MemberSettings.Contains(LocalId.ToSharedRef());
			Fields = &Settings->MemberSettings.FindOrAdd(LocalId.ToSharedRef());
		}
		if (const auto* Previous = Fields->Find(FName(*Key))) PreviousUpdateSetting = *Previous;
		Fields->Add(FName(*Key), FOnlineSessionSetting(Value, EOnlineDataAdvertisementType::ViaOnlineService));
	}
	UpdateLobbyCompleteHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
		FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleUpdateLobbySessionComplete));
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyUpdate Request=%lld Lobby=%s Generation=%llu Kind=%d Key=%s Queued=%d"),
		Token, *FEEOSNativeOperationLease::SafeField(CurrentLobbyId), LobbyGeneration, int32(Kind), *FEEOSNativeOperationLease::SafeField(Key), QueuedUpdates.Num());
	const bool bStarted = Sessions->UpdateSession(LOBBY_SESSION_NAME, *Settings);
	if (!bStarted && PendingUpdateRequestId == Token)
	{
		bUpdateSubmissionRejected = true;
		HandleUpdateLobbySessionComplete(LOBBY_SESSION_NAME, false);
		return false;
	}
	return bStarted;
}

FEEOSOperationOutcome UEEOSLobbySubsystem::GetLobbyUpdateOutcome(int64 RequestId) const
{
	if (const auto* Outcome = UpdateOutcomeHistory.Find(RequestId)) return *Outcome;
	return FEEOSOperationOutcome();
}

void UEEOSLobbySubsystem::PublishLobbyUpdateOutcome(const FEEOSOperationOutcome& Outcome)
{
	if (UpdateOutcomeHistory.Contains(Outcome.RequestId)) return;
	auto Recorded = Outcome;
	if (const auto* Admission = AcceptedUpdateMetadata.Find(Outcome.RequestId))
	{
		Recorded.StartedUtc = Admission->StartedUtc;
		Recorded.ParentRequestId = Admission->ParentRequestId;
		Recorded.ContextGeneration = Admission->ContextGeneration;
		Recorded.MembershipGeneration = Admission->MembershipGeneration;
	}
	Recorded.CompletedUtc = Recorded.LastTransitionUtc = FDateTime::UtcNow();
	if (const double* Started = AcceptedUpdateStartSeconds.Find(Outcome.RequestId))
		Recorded.ElapsedMilliseconds = (FPlatformTime::Seconds() - *Started) * 1000.0;
	AcceptedUpdateStartSeconds.Remove(Outcome.RequestId);
	AcceptedUpdateMetadata.Remove(Outcome.RequestId);
	if (GetEOSSettings()->bEnableOperationLogging) UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyUpdate Request=%lld ParentRequest=%lld Phase=TerminalRecorded Code=%d Success=%d Generation=%lld ElapsedMs=%.0f Shutdown=%d"),
		Recorded.RequestId, Recorded.ParentRequestId, int32(Recorded.Code), Recorded.bSuccess, Recorded.MembershipGeneration, Recorded.ElapsedMilliseconds, bShuttingDown);
	UpdateOutcomeHistory.Add(Recorded.RequestId, Recorded); UpdateOutcomeOrder.Add(Recorded.RequestId);
	if (UpdateOutcomeOrder.Num() > 128) { UpdateOutcomeHistory.Remove(UpdateOutcomeOrder[0]); UpdateOutcomeOrder.RemoveAt(0); }
	if (!bShuttingDown) OnLobbyUpdateCompleted.Broadcast(Recorded);
}
bool UEEOSLobbySubsystem::AcquireLobbyMembership(FName Operation, const FString& TargetId)
{
	IOnlineSubsystem* OSS = GetEOSOnlineSubsystem();
	const auto Sessions = OSS ? OSS->GetSessionInterface() : IOnlineSessionPtr();
	if (!Sessions.IsValid())
	{
		RejectOperation(Operation, EEOSOperationCode::UnsupportedCapability, TEXT("The native EOS session interface is unavailable.")); return false;
	}
	if (!MembershipLease.TryAcquire(Sessions.Get(), LOBBY_SESSION_NAME, this, Operation))
	{
		RejectOperation(Operation, EEOSOperationCode::Busy, TEXT("The native lobby is owned by another operation."));
		return false;
	}
	OperationSessions = Sessions; MembershipContext = CaptureEOSContext();
	LastMembershipRequestId = BeginOperation(Operation, TargetId, MembershipLease.GetRequestId());
	TagOperationContext(Operation, CurrentLobbyId, int64(LobbyGeneration));
	return true;
}

bool UEEOSLobbySubsystem::SubmitLobbyCreation()
{
	const IOnlineSessionPtr Sessions = OperationSessions;
	if (!Sessions.IsValid() || !IsEOSContextCurrent(MembershipContext) || Sessions->GetNamedSession(LOBBY_SESSION_NAME))
	{
		FinishLobbyCreation(false, EEOSOperationCode::ExistingLobbyCloseFailed, TEXT("The native lobby name is unavailable for creation.")); return false;
	}
	const int64 Token = MembershipLease.GetRequestId();
	CreateLobbyCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEEOSLobbySubsystem::HandleCreateSessionComplete));
	const FOnlineSessionSettings Settings = PendingCreateLobbySettings;
	const bool bStarted = Sessions->CreateSession(0, LOBBY_SESSION_NAME, Settings);
	if (!bStarted && CreateLobbyCompleteHandle.IsValid() && MembershipLease.GetRequestId() == Token)
		FinishLobbyCreation(false, EEOSOperationCode::NativeStartRejected, TEXT("Native lobby creation could not start."));
	return bStarted;
}
void UEEOSLobbySubsystem::FinishLobbyCreation(bool bSuccess, EEOSOperationCode Code, const FString& Message, EEOSResultSource Source, const FString& NativeResult)
{
	const IOnlineSessionPtr Sessions = OperationSessions;
	if (Sessions.IsValid()) Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateLobbyCompleteHandle);
	CreateLobbyCompleteHandle.Reset();
	const FNamedOnlineSession* Native = Sessions.IsValid() ? Sessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	RefreshLobbyState(Native);
	const bool bContextCurrent = bShuttingDown || IsEOSContextCurrent(MembershipContext);
	if (!bContextCurrent) Code = EEOSOperationCode::Canceled;
	bSuccess = bSuccess && bContextCurrent && bInLobby && Native && Native->SessionState != EOnlineSessionState::Creating && Native->SessionState != EOnlineSessionState::Destroying;
	if (!bSuccess && Code == EEOSOperationCode::Succeeded) Code = EEOSOperationCode::NativeFailure;
	if (Native)
	{
		const auto Capacity = GetLobbyCapacity();
		bool bMigration = true; Native->SessionSettings.Get(SETTING_HOST_MIGRATION, bMigration);
		UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyHost Lobby=%s CapacityKnown=%d Max=%d Members=%d Slots=%d Voice=%d Migration=%d Advertise=%d"),
			*FEEOSNativeOperationLease::SafeField(CurrentLobbyId), Capacity.bKnown, Capacity.Maximum, Capacity.Members, Capacity.AvailableSlots,
			Native->SessionSettings.bUseLobbiesVoiceChatIfAvailable, bMigration, Native->SessionSettings.bShouldAdvertise);
	}
	const auto Readiness = GetEOSReadiness();
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyHostContext Request=%lld Instance=%s Backend=Lobby RequestedCapacity=%d EffectiveCapacity=%d PrivateSlotsPolicy=PermissionSelected PresenceEligible=%d ConnectReady=%d LocalOwner=%d WorldNetMode=%d"),
		MembershipLease.GetRequestId(), *FEEOSNativeOperationLease::SafeField(Readiness.Instance.ToString()), PendingCreateLobbySettings.NumPublicConnections + PendingCreateLobbySettings.NumPrivateConnections,
		GetLobbyCapacity().Maximum, Readiness.bHasEpicAccount, Readiness.bConnectLoggedIn, IsLobbyOwner(), GetWorld() ? int32(GetWorld()->GetNetMode()) : -1);

	MembershipLease.Reset(); OperationSessions.Reset();
	const auto Outcome = CompleteOperation(TEXT("CreateLobby"), bSuccess, bSuccess ? EEOSOperationCode::Succeeded : Code, bSuccess ? Message : !bContextCurrent ? TEXT("Original identity/platform retired before creation completion.") : Code == EEOSOperationCode::NativeFailure ? TEXT("Native lobby creation did not leave usable membership.") : Message, CurrentLobbyId, NativeResult, Source);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (!bShuttingDown) { OnLobbyCreated.Broadcast(bSuccess, CurrentLobbyId); OnOperationCompleted.Broadcast(Outcome); }
}


bool UEEOSLobbySubsystem::RefreshJoinTargetCapacity(const FString& ExistingLobbyId)
{
	const auto Platform = GetOwningEOSPlatform();
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const FString Puid = Local.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()) : FString();
	EOS_HLobby Lobby = Platform.IsValid() ? EOS_Platform_GetLobbyInterface(*Platform) : nullptr;
	struct FContext
	{
		TWeakObjectPtr<UEEOSLobbySubsystem> Self;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
		IOnlineSessionPtr Sessions;
		FEEOSNativeOperationLease Membership, SearchLease;
		EOS_HLobbySearch Search = nullptr;
		FString ExistingId;
		int64 RequestId = 0;
		int64 ChildRequestId = 0;
		~FContext() { if (Search) EOS_LobbySearch_Release(Search); }
	};
	TUniquePtr<FContext> Context = MakeUnique<FContext>();
	Context->Self = this; Context->Platform = Platform; Context->Sessions = OperationSessions;
	Context->ChildRequestId = FEEOSNativeOperationLease::NextRequestId();
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyPreflight Request=%lld ParentRequest=%lld Generation=%llu Phase=Preflight Target=%s"), Context->ChildRequestId, MembershipLease.GetRequestId(), LobbyGeneration, *FEEOSNativeOperationLease::SafeField(JoinRequest.GetResult().GetSessionIdStr()));
	Context->Membership = MembershipLease; Context->ExistingId = ExistingLobbyId; Context->RequestId = MembershipLease.GetRequestId();
	if (bJoinTargetAuthorizedDetails && !JoinRequest.GetResult().Session.SessionSettings.bShouldAdvertise)
	{
		FEEOSCapacitySnapshot Unknown; Unknown.Backend = EEOSSessionBackend::Lobby; Unknown.Source = TEXT("RetainedAuthorizedDetails"); Unknown.UnknownReason = TEXT("Private target refreshed by native authorized join, not public search.");
		HandleJoinCapacityPreflight(MembershipLease.GetRequestId(), ExistingLobbyId, Unknown, FString()); return true;
	}
	if (!Lobby || Puid.IsEmpty() || !OperationSessions.IsValid()
		|| !Context->SearchLease.TryAcquire(OperationSessions.Get(), TEXT("Search"), this, TEXT("JoinLobbyPreflight")))
	{
		Context.Reset();
		FinishJoiningLobby(false, TEXT("Target capacity could not be refreshed; current lobby membership was preserved."), EEOSOperationCode::StaleResult); return false;
	}
	EOS_Lobby_CreateLobbySearchOptions Create = {};
	Create.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST; Create.MaxResults = 1;
	EOS_EResult Result = EOS_Lobby_CreateLobbySearch(Lobby, &Create, &Context->Search);
	const FTCHARToUTF8 TargetId(*FEEOSNativeOperationLease::SafeField(JoinRequest.GetResult().GetSessionIdStr()));
	if (Result == EOS_EResult::EOS_Success && Context->Search)
	{
		EOS_LobbySearch_SetLobbyIdOptions Target = {};
		Target.ApiVersion = EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST; Target.LobbyId = TargetId.Get();
		Result = EOS_LobbySearch_SetLobbyId(Context->Search, &Target);
	}
	if (Result != EOS_EResult::EOS_Success || !Context->Search)
	{
		Context.Reset();
		FinishJoiningLobby(false, TEXT("Target capacity lookup refused; current membership was preserved."), EEOSOperationCode::StaleResult,
			FString(), EEOSResultSource::SDKCall, ANSI_TO_TCHAR(EOS_EResult_ToString(Result))); return false;
	}
	SetOperationPhase(TEXT("JoinLobby"), TEXT("RefreshingTargetCapacity"));
	EOS_LobbySearch_FindOptions Find = {};
	Find.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST; Find.LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*Puid));
	const auto Search = Context->Search;
	EOS_LobbySearch_Find(Search, &Find, Context.Release(), [](const EOS_LobbySearch_FindCallbackInfo* Data)
	{
		if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
		TUniquePtr<FContext> Ctx(static_cast<FContext*>(Data->ClientData));
		FEEOSCapacitySnapshot Capacity; Capacity.Backend = EEOSSessionBackend::Lobby; Capacity.Source = TEXT("EOSLobbySearchInfo"); Capacity.UnknownReason = TEXT("NoSupportedLobbyDetails");
		EOS_EResult Result = Data->ResultCode;
		if (Result == EOS_EResult::EOS_Success)
		{
			EOS_LobbySearch_GetSearchResultCountOptions Count = {}; Count.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
			if (EOS_LobbySearch_GetSearchResultCount(Ctx->Search, &Count) == 1)
			{
				EOS_LobbySearch_CopySearchResultByIndexOptions Copy = {}; Copy.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST; Copy.LobbyIndex = 0;
				EOS_HLobbyDetails Details = nullptr;
				Result = EOS_LobbySearch_CopySearchResultByIndex(Ctx->Search, &Copy, &Details);
				if (Result == EOS_EResult::EOS_Success && Details)
				{
					EOS_LobbyDetails_CopyInfoOptions Options = {}; Options.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
					EOS_LobbyDetails_Info* Info = nullptr; Result = EOS_LobbyDetails_CopyInfo(Details, &Options, &Info);
					if (Result == EOS_EResult::EOS_Success && Info && Info->MaxMembers <= EOS_LOBBY_MAX_LOBBY_MEMBERS
						&& Info->MaxMembers > 0 && Info->AvailableSlots <= Info->MaxMembers)
					{
						Capacity.Permission = EEOSCapacity::ReadPermission(Info->PermissionLevel);
						Capacity.Maximum = int32(Info->MaxMembers); Capacity.AvailableSlots = int32(Info->AvailableSlots);
						Capacity.Members = Capacity.Maximum - Capacity.AvailableSlots; Capacity.bKnown = Capacity.bConsistent = true; Capacity.UnknownReason.Empty();
					}
					if (Info && !Capacity.bKnown) Capacity.UnknownReason = TEXT("InconsistentMaximumOrSlots");
					if (Info) EOS_LobbyDetails_Info_Release(Info);
				}
				if (Details) EOS_LobbyDetails_Release(Details);
			}
		}
		const auto Weak = Ctx->Self; const int64 Request = Ctx->RequestId; const FString Existing = Ctx->ExistingId;
		const FString SDKResult = ANSI_TO_TCHAR(EOS_EResult_ToString(Result));
		// Release the search and its locks before terminal events or a continuation can re-enter.
		Ctx.Reset();
		OnLobbyGameThread([Weak, Request, Existing, Capacity, SDKResult]()
		{
			if (auto* Self = Weak.Get()) Self->HandleJoinCapacityPreflight(Request, Existing, Capacity, SDKResult);
		});
	});
	return true;
}

void UEEOSLobbySubsystem::HandleJoinCapacityPreflight(int64 RequestId, const FString& ExistingLobbyId,
	const FEEOSCapacitySnapshot& Capacity, const FString& SDKResult)
{
	if (bShuttingDown || !JoinRequest.IsActive() || MembershipLease.GetRequestId() != RequestId)
	{ LogCallbackDisposition(TEXT("JoinLobbyPreflight"), RequestId, bShuttingDown ? TEXT("ShutdownInternalOnly") : TEXT("DifferentOwner"), int64(LobbyGeneration)); return; }
	if (!IsEOSContextCurrent(MembershipContext))
	{ LogCallbackDisposition(TEXT("JoinLobbyPreflight"), RequestId, TEXT("StaleGeneration"), int64(LobbyGeneration)); FinishJoiningLobby(false, TEXT("Identity/platform changed during target preflight; current membership preserved."), EEOSOperationCode::Canceled); return; }
	LogCallbackDisposition(TEXT("JoinLobbyPreflight"), RequestId, TEXT("Consumed"), int64(LobbyGeneration));
	UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyJoin Request=%lld Phase=TargetCapacityRefreshed Target=%s Known=%d Max=%d Members=%d Slots=%d Source=%s UnknownReason=%s SDKResult=%s"),
		RequestId, *FEEOSNativeOperationLease::SafeField(JoinRequest.GetResult().GetSessionIdStr()), Capacity.bKnown, Capacity.Maximum, Capacity.Members, Capacity.AvailableSlots, *FEEOSNativeOperationLease::SafeField(Capacity.Source), *FEEOSNativeOperationLease::SafeField(Capacity.UnknownReason), *FEEOSNativeOperationLease::SafeField(SDKResult));
	if ((!Capacity.bKnown && !bJoinTargetAuthorizedDetails) || (Capacity.bKnown && Capacity.AvailableSlots == 0))
	{
		FinishJoiningLobby(false, Capacity.bKnown ? TEXT("Target lobby is full; current membership was preserved.") : TEXT("Target capacity is unavailable; current membership was preserved."),
			Capacity.bKnown ? EEOSOperationCode::LobbyFull : EEOSOperationCode::StaleResult, FString(), EEOSResultSource::SDKCallback, SDKResult); return;
	}
	const auto* Existing = OperationSessions.IsValid() ? OperationSessions->GetNamedSession(LOBBY_SESSION_NAME) : nullptr;
	if (Existing && (!Existing->SessionInfo.IsValid() || Existing->GetSessionIdStr() != ExistingLobbyId
		|| Existing->SessionState == EOnlineSessionState::Creating || Existing->SessionState == EOnlineSessionState::Destroying))
	{
		FinishJoiningLobby(false, TEXT("Native membership changed during target lookup; no lobby was closed."), EEOSOperationCode::StaleResult); return;
	}
	if (!Capacity.bKnown) UE_LOG(LogExtendedEOS, Log, TEXT("EOSLobbyJoin Request=%lld Phase=AuthorizedDetailsFallback CapacityKnown=0; native retained details enforce authorization and capacity"), RequestId);
	const auto Target = JoinRequest.GetResult();
	JoinRequest.CompletePreflight(Existing != nullptr); RefreshLobbyState(Existing);
	if (!Existing) { StartJoiningLobby(); return; }
	SetOperationPhase(TEXT("JoinLobby"), TEXT("ClosingExisting"));
	if (!StartLobbyExit(OperationSessions, IsLobbyOwner(), FEEOSLobbyExitRequest::EContinuation::Join))
		FinishJoiningLobby(false, TEXT("Could not start closing the existing lobby after capacity refresh."), EEOSOperationCode::ExistingLobbyCloseFailed);
}


bool UEEOSLobbySubsystem::IsLobbyExitInProgress() const
{
	return ExitRequest.IsActive();
}

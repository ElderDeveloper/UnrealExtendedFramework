// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceSubsystem.h"
#include "EEOSVoiceCaptureState.h"
#include "Shared/EEOSSettings.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "IOnlineSubsystemEOS.h"
#include "VoiceChat.h"
#include "Async/Async.h"
#include "UnrealExtendedEOS.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_lobby.h"
#include "eos_sdk.h"

/** EOS calls back on the game thread, from the platform tick. */
struct FEEOSVoiceLobbyRTCCallbacks
{
	static void EOS_CALL OnRoomConnectionChanged(const EOS_Lobby_RTCRoomConnectionChangedCallbackInfo* Data)
	{
		if (!Data || !Data->ClientData || Data->bIsConnected != EOS_TRUE)
		{
			return;
		}

		FString LocalUserId;
		char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t BufferLength = sizeof(Buffer);
		if (Data->LocalUserId && EOS_ProductUserId_ToString(Data->LocalUserId, Buffer, &BufferLength) == EOS_EResult::EOS_Success)
		{
			LocalUserId = UTF8_TO_TCHAR(Buffer);
		}
		static_cast<UEEOSVoiceSubsystem*>(Data->ClientData)->HandleLobbyRTCRoomConnected(LocalUserId);
	}
};
#endif

namespace
{
	/** Must match LOBBY_SESSION_NAME in Sessions/EEOSLobbySubsystem.cpp. */
	const FName GVoiceLobbySessionName(TEXT("EOS_Lobby"));
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableVoiceChat)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat is disabled in settings — subsystem inactive"));
		return;
	}

	// Watch identity login status: the voice user can only be resolved once the local player
	// has a logged-in EOS identity (the OSS logs the voice user in with the Product User Id),
	// and it must be dropped when the identity logs out (the OSS releases it right after the
	// status-changed broadcast — see UserManagerEOS::Logout → RemoveLocalUser).
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
		if (Identity.IsValid())
		{
			IdentityStatusChangedHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(0,
				FOnLoginStatusChangedDelegate::CreateUObject(this, &UEEOSVoiceSubsystem::HandleIdentityLoginStatusChanged));
		}
	}

	// The identity may already be logged in (subsystem init order is undefined) — try now.
	ResolveVoiceUser();

	// Periodic stale-contribution sweep: a component destroyed without EndPlay (abnormal
	// teardown) never unregisters its volume contributions, and if no live component remains
	// to drive aggregation, the dead values would pin player volumes forever. A slow sweep
	// is enough — the per-aggregation purge handles the common case promptly.
	StaleContributionSweepHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEEOSVoiceSubsystem::HandleStaleContributionSweep), 5.0f);
}

void UEEOSVoiceSubsystem::Deinitialize()
{
	if (StaleContributionSweepHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(StaleContributionSweepHandle);
		StaleContributionSweepHandle.Reset();
	}

	// Existence checks below use IOnlineSubsystem::DoesInstanceExist, never Get(): during
	// shutdown Get() would lazily CREATE the OSS (so it can never report destruction), and
	// spinning up an online subsystem mid-teardown is itself harmful.

	// Unhook the identity watcher first (the identity interface may already be gone at shutdown).
	if (IdentityStatusChangedHandle.IsValid())
	{
		if (IOnlineSubsystem::DoesInstanceExist(EOS_SUBSYSTEM))
		{
			if (IOnlineSubsystem* EOSSub = IOnlineSubsystem::Get(EOS_SUBSYSTEM))
			{
				IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
				if (Identity.IsValid())
				{
					Identity->ClearOnLoginStatusChangedDelegate_Handle(0, IdentityStatusChangedHandle);
				}
			}
		}
		IdentityStatusChangedHandle.Reset();
	}

	// If the OSS instance is already destroyed, an OSS-managed voice user wrapper died with it —
	// touching it (even to unbind its delegates) would be use-after-free. Just forget the
	// pointer; TearDownVoiceUser below then has nothing to unbind.
	if (CachedVoiceChatUser && !bOwnsVoiceUser && !IOnlineSubsystem::DoesInstanceExist(EOS_SUBSYSTEM))
	{
		CachedVoiceChatUser = nullptr;
	}

	// Deliberately no LeaveAllVoiceRooms here: lobby-managed RTC rooms reject LeaveChannel
	// (engine: NotPermitted, "lobby rooms can only be removed with RemoveLobbyRoom") and the
	// standalone path's Logout leaves all channels internally.
	TearDownVoiceUser();
	{ FScopeLock Lock(&CaptureStatesMutex); CaptureStates.Empty(); }

	JoinedRooms.Empty();
	RoomRefCounts.Empty();
	RoomTransmitRefCounts.Empty();
	VolumeContributions.Empty();
	PlayerVolumeScales.Empty();
	AppliedPlayerVolumes.Empty();

	Super::Deinitialize();
}

IVoiceChatUser* UEEOSVoiceSubsystem::GetCachedVoiceChatUser() const
{
	return CachedVoiceChatUser;
}

bool UEEOSVoiceSubsystem::IsUsingStandaloneVoiceUser() const
{
	return bOwnsVoiceUser && CachedVoiceChatUser != nullptr;
}

bool UEEOSVoiceSubsystem::HasLobbyVoiceUser() const
{
	return CachedVoiceChatUser != nullptr && !bOwnsVoiceUser && bVoiceUserLoggedIn;
}

void UEEOSVoiceSubsystem::SetLocalSpeechGate(const bool bInSilenceWhenInactive, const float RmsThreshold, const float ReleaseSeconds)
{
	bSilenceWhenInactive = bInSilenceWhenInactive;
	SpeechRmsThreshold = FMath::Clamp(RmsThreshold, 0.0f, 1.0f);
	SpeechReleaseSeconds = FMath::Clamp(ReleaseSeconds, 0.05f, 2.0f);
}

void UEEOSVoiceSubsystem::SetMicrophoneTestMode(const bool bEnabled)
{
	if (bMicrophoneTest == bEnabled)
	{
		return;
	}
	bMicrophoneTest = bEnabled;
	// Clear test activity only on a mode transition, never on each policy refresh.
	bLocalSpeechActive = false;
	SpeechHoldUntilSeconds = 0.0;
}

bool UEEOSVoiceSubsystem::IsLocalSpeechActive() const
{
	return bLocalSpeechActive;
}

float UEEOSVoiceSubsystem::GetLocalCaptureLevel() const
{
	return LocalCaptureLevel;
}

void UEEOSVoiceSubsystem::HandleCapturedAudio(const FString& ChannelName, TArrayView<int16> PcmSamples, int SampleRate, int Channels)
{
	double Sum = 0.0;
	for (const int16 Sample : PcmSamples)
	{
		const double Normalized = static_cast<double>(Sample) / 32768.0;
		Sum += Normalized * Normalized;
	}

	const float Rms = PcmSamples.Num() > 0 ? static_cast<float>(FMath::Sqrt(Sum / static_cast<double>(PcmSamples.Num()))) : 0.0f;
	LocalCaptureLevel = Rms;

	// Snapshot only shared capture state. No component UObject access on this audio thread.
	TArray<TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>> States;
	{
		FScopeLock Lock(&CaptureStatesMutex);
		CaptureStates.GenerateValueArray(States);
	}
	if (!States.IsEmpty())
	{
		bool bPass = false;
		bool bTestCapture = false;
		bool bAnySpeech = false;
		const double CaptureTime = FPlatformTime::Seconds();
		for (const auto& State : States)
		{
			{
				FScopeLock Lock(&State->Mutex);
				bTestCapture |= State->bTest;
			}
			bPass |= State->Process(ChannelName, Rms, CaptureTime);
			bAnySpeech |= State->IsTalking();
			// Before the gate below zeroes anything: a replay take keeps what the key captured.
			State->Record(ChannelName, PcmSamples, SampleRate, Channels);
		}
		bLocalSpeechActive = bAnySpeech && !bTestCapture;
		if (!bPass || bTestCapture)
			for (int16& Sample : PcmSamples) Sample = 0;
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Rms >= SpeechRmsThreshold)
	{
		SpeechHoldUntilSeconds = Now + static_cast<double>(SpeechReleaseSeconds);
		bLocalSpeechActive = true;
	}
	else if (Now >= SpeechHoldUntilSeconds)
	{
		bLocalSpeechActive = false;
	}

	if (bMicrophoneTest || (bSilenceWhenInactive && !bLocalSpeechActive))
	{
		for (int16& Sample : PcmSamples)
		{
			Sample = 0;
		}
	}
}

void UEEOSVoiceSubsystem::HandleAudioAboutToSend(const FString& /*ChannelName*/, TArrayView<const int16> /*PcmSamples*/, int /*SampleRate*/, int /*Channels*/, const bool bIsSpeaking)
{
	{
		FScopeLock Lock(&CaptureStatesMutex);
		if (!CaptureStates.IsEmpty()) return;
	}
	if (!bIsSpeaking)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	SpeechHoldUntilSeconds = Now + static_cast<double>(SpeechReleaseSeconds);
	bLocalSpeechActive = true;
}

// ── Voice user resolution ────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::ResolveVoiceUser()
{
	if (CachedVoiceChatUser)
	{
		return;
	}

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableVoiceChat)
	{
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	if (!EOSSub)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: EOS OSS not available yet — voice user resolution deferred"));
		return;
	}

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Identity interface not available — voice user resolution deferred"));
		return;
	}

	// The OSS may not have existed at Initialize (subsystem init order is undefined) — make
	// sure the identity watcher is bound the first time it is reachable.
	if (!IdentityStatusChangedHandle.IsValid())
	{
		IdentityStatusChangedHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(0,
			FOnLoginStatusChangedDelegate::CreateUObject(this, &UEEOSVoiceSubsystem::HandleIdentityLoginStatusChanged));
	}

	FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
	if (!UserId.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Local identity not logged in yet — voice user resolves on login"));
		return;
	}

	// ── Primary route: the OSS-managed EOS voice user ────────────────────────
	// FOnlineSubsystemEOS::GetVoiceChatUserInterface lazily creates the shared IVoiceChat
	// instance (Initialize + Connect) and creates/logs in the local voice user with the
	// Product User Id string itself. The returned wrapper is engine-owned: Login/Logout on it
	// are checkNoEntry'd, and the OSS releases it on identity logout and at shutdown. This is
	// also the exact user instance the engine attaches lobby RTC rooms to on lobby join.
	IOnlineSubsystemEOS* EOSInterface = static_cast<IOnlineSubsystemEOS*>(EOSSub);
	if (IVoiceChatUser* OSSVoiceUser = EOSInterface->GetVoiceChatUserInterface(*UserId))
	{
		CachedVoiceChatUser = OSSVoiceUser;
		bOwnsVoiceUser = false;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice user resolved via IOnlineSubsystemEOS (OSS-managed, PUID login handled by the engine)"));

		BindVoiceUserDelegates();

		if (CachedVoiceChatUser->IsLoggedIn())
		{
			HandleVoiceChatLoggedIn(CachedVoiceChatUser->GetLoggedInPlayerName());
		}

		// Seed channels the engine already joined (e.g. we resolved after a lobby join).
		for (const FString& Channel : CachedVoiceChatUser->GetChannels())
		{
			if (!JoinedRooms.Contains(Channel))
			{
				HandleChannelJoined(Channel);
			}
		}
		return;
	}

	// ── Fallback: standalone IVoiceChat ──────────────────────────────────────
	// Only reachable when the OSS voice route is unavailable (WITH_EOSVOICECHAT disabled or
	// the engine's voice bring-up failed). NOTE: a standalone IVoiceChat runs its own EOS
	// platform instance, so lobby RTC rooms joined by the game's OSS will NOT appear on it.
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: OSS voice route unavailable — falling back to standalone IVoiceChat (lobby RTC rooms will not be visible on this path)"));

	const FString ProductUserId = UEEOSBlueprintLibrary::ExtractProductUserId(UserId->ToString());
	if (ProductUserId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Local identity has no Product User Id — cannot login the standalone voice user"));
		return;
	}

	ResolveStandaloneVoiceUser(ProductUserId);
}

void UEEOSVoiceSubsystem::ResolveStandaloneVoiceUser(const FString& ProductUserId)
{
	IVoiceChat* VoiceChat = IVoiceChat::Get();
	if (!VoiceChat)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: IVoiceChat interface not available — voice will not function"));
		return;
	}

	if (!VoiceChat->IsInitialized() && !VoiceChat->Initialize())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: IVoiceChat::Initialize failed — voice will not function"));
		return;
	}

	if (VoiceChat->IsConnected())
	{
		CreateAndLoginStandaloneUser(ProductUserId);
		return;
	}

	VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda(
		[WeakThis = TWeakObjectPtr<UEEOSVoiceSubsystem>(this), ProductUserId](const FVoiceChatResult& Result)
		{
			UEEOSVoiceSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return; // no user was created yet — nothing to release
			}

			if (!Result.IsSuccess())
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: IVoiceChat::Connect failed — %s"), *Result.ErrorDesc);
				return;
			}

			Self->CreateAndLoginStandaloneUser(ProductUserId);
		}));
}

void UEEOSVoiceSubsystem::CreateAndLoginStandaloneUser(const FString& ProductUserId)
{
	if (CachedVoiceChatUser)
	{
		return; // resolved through another path while connecting
	}

	IVoiceChat* VoiceChat = IVoiceChat::Get();
	if (!VoiceChat)
	{
		return;
	}

	CachedVoiceChatUser = VoiceChat->CreateUser();
	if (!CachedVoiceChatUser)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Failed to create standalone voice chat user"));
		return;
	}
	bOwnsVoiceUser = true;

	// Bind before login so the LoggedIn event (which applies the configured defaults) cannot be missed.
	BindVoiceUserDelegates();

	const FPlatformUserId PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
	bVoiceLoginPending = true;
	// Deliberately NOT CreateWeakLambda: if the subsystem dies while the login is in flight,
	// this callback must still run to release the orphaned voice user — a weak lambda would
	// silently never fire. Raw `this` is never touched; access goes through the weak ptr.
	CachedVoiceChatUser->Login(PlatformUserId, ProductUserId, TEXT(""),
		FOnVoiceChatLoginCompleteDelegate::CreateLambda(
			[WeakThis = TWeakObjectPtr<UEEOSVoiceSubsystem>(this), LoginUser = CachedVoiceChatUser](const FString& LoggedInUserId, const FVoiceChatResult& Result)
			{
				UEEOSVoiceSubsystem* Self = WeakThis.Get();
				if (!Self || Self->CachedVoiceChatUser != LoginUser)
				{
					// Subsystem was destroyed or deinitialized while the login was pending —
					// the login op is done now, so it is safe to release the user it was
					// operating on.
					if (IVoiceChat* CurrentVoiceChat = IVoiceChat::Get())
					{
						CurrentVoiceChat->ReleaseUser(LoginUser);
					}
					return;
				}

				Self->bVoiceLoginPending = false;
				if (Result.IsSuccess())
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Standalone voice user logged in as '%s'"), *LoggedInUserId);
					// Defaults apply from the OnVoiceChatLoggedIn event handler.
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: Standalone voice login failed — %s"), *Result.ErrorDesc);
				}
			}));
}

void UEEOSVoiceSubsystem::BindVoiceUserDelegates()
{
	if (!CachedVoiceChatUser)
	{
		return;
	}

	LoggedInHandle      = CachedVoiceChatUser->OnVoiceChatLoggedIn().AddUObject(this, &UEEOSVoiceSubsystem::HandleVoiceChatLoggedIn);
	LoggedOutHandle     = CachedVoiceChatUser->OnVoiceChatLoggedOut().AddUObject(this, &UEEOSVoiceSubsystem::HandleVoiceChatLoggedOut);
	ChannelJoinedHandle = CachedVoiceChatUser->OnVoiceChatChannelJoined().AddUObject(this, &UEEOSVoiceSubsystem::HandleChannelJoined);
	ChannelExitedHandle = CachedVoiceChatUser->OnVoiceChatChannelExited().AddUObject(this, &UEEOSVoiceSubsystem::HandleChannelExited);
	PlayerAddedHandle   = CachedVoiceChatUser->OnVoiceChatPlayerAdded().AddUObject(this, &UEEOSVoiceSubsystem::HandlePlayerAdded);
	PlayerRemovedHandle = CachedVoiceChatUser->OnVoiceChatPlayerRemoved().AddUObject(this, &UEEOSVoiceSubsystem::HandlePlayerRemoved);
	PlayerTalkingHandle = CachedVoiceChatUser->OnVoiceChatPlayerTalkingUpdated().AddUObject(this, &UEEOSVoiceSubsystem::HandlePlayerTalkingUpdated);
	AudioDevicesChangedHandle = CachedVoiceChatUser->OnVoiceChatAvailableAudioDevicesChanged().AddUObject(this, &UEEOSVoiceSubsystem::HandleAvailableAudioDevicesChanged);
	BindLobbyRTCNotification();
}

void UEEOSVoiceSubsystem::UnbindVoiceUserDelegates()
{
	if (!CachedVoiceChatUser)
	{
		return;
	}

	CachedVoiceChatUser->OnVoiceChatLoggedIn().Remove(LoggedInHandle);
	CachedVoiceChatUser->OnVoiceChatLoggedOut().Remove(LoggedOutHandle);
	CachedVoiceChatUser->OnVoiceChatChannelJoined().Remove(ChannelJoinedHandle);
	CachedVoiceChatUser->OnVoiceChatChannelExited().Remove(ChannelExitedHandle);
	CachedVoiceChatUser->OnVoiceChatPlayerAdded().Remove(PlayerAddedHandle);
	CachedVoiceChatUser->OnVoiceChatPlayerRemoved().Remove(PlayerRemovedHandle);
	CachedVoiceChatUser->OnVoiceChatPlayerTalkingUpdated().Remove(PlayerTalkingHandle);
	CachedVoiceChatUser->OnVoiceChatAvailableAudioDevicesChanged().Remove(AudioDevicesChangedHandle);

	if (CaptureReadHandle.IsValid())
	{
		CachedVoiceChatUser->UnregisterOnVoiceChatAfterCaptureAudioReadDelegate(CaptureReadHandle);
		CaptureReadHandle.Reset();
	}
	if (CaptureSentHandle.IsValid())
	{
		CachedVoiceChatUser->UnregisterOnVoiceChatBeforeCaptureAudioSentDelegate(CaptureSentHandle);
		CaptureSentHandle.Reset();
	}

	LoggedInHandle.Reset();
	LoggedOutHandle.Reset();
	ChannelJoinedHandle.Reset();
	ChannelExitedHandle.Reset();
	PlayerAddedHandle.Reset();
	PlayerRemovedHandle.Reset();
	PlayerTalkingHandle.Reset();
	AudioDevicesChangedHandle.Reset();
}

void UEEOSVoiceSubsystem::TearDownVoiceUser()
{
	// First and unconditionally: the notification holds this object as its client data, and the
	// early return below (voice user already forgotten) must not leave it registered.
	UnbindLobbyRTCNotification();

	TGuardValue<bool> TeardownGuard(bTearingDownVoiceUser, true);
	bLocalMuteApplied = false;
	++VoiceUserGeneration;
	const TSet<FString> JoinsToFail = MoveTemp(PendingJoins);
	const TSet<FString> LeavesToFail = MoveTemp(PendingLeaves);
	PendingJoins.Reset();
	PendingLeaves.Reset();
	ManagedRooms.Reset();
	TalkingRoomsByPlayer.Reset();
	for (const FString& Room : JoinsToFail) OnVoiceRoomJoinFailed.Broadcast(Room, TEXT("Voice user disconnected."));
	for (const FString& Room : LeavesToFail) OnVoiceRoomLeaveFailed.Broadcast(Room, TEXT("Voice user disconnected."));
	if (!CachedVoiceChatUser)
	{
		bVoiceUserLoggedIn = false;
		bVoiceLoginPending = false;
		bDefaultsApplied = false;
		bOwnsVoiceUser = false;
		return;
	}

	UnbindVoiceUserDelegates();

	IVoiceChatUser* UserToRelease = CachedVoiceChatUser;
	CachedVoiceChatUser = nullptr;
	AppliedPlayerVolumes.Empty();
	bDefaultsApplied = false;

	if (!bOwnsVoiceUser)
	{
		// OSS-managed wrapper: the engine owns its lifetime. Logout/Release on the wrapper are
		// forbidden (checkNoEntry in FOnlineSubsystemEOSVoiceChatUserWrapper) — just forget it.
		bVoiceUserLoggedIn = false;
		bVoiceLoginPending = false;
		return;
	}

	bOwnsVoiceUser = false;

	// Logout and release the standalone voice user — always, if one was created, not only when
	// logged in. The subsystem gives up ownership here so no completion callback can hand the
	// user back to it afterwards.
	if (bVoiceUserLoggedIn)
	{
		bVoiceUserLoggedIn = false;

		// Release only after logout completes
		UserToRelease->Logout(
			FOnVoiceChatLogoutCompleteDelegate::CreateLambda(
				[UserToRelease](const FString& UserId, const FVoiceChatResult& Result)
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user logged out — %s"),
						Result.IsSuccess() ? TEXT("success") : TEXT("failed"));

					// Now safe to release — logout flow is complete
					if (IVoiceChat* VoiceChat = IVoiceChat::Get())
					{
						VoiceChat->ReleaseUser(UserToRelease);
					}
				}));
	}
	else if (bVoiceLoginPending)
	{
		// A login is still in flight — releasing now would pull the user out from under the
		// pending operation. The login completion delegate sees that the subsystem no longer
		// owns the user and releases it instead.
		bVoiceLoginPending = false;
	}
	else
	{
		// No login attempted (or it already failed) — safe to release immediately
		if (IVoiceChat* VoiceChat = IVoiceChat::Get())
		{
			VoiceChat->ReleaseUser(UserToRelease);
		}
	}
}

// ── Engine event handlers ────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::HandleIdentityLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (LocalUserNum != 0)
	{
		return;
	}

	if (NewStatus == ELoginStatus::LoggedIn)
	{
		ResolveVoiceUser();
	}
	else if (NewStatus == ELoginStatus::NotLoggedIn && CachedVoiceChatUser)
	{
		// The OSS releases its voice user right after this broadcast (UserManagerEOS::Logout
		// fires the status change BEFORE RemoveLocalUser releases the wrapper), so drop our
		// reference now while it is still valid.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Identity logged out — dropping voice user"));

		// Surface room exits to listeners; the engine tears the channels down with the user.
		// Iterate a MOVED copy: a listener reacting synchronously (e.g. re-joining a room)
		// mutates JoinedRooms, which would invalidate a live TSet iterator (UB). Anything a
		// listener adds during the broadcast lands in the fresh set and is preserved.
		TSet<FString> RoomsToClose = MoveTemp(JoinedRooms);
		JoinedRooms.Reset();
		for (const FString& Room : RoomsToClose)
		{
			OnVoiceRoomLeft.Broadcast(Room);
		}

		TearDownVoiceUser();
	}
}

void UEEOSVoiceSubsystem::HandleVoiceChatLoggedIn(const FString& PlayerName)
{
	if (bVoiceUserLoggedIn)
	{
		return;
	}
	bVoiceUserLoggedIn = true;
	bLocalMuteApplied = false;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user logged in as '%s'"), *PlayerName);
	ApplyVoiceDefaults();

	if (CachedVoiceChatUser && !CaptureReadHandle.IsValid())
	{
		CaptureReadHandle = CachedVoiceChatUser->RegisterOnVoiceChatAfterCaptureAudioReadDelegate(
			FOnVoiceChatAfterCaptureAudioReadDelegate2::FDelegate::CreateUObject(this, &UEEOSVoiceSubsystem::HandleCapturedAudio));
		CaptureSentHandle = CachedVoiceChatUser->RegisterOnVoiceChatBeforeCaptureAudioSentDelegate(
			FOnVoiceChatBeforeCaptureAudioSentDelegate2::FDelegate::CreateUObject(this, &UEEOSVoiceSubsystem::HandleAudioAboutToSend));
	}
}

void UEEOSVoiceSubsystem::HandleVoiceChatLoggedOut(const FString& PlayerName)
{
	bVoiceUserLoggedIn = false;
	bDefaultsApplied = false;
	bLocalMuteApplied = false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Voice chat user '%s' logged out"), *PlayerName);
}

void UEEOSVoiceSubsystem::HandleChannelJoined(const FString& ChannelName)
{
	JoinedRooms.Add(ChannelName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Joined voice room '%s'"), *ChannelName);

	// A registered transmit room may just have become live.
	RecomputeTransmitChannels();

	OnVoiceRoomJoined.Broadcast(ChannelName);
}

void UEEOSVoiceSubsystem::HandleChannelExited(const FString& ChannelName, const FVoiceChatResult& Reason)
{
	JoinedRooms.Remove(ChannelName);
	ManagedRooms.Remove(ChannelName);
	PendingLeaves.Remove(ChannelName);
	for (auto& Player : TalkingRoomsByPlayer)
	{
		if (Player.Value.Remove(ChannelName) && Player.Value.IsEmpty()) OnPlayerTalking.Broadcast(Player.Key, false);
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Left voice room '%s'%s%s"), *ChannelName,
		Reason.IsSuccess() ? TEXT("") : TEXT(" — "), Reason.IsSuccess() ? TEXT("") : *Reason.ErrorDesc);

	RecomputeTransmitChannels();

	OnVoiceRoomLeft.Broadcast(ChannelName);
}

void UEEOSVoiceSubsystem::HandlePlayerAdded(const FString& ChannelName, const FString& PlayerName)
{
	OnPlayerJoinedRoom.Broadcast(ChannelName, PlayerName);
}

void UEEOSVoiceSubsystem::HandlePlayerRemoved(const FString& ChannelName, const FString& PlayerName)
{
	if (TSet<FString>* Rooms = TalkingRoomsByPlayer.Find(PlayerName))
	{
		if (Rooms->Remove(ChannelName) && Rooms->IsEmpty()) OnPlayerTalking.Broadcast(PlayerName, false);
	}
	OnPlayerLeftRoom.Broadcast(ChannelName, PlayerName);
}

void UEEOSVoiceSubsystem::HandlePlayerTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking)
{
	TSet<FString>& Rooms = TalkingRoomsByPlayer.FindOrAdd(PlayerName);
	const bool bWasTalking = !Rooms.IsEmpty();
	if (bIsTalking) Rooms.Add(ChannelName); else Rooms.Remove(ChannelName);
	const bool bNowTalking = !Rooms.IsEmpty();
	if (bNowTalking != bWasTalking || !bIsTalking) OnPlayerTalking.Broadcast(PlayerName, bNowTalking);
}

void UEEOSVoiceSubsystem::HandleAvailableAudioDevicesChanged()
{
	// EOS delivers this through its platform tick, which is the game thread today. Dynamic
	// delegates must not broadcast from anywhere else, so hop over if that ever changes.
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UEEOSVoiceSubsystem>(this)]()
		{
			if (UEEOSVoiceSubsystem* Self = WeakThis.Get())
			{
				Self->HandleAvailableAudioDevicesChanged();
			}
		});
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Audio devices changed (%d input, %d output)"),
		GetInputDevices().Num(), GetOutputDevices().Num());
	OnAudioDevicesChanged.Broadcast();
}

void UEEOSVoiceSubsystem::ApplyVoiceDefaults()
{
	if (bDefaultsApplied || !CachedVoiceChatUser)
	{
		return;
	}
	bDefaultsApplied = true;

	// IVoiceChatUser starts in TransmitMode::All, and nothing in the engine changes that when it joins
	// a lobby RTC room. Left alone, every channel joined before a voice point registered would carry
	// the raw microphone. Start closed: components open it through RecomputeTransmitChannels, and a
	// game without components opens it with one of the TransmitTo* calls.
	if (IsTransmitCompositionActive())
	{
		RecomputeTransmitChannels();
	}
	else
	{
		CachedVoiceChatUser->TransmitToNoChannels();
	}

	// Apply the configured voice defaults now that the user is live. Routed through the
	// subsystem's own wrappers so their cached state (CurrentInputVolume, bLocalMuted) stays
	// consistent with what was pushed to the IVoiceChatUser.
	if (const UEEOSSettings* VoiceSettings = GetEOSSettings())
	{
		SetOutputVolume(VoiceSettings->DefaultOutputVolume);
		SetInputVolume(VoiceSettings->DefaultInputVolume);
		SetLocalMuted(VoiceSettings->bStartMuted);
	}
}

// ── Room Management ──────────────────────────────────────────────────────────


bool UEEOSVoiceSubsystem::JoinVoiceRoom(const FString& RoomName, const FString& ChannelCredentials)
{
	auto Fail = [this, &RoomName](const FString& Error)
	{
		OnVoiceRoomJoinFailed.Broadcast(RoomName, Error);
		return false;
	};
	if (bTearingDownVoiceUser) return Fail(TEXT("Voice user is disconnecting."));
	const UEEOSSettings* Settings = GetEOSSettings();
	if (RoomName.IsEmpty()) return Fail(TEXT("Room name is empty."));
	if (Settings && !Settings->bEnableVoiceChat) return Fail(TEXT("Voice chat is disabled."));
	if (!CachedVoiceChatUser) ResolveVoiceUser();
	if (!CachedVoiceChatUser || !bVoiceUserLoggedIn) return Fail(TEXT("Voice user is not logged in."));
	if (PendingLeaves.Contains(RoomName)) return Fail(TEXT("The room is still leaving."));
	if (CachedVoiceChatUser->GetChannels().Contains(RoomName))
	{
		JoinedRooms.Add(RoomName);
		OnVoiceRoomJoined.Broadcast(RoomName);
		return true;
	}
	if (PendingJoins.Contains(RoomName)) return true;
	if (ChannelCredentials.IsEmpty())
		return Fail(TEXT("Custom rooms require trusted-server credentials. Lobby rooms are joined through lobby membership."));
	PendingJoins.Add(RoomName);
	ManagedRooms.Add(RoomName);
	const uint64 Generation = VoiceUserGeneration;
	CachedVoiceChatUser->JoinChannel(RoomName, ChannelCredentials, EVoiceChatChannelType::NonPositional,
		FOnVoiceChatChannelJoinCompleteDelegate::CreateWeakLambda(this,
			[this, Generation](const FString& ChannelName, const FVoiceChatResult& Result)
			{
				if (Generation != VoiceUserGeneration) return;
				PendingJoins.Remove(ChannelName);
				if (!Result.IsSuccess())
				{
					ManagedRooms.Remove(ChannelName);
					OnVoiceRoomJoinFailed.Broadcast(ChannelName, Result.ErrorDesc);
				}
				// Successful membership is published by HandleChannelJoined, not optimistically.
			}));
	return true;
}

bool UEEOSVoiceSubsystem::LeaveVoiceRoom(const FString& RoomName)
{
	auto Fail = [this, &RoomName](const FString& Error)
	{
		OnVoiceRoomLeaveFailed.Broadcast(RoomName, Error);
		return false;
	};
	if (bTearingDownVoiceUser) return Fail(TEXT("Voice user is disconnecting."));
	if (PendingJoins.Contains(RoomName)) return Fail(TEXT("The join is still pending. Wait for its completion before leaving."));
	if (!CachedVoiceChatUser || !CachedVoiceChatUser->GetChannels().Contains(RoomName))
		return Fail(TEXT("The local user is not in this room."));
	if (!ManagedRooms.Contains(RoomName))
		return Fail(TEXT("This channel is managed externally. Lobby RTC channels leave with lobby membership; remove the component binding to stop using it locally."));
	if (PendingLeaves.Contains(RoomName)) return true;
	PendingLeaves.Add(RoomName);
	const uint64 Generation = VoiceUserGeneration;
	CachedVoiceChatUser->LeaveChannel(RoomName,
		FOnVoiceChatChannelLeaveCompleteDelegate::CreateWeakLambda(this,
			[this, Generation](const FString& ChannelName, const FVoiceChatResult& Result)
			{
				if (Generation != VoiceUserGeneration) return;
				PendingLeaves.Remove(ChannelName);
				if (!Result.IsSuccess()) OnVoiceRoomLeaveFailed.Broadcast(ChannelName, Result.ErrorDesc);
			}));
	return true;
}

void UEEOSVoiceSubsystem::LeaveAllVoiceRooms()
{
	const TArray<FString> Rooms = GetActiveVoiceRooms();
	for (const FString& Room : Rooms)
	{
		LeaveVoiceRoom(Room);
	}
}

// ── Component Aggregation ────────────────────────────────────────────────────

bool UEEOSVoiceSubsystem::RegisterVoiceRoomUser(const FString& RoomName, bool bTransmit)
{
	if (RoomName.IsEmpty())
	{
		return false;
	}

	RoomRefCounts.FindOrAdd(RoomName) += 1;
	if (bTransmit)
	{
		RoomTransmitRefCounts.FindOrAdd(RoomName) += 1;
	}

	RecomputeTransmitChannels();

	const bool bJoined = IsInRoom(RoomName);
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSVoiceSubsystem: Component registered on room '%s' (refs=%d, transmit=%s, joined=%s)"),
		*RoomName, RoomRefCounts[RoomName], bTransmit ? TEXT("yes") : TEXT("no"), bJoined ? TEXT("yes") : TEXT("no"));
	return bJoined;
}

void UEEOSVoiceSubsystem::UnregisterVoiceRoomUser(const FString& RoomName, bool bTransmit)
{
	if (RoomName.IsEmpty())
	{
		return;
	}

	if (int32* Refs = RoomRefCounts.Find(RoomName))
	{
		if (--(*Refs) <= 0)
		{
			RoomRefCounts.Remove(RoomName);
			// Last user out: the room drops from the transmit set below. Channel membership
			// itself is owned by the lobby and ends when the lobby is left — nothing to leave here.
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Last component left room '%s' — removed from transmit composition (channel membership stays with the lobby)"), *RoomName);
		}
	}

	if (bTransmit)
	{
		if (int32* TransmitRefs = RoomTransmitRefCounts.Find(RoomName))
		{
			if (--(*TransmitRefs) <= 0)
			{
				RoomTransmitRefCounts.Remove(RoomName);
			}
		}
	}

	if (IsTransmitCompositionActive())
	{
		RecomputeTransmitChannels();
	}
	else if (CachedVoiceChatUser)
	{
		// No components remain: never reopen transmission during teardown.
		CachedVoiceChatUser->TransmitToNoChannels();
	}
}

void UEEOSVoiceSubsystem::SetPlayerVolumeContribution(const UObject* Source, const FString& UserId, float Volume)
{
	if (!Source || UserId.IsEmpty())
	{
		return;
	}

	Volume = FMath::Clamp(Volume, 0.f, 2.f);
	VolumeContributions.FindOrAdd(FObjectKey(Source)).Add(UserId, Volume);
	ApplyAggregatedVolumeForPlayer(UserId);
}

void UEEOSVoiceSubsystem::ClearPlayerVolumeContribution(const UObject* Source, const FString& UserId)
{
	if (!Source || UserId.IsEmpty())
	{
		return;
	}

	const FObjectKey SourceKey(Source);
	if (TMap<FString, float>* Contributions = VolumeContributions.Find(SourceKey))
	{
		if (Contributions->Remove(UserId) > 0)
		{
			if (Contributions->Num() == 0)
			{
				VolumeContributions.Remove(SourceKey);
			}
			ApplyAggregatedVolumeForPlayer(UserId);
		}
	}
}

void UEEOSVoiceSubsystem::ClearPlayerVolumeContributions(const UObject* Source)
{
	if (!Source)
	{
		return;
	}

	TMap<FString, float> Removed;
	if (VolumeContributions.RemoveAndCopyValue(FObjectKey(Source), Removed))
	{
		for (const TPair<FString, float>& Pair : Removed)
		{
			ApplyAggregatedVolumeForPlayer(Pair.Key);
		}
	}
}

void UEEOSVoiceSubsystem::PurgeStaleVolumeContributions()
{
	// A component destroyed without EndPlay never unregisters. Removing only the dead source's
	// entry is not enough: every player it contributed for must be RE-AGGREGATED, or the last
	// value the dead source reported stays pinned on their global volume.
	TSet<FString> AffectedPlayers;
	for (auto It = VolumeContributions.CreateIterator(); It; ++It)
	{
		if (It.Key().ResolveObjectPtr() == nullptr)
		{
			for (const TPair<FString, float>& Pair : It.Value())
			{
				AffectedPlayers.Add(Pair.Key);
			}
			It.RemoveCurrent();
		}
	}

	for (const FString& Player : AffectedPlayers)
	{
		ApplyAggregatedVolumeForPlayerInternal(Player);
	}
}

bool UEEOSVoiceSubsystem::HandleStaleContributionSweep(float DeltaTime)
{
	PurgeStaleVolumeContributions();
	return true; // keep ticking
}

void UEEOSVoiceSubsystem::ApplyAggregatedVolumeForPlayer(const FString& UserId)
{
	// Purge first so a dead source can neither count toward this player's max nor stay pinned
	// on any OTHER player it contributed for (the purge re-applies those players itself).
	PurgeStaleVolumeContributions();
	ApplyAggregatedVolumeForPlayerInternal(UserId);
}

void UEEOSVoiceSubsystem::ApplyAggregatedVolumeForPlayerInternal(const FString& UserId)
{
	if (!CachedVoiceChatUser)
	{
		return;
	}

	// Max-wins across all live contributions. IVoiceChatUser volume is GLOBAL per player —
	// there is no per-channel receive volume — so overlapping rooms must not fight downward:
	// the loudest room a player shares with us decides how loud they are.
	float MaxVolume = -1.f;
	for (const TPair<FObjectKey, TMap<FString, float>>& SourcePair : VolumeContributions)
	{
		if (const float* Contribution = SourcePair.Value.Find(UserId))
		{
			MaxVolume = FMath::Max(MaxVolume, *Contribution);
		}
	}

	const float* Scale = PlayerVolumeScales.Find(UserId);
	const float* Applied = AppliedPlayerVolumes.Find(UserId);
	if (MaxVolume < 0.f && !Scale)
	{
		// No contributions or scale left — restore 1.0, but only if we ever overrode this player.
		if (Applied)
		{
			CachedVoiceChatUser->SetPlayerVolume(UserId, 1.0f);
			AppliedPlayerVolumes.Remove(UserId);
		}
		return;
	}

	const float Target = FMath::Clamp((MaxVolume < 0.f ? 1.0f : MaxVolume) * (Scale ? *Scale : 1.0f), 0.f, 2.f);
	if (Applied && FMath::IsNearlyEqual(*Applied, Target, 0.001f))
	{
		return; // unchanged — skip the SDK call (proximity timers tick frequently)
	}

	CachedVoiceChatUser->SetPlayerVolume(UserId, Target);
	AppliedPlayerVolumes.Add(UserId, Target);
}

void UEEOSVoiceSubsystem::SetPlayerVolumeScale(const FString& UserId, float Scale)
{
	if (UserId.IsEmpty())
	{
		return;
	}

	Scale = FMath::Clamp(Scale, 0.f, 2.f);
	const float* Existing = PlayerVolumeScales.Find(UserId);
	if (FMath::IsNearlyEqual(Scale, 1.0f))
	{
		if (!Existing)
		{
			return;
		}
		PlayerVolumeScales.Remove(UserId);
	}
	else
	{
		if (Existing && FMath::IsNearlyEqual(*Existing, Scale))
		{
			return;
		}
		PlayerVolumeScales.Add(UserId, Scale);
	}

	ApplyAggregatedVolumeForPlayer(UserId);
}

float UEEOSVoiceSubsystem::GetPlayerVolumeScale(const FString& UserId) const
{
	const float* Scale = PlayerVolumeScales.Find(UserId);
	return Scale ? *Scale : 1.0f;
}

void UEEOSVoiceSubsystem::RecomputeTransmitChannels()
{
	if (!CachedVoiceChatUser || !IsTransmitCompositionActive())
	{
		return; // no components registered — leave the transmit mode alone
	}

	// Compose: rooms wanted by transmit-role components, restricted to channels actually
	// joined. Always explicit TransmitToSpecificChannels — the engine default is transmit-to-
	// ALL channels, which would leak the mic into rooms used by listener-only components.
	TSet<FString> TransmitUnion;
	const TArray<FString> Channels = CachedVoiceChatUser->GetChannels();
	for (const TPair<FString, int32>& Pair : RoomTransmitRefCounts)
	{
		if (Pair.Value > 0 && Channels.Contains(Pair.Key))
		{
			TransmitUnion.Add(Pair.Key);
		}
	}

	CachedVoiceChatUser->TransmitToSpecificChannels(TransmitUnion);
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSVoiceSubsystem: Transmit set recomposed (%d room(s))"), TransmitUnion.Num());
}

void UEEOSVoiceSubsystem::ReapplyTransmitState()
{
	if (!CachedVoiceChatUser)
	{
		return;
	}

	// Whatever the mode is, including one a game set by hand, it is restored exactly; only the
	// detour through another mode is new, and that detour never turns sending on anywhere.
	const EVoiceChatTransmitMode Mode = CachedVoiceChatUser->GetTransmitMode();
	if (Mode == EVoiceChatTransmitMode::None)
	{
		CachedVoiceChatUser->TransmitToSpecificChannels(TSet<FString>());
		CachedVoiceChatUser->TransmitToNoChannels();
		return;
	}

	const TSet<FString> Channels = CachedVoiceChatUser->GetTransmitChannels();
	CachedVoiceChatUser->TransmitToNoChannels();
	if (Mode == EVoiceChatTransmitMode::All)
	{
		CachedVoiceChatUser->TransmitToAllChannels();
	}
	else
	{
		CachedVoiceChatUser->TransmitToSpecificChannels(Channels);
	}
}

void UEEOSVoiceSubsystem::HandleLobbyRTCRoomConnected(const FString& LocalUserId)
{
	if (!CachedVoiceChatUser || !bVoiceUserLoggedIn)
	{
		return; // login applies the transmit state itself (ApplyVoiceDefaults)
	}

	// The notification covers every local user on the platform.
	if (!LocalUserId.IsEmpty() && !LocalUserId.Equals(CachedVoiceChatUser->GetLoggedInPlayerName(), ESearchCase::IgnoreCase))
	{
		return;
	}

	// The SDK joins a lobby's RTC room sending, whatever the engine last asked for, and the engine
	// sends nothing more until its transmit mode changes. Left alone that is an open microphone until
	// the first push-to-talk press. This fires again after a reconnect, which rejoins the same way.
	ReapplyTransmitState();
	const EVoiceChatTransmitMode Mode = CachedVoiceChatUser->GetTransmitMode();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Lobby RTC room connected - transmit state re-applied (%s)"),
		Mode == EVoiceChatTransmitMode::None ? TEXT("none")
		: Mode == EVoiceChatTransmitMode::All ? TEXT("all channels")
		: *FString::Printf(TEXT("%d channel(s)"), CachedVoiceChatUser->GetTransmitChannels().Num()));
}

void UEEOSVoiceSubsystem::BindLobbyRTCNotification()
{
#if WITH_EOS_SDK
	if (LobbyRTCConnectionNotifyId != 0 || bOwnsVoiceUser)
	{
		return;
	}

	// The OSS's own platform, not the first active one: with several PIE clients in one process each
	// OSS instance has its own platform and its own lobbies.
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	const IEOSPlatformHandlePtr Platform = EOSSub ? static_cast<IOnlineSubsystemEOS*>(EOSSub)->GetEOSPlatformHandle() : nullptr;
	const EOS_HLobby Lobby = Platform.IsValid() ? EOS_Platform_GetLobbyInterface(*Platform) : nullptr;
	if (!Lobby)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: No EOS lobby interface - a lobby RTC room may send the microphone until the first transmit change"));
		return;
	}

	EOS_Lobby_AddNotifyRTCRoomConnectionChangedOptions Options = {};
	Options.ApiVersion = EOS_LOBBY_ADDNOTIFYRTCROOMCONNECTIONCHANGED_API_LATEST;
	LobbyRTCConnectionNotifyId = EOS_Lobby_AddNotifyRTCRoomConnectionChanged(Lobby, &Options, this, &FEEOSVoiceLobbyRTCCallbacks::OnRoomConnectionChanged);
	if (LobbyRTCConnectionNotifyId == EOS_INVALID_NOTIFICATIONID)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSVoiceSubsystem: EOS_Lobby_AddNotifyRTCRoomConnectionChanged failed - a lobby RTC room may send the microphone until the first transmit change"));
		LobbyRTCConnectionNotifyId = 0;
		return;
	}
	LobbyRTCPlatform = Platform;
#endif
}

void UEEOSVoiceSubsystem::UnbindLobbyRTCNotification()
{
#if WITH_EOS_SDK
	if (LobbyRTCConnectionNotifyId == 0)
	{
		return;
	}

	// A platform that is already released took its notifications with it.
	if (const IEOSPlatformHandlePtr Platform = LobbyRTCPlatform.Pin())
	{
		if (const EOS_HLobby Lobby = EOS_Platform_GetLobbyInterface(*Platform))
		{
			EOS_Lobby_RemoveNotifyRTCRoomConnectionChanged(Lobby, LobbyRTCConnectionNotifyId);
		}
	}
	LobbyRTCConnectionNotifyId = 0;
	LobbyRTCPlatform.Reset();
#endif
}

bool UEEOSVoiceSubsystem::IsTransmitCompositionActive() const
{
	for (const TPair<FString, int32>& Pair : RoomRefCounts)
	{
		if (Pair.Value > 0)
		{
			return true;
		}
	}
	return false;
}

// ── Per-Player Controls ──────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::MutePlayer(const FString& UserId)
{
	SetPlayerMutedIfChanged(UserId, true);
}

void UEEOSVoiceSubsystem::UnmutePlayer(const FString& UserId)
{
	SetPlayerMutedIfChanged(UserId, false);
}

void UEEOSVoiceSubsystem::SetPlayerMutedIfChanged(const FString& UserId, const bool bMuted)
{
	// The engine refuses (with a warning) while logged out, and games commonly re-assert mute state
	// on every policy pass, so a missing user is not worth more than a verbose line.
	if (!CachedVoiceChatUser || !bVoiceUserLoggedIn || UserId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSVoiceSubsystem: %s '%s' skipped — no logged-in voice user"),
			bMuted ? TEXT("Mute") : TEXT("Unmute"), *UserId);
		return;
	}

	if (CachedVoiceChatUser->IsPlayerMuted(UserId) == bMuted)
	{
		return;
	}

	CachedVoiceChatUser->SetPlayerMuted(UserId, bMuted);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: %s '%s'"), bMuted ? TEXT("Muted") : TEXT("Unmuted"), *UserId);
}

void UEEOSVoiceSubsystem::SetPlayerVolume(const FString& UserId, float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 2.f);
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetPlayerVolume(UserId, Volume);
	}
}

float UEEOSVoiceSubsystem::GetPlayerVolume(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->GetPlayerVolume(UserId);
	}
	return 1.0f;
}

bool UEEOSVoiceSubsystem::IsPlayerTalking(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->IsPlayerTalking(UserId);
	}
	return false;
}

bool UEEOSVoiceSubsystem::IsPlayerMuted(const FString& UserId) const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->IsPlayerMuted(UserId);
	}
	return false;
}

// ── Volume & Muting ──────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::SetOutputVolume(float Volume)
{
	Volume = FMath::Clamp(Volume, 0.f, 1.f);
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetAudioOutputVolume(Volume);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Output volume set to %.2f"), Volume);
	}
}

void UEEOSVoiceSubsystem::SetInputVolume(float Volume)
{
	// 0..2 as FEOSVoiceChatUser::SetAudioInputVolume takes it (EOS volume = Volume * 50, 50 = unchanged).
	// Clamping at 1 made any boost impossible.
	Volume = FMath::Clamp(Volume, 0.f, 2.f);
	CurrentInputVolume = Volume;

	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetAudioInputVolume(Volume);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Input volume set to %.2f"), Volume);
	}
}

float UEEOSVoiceSubsystem::GetInputVolume() const
{
	return CurrentInputVolume;
}

void UEEOSVoiceSubsystem::SetLocalMuted(bool bMuted)
{
	const bool bNeedsApply = CachedVoiceChatUser && (!bLocalMuteApplied || bLocalMuted != bMuted);
	bLocalMuted = bMuted;

	if (bNeedsApply)
	{
		CachedVoiceChatUser->SetAudioInputDeviceMuted(bMuted);
		bLocalMuteApplied = true;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Local muted = %s"), bMuted ? TEXT("true") : TEXT("false"));
	}
}

// ── Transmit Modes ───────────────────────────────────────────────────────────

void UEEOSVoiceSubsystem::TransmitToAllRooms()
{
	if (CachedVoiceChatUser)
	{
		if (IsTransmitCompositionActive())
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: TransmitToAllRooms — manual override; active voice components will recompose the transmit set on their next change"));
		}
		CachedVoiceChatUser->TransmitToAllChannels();
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Transmitting to all rooms"));
	}
}

void UEEOSVoiceSubsystem::TransmitToSelectedRoom(const FString& RoomName)
{
	if (CachedVoiceChatUser)
	{
		if (IsTransmitCompositionActive())
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: TransmitToSelectedRoom — manual override; active voice components will recompose the transmit set on their next change"));
		}
		TSet<FString> Channels;
		Channels.Add(RoomName);
		CachedVoiceChatUser->TransmitToSpecificChannels(Channels);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Transmitting to '%s'"), *RoomName);
	}
}

void UEEOSVoiceSubsystem::TransmitToNoRoom()
{
	if (CachedVoiceChatUser)
	{
		if (IsTransmitCompositionActive())
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: TransmitToNoRoom — manual override; active voice components will recompose the transmit set on their next change"));
		}
		CachedVoiceChatUser->TransmitToNoChannels();
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Stopped transmitting"));
	}
}

// ── Device Management ────────────────────────────────────────────────────────

TArray<FEEOSVoiceDeviceInfo> UEEOSVoiceSubsystem::GetInputDevices() const
{
	TArray<FEEOSVoiceDeviceInfo> Devices;

	if (CachedVoiceChatUser)
	{
		TArray<FVoiceChatDeviceInfo> RawDevices = CachedVoiceChatUser->GetAvailableInputDeviceInfos();
		for (const auto& RawDevice : RawDevices)
		{
			FEEOSVoiceDeviceInfo Info;
			Info.DeviceId = RawDevice.Id;
			Info.DisplayName = RawDevice.DisplayName;
			Devices.Add(Info);
		}
	}

	return Devices;
}

TArray<FEEOSVoiceDeviceInfo> UEEOSVoiceSubsystem::GetOutputDevices() const
{
	TArray<FEEOSVoiceDeviceInfo> Devices;

	if (CachedVoiceChatUser)
	{
		TArray<FVoiceChatDeviceInfo> RawDevices = CachedVoiceChatUser->GetAvailableOutputDeviceInfos();
		for (const auto& RawDevice : RawDevices)
		{
			FEEOSVoiceDeviceInfo Info;
			Info.DeviceId = RawDevice.Id;
			Info.DisplayName = RawDevice.DisplayName;
			Devices.Add(Info);
		}
	}

	return Devices;
}

void UEEOSVoiceSubsystem::SetInputDevice(const FString& DeviceId)
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetInputDeviceId(DeviceId);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Input device set to '%s'"), *DeviceId);
	}
}

void UEEOSVoiceSubsystem::SetOutputDevice(const FString& DeviceId)
{
	if (CachedVoiceChatUser)
	{
		CachedVoiceChatUser->SetOutputDeviceId(DeviceId);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSVoiceSubsystem: Output device set to '%s'"), *DeviceId);
	}
}

// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FString> UEEOSVoiceSubsystem::GetActiveVoiceRooms() const
{
	if (CachedVoiceChatUser)
	{
		return CachedVoiceChatUser->GetChannels();
	}
	return JoinedRooms.Array();
}

FString UEEOSVoiceSubsystem::GetLobbyVoiceRoomName() const
{
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	if (!EOSSub)
	{
		return FString();
	}

	IOnlineSessionPtr Sessions = EOSSub->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return FString();
	}

	// EOS dereferences SessionInfo without checking it. A named session is published
	// before async lobby creation/join has populated that shared pointer.
	const FNamedOnlineSession* LobbySession = Sessions->GetNamedSession(GVoiceLobbySessionName);
	if (!LobbySession || !LobbySession->SessionInfo.IsValid()
		|| !LobbySession->SessionInfo->IsValid()
		|| !LobbySession->SessionSettings.bUseLobbiesIfAvailable
		|| LobbySession->SessionState == EOnlineSessionState::Creating
		|| LobbySession->SessionState == EOnlineSessionState::Destroying)
	{
		return FString();
	}

	const IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn
		|| !Identity->GetUniquePlayerId(0).IsValid())
	{
		return FString();
	}

	return Sessions->GetVoiceChatRoomName(0, GVoiceLobbySessionName);
}

bool UEEOSVoiceSubsystem::IsInVoiceRoom() const
{
	return GetActiveVoiceRooms().Num() > 0;
}

bool UEEOSVoiceSubsystem::IsInRoom(const FString& RoomName) const
{
	return GetActiveVoiceRooms().Contains(RoomName);
}

FString UEEOSVoiceSubsystem::GetCurrentRoomName() const
{
	const TArray<FString> Rooms = GetActiveVoiceRooms();
	return Rooms.Num() > 0 ? Rooms[0] : FString();
}

bool UEEOSVoiceSubsystem::IsLocalMuted() const
{
	return bLocalMuted;
}

FString UEEOSVoiceSubsystem::GetLocalVoicePlayerName() const
{
	return CachedVoiceChatUser && bVoiceUserLoggedIn ? CachedVoiceChatUser->GetLoggedInPlayerName() : FString();
}

TArray<FString> UEEOSVoiceSubsystem::GetPlayersInRoom(const FString& RoomName) const
{
	TArray<FString> Players;

	if (CachedVoiceChatUser && !RoomName.IsEmpty())
	{
		Players = CachedVoiceChatUser->GetPlayersInChannel(RoomName);
	}

	return Players;
}

TArray<FString> UEEOSVoiceSubsystem::GetJoinedRooms() const
{
	return GetActiveVoiceRooms();
}

void UEEOSVoiceSubsystem::RegisterCaptureState(const UObject* Source, const TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>& State)
{
	if (!Source || !State) return;
	FScopeLock Lock(&CaptureStatesMutex);
	CaptureStates.Add(FObjectKey(Source), State);
}

void UEEOSVoiceSubsystem::UnregisterCaptureState(const UObject* Source)
{
	FScopeLock Lock(&CaptureStatesMutex);
	CaptureStates.Remove(FObjectKey(Source));
}

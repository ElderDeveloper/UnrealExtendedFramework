// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAntiCheatSubsystem.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"
#include "OnlineSubsystemUtils.h"
#include "Async/Async.h"

#include "eos_anticheatclient.h"
#include "eos_anticheatclient_types.h"
#include "eos_sdk.h"

/**
 * Heap context passed as ClientData to the persistent EOS anti-cheat notifications.
 * The EOS platform outlives this subsystem, so the callbacks must resolve a weak
 * pointer instead of dereferencing a raw 'this' (Phase 1 lifetime pattern).
 */
struct FEEOSAntiCheatNotifyContext
{
	TWeakObjectPtr<UEEOSAntiCheatSubsystem> Self;
};

// ── Static EOS_CALL callbacks ────────────────────────────────────────────────
// All four are issued from within EOS_Platform_Tick. Payloads and handles are
// copied INSIDE the callback (the SDK buffers are only valid for its duration),
// then dispatched to the game thread through the weak context.

static void EOS_CALL EEOSAntiCheat_OnMessageToPeer(const EOS_AntiCheatCommon_OnMessageToClientCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	const FEEOSAntiCheatNotifyContext* Ctx = static_cast<const FEEOSAntiCheatNotifyContext*>(Data->ClientData);

	TArray<uint8> Payload;
	if (Data->MessageData && Data->MessageDataSizeBytes > 0)
	{
		Payload.Append(static_cast<const uint8*>(Data->MessageData), Data->MessageDataSizeBytes);
	}
	const EOS_AntiCheatCommon_ClientHandle TargetHandle = Data->ClientHandle;

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, TargetHandle, Payload = MoveTemp(Payload)]()
	{
		if (UEEOSAntiCheatSubsystem* Self = WeakSelf.Get())
		{
			Self->HandleMessageToPeer(TargetHandle, Payload);
		}
	});
}

static void EOS_CALL EEOSAntiCheat_OnPeerActionRequired(const EOS_AntiCheatCommon_OnClientActionRequiredCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	const FEEOSAntiCheatNotifyContext* Ctx = static_cast<const FEEOSAntiCheatNotifyContext*>(Data->ClientData);

	const EEOSAntiCheatAction Action =
		(Data->ClientAction == EOS_EAntiCheatCommonClientAction::EOS_ACCCA_RemovePlayer)
			? EEOSAntiCheatAction::RemovePlayer
			: EEOSAntiCheatAction::Invalid;
	FString Message(UTF8_TO_TCHAR(Data->ActionReasonDetailsString ? Data->ActionReasonDetailsString : ""));
	const EOS_AntiCheatCommon_ClientHandle PeerHandle = Data->ClientHandle;

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: Peer action required — reason code %d: %s"),
		static_cast<int32>(Data->ActionReasonCode), *Message);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, PeerHandle, Action, Message = MoveTemp(Message)]()
	{
		if (UEEOSAntiCheatSubsystem* Self = WeakSelf.Get())
		{
			Self->HandlePeerActionRequired(PeerHandle, Action, Message);
		}
	});
}

static void EOS_CALL EEOSAntiCheat_OnPeerAuthStatusChanged(const EOS_AntiCheatCommon_OnClientAuthStatusChangedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	const FEEOSAntiCheatNotifyContext* Ctx = static_cast<const FEEOSAntiCheatNotifyContext*>(Data->ClientData);

	const bool bAuthenticated =
		(Data->ClientAuthStatus == EOS_EAntiCheatCommonClientAuthStatus::EOS_ACCCAS_RemoteAuthComplete);
	const EOS_AntiCheatCommon_ClientHandle PeerHandle = Data->ClientHandle;

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, PeerHandle, bAuthenticated]()
	{
		if (UEEOSAntiCheatSubsystem* Self = WeakSelf.Get())
		{
			Self->HandlePeerAuthStatusChanged(PeerHandle, bAuthenticated);
		}
	});
}

static void EOS_CALL EEOSAntiCheat_OnClientIntegrityViolated(const EOS_AntiCheatClient_OnClientIntegrityViolatedCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	const FEEOSAntiCheatNotifyContext* Ctx = static_cast<const FEEOSAntiCheatNotifyContext*>(Data->ClientData);

	FString Message(UTF8_TO_TCHAR(Data->ViolationMessage ? Data->ViolationMessage : ""));
	UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem: CLIENT INTEGRITY VIOLATED (type %d) — %s"),
		static_cast<int32>(Data->ViolationType), *Message);

	AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, Message = MoveTemp(Message)]()
	{
		if (UEEOSAntiCheatSubsystem* Self = WeakSelf.Get())
		{
			Self->OnIntegrityChanged.Broadcast(false);
			Self->OnClientActionRequired.Broadcast(EEOSAntiCheatAction::RemovePlayer, Message);
		}
	});
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void UEEOSAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// All SDK plumbing is session-scoped: notifications are registered in
	// BeginSession and removed in EndSession. When the feature is disabled this
	// subsystem performs ZERO SDK calls.
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings || !Settings->bEnableAntiCheat)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem: Anti-cheat is disabled in settings (bEnableAntiCheat=false)"));
	}
}

void UEEOSAntiCheatSubsystem::Deinitialize()
{
	if (bSessionActive)
	{
		EndSession();
	}
	else if (NotifyContext)
	{
		// Partial BeginSession state should never survive, but be defensive.
		RemoveNotifications();
	}

	PuidToHandle.Empty();
	HandleToPuid.Empty();
	CachedLocalPuid.Reset();
	Super::Deinitialize();
}

bool UEEOSAntiCheatSubsystem::IsAntiCheatEnabled(const TCHAR* CallSite) const
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && Settings->bEnableAntiCheat)
	{
		return true;
	}
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::%s — Anti-cheat is disabled in settings (bEnableAntiCheat=false). No SDK call performed."), CallSite);
	return false;
}

// ── Session Management ───────────────────────────────────────────────────────

bool UEEOSAntiCheatSubsystem::BeginSession()
{
	// FAIL-CLOSED: every failure path below broadcasts OnAntiCheatSessionStarted(false)
	// and leaves bSessionActive == false.
	if (!IsAntiCheatEnabled(TEXT("BeginSession")))
	{
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	if (bSessionActive)
	{
		// Pre-flight rejection of a redundant call. BeginSession is fully
		// synchronous, so no conflicting async op is waiting on this delegate —
		// broadcasting false here is unambiguous and honors the header contract
		// (false on EVERY failure path). The ACTIVE session is left untouched;
		// a NEW session requires EndSession first.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::BeginSession — Session already active; a new session requires EndSession first. The active session is unchanged."));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("BeginSession"));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Platform handle not available"));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle);
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — AntiCheatClient interface not available. "
			"Easy Anti-Cheat must be enabled for the product in the Epic Dev Portal and the game must be launched "
			"through the EAC bootstrapper for the client module to load."));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	// Local Product User ID. ToString() of the OSS net id is the composite
	// "<EpicAccountId>|<ProductUserId>" — extract the PUID half (Phase 3 helper).
	// EOS_ProductUserId_FromString performs NO validation, so an empty extracted
	// half is the only reliable failure signal.
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr Identity = EOSSub ? EOSSub->GetIdentityInterface() : nullptr;
	FUniqueNetIdPtr LocalPlayerId = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : nullptr;
	if (!LocalPlayerId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — No logged-in user (must login via Connect first)"));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalPlayerId->ToString());
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Logged-in user has no Product User ID (no Connect session)"));
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}
	EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*LocalPUIDStr));

	// ── Register ALL notifications BEFORE BeginSession ──────────────────────
	// Per eos_anticheatclient.h all four are valid for EOS_ACCM_PeerToPeer:
	//   AddNotifyMessageToPeer        — Mode: EOS_ACCM_PeerToPeer (REQUIRED outbound transport)
	//   AddNotifyPeerActionRequired   — Mode: EOS_ACCM_PeerToPeer
	//   AddNotifyPeerAuthStatusChanged— Mode: EOS_ACCM_PeerToPeer (optional)
	//   AddNotifyClientIntegrityViolated — Mode: Any
	NotifyContext = new FEEOSAntiCheatNotifyContext{this};

	EOS_AntiCheatClient_AddNotifyMessageToPeerOptions MsgOptions = {};
	MsgOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYMESSAGETOPEER_API_LATEST;
	MessageToPeerNotifId = EOS_AntiCheatClient_AddNotifyMessageToPeer(ACHandle, &MsgOptions, NotifyContext, &EEOSAntiCheat_OnMessageToPeer);

	EOS_AntiCheatClient_AddNotifyPeerActionRequiredOptions ActionOptions = {};
	ActionOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERACTIONREQUIRED_API_LATEST;
	PeerActionRequiredNotifId = EOS_AntiCheatClient_AddNotifyPeerActionRequired(ACHandle, &ActionOptions, NotifyContext, &EEOSAntiCheat_OnPeerActionRequired);

	EOS_AntiCheatClient_AddNotifyPeerAuthStatusChangedOptions AuthOptions = {};
	AuthOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYPEERAUTHSTATUSCHANGED_API_LATEST;
	PeerAuthStatusNotifId = EOS_AntiCheatClient_AddNotifyPeerAuthStatusChanged(ACHandle, &AuthOptions, NotifyContext, &EEOSAntiCheat_OnPeerAuthStatusChanged);

	EOS_AntiCheatClient_AddNotifyClientIntegrityViolatedOptions IntegrityOptions = {};
	IntegrityOptions.ApiVersion = EOS_ANTICHEATCLIENT_ADDNOTIFYCLIENTINTEGRITYVIOLATED_API_LATEST;
	IntegrityViolatedNotifId = EOS_AntiCheatClient_AddNotifyClientIntegrityViolated(ACHandle, &IntegrityOptions, NotifyContext, &EEOSAntiCheat_OnClientIntegrityViolated);

	if (MessageToPeerNotifId == EOS_INVALID_NOTIFICATIONID ||
		PeerActionRequiredNotifId == EOS_INVALID_NOTIFICATIONID ||
		PeerAuthStatusNotifId == EOS_INVALID_NOTIFICATIONID ||
		IntegrityViolatedNotifId == EOS_INVALID_NOTIFICATIONID)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — Failed to register anti-cheat notifications (MessageToPeer=%llu, PeerAction=%llu, PeerAuth=%llu, Integrity=%llu)"),
			MessageToPeerNotifId, PeerActionRequiredNotifId, PeerAuthStatusNotifId, IntegrityViolatedNotifId);
		RemoveNotifications();
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	// ── BeginSession in PeerToPeer mode (listen-server co-op: no dedicated server, every machine is a peer) ──
	EOS_AntiCheatClient_BeginSessionOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_BEGINSESSION_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.Mode = EOS_EAntiCheatClientMode::EOS_ACCM_PeerToPeer;

	const EOS_EResult Result = EOS_AntiCheatClient_BeginSession(ACHandle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::BeginSession — EOS_AntiCheatClient_BeginSession failed: %s"),
			ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		RemoveNotifications();
		OnAntiCheatSessionStarted.Broadcast(false);
		return false;
	}

	bSessionActive = true;
	CachedLocalPuid = LocalPUIDStr;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::BeginSession — Anti-cheat session started (PeerToPeer mode, local PUID %s)"), *CachedLocalPuid);
	OnAntiCheatSessionStarted.Broadcast(true);
	return true;
}

bool UEEOSAntiCheatSubsystem::EndSession()
{
	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::EndSession — No active session"));
		return false;
	}

	// Report inactive BEFORE the unregister loop: each UnregisterPeer fires
	// OnPeerAuthStatusChanged, whose listeners can re-enter this subsystem — a
	// re-entrant EndSession must no-op on the guard above instead of running the
	// teardown twice. UnregisterPeer itself never reads bSessionActive, and the
	// SDK session stays alive until EOS_AntiCheatClient_EndSession below.
	bSessionActive = false;

	// Unregister peers while the SDK session is still active (UnregisterPeer is a
	// P2P in-session call), then end the SDK session, then drop notifications.
	UnregisterAllPeers();

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HAntiCheatClient ACHandle = PlatformHandle ? EOS_Platform_GetAntiCheatClientInterface(PlatformHandle) : nullptr;
	if (ACHandle)
	{
		EOS_AntiCheatClient_EndSessionOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_ENDSESSION_API_LATEST;

		const EOS_EResult Result = EOS_AntiCheatClient_EndSession(ACHandle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::EndSession — Anti-cheat session ended"));
		}
		else
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::EndSession — EOS_AntiCheatClient_EndSession failed: %s"),
				ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		}
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::EndSession — AntiCheatClient interface unavailable during teardown; clearing local state only"));
	}

	RemoveNotifications();

	// Local state is cleared unconditionally (bSessionActive already dropped
	// above): after EndSession this subsystem reports inactive no matter what
	// the SDK teardown returned (fail-closed).
	PuidToHandle.Empty();
	HandleToPuid.Empty();
	CachedLocalPuid.Reset();
	OnAntiCheatSessionEnded.Broadcast();
	return true;
}

void UEEOSAntiCheatSubsystem::RemoveNotifications()
{
	bool bRemoved = false;
	if (EOS_HPlatform PlatformHandle = GetPlatformHandle())
	{
		if (EOS_HAntiCheatClient ACHandle = EOS_Platform_GetAntiCheatClientInterface(PlatformHandle))
		{
			if (MessageToPeerNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyMessageToPeer(ACHandle, MessageToPeerNotifId);
			}
			if (PeerActionRequiredNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyPeerActionRequired(ACHandle, PeerActionRequiredNotifId);
			}
			if (PeerAuthStatusNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyPeerAuthStatusChanged(ACHandle, PeerAuthStatusNotifId);
			}
			if (IntegrityViolatedNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_AntiCheatClient_RemoveNotifyClientIntegrityViolated(ACHandle, IntegrityViolatedNotifId);
			}
			bRemoved = true;
		}
	}

	MessageToPeerNotifId = EOS_INVALID_NOTIFICATIONID;
	PeerActionRequiredNotifId = EOS_INVALID_NOTIFICATIONID;
	PeerAuthStatusNotifId = EOS_INVALID_NOTIFICATIONID;
	IntegrityViolatedNotifId = EOS_INVALID_NOTIFICATIONID;

	// Free the context only once the SDK can no longer call into it. If removal
	// was impossible (platform already gone) the context is deliberately leaked —
	// the weak pointer inside keeps any late callback safe.
	if (NotifyContext)
	{
		if (bRemoved)
		{
			delete NotifyContext;
		}
		NotifyContext = nullptr;
	}
}

// ── Peer Management ──────────────────────────────────────────────────────────

bool UEEOSAntiCheatSubsystem::RegisterPeer(const FString& PeerPuid)
{
	if (!IsAntiCheatEnabled(TEXT("RegisterPeer")))
	{
		return false;
	}

	// Normalize: callers commonly pass a net-id ToString() composite
	// "<EpicAccountId>|<ProductUserId>" — extract the PUID half (no-op for bare PUIDs).
	const FString BarePuid = UEEOSBlueprintLibrary::ExtractProductUserId(PeerPuid);
	if (BarePuid.IsEmpty())
	{
		// Extraction failed, so there is NO bare PUID to broadcast — delegates
		// carry bare PUIDs or an empty string, never the raw unparseable input
		// (the input is preserved in the log line for diagnosis).
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — '%s' contains no Product User ID"), *PeerPuid);
		OnPeerAuthStatusChanged.Broadcast(FString(), false);
		return false;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — No active session; peer '%s' NOT registered"), *BarePuid);
		OnPeerAuthStatusChanged.Broadcast(BarePuid, false);
		return false;
	}

	if (BarePuid == CachedLocalPuid)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — '%s' is the LOCAL user; only remote participants are registered"), *BarePuid);
		return false;
	}

	if (PuidToHandle.Contains(BarePuid))
	{
		// The desired state already holds — report success without a second SDK registration.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Peer '%s' already registered"), *BarePuid);
		return true;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HAntiCheatClient ACHandle = PlatformHandle ? EOS_Platform_GetAntiCheatClientInterface(PlatformHandle) : nullptr;
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — AntiCheatClient interface not available"));
		OnPeerAuthStatusChanged.Broadcast(BarePuid, false);
		return false;
	}

	EOS_ProductUserId PeerPUIDHandle = EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*BarePuid));

	// Mint an opaque, locally unique client handle from the monotonic counter.
	// Handles are bookkeeping tokens only — never derived from or rendered as ids.
	const EOS_AntiCheatCommon_ClientHandle PeerHandle =
		reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(NextPeerHandleValue);

	EOS_AntiCheatClient_RegisterPeerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_REGISTERPEER_API_LATEST;
	Options.PeerHandle = PeerHandle;
	Options.ClientType = EOS_EAntiCheatCommonClientType::EOS_ACCCT_ProtectedClient;
	Options.ClientPlatform = EOS_EAntiCheatCommonClientPlatform::EOS_ACCCP_Unknown;
	Options.AuthenticationTimeout = EOS_ANTICHEATCLIENT_REGISTERPEER_MIN_AUTHENTICATIONTIMEOUT; // 40 seconds
	Options.AccountId_DEPRECATED = nullptr;
	Options.IpAddress = nullptr;
	Options.PeerProductUserId = PeerPUIDHandle;

	const EOS_EResult Result = EOS_AntiCheatClient_RegisterPeer(ACHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		// Consume the counter value and record the mapping ONLY on SDK success.
		NextPeerHandleValue++;
		PuidToHandle.Add(BarePuid, PeerHandle);
		HandleToPuid.Add(PeerHandle, BarePuid);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Registered '%s' (%d peers). OnPeerAuthStatusChanged fires when authentication completes."),
			*BarePuid, PuidToHandle.Num());
		return true;
	}

	UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RegisterPeer — Failed for '%s': %s"),
		*BarePuid, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	OnPeerAuthStatusChanged.Broadcast(BarePuid, false);
	return false;
}

bool UEEOSAntiCheatSubsystem::UnregisterPeer(const FString& PeerPuid)
{
	if (!IsAntiCheatEnabled(TEXT("UnregisterPeer")))
	{
		return false;
	}

	const FString BarePuid = UEEOSBlueprintLibrary::ExtractProductUserId(PeerPuid);
	const EOS_AntiCheatCommon_ClientHandle* HandlePtr = PuidToHandle.Find(BarePuid);
	if (!HandlePtr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Peer '%s' is not registered"), *BarePuid);
		return false;
	}
	const EOS_AntiCheatCommon_ClientHandle PeerHandle = *HandlePtr;

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HAntiCheatClient ACHandle = PlatformHandle ? EOS_Platform_GetAntiCheatClientInterface(PlatformHandle) : nullptr;

	bool bDropMapping = true;
	if (ACHandle)
	{
		EOS_AntiCheatClient_UnregisterPeerOptions Options = {};
		Options.ApiVersion = EOS_ANTICHEATCLIENT_UNREGISTERPEER_API_LATEST;
		Options.PeerHandle = PeerHandle;

		const EOS_EResult Result = EOS_AntiCheatClient_UnregisterPeer(ACHandle, &Options);
		if (Result == EOS_EResult::EOS_Success)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Unregistered '%s'"), *BarePuid);
		}
		else if (Result == EOS_EResult::EOS_InvalidParameters)
		{
			// The SDK does not know this handle — our entry is stale either way; drop it.
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — SDK does not know peer '%s' (stale mapping dropped)"), *BarePuid);
		}
		else
		{
			// Keep the mapping so outbound messages for this handle still resolve;
			// the maps must mirror the SDK's registration state.
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — Failed for '%s': %s (peer remains registered)"),
				*BarePuid, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
			bDropMapping = false;
		}
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::UnregisterPeer — AntiCheatClient interface unavailable; dropping local mapping for '%s'"), *BarePuid);
	}

	if (bDropMapping)
	{
		PuidToHandle.Remove(BarePuid);
		HandleToPuid.Remove(PeerHandle);
		OnPeerAuthStatusChanged.Broadcast(BarePuid, false);
	}
	return bDropMapping;
}

void UEEOSAntiCheatSubsystem::UnregisterAllPeers()
{
	TArray<FString> Puids;
	PuidToHandle.GenerateKeyArray(Puids);
	for (const FString& Puid : Puids)
	{
		UnregisterPeer(Puid);
	}
}

// ── Message Transport ────────────────────────────────────────────────────────

bool UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer(const FString& SenderPeerPuid, const TArray<uint8>& Payload)
{
	if (!IsAntiCheatEnabled(TEXT("ReceiveMessageFromPeer")))
	{
		return false;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — No active session; payload dropped"));
		return false;
	}

	if (Payload.Num() <= 0)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — Empty payload from '%s' dropped"), *SenderPeerPuid);
		return false;
	}

	// This is the relay's attacker-controlled input surface: legitimate SDK
	// payloads never exceed EOS_ANTICHEATCLIENT_ONMESSAGETOPEERCALLBACK_MAX_MESSAGE_SIZE
	// (512 bytes, eos_anticheatclient_types.h) — reject oversized input before
	// it reaches the SDK.
	if (Payload.Num() > EOS_ANTICHEATCLIENT_ONMESSAGETOPEERCALLBACK_MAX_MESSAGE_SIZE)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — Oversized payload (%d bytes, max %d) from '%s' dropped"),
			Payload.Num(), EOS_ANTICHEATCLIENT_ONMESSAGETOPEERCALLBACK_MAX_MESSAGE_SIZE, *SenderPeerPuid);
		return false;
	}

	const FString BarePuid = UEEOSBlueprintLibrary::ExtractProductUserId(SenderPeerPuid);
	const EOS_AntiCheatCommon_ClientHandle* HandlePtr = PuidToHandle.Find(BarePuid);
	if (!HandlePtr)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — Sender '%s' is not a registered peer; payload dropped (RegisterPeer must run before the relay)"), *BarePuid);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HAntiCheatClient ACHandle = PlatformHandle ? EOS_Platform_GetAntiCheatClientInterface(PlatformHandle) : nullptr;
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — AntiCheatClient interface not available; payload dropped"));
		return false;
	}

	EOS_AntiCheatClient_ReceiveMessageFromPeerOptions Options = {};
	Options.ApiVersion = EOS_ANTICHEATCLIENT_RECEIVEMESSAGEFROMPEER_API_LATEST;
	Options.PeerHandle = *HandlePtr;
	Options.DataLengthBytes = static_cast<uint32_t>(Payload.Num());
	Options.Data = Payload.GetData();

	const EOS_EResult Result = EOS_AntiCheatClient_ReceiveMessageFromPeer(ACHandle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		// A corrupt/invalid message stream ultimately surfaces as a peer auth
		// failure → OnPeerActionRequired; here we only log.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReceiveMessageFromPeer — Failed for '%s': %s"),
			*BarePuid, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}
	return true;
}

// ── Internal SDK-callback handlers (game thread) ─────────────────────────────

FString UEEOSAntiCheatSubsystem::ResolveHandleToPuid(EOS_AntiCheatCommon_ClientHandle Handle) const
{
	if (Handle == EOS_ANTICHEATCLIENT_PEER_SELF)
	{
		return CachedLocalPuid;
	}
	if (const FString* Puid = HandleToPuid.Find(Handle))
	{
		return *Puid;
	}
	return FString();
}

void UEEOSAntiCheatSubsystem::HandleMessageToPeer(EOS_AntiCheatCommon_ClientHandle TargetHandle, const TArray<uint8>& Payload)
{
	const FString* TargetPuid = HandleToPuid.Find(TargetHandle);
	if (!TargetPuid)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: MessageToPeer for unknown peer handle — payload dropped (peer unregistered mid-flight?)"));
		return;
	}

	// The GAME must now ship this payload to the machine owned by TargetPuid
	// (reliable, ordered) and call ReceiveMessageFromPeer there.
	OnAntiCheatMessageToPeer.Broadcast(*TargetPuid, Payload);
}

void UEEOSAntiCheatSubsystem::HandlePeerActionRequired(EOS_AntiCheatCommon_ClientHandle PeerHandle, EEOSAntiCheatAction Action, const FString& Message)
{
	const FString PeerPuid = ResolveHandleToPuid(PeerHandle);
	if (PeerPuid.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: PeerActionRequired for unknown peer handle — dropped. Message: %s"), *Message);
		return;
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: Action %s required for peer '%s' — %s"),
		*UEnum::GetValueAsString(Action), *PeerPuid, *Message);
	OnPeerActionRequired.Broadcast(PeerPuid, Action, Message);
	OnClientActionRequired.Broadcast(Action, Message);
}

void UEEOSAntiCheatSubsystem::HandlePeerAuthStatusChanged(EOS_AntiCheatCommon_ClientHandle PeerHandle, bool bAuthenticated)
{
	const FString PeerPuid = ResolveHandleToPuid(PeerHandle);
	if (PeerPuid.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem: PeerAuthStatusChanged for unknown peer handle — dropped"));
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem: Peer '%s' auth status — %s"),
		*PeerPuid, bAuthenticated ? TEXT("RemoteAuthComplete") : TEXT("Not authenticated"));
	OnPeerAuthStatusChanged.Broadcast(PeerPuid, bAuthenticated);
}

// ── Player Action Reporting (game-side validation surface) ───────────────────

void UEEOSAntiCheatSubsystem::ReportPlayerAction(const FString& PlayerId, const FString& ActionType, const FString& ActionData)
{
	if (!IsAntiCheatEnabled(TEXT("ReportPlayerAction")))
	{
		return;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — No active session"));
		return;
	}

	// NOTE: EOS Anti-Cheat has no per-action "report" API on the client. This is
	// a local broadcast so game code (e.g. the host) can aggregate and validate.
	UE_LOG(LogExtendedEOS, Verbose, TEXT("EEOSAntiCheatSubsystem::ReportPlayerAction — Player='%s' Action='%s' Data='%s'"),
		*PlayerId, *ActionType, *ActionData);
	OnPlayerActionReported.Broadcast(PlayerId, ActionType, ActionData);
}

void UEEOSAntiCheatSubsystem::ReportViolation(const FString& PlayerId, EEOSAntiCheatViolationType ViolationType, const FString& Details)
{
	if (!IsAntiCheatEnabled(TEXT("ReportViolation")))
	{
		return;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportViolation — No active session"));
		return;
	}

	// EAC detects module/memory violations automatically; this surface is for
	// game-specific rule violations (speed checks, damage limits, ...).
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::ReportViolation — Player='%s' Type=%d Details='%s'"),
		*PlayerId, static_cast<int32>(ViolationType), *Details);
	OnViolationDetected.Broadcast(PlayerId, ViolationType, Details);
}

bool UEEOSAntiCheatSubsystem::RequestIntegrityCheck(const FString& PlayerId)
{
	// FAIL-CLOSED: every failure path broadcasts OnIntegrityChanged(false) —
	// integrity that cannot be verified is treated as not verified. The bool
	// return says whether the poll RAN, never what the verdict was.
	if (!IsAntiCheatEnabled(TEXT("RequestIntegrityCheck")))
	{
		OnIntegrityChanged.Broadcast(false);
		return false;
	}

	if (!bSessionActive)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — No active session"));
		OnIntegrityChanged.Broadcast(false);
		return false;
	}

	// IMPORTANT: EOS_AntiCheatClient_PollStatus checks the LOCAL client only.
	// Remote player integrity is enforced through the P2P auth handshake
	// (OnPeerAuthStatusChanged / OnPeerActionRequired), never by polling.
	if (!PlayerId.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — Checks LOCAL client integrity only; PlayerId '%s' is logged for context"), *PlayerId);
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HAntiCheatClient ACHandle = PlatformHandle ? EOS_Platform_GetAntiCheatClientInterface(PlatformHandle) : nullptr;
	if (!ACHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — AntiCheatClient interface not available"));
		OnIntegrityChanged.Broadcast(false);
		return false;
	}

	// PollStatus is deprecated in favor of the integrity-violated notification
	// (registered for the session); kept as an on-demand pull for UI flows.
	char OutMessage[256] = {};
	EOS_AntiCheatClient_PollStatusOptions PollOptions = {};
	PollOptions.ApiVersion = EOS_ANTICHEATCLIENT_POLLSTATUS_API_LATEST;
	PollOptions.OutMessageLength = sizeof(OutMessage);

	EOS_EAntiCheatClientViolationType OutViolationType = EOS_EAntiCheatClientViolationType::EOS_ACCVT_Invalid;
	const EOS_EResult Result = EOS_AntiCheatClient_PollStatus(ACHandle, &PollOptions, &OutViolationType, OutMessage);
	if (Result == EOS_EResult::EOS_NotFound)
	{
		// Header-documented: EOS_NotFound == no violation since the last call.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — Local client integrity OK"));
		OnIntegrityChanged.Broadcast(true);
		return true;
	}
	if (Result == EOS_EResult::EOS_Success)
	{
		const FString ViolationMsg = UTF8_TO_TCHAR(OutMessage);
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — LOCAL VIOLATION (type %d): %s"),
			static_cast<int32>(OutViolationType), *ViolationMsg);
		OnIntegrityChanged.Broadcast(false);
		OnViolationDetected.Broadcast(PlayerId, EEOSAntiCheatViolationType::CustomViolation, ViolationMsg);
		return true; // the check RAN — the (failed) verdict went out via the delegates
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAntiCheatSubsystem::RequestIntegrityCheck — PollStatus failed: %s"),
		ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	OnIntegrityChanged.Broadcast(false);
	return false;
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool UEEOSAntiCheatSubsystem::IsSessionActive() const
{
	return bSessionActive;
}

TArray<FString> UEEOSAntiCheatSubsystem::GetRegisteredPeers() const
{
	TArray<FString> Puids;
	PuidToHandle.GenerateKeyArray(Puids);
	return Puids;
}

int32 UEEOSAntiCheatSubsystem::GetRegisteredPeerCount() const
{
	return PuidToHandle.Num();
}

bool UEEOSAntiCheatSubsystem::IsPeerRegistered(const FString& PeerPuid) const
{
	return PuidToHandle.Contains(UEEOSBlueprintLibrary::ExtractProductUserId(PeerPuid));
}

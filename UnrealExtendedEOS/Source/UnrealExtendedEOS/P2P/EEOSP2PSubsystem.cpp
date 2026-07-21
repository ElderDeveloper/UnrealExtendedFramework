// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSP2PSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"
#include "IEOSSDKManager.h"

#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "eos_connect.h"
#include "eos_sdk.h"

/**
 * Heap context passed as ClientData to the persistent EOS P2P notifications.
 * The EOS platform outlives this subsystem, so the callbacks must resolve a weak
 * pointer instead of dereferencing a raw 'this'.
 */
struct FEEOSP2PNotifyContext
{
	TWeakObjectPtr<UEEOSP2PSubsystem> Self;
};

/** Parse the DefaultRelayMode settings string ("NoRelays" | "AllowRelays" | "ForceRelays", case-insensitive).
 *  Unknown values warn and fall back to AllowRelays. */
static EEOSRelayControl UEEOSP2PSubsystem_ParseRelayMode(const FString& RelayModeString)
{
	if (RelayModeString.Equals(TEXT("NoRelays"), ESearchCase::IgnoreCase))
	{
		return EEOSRelayControl::NoRelays;
	}
	if (RelayModeString.Equals(TEXT("AllowRelays"), ESearchCase::IgnoreCase))
	{
		return EEOSRelayControl::AllowRelays;
	}
	if (RelayModeString.Equals(TEXT("ForceRelays"), ESearchCase::IgnoreCase))
	{
		return EEOSRelayControl::ForceRelays;
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem: Unknown DefaultRelayMode '%s' in Extended EOS settings — falling back to AllowRelays"), *RelayModeString);
	return EEOSRelayControl::AllowRelays;
}

void UEEOSP2PSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableP2P)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: P2P is disabled in settings"));
		return;
	}

	// Every P2P SDK call and notification registration requires the local user's Product
	// User ID, but Connect login happens after subsystem Initialize. The ticker therefore
	// drives lazy initialization: each tick it retries TryLazyInitNotifications() until the
	// EOS platform handle and a logged-in Connect user are both available (this also covers
	// the platform handle itself not being ready yet), then drains incoming packets.
	ReceiveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEEOSP2PSubsystem::PollIncomingPackets), 0.0f);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Initialization deferred until the EOS platform and Connect login are ready"));
}

void UEEOSP2PSubsystem::Deinitialize()
{
	// Stop the ticker first — this also stops lazy-init retries if init never completed
	if (ReceiveTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReceiveTickerHandle);
		ReceiveTickerHandle.Reset();
	}

	// Close open connections while the notifications are still registered. Peers whose close
	// succeeds are removed from ConnectedPeers here; any close that fails leaves the peer in
	// the list and TeardownNotifications synthesizes its close event below.
	CloseAllConnections();

	// Remove notification callbacks against the platform they were registered on and free (or
	// intentionally leak) the notify context — see TeardownNotifications for the full contract.
	TeardownNotifications(TEXT("Shutdown"));

	ConnectedPeers.Empty();
	PeerSockets.Empty();
	Super::Deinitialize();
}

// ── Lazy Initialization ──────────────────────────────────────────────────────

EOS_HPlatform UEEOSP2PSubsystem::GetPlatformHandleQuiet()
{
	// Same lookup as the Shared base's GetPlatformHandle(), WITHOUT its unconditional
	// "not available" warning: the lazy-init/receive ticker calls this every frame while
	// waiting for the platform to come up, which would otherwise log 60+ warnings/second.
	// (The Shared base is owned by another module and is not changed here.)
	if (IEOSSDKManager* SDKManager = IEOSSDKManager::Get())
	{
		TArray<IEOSPlatformHandlePtr> ActivePlatforms = SDKManager->GetActivePlatforms();
		if (ActivePlatforms.Num() > 0 && ActivePlatforms[0].IsValid())
		{
			return *ActivePlatforms[0];
		}
	}
	return nullptr;
}

bool UEEOSP2PSubsystem::IsPlatformStillActive(EOS_HPlatform PlatformHandle)
{
	if (!PlatformHandle)
	{
		return false;
	}

	if (IEOSSDKManager* SDKManager = IEOSSDKManager::Get())
	{
		for (const IEOSPlatformHandlePtr& Active : SDKManager->GetActivePlatforms())
		{
			if (Active.IsValid() && static_cast<EOS_HPlatform>(*Active) == PlatformHandle)
			{
				return true;
			}
		}
	}
	return false;
}

EOS_ProductUserId UEEOSP2PSubsystem::GetLocalProductUserId(EOS_HPlatform PlatformHandle)
{
	if (!PlatformHandle)
	{
		return nullptr;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		return nullptr;
	}

	// Index 0 = first locally logged-in Connect user. Returns an invalid ID before login.
	EOS_ProductUserId LocalUserId = EOS_Connect_GetLoggedInUserByIndex(ConnectHandle, 0);
	if (!EOS_ProductUserId_IsValid(LocalUserId))
	{
		return nullptr;
	}

	return LocalUserId;
}

void UEEOSP2PSubsystem::TeardownNotifications(const FString& CloseReason)
{
	if (!bNotificationsInitialized)
	{
		return;
	}

	// Remove the notifications from the platform they were REGISTERED on. GetPlatformHandle()
	// returns ActivePlatforms[0] *at call time* — under platform churn / multi-instance PIE
	// that can be a different platform, where the RemoveNotify calls would silently no-op
	// while the original platform keeps calling into the (about to be freed) notify context.
	bool bNotificationsRemoved = false;
	if (IsPlatformStillActive(RegisteredPlatform))
	{
		EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(RegisteredPlatform);
		if (P2PHandle)
		{
			if (ConnectionRequestNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_P2P_RemoveNotifyPeerConnectionRequest(P2PHandle, ConnectionRequestNotifId);
			}
			if (ConnectionClosedNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_P2P_RemoveNotifyPeerConnectionClosed(P2PHandle, ConnectionClosedNotifId);
			}
			if (ConnectionEstablishedNotifId != EOS_INVALID_NOTIFICATIONID)
			{
				EOS_P2P_RemoveNotifyPeerConnectionEstablished(P2PHandle, ConnectionEstablishedNotifId);
			}
			bNotificationsRemoved = true;
		}
	}

	ConnectionRequestNotifId = 0;
	ConnectionClosedNotifId = 0;
	ConnectionEstablishedNotifId = 0;

	// Free the notify context only if removal actually ran against the registration platform.
	// If the registration platform is gone (or its P2P interface unreachable), intentionally
	// leak the context: the platform may still hold callbacks referencing it, and the weak
	// pointer inside keeps any late callback safe. A re-init allocates a fresh context.
	if (NotifyContext)
	{
		if (bNotificationsRemoved)
		{
			delete NotifyContext;
		}
		else
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Registration platform no longer reachable — leaking the notify context intentionally (weak-pointer safe)"));
		}
		NotifyContext = nullptr;
	}

	// The tracked connections died with the registered identity/platform, and the SDK's
	// PeerConnectionClosed notification can no longer deliver their close events (it was
	// bound to the old PUID / just removed). Synthesize exactly one close per tracked peer
	// so listeners see a consistent lifecycle. Iterate a moved copy: a listener may call
	// back into this subsystem (e.g. CloseConnection) and mutate the arrays.
	TArray<FString> ClosedPeers = MoveTemp(ConnectedPeers);
	ConnectedPeers.Reset();
	PeerSockets.Reset();
	for (const FString& PeerId : ClosedPeers)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Connection to %s ended with the local identity — %s"), *PeerId, *CloseReason);
		OnConnectionClosed.Broadcast(PeerId, CloseReason);
	}

	RegisteredLocalUserId = nullptr;
	RegisteredPlatform = nullptr;
	bNotificationsInitialized = false;
}

bool UEEOSP2PSubsystem::TryLazyInitNotifications()
{
	if (bNotificationsInitialized)
	{
		return true;
	}

	// Quiet lookup — this runs every tick until the platform comes up (see GetPlatformHandleQuiet).
	EOS_HPlatform PlatformHandle = GetPlatformHandleQuiet();
	if (!PlatformHandle)
	{
		return false; // Platform not up yet — retry next tick
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		return false;
	}

	EOS_ProductUserId LocalUserId = GetLocalProductUserId(PlatformHandle);
	if (!LocalUserId)
	{
		return false; // No Connect user logged in yet — retry next tick
	}

	// Shared heap context with a weak self pointer for all persistent notifications
	NotifyContext = new FEEOSP2PNotifyContext{this};

	// Register connection request notification so incoming connections trigger OnConnectionRequest
	EOS_P2P_AddNotifyPeerConnectionRequestOptions RequestOptions = {};
	RequestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
	RequestOptions.LocalUserId = LocalUserId;
	RequestOptions.SocketId = nullptr; // Accept from any socket

	ConnectionRequestNotifId = EOS_P2P_AddNotifyPeerConnectionRequest(P2PHandle, &RequestOptions, NotifyContext,
		[](const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
		{
			const FEEOSP2PNotifyContext* Ctx = static_cast<const FEEOSP2PNotifyContext*>(Data->ClientData);
			UEEOSP2PSubsystem* Self = Ctx ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
			int32_t BufferLen = sizeof(RemoteUserIdStr);
			if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
			{
				FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
				FString SocketName = Data->SocketId ? ANSI_TO_TCHAR(Data->SocketId->SocketName) : FString();
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Incoming connection request from %s on socket '%s'"), *RemoteId, *SocketName);
				// Carry the requesting socket name through so AcceptConnection can accept on it
				Self->OnConnectionRequest.Broadcast(RemoteId, SocketName);
			}
		});

	// Register connection closed notification
	EOS_P2P_AddNotifyPeerConnectionClosedOptions ClosedOptions = {};
	ClosedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST;
	ClosedOptions.LocalUserId = LocalUserId;
	ClosedOptions.SocketId = nullptr;

	ConnectionClosedNotifId = EOS_P2P_AddNotifyPeerConnectionClosed(P2PHandle, &ClosedOptions, NotifyContext,
		[](const EOS_P2P_OnRemoteConnectionClosedInfo* Data)
		{
			const FEEOSP2PNotifyContext* Ctx = static_cast<const FEEOSP2PNotifyContext*>(Data->ClientData);
			UEEOSP2PSubsystem* Self = Ctx ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
			int32_t BufferLen = sizeof(RemoteUserIdStr);
			if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
			{
				FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
				Self->ConnectedPeers.Remove(RemoteId);
				Self->PeerSockets.Remove(RemoteId);

				FString ReasonStr = TEXT("Unknown");
				switch (Data->Reason)
				{
				case EOS_EConnectionClosedReason::EOS_CCR_ClosedByLocalUser: ReasonStr = TEXT("ClosedByLocalUser"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_ClosedByPeer: ReasonStr = TEXT("ClosedByPeer"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_TimedOut: ReasonStr = TEXT("TimedOut"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_TooManyConnections: ReasonStr = TEXT("TooManyConnections"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_InvalidMessage: ReasonStr = TEXT("InvalidMessage"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_InvalidData: ReasonStr = TEXT("InvalidData"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_ConnectionFailed: ReasonStr = TEXT("ConnectionFailed"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_ConnectionClosed: ReasonStr = TEXT("ConnectionClosed"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_NegotiationFailed: ReasonStr = TEXT("NegotiationFailed"); break;
				case EOS_EConnectionClosedReason::EOS_CCR_UnexpectedError: ReasonStr = TEXT("UnexpectedError"); break;
				default: break;
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Connection closed from %s — %s"), *RemoteId, *ReasonStr);
				// This notification is the single source of OnConnectionClosed broadcasts —
				// CloseConnection deliberately does not broadcast locally.
				Self->OnConnectionClosed.Broadcast(RemoteId, ReasonStr);
			}
		});

	// Register connection established notification
	EOS_P2P_AddNotifyPeerConnectionEstablishedOptions EstOptions = {};
	EstOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONESTABLISHED_API_LATEST;
	EstOptions.LocalUserId = LocalUserId;
	EstOptions.SocketId = nullptr;

	ConnectionEstablishedNotifId = EOS_P2P_AddNotifyPeerConnectionEstablished(P2PHandle, &EstOptions, NotifyContext,
		[](const EOS_P2P_OnPeerConnectionEstablishedInfo* Data)
		{
			const FEEOSP2PNotifyContext* Ctx = static_cast<const FEEOSP2PNotifyContext*>(Data->ClientData);
			UEEOSP2PSubsystem* Self = Ctx ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
			int32_t BufferLen = sizeof(RemoteUserIdStr);
			if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
			{
				FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
				FString SocketName = Data->SocketId ? ANSI_TO_TCHAR(Data->SocketId->SocketName) : FString();

				if (!Self->ConnectedPeers.Contains(RemoteId))
				{
					Self->ConnectedPeers.Add(RemoteId);
				}
				// Track the socket this peer's connection runs on so SendPacket targets it
				if (!SocketName.IsEmpty())
				{
					Self->PeerSockets.Add(RemoteId, SocketName);
				}

				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Connection established with %s on socket '%s'"), *RemoteId, *SocketName);
				Self->OnConnectionEstablished.Broadcast(RemoteId, SocketName);
			}
		});

	// Apply the configured default relay mode now that the platform is ready. Routed through
	// SetRelayControl so CurrentRelayControl only reflects what the SDK actually accepted.
	if (const UEEOSSettings* Settings = GetEOSSettings())
	{
		SetRelayControl(UEEOSP2PSubsystem_ParseRelayMode(Settings->DefaultRelayMode));
	}

	// Pin the identity and platform this registration is bound to: the notifications are
	// per-local-user filtered SDK-side, and removal must target this exact platform later.
	RegisteredLocalUserId = LocalUserId;
	RegisteredPlatform = PlatformHandle;

	bNotificationsInitialized = true;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Lazy initialization complete — P2P notifications registered for the local Connect user"));
	return true;
}

// ── Receive Polling ──────────────────────────────────────────────────────────

bool UEEOSP2PSubsystem::PollIncomingPackets(float DeltaTime)
{
	// While registered, verify the registration is still current BEFORE anything else:
	// the SDK notifications are bound to (RegisteredPlatform, RegisteredLocalUserId).
	if (bNotificationsInitialized)
	{
		if (!IsPlatformStillActive(RegisteredPlatform))
		{
			// The platform we registered on is gone (churn / PIE instance shutdown). Removal is
			// impossible — TeardownNotifications takes the intentional-leak path for the context.
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Registration platform disappeared — tearing down P2P state and re-initializing"));
			TeardownNotifications(TEXT("PlatformLost"));
		}
		else if (GetLocalProductUserId(RegisteredPlatform) != RegisteredLocalUserId)
		{
			// The Connect identity changed under us: logout (now nullptr) or a re-login as a
			// different user (device-id transfer / account switch). The per-user-filtered
			// notifications and every tracked connection belong to the OLD identity — tear it
			// all down. If a user is logged in, lazy init below re-registers with the new PUID
			// this same tick; on plain logout it idles until the next login.
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Local Connect identity changed — re-binding P2P notifications"));
			TeardownNotifications(TEXT("LocalUserChanged"));
		}
	}

	// Retry lazy init until the platform and a logged-in Connect user are available
	// (also re-registers right after a teardown above)
	if (!TryLazyInitNotifications())
	{
		return true; // Keep ticking
	}

	// Receive on the platform/identity the notifications are registered with — never the
	// current GetPlatformHandle(), which may point at a different platform instance. The
	// checks above guarantee RegisteredPlatform is active and RegisteredLocalUserId current.
	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(RegisteredPlatform);
	if (!P2PHandle) return true;

	EOS_ProductUserId LocalUserId = RegisteredLocalUserId;

	// Drain all available packets in this frame
	while (true)
	{
		// Check if there is a packet to receive
		EOS_P2P_GetNextReceivedPacketSizeOptions SizeOptions = {};
		SizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		SizeOptions.LocalUserId = LocalUserId;
		SizeOptions.RequestedChannel = nullptr; // any channel

		uint32_t PacketSize = 0;
		EOS_EResult SizeResult = EOS_P2P_GetNextReceivedPacketSize(P2PHandle, &SizeOptions, &PacketSize);
		if (SizeResult != EOS_EResult::EOS_Success || PacketSize == 0)
		{
			break; // No more packets
		}

		// Receive the packet
		EOS_P2P_ReceivePacketOptions RecvOptions = {};
		RecvOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
		RecvOptions.LocalUserId = LocalUserId;
		RecvOptions.MaxDataSizeBytes = PacketSize;
		RecvOptions.RequestedChannel = nullptr; // any channel

		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(PacketSize);

		EOS_ProductUserId OutPeerId = nullptr;
		EOS_P2P_SocketId OutSocketId = {};
		uint8_t OutChannel = 0;
		uint32_t OutBytesWritten = 0;

		EOS_EResult RecvResult = EOS_P2P_ReceivePacket(P2PHandle, &RecvOptions,
			&OutPeerId, &OutSocketId, &OutChannel, Buffer.GetData(), &OutBytesWritten);

		if (RecvResult == EOS_EResult::EOS_Success && OutBytesWritten > 0)
		{
			Buffer.SetNum(OutBytesWritten);

			// Convert sender ID to string
			char PeerIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
			int32_t PeerIdLen = sizeof(PeerIdStr);
			FString RemoteId;
			if (EOS_ProductUserId_ToString(OutPeerId, PeerIdStr, &PeerIdLen) == EOS_EResult::EOS_Success)
			{
				RemoteId = ANSI_TO_TCHAR(PeerIdStr);
			}

			OnPacketReceived.Broadcast(RemoteId, static_cast<int32>(OutChannel), Buffer);
		}
		else
		{
			break;
		}
	}

	return true; // Keep ticking
}

// ── Sending ──────────────────────────────────────────────────────────────────

bool UEEOSP2PSubsystem::SendPacket(const FString& RemoteUserId, const TArray<uint8>& Data, int32 Channel, EEOSPacketReliability Reliability)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendPacket"));
		return false;
	}

	if (Data.Num() > EOS_P2P_MAX_PACKET_SIZE)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SendPacket — Payload of %d bytes exceeds EOS_P2P_MAX_PACKET_SIZE (%d)"), Data.Num(), EOS_P2P_MAX_PACKET_SIZE);
		return false;
	}

	if (Channel < 0 || Channel > 255)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SendPacket — Channel %d is out of range (0-255)"), Channel);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — Platform handle not available"));
		return false;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — P2P interface not available"));
		return false;
	}

	EOS_ProductUserId LocalPUID = GetLocalProductUserId(PlatformHandle);
	if (!LocalPUID)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SendPacket — No logged-in Connect user; cannot send"));
		return false;
	}

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));
	if (!EOS_ProductUserId_IsValid(RemotePUID))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — Invalid remote user ID: %s"), *RemoteUserId);
		return false;
	}

	// Use the socket this peer's connection runs on (tracked from accept/established);
	// fall back to the current outgoing socket for peers we initiated to.
	const FString* TrackedSocket = PeerSockets.Find(RemoteUserId);
	const FString& SocketNameToUse = TrackedSocket ? *TrackedSocket : CurrentSocketName;

	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*SocketNameToUse), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

	// Map reliability
	EOS_EPacketReliability EosReliability;
	switch (Reliability)
	{
	case EEOSPacketReliability::UnreliableUnordered: EosReliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered; break;
	case EEOSPacketReliability::ReliableUnordered:   EosReliability = EOS_EPacketReliability::EOS_PR_ReliableUnordered; break;
	case EEOSPacketReliability::UnreliableOrdered:   // EOS SDK does not support UnreliableOrdered — fall through to ReliableOrdered
	case EEOSPacketReliability::ReliableOrdered:
	default:                                        EosReliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered; break;
	}

	EOS_P2P_SendPacketOptions Options = {};
	Options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.RemoteUserId = RemotePUID;
	Options.SocketId = &SocketId;
	Options.Channel = static_cast<uint8>(Channel);
	Options.DataLengthBytes = Data.Num();
	Options.Data = Data.GetData();
	Options.bAllowDelayedDelivery = EOS_TRUE;
	Options.Reliability = EosReliability;
	Options.bDisableAutoAcceptConnection = EOS_FALSE;

	EOS_EResult Result = EOS_P2P_SendPacket(P2PHandle, &Options);
	if (Result != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SendPacket — Failed to send %d bytes to %s: %s"),
			Data.Num(), *RemoteUserId, ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
		return false;
	}

	return true;
}

bool UEEOSP2PSubsystem::SendStringPacket(const FString& RemoteUserId, const FString& Message, int32 Channel, EEOSPacketReliability Reliability)
{
	TArray<uint8> Data;
	FTCHARToUTF8 Converter(*Message);
	Data.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	return SendPacket(RemoteUserId, Data, Channel, Reliability);
}

void UEEOSP2PSubsystem::BroadcastPacket(const TArray<uint8>& Data, int32 Channel, EEOSPacketReliability Reliability)
{
	for (const FString& PeerId : ConnectedPeers)
	{
		SendPacket(PeerId, Data, Channel, Reliability);
	}
}

// ── Connection Management ────────────────────────────────────────────────────

bool UEEOSP2PSubsystem::AcceptConnection(const FString& RemoteUserId, const FString& SocketName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptConnection"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — Platform handle not available"));
		return false;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — P2P interface not available"));
		return false;
	}

	EOS_ProductUserId LocalPUID = GetLocalProductUserId(PlatformHandle);
	if (!LocalPUID)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::AcceptConnection — No logged-in Connect user; cannot accept"));
		return false;
	}

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));
	if (!EOS_ProductUserId_IsValid(RemotePUID))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — Invalid remote user ID: %s"), *RemoteUserId);
		return false;
	}

	// The accept must use the socket ID the remote requested (delivered via OnConnectionRequest);
	// accepting on any other socket never matches the pending request.
	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*SocketName), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

	EOS_P2P_AcceptConnectionOptions Options = {};
	Options.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.RemoteUserId = RemotePUID;
	Options.SocketId = &SocketId;

	EOS_EResult Result = EOS_P2P_AcceptConnection(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		// Track the socket per peer — deliberately does NOT retarget CurrentSocketName,
		// which only governs connections we initiate.
		PeerSockets.Add(RemoteUserId, SocketName);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::AcceptConnection — Accepted connection from %s on socket '%s'"), *RemoteUserId, *SocketName);
		// Note: OnConnectionEstablished will fire from the notification callback when the connection is actually established
		return true;
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::AcceptConnection — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	return false;
}

void UEEOSP2PSubsystem::CloseConnection(const FString& RemoteUserId, const FString& SocketName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CloseConnection"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::CloseConnection — Platform handle not available"));
		return;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		return;
	}

	EOS_ProductUserId LocalPUID = GetLocalProductUserId(PlatformHandle);
	if (!LocalPUID)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::CloseConnection — No logged-in Connect user; cannot close"));
		return;
	}

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));

	// Empty socket name = close the socket tracked for this peer (from accept/established),
	// falling back to the current outgoing socket.
	FString ResolvedSocketName = SocketName;
	if (ResolvedSocketName.IsEmpty())
	{
		const FString* TrackedSocket = PeerSockets.Find(RemoteUserId);
		ResolvedSocketName = TrackedSocket ? *TrackedSocket : CurrentSocketName;
	}

	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*ResolvedSocketName), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

	EOS_P2P_CloseConnectionOptions Options = {};
	Options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.RemoteUserId = RemotePUID;
	Options.SocketId = &SocketId;

	EOS_EResult Result = EOS_P2P_CloseConnection(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::CloseConnection — Closed connection to %s on socket '%s'"), *RemoteUserId, *ResolvedSocketName);
		ConnectedPeers.Remove(RemoteUserId);
		PeerSockets.Remove(RemoteUserId);
		// No local OnConnectionClosed broadcast here: the PeerConnectionClosed notification
		// (EOS_CCR_ClosedByLocalUser) delivers it — exactly one close event per actual close.
	}
	else
	{
		// Nothing was closed — do not broadcast a close event
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::CloseConnection — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
}

void UEEOSP2PSubsystem::CloseAllConnections()
{
	TArray<FString> PeersCopy = ConnectedPeers;
	for (const FString& PeerId : PeersCopy)
	{
		CloseConnection(PeerId);
	}
}

// ── Relay & NAT ──────────────────────────────────────────────────────────────

void UEEOSP2PSubsystem::SetRelayControl(EEOSRelayControl RelayMode)
{
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SetRelayControl — Platform handle not available; relay mode not applied"));
		return;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SetRelayControl — P2P interface not available; relay mode not applied"));
		return;
	}

	EOS_ERelayControl EosRelayMode;
	switch (RelayMode)
	{
	case EEOSRelayControl::NoRelays:    EosRelayMode = EOS_ERelayControl::EOS_RC_NoRelays; break;
	case EEOSRelayControl::ForceRelays: EosRelayMode = EOS_ERelayControl::EOS_RC_ForceRelays; break;
	case EEOSRelayControl::AllowRelays:
	default:                           EosRelayMode = EOS_ERelayControl::EOS_RC_AllowRelays; break;
	}

	// Note: EOS_P2P_SetRelayControlOptions is platform-wide — it takes no LocalUserId.
	EOS_P2P_SetRelayControlOptions Options = {};
	Options.ApiVersion = EOS_P2P_SETRELAYCONTROL_API_LATEST;
	Options.RelayControl = EosRelayMode;

	EOS_EResult Result = EOS_P2P_SetRelayControl(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		// Cache only what the SDK actually accepted so GetRelayControl never reports
		// a mode that isn't in effect.
		CurrentRelayControl = RelayMode;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::SetRelayControl — Set to %d"), static_cast<int32>(RelayMode));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SetRelayControl — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
}

EEOSRelayControl UEEOSP2PSubsystem::GetRelayControl() const
{
	return CurrentRelayControl;
}

bool UEEOSP2PSubsystem::QueryNATType()
{
	// Failure-to-start paths return false WITHOUT broadcasting: OnNATTypeQueried is a shared
	// multicast, and a rejection broadcast would be indistinguishable from (or misreported as)
	// a legitimately started query's completion. The bool return carries the rejection.
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryNATType"));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		return false;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::QueryNATType — P2P interface not available"));
		return false;
	}

	// Note: EOS_P2P_QueryNATTypeOptions takes no LocalUserId — the query is platform-wide.
	EOS_P2P_QueryNATTypeOptions Options = {};
	Options.ApiVersion = EOS_P2P_QUERYNATTYPE_API_LATEST;

	// Heap context with a weak self pointer — the EOS platform outlives this subsystem,
	// so the callback must not dereference a raw 'this'
	struct FQueryNATTypeContext
	{
		TWeakObjectPtr<UEEOSP2PSubsystem> Self;
	};
	FQueryNATTypeContext* Context = new FQueryNATTypeContext{this};

	EOS_P2P_QueryNATType(P2PHandle, &Options, Context,
		[](const EOS_P2P_OnQueryNATTypeCompleteInfo* Data)
		{
			if (!Data || !Data->ClientData) return;
			TUniquePtr<FQueryNATTypeContext> Ctx(static_cast<FQueryNATTypeContext*>(Data->ClientData));

			UEEOSP2PSubsystem* Self = Ctx->Self.Get();
			if (!Self) return;

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				switch (Data->NATType)
				{
				case EOS_ENATType::EOS_NAT_Open:     Self->CachedNATType = EEOSNATType::Open; break;
				case EOS_ENATType::EOS_NAT_Moderate:  Self->CachedNATType = EEOSNATType::Moderate; break;
				case EOS_ENATType::EOS_NAT_Strict:    Self->CachedNATType = EEOSNATType::Strict; break;
				default:                              Self->CachedNATType = EEOSNATType::Unknown; break;
				}
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: NAT type = %d"), static_cast<int32>(Self->CachedNATType));
			}
			else
			{
				Self->CachedNATType = EEOSNATType::Unknown;
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::QueryNATType — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}

			Self->OnNATTypeQueried.Broadcast(Self->CachedNATType);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::QueryNATType — Querying NAT type..."));
	return true;
}

void UEEOSP2PSubsystem::SetPortRange(int32 Port, int32 MaxAdditionalPorts)
{
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle) return;

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle) return;

	EOS_P2P_SetPortRangeOptions Options = {};
	Options.ApiVersion = EOS_P2P_SETPORTRANGE_API_LATEST;
	Options.Port = static_cast<uint16>(Port);
	Options.MaxAdditionalPortsToTry = static_cast<uint16>(MaxAdditionalPorts);

	EOS_EResult Result = EOS_P2P_SetPortRange(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::SetPortRange — Port=%d, MaxAdditional=%d"), Port, MaxAdditionalPorts);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::SetPortRange — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
}

// ── Queries ──────────────────────────────────────────────────────────────────

EEOSNATType UEEOSP2PSubsystem::GetNATType() const
{
	return CachedNATType;
}

TArray<FString> UEEOSP2PSubsystem::GetConnectedPeers() const
{
	return ConnectedPeers;
}

bool UEEOSP2PSubsystem::IsConnectedToPeer(const FString& RemoteUserId) const
{
	return ConnectedPeers.Contains(RemoteUserId);
}

int32 UEEOSP2PSubsystem::GetConnectionCount() const
{
	return ConnectedPeers.Num();
}

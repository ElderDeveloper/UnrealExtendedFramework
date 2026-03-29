// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSP2PSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "UnrealExtendedEOS.h"

#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "eos_sdk.h"

void UEEOSP2PSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && !Settings->bEnableP2P)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: P2P is disabled in settings"));
		return;
	}

	// Register connection request notification so incoming connections trigger OnConnectionRequest
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
		if (P2PHandle)
		{
			EOS_P2P_AddNotifyPeerConnectionRequestOptions RequestOptions = {};
			RequestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
			// Accept from any socket
			RequestOptions.LocalUserId = nullptr;
			RequestOptions.SocketId = nullptr;

			ConnectionRequestNotifId = EOS_P2P_AddNotifyPeerConnectionRequest(P2PHandle, &RequestOptions, this,
				[](const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
				{
					UEEOSP2PSubsystem* Self = static_cast<UEEOSP2PSubsystem*>(Data->ClientData);
					if (!Self) return;

					char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
					int32_t BufferLen = sizeof(RemoteUserIdStr);
					if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
					{
						FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
						FString SocketName = ANSI_TO_TCHAR(Data->SocketId->SocketName);
						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Incoming connection request from %s on socket '%s'"), *RemoteId, *SocketName);
						Self->OnConnectionRequest.Broadcast(RemoteId);
					}
				});

			// Register connection closed notification
			EOS_P2P_AddNotifyPeerConnectionClosedOptions ClosedOptions = {};
			ClosedOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST;
			ClosedOptions.LocalUserId = nullptr;
			ClosedOptions.SocketId = nullptr;

			ConnectionClosedNotifId = EOS_P2P_AddNotifyPeerConnectionClosed(P2PHandle, &ClosedOptions, this,
				[](const EOS_P2P_OnRemoteConnectionClosedInfo* Data)
				{
					UEEOSP2PSubsystem* Self = static_cast<UEEOSP2PSubsystem*>(Data->ClientData);
					if (!Self) return;

					char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
					int32_t BufferLen = sizeof(RemoteUserIdStr);
					if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
					{
						FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
						Self->ConnectedPeers.Remove(RemoteId);

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
						Self->OnConnectionClosed.Broadcast(RemoteId, ReasonStr);
					}
				});

			// Register connection established notification
			EOS_P2P_AddNotifyPeerConnectionEstablishedOptions EstOptions = {};
			EstOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONESTABLISHED_API_LATEST;
			EstOptions.LocalUserId = nullptr;
			EstOptions.SocketId = nullptr;

			ConnectionEstablishedNotifId = EOS_P2P_AddNotifyPeerConnectionEstablished(P2PHandle, &EstOptions, this,
				[](const EOS_P2P_OnPeerConnectionEstablishedInfo* Data)
				{
					UEEOSP2PSubsystem* Self = static_cast<UEEOSP2PSubsystem*>(Data->ClientData);
					if (!Self) return;

					char RemoteUserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
					int32_t BufferLen = sizeof(RemoteUserIdStr);
					if (EOS_ProductUserId_ToString(Data->RemoteUserId, RemoteUserIdStr, &BufferLen) == EOS_EResult::EOS_Success)
					{
						FString RemoteId = ANSI_TO_TCHAR(RemoteUserIdStr);
						FString SocketName = ANSI_TO_TCHAR(Data->SocketId->SocketName);

						if (!Self->ConnectedPeers.Contains(RemoteId))
						{
							Self->ConnectedPeers.Add(RemoteId);
						}

						UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem: Connection established with %s on socket '%s'"), *RemoteId, *SocketName);
					Self->OnConnectionEstablished.Broadcast(RemoteId, SocketName);
					}
				});

			// Start the tick-based receive poll for incoming packets
			ReceiveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UEEOSP2PSubsystem::PollIncomingPackets), 0.0f);
		}
	}
}

void UEEOSP2PSubsystem::Deinitialize()
{
	// Stop the receive ticker
	if (ReceiveTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReceiveTickerHandle);
		ReceiveTickerHandle.Reset();
	}

	// Remove notification callbacks
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (PlatformHandle)
	{
		EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
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
		}
	}

	CloseAllConnections();
	ConnectedPeers.Empty();
	Super::Deinitialize();
}

// ── Receive Polling ──────────────────────────────────────────────────────────

bool UEEOSP2PSubsystem::PollIncomingPackets(float DeltaTime)
{
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle) return true; // Keep ticking

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle) return true;

	// Drain all available packets in this frame
	while (true)
	{
		// Check if there is a packet to receive
		EOS_P2P_GetNextReceivedPacketSizeOptions SizeOptions = {};
		SizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		SizeOptions.LocalUserId = nullptr; // any local user
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
		RecvOptions.LocalUserId = nullptr; // any local user
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

void UEEOSP2PSubsystem::SendPacket(const FString& RemoteUserId, const TArray<uint8>& Data, int32 Channel, EEOSPacketReliability Reliability)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendPacket"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — Platform handle not available"));
		return;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — P2P interface not available"));
		return;
	}

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));
	if (!EOS_ProductUserId_IsValid(RemotePUID))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::SendPacket — Invalid remote user ID: %s"), *RemoteUserId);
		return;
	}

	// Set up socket
	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*CurrentSocketName), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

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
	Options.LocalUserId = nullptr; // Will use the logged-in user
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
	}
}

void UEEOSP2PSubsystem::SendStringPacket(const FString& RemoteUserId, const FString& Message, int32 Channel, EEOSPacketReliability Reliability)
{
	TArray<uint8> Data;
	FTCHARToUTF8 Converter(*Message);
	Data.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	SendPacket(RemoteUserId, Data, Channel, Reliability);
}

void UEEOSP2PSubsystem::BroadcastPacket(const TArray<uint8>& Data, int32 Channel, EEOSPacketReliability Reliability)
{
	for (const FString& PeerId : ConnectedPeers)
	{
		SendPacket(PeerId, Data, Channel, Reliability);
	}
}

// ── Connection Management ────────────────────────────────────────────────────

void UEEOSP2PSubsystem::AcceptConnection(const FString& RemoteUserId, const FString& SocketName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptConnection"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — Platform handle not available"));
		return;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — P2P interface not available"));
		return;
	}

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));
	if (!EOS_ProductUserId_IsValid(RemotePUID))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSP2PSubsystem::AcceptConnection — Invalid remote user ID: %s"), *RemoteUserId);
		return;
	}

	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*SocketName), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

	EOS_P2P_AcceptConnectionOptions Options = {};
	Options.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
	Options.LocalUserId = nullptr; // Will use the logged-in user
	Options.RemoteUserId = RemotePUID;
	Options.SocketId = &SocketId;

	EOS_EResult Result = EOS_P2P_AcceptConnection(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		CurrentSocketName = SocketName;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::AcceptConnection — Accepted connection from %s on socket '%s'"), *RemoteUserId, *SocketName);
		// Note: OnConnectionEstablished will fire from the notification callback when the connection is actually established
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::AcceptConnection — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}
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

	EOS_ProductUserId RemotePUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RemoteUserId));

	EOS_P2P_SocketId SocketId = {};
	SocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
	FCStringAnsi::Strncpy(SocketId.SocketName, TCHAR_TO_ANSI(*SocketName), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

	EOS_P2P_CloseConnectionOptions Options = {};
	Options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
	Options.LocalUserId = nullptr;
	Options.RemoteUserId = RemotePUID;
	Options.SocketId = &SocketId;

	EOS_EResult Result = EOS_P2P_CloseConnection(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSP2PSubsystem::CloseConnection — Closed connection to %s on socket '%s'"), *RemoteUserId, *SocketName);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSP2PSubsystem::CloseConnection — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Result)));
	}

	ConnectedPeers.Remove(RemoteUserId);
	OnConnectionClosed.Broadcast(RemoteUserId, TEXT("LocalDisconnect"));
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
	CurrentRelayControl = RelayMode;

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle) return;

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle) return;

	EOS_ERelayControl EosRelayMode;
	switch (RelayMode)
	{
	case EEOSRelayControl::NoRelays:    EosRelayMode = EOS_ERelayControl::EOS_RC_NoRelays; break;
	case EEOSRelayControl::ForceRelays: EosRelayMode = EOS_ERelayControl::EOS_RC_ForceRelays; break;
	case EEOSRelayControl::AllowRelays:
	default:                           EosRelayMode = EOS_ERelayControl::EOS_RC_AllowRelays; break;
	}

	EOS_P2P_SetRelayControlOptions Options = {};
	Options.ApiVersion = EOS_P2P_SETRELAYCONTROL_API_LATEST;
	Options.RelayControl = EosRelayMode;

	EOS_EResult Result = EOS_P2P_SetRelayControl(P2PHandle, &Options);
	if (Result == EOS_EResult::EOS_Success)
	{
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

void UEEOSP2PSubsystem::QueryNATType()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryNATType"));
		OnNATTypeQueried.Broadcast(EEOSNATType::Unknown);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		OnNATTypeQueried.Broadcast(EEOSNATType::Unknown);
		return;
	}

	EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
	if (!P2PHandle)
	{
		OnNATTypeQueried.Broadcast(EEOSNATType::Unknown);
		return;
	}

	EOS_P2P_QueryNATTypeOptions Options = {};
	Options.ApiVersion = EOS_P2P_QUERYNATTYPE_API_LATEST;

	EOS_P2P_QueryNATType(P2PHandle, &Options, this,
		[](const EOS_P2P_OnQueryNATTypeCompleteInfo* Data)
		{
			UEEOSP2PSubsystem* Self = static_cast<UEEOSP2PSubsystem*>(Data->ClientData);
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

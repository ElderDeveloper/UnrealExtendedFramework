// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Socket/ExtendedSteamSocket.h"
#include "Subsystem/ExtendedSteamSocketsSubsystem.h"
#include "Shared/ExtendedSteamSocketsTypes.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
THIRD_PARTY_INCLUDES_END
#endif

FExtendedSteamSocket::FExtendedSteamSocket(ESocketType InSocketType, const FString& InSocketDescription,
	const FName& InSocketProtocol, FExtendedSteamSocketsSubsystem* InSubsystem)
	: FSocket(InSocketType, InSocketDescription, InSocketProtocol)
	, BindAddress(InSocketProtocol)
	, Subsystem(InSubsystem)
	, bIsListenSocket(false)
#if WITH_EXTENDEDSTEAM_SDK
	, SendFlags(k_nSteamNetworkingSend_UnreliableNoDelay)
	, Connection(k_HSteamNetConnection_Invalid)
	, ListenSocket(k_HSteamListenSocket_Invalid)
	, PollGroup(k_HSteamNetPollGroup_Invalid)
#else
	, SendFlags(0)
#endif
{
}

FExtendedSteamSocket::~FExtendedSteamSocket()
{
	Close();
}

bool FExtendedSteamSocket::Close()
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr)
	{
		return false;
	}

	bool bClosedSomething = false;

	if (Connection != k_HSteamNetConnection_Invalid)
	{
		Interface->CloseConnection(Connection, 0 /*k_ESteamNetConnectionEnd_App_Generic*/, nullptr, false);
		Connection = k_HSteamNetConnection_Invalid;
		bClosedSomething = true;
	}

	// Only a listen socket owns its poll group; accepted child sockets share the listener's group.
	if (PollGroup != k_HSteamNetPollGroup_Invalid)
	{
		Interface->DestroyPollGroup(PollGroup);
		PollGroup = k_HSteamNetPollGroup_Invalid;
	}

	if (ListenSocket != k_HSteamListenSocket_Invalid)
	{
		Interface->CloseListenSocket(ListenSocket);
		ListenSocket = k_HSteamListenSocket_Invalid;
		bClosedSomething = true;
	}

	return bClosedSomething;
#else
	return false;
#endif
}

bool FExtendedSteamSocket::Bind(const FInternetAddr& Addr)
{
	// SteamNetworkingSockets has no distinct bind step for P2P listen sockets; the local virtual
	// port is supplied to CreateListenSocketP2P in Listen(). We simply record the requested address.
	if (Addr.GetProtocolType() == FExtendedSteamNetworkProtocolTypes::SteamP2P()
		|| Addr.GetProtocolType() == FExtendedSteamNetworkProtocolTypes::SteamIP())
	{
		BindAddress = static_cast<const FInternetAddrExtendedSteam&>(Addr);
	}
	return true;
}

bool FExtendedSteamSocket::Connect(const FInternetAddr& Addr)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ExtendedSteamSocket::Connect: SteamNetworkingSockets interface unavailable"));
		return false;
	}

	if (Addr.GetProtocolType() != FExtendedSteamNetworkProtocolTypes::SteamP2P()
		&& Addr.GetProtocolType() != FExtendedSteamNetworkProtocolTypes::SteamIP())
	{
		return false;
	}

	const FInternetAddrExtendedSteam& SteamAddr = static_cast<const FInternetAddrExtendedSteam&>(Addr);
	BindAddress = SteamAddr;

	const SteamNetworkingIdentity& Identity = SteamAddr.GetIdentity();

	// P2P: route through the Steam backend by identity. Direct-IP is only attempted when the identity
	// actually carries an IP address; otherwise we fall back to a P2P connect by identity.
	const SteamNetworkingIPAddr* IpAddr = Identity.GetIPAddr();
	if (Addr.GetProtocolType() == FExtendedSteamNetworkProtocolTypes::SteamIP() && IpAddr != nullptr)
	{
		Connection = Interface->ConnectByIPAddress(*IpAddr, 0, nullptr);
	}
	else
	{
		Connection = Interface->ConnectP2P(Identity, SteamAddr.GetPort(), 0, nullptr);
	}

	return Connection != k_HSteamNetConnection_Invalid;
#else
	return false;
#endif
}

bool FExtendedSteamSocket::Listen(int32 MaxBacklog)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ExtendedSteamSocket::Listen: SteamNetworkingSockets interface unavailable"));
		return false;
	}

	// Listen for P2P clients on the bound virtual port. A poll group lets the driver drain all
	// accepted connections with a single ReceiveMessagesOnPollGroup call.
	ListenSocket = Interface->CreateListenSocketP2P(BindAddress.GetPort(), 0, nullptr);
	if (ListenSocket == k_HSteamListenSocket_Invalid)
	{
		return false;
	}

	PollGroup = Interface->CreatePollGroup();
	bIsListenSocket = true;
	return true;
#else
	return false;
#endif
}

FSocket* FExtendedSteamSocket::Accept(const FString& InSocketDescription)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (Subsystem == nullptr)
	{
		return nullptr;
	}

	// Build a child socket that will carry the already-accepted connection handle (assigned by the
	// driver via SetConnectionHandle). It shares the listener's poll group, so it keeps PollGroup
	// invalid and never tears the group down on close.
	FExtendedSteamSocket* NewSocket = static_cast<FExtendedSteamSocket*>(
		Subsystem->CreateSocket(FName(TEXT("SteamNetworkingSockets")), InSocketDescription, GetProtocol()));
	if (NewSocket != nullptr)
	{
		NewSocket->SendFlags = SendFlags;
		// Preserve the listener's virtual port so peer-address matching stays consistent over P2P.
		NewSocket->BindAddress = BindAddress;
	}
	return NewSocket;
#else
	return nullptr;
#endif
}

bool FExtendedSteamSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = 0;
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr || Connection == k_HSteamNetConnection_Invalid || Count <= 0)
	{
		return false;
	}

	const EResult Result = Interface->SendMessageToConnection(
		Connection, Data, static_cast<uint32>(Count), SendFlags, nullptr);
	if (Result == k_EResultOK)
	{
		BytesSent = Count;
		return true;
	}
	return false;
#else
	return false;
#endif
}

bool FExtendedSteamSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	// SteamNetworkingSockets is connection-oriented: a socket can only send over its own established
	// connection. The net driver selects the correct connection socket for a destination address
	// before calling us, so SendTo is equivalent to Send on the resolved socket.
	return Send(Data, Count, BytesSent);
}

#if WITH_EXTENDEDSTEAM_SDK
bool FExtendedSteamSocket::RecvRaw(SteamNetworkingMessage_t*& OutMessage, int32 MaxMessages, int32& MessagesRead)
{
	MessagesRead = 0;
	OutMessage = nullptr;

	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr)
	{
		return false;
	}

	if (bIsListenSocket)
	{
		if (PollGroup == k_HSteamNetPollGroup_Invalid)
		{
			return false;
		}
		MessagesRead = Interface->ReceiveMessagesOnPollGroup(PollGroup, &OutMessage, MaxMessages);
	}
	else
	{
		if (Connection == k_HSteamNetConnection_Invalid)
		{
			return false;
		}
		MessagesRead = Interface->ReceiveMessagesOnConnection(Connection, &OutMessage, MaxMessages);
	}

	if (MessagesRead < 0)
	{
		// Negative return means the handle is invalid / closed.
		MessagesRead = 0;
		return false;
	}
	return true;
}
#endif

bool FExtendedSteamSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	BytesRead = 0;
#if WITH_EXTENDEDSTEAM_SDK
	// Peek is not supported without consuming a message; a production driver would keep a one-message
	// lookahead buffer. We treat Peek as "no data available" and let the driver poll via RecvRaw.
	if ((Flags & ESocketReceiveFlags::Peek) != 0)
	{
		return true;
	}

	// Serve any bytes left over from a previous message that did not fit the caller's buffer, so a
	// message larger than BufferSize is delivered across successive Recv() calls rather than dropped.
	if (PendingRecvData.Num() > 0)
	{
		const int32 CopyBytes = FMath::Min(BufferSize, PendingRecvData.Num());
		FMemory::Memcpy(Data, PendingRecvData.GetData(), CopyBytes);
		BytesRead = CopyBytes;
		PendingRecvData.RemoveAt(0, CopyBytes);
		return true;
	}

	SteamNetworkingMessage_t* Message = nullptr;
	int32 NumMessages = 0;
	if (!RecvRaw(Message, 1, NumMessages))
	{
		return false;
	}
	if (NumMessages == 0 || Message == nullptr)
	{
		// No data pending — still a successful, non-error call.
		return true;
	}

	const int32 MessageSize = Message->m_cbSize;
	const int32 CopyBytes = FMath::Min(BufferSize, MessageSize);
	if (CopyBytes > 0)
	{
		FMemory::Memcpy(Data, Message->m_pData, CopyBytes);
		BytesRead = CopyBytes;
	}
	// Stash whatever did not fit so the next Recv() delivers it instead of losing it.
	if (MessageSize > CopyBytes)
	{
		const uint8* Remainder = static_cast<const uint8*>(Message->m_pData) + CopyBytes;
		PendingRecvData.Append(Remainder, MessageSize - CopyBytes);
	}
	Message->Release();
	return true;
#else
	return false;
#endif
}

bool FExtendedSteamSocket::HasPendingData(uint32& PendingDataSize)
{
	// Honest stub: the SDK message queue cannot be inspected for size without consuming a message.
	// The net driver polls via TickDispatch/RecvRaw instead of relying on this probe.
	PendingDataSize = 0;
	return false;
}

ESocketConnectionState FExtendedSteamSocket::GetConnectionState()
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr || Connection == k_HSteamNetConnection_Invalid)
	{
		return SCS_NotConnected;
	}

	SteamNetConnectionInfo_t Info;
	FMemory::Memzero(Info);
	if (!Interface->GetConnectionInfo(Connection, &Info))
	{
		return SCS_NotConnected;
	}

	switch (Info.m_eState)
	{
	case k_ESteamNetworkingConnectionState_Connected:
		return SCS_Connected;
	case k_ESteamNetworkingConnectionState_Connecting:
	case k_ESteamNetworkingConnectionState_FindingRoute:
		return SCS_NotConnected;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		return SCS_ConnectionError;
	default:
		return SCS_NotConnected;
	}
#else
	return SCS_NotConnected;
#endif
}

void FExtendedSteamSocket::GetAddress(FInternetAddr& OutAddr)
{
	OutAddr = BindAddress;
}

bool FExtendedSteamSocket::GetPeerAddress(FInternetAddr& OutAddr)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
	if (Interface == nullptr || Connection == k_HSteamNetConnection_Invalid)
	{
		return false;
	}

	SteamNetConnectionInfo_t Info;
	FMemory::Memzero(Info);
	if (!Interface->GetConnectionInfo(Connection, &Info))
	{
		return false;
	}

	if (OutAddr.GetProtocolType() == FExtendedSteamNetworkProtocolTypes::SteamP2P()
		|| OutAddr.GetProtocolType() == FExtendedSteamNetworkProtocolTypes::SteamIP())
	{
		FInternetAddrExtendedSteam& SteamOut = static_cast<FInternetAddrExtendedSteam&>(OutAddr);
		SteamOut.SetSteamID64(Info.m_identityRemote.GetSteamID64());
		// Record the virtual port the peer is reached on (informational; the driver routes by SteamID).
		SteamOut.SetPort(BindAddress.GetPort());
		return true;
	}
	return false;
#else
	return false;
#endif
}

bool FExtendedSteamSocket::WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	// Incoming connections are surfaced by Steam callbacks, not by polling the listen socket here.
	bHasPendingConnection = false;
	return true;
}

bool FExtendedSteamSocket::SetLinger(bool bShouldLinger, int32 Timeout)
{
	// Linger is honored at CloseConnection time via its bEnableLinger argument; we accept the request
	// but keep Close() non-lingering for determinism. Documented as a partial implementation.
	return true;
}

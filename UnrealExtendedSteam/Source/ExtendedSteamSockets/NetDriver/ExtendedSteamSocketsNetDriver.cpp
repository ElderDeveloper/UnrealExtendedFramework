// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "NetDriver/ExtendedSteamSocketsNetDriver.h"
#include "Subsystem/ExtendedSteamSocketsSubsystem.h"
#include "Socket/ExtendedSteamSocket.h"
#include "Networking/ExtendedSteamNetworkingTypes.h"
#include "Shared/ExtendedSteamSocketsTypes.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/World.h"
#include "Net/DataChannel.h"
#include "PacketHandler.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
THIRD_PARTY_INCLUDES_END
#endif

// SteamNetworkingSockets frames and fragments its own messages, so the app-level MTU is not bound by
// the wire MTU. Historically UNetConnection capped the max packet near 1024; modern UE allows more, so
// the transport advertises a larger MTU (1 byte overhead) for better throughput on bulk sends.
// VALIDATE PER ENGINE VERSION: if UNetConnection still asserts above 1024 on your build, set this
// back to 1024 — the previous value here was chosen to work around exactly that assert.
namespace ExtendedSteamSocketsNetDriverPrivate
{
	static const int32 ExtendedSteamMaxPacket = 1200;
}

// ---------------------------------------------------------------------------------------------
// UExtendedSteamSocketsNetConnection
// ---------------------------------------------------------------------------------------------

void UExtendedSteamSocketsNetConnection::InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
	EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Clamp to the transport's advertised MTU/overhead when the caller passes the defaults.
	Super::InitBase(InDriver, InSocket, InURL, InState,
		(InMaxPacket == 0) ? ExtendedSteamSocketsNetDriverPrivate::ExtendedSteamMaxPacket : InMaxPacket,
		(InPacketOverhead == 0) ? 1 : InPacketOverhead);

	ConnectionSocket = static_cast<FExtendedSteamSocket*>(InSocket);
}

void UExtendedSteamSocketsNetConnection::InitRemoteConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
	const FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	// Record the peer address for LowLevelGetRemoteAddress / send routing.
	RemoteAddr = InRemoteAddr.Clone();
	InitSendBuffer();

	// A remote (server-side) connection expects the client to log in.
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UExtendedSteamSocketsNetConnection::InitLocalConnection(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL,
	EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	// Resolve the server address we are connecting to from the URL.
	if (InDriver != nullptr && InDriver->GetSocketSubsystem() != nullptr)
	{
		RemoteAddr = InDriver->GetSocketSubsystem()->GetAddressFromString(InURL.Host);
		if (RemoteAddr.IsValid())
		{
			RemoteAddr->SetPort(InURL.Port);
		}
	}

	InitSendBuffer();
}

void UExtendedSteamSocketsNetConnection::CleanUp()
{
	// Server-accepted connections own their socket; destroy it (and stop routing its handle) here.
	if (bOwnsSocket && ConnectionSocket != nullptr && GetDriver() != nullptr && GetDriver()->GetSocketSubsystem() != nullptr)
	{
		ISocketSubsystem* SocketSub = GetDriver()->GetSocketSubsystem();
#if WITH_EXTENDEDSTEAM_SDK
		if (FExtendedSteamSocketsSubsystem* SteamSub = static_cast<FExtendedSteamSocketsSubsystem*>(SocketSub))
		{
			SteamSub->UnregisterHandle(ConnectionSocket->GetConnectionHandle());
		}
#endif
		SocketSub->DestroySocket(ConnectionSocket);
		ConnectionSocket = nullptr;
	}

	UNetConnection::CleanUp();
}

void UExtendedSteamSocketsNetConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (ConnectionSocket == nullptr)
	{
		return;
	}

	const uint8* SendData = reinterpret_cast<const uint8*>(Data);

	// Run per-connection packet modifiers (encryption, the stateless-connect handshake, etc).
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits, Traits);
		if (!ProcessedData.bError)
		{
			SendData = ProcessedData.Data;
			CountBits = ProcessedData.CountBits;
		}
		else
		{
			CountBits = 0;
		}
	}

	if (CountBits <= 0)
	{
		return;
	}

	const int32 BytesToSend = FMath::DivideAndRoundUp(CountBits, 8);
	int32 BytesSent = 0;
	ConnectionSocket->Send(SendData, BytesToSend, BytesSent);
}

FString UExtendedSteamSocketsNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return RemoteAddr.IsValid() ? RemoteAddr->ToString(bAppendPort) : TEXT("ExtendedSteamSockets:invalid");
}

FString UExtendedSteamSocketsNetConnection::LowLevelDescribe()
{
	return FString::Printf(TEXT("ExtendedSteamSockets connection to %s"), *LowLevelGetRemoteAddress(true));
}

void UExtendedSteamSocketsNetConnection::HandleRecvMessage(void* InData, int32 SizeOfData, const FInternetAddr* InFormattedAddress)
{
	uint8* RecvData = reinterpret_cast<uint8*>(InData);
	if (RecvData == nullptr || InFormattedAddress == nullptr)
	{
		return;
	}

	// Until the stateless-connect challenge passes, inbound packets are handshake traffic that must be
	// run through the driver's connectionless handler rather than delivered to the connection directly.
	if (bInConnectionlessHandshake)
	{
		UExtendedSteamSocketsNetDriver* SteamNetDriver = static_cast<UExtendedSteamSocketsNetDriver*>(Driver);
		if (SteamNetDriver != nullptr && !SteamNetDriver->ArePacketHandlersDisabled()
			&& SteamNetDriver->ConnectionlessHandler.IsValid() && SteamNetDriver->StatelessConnectComponent.IsValid())
		{
			bool bRestartedHandshake = false;

			TSharedPtr<const FInternetAddr> SharedAddress = InFormattedAddress->Clone();
			const ProcessedPacket RawPacket = SteamNetDriver->ConnectionlessHandler->IncomingConnectionless(SharedAddress, RecvData, SizeOfData);
			TSharedPtr<StatelessConnectHandlerComponent> StatelessConnect = SteamNetDriver->StatelessConnectComponent.Pin();

			if (!RawPacket.bError && StatelessConnect.IsValid()
				&& StatelessConnect->HasPassedChallenge(SharedAddress, bRestartedHandshake) && !bRestartedHandshake)
			{
				// Seed the packet sequence from the handshake and begin the per-connection handshake.
				int32 ServerSequence = 0;
				int32 ClientSequence = 0;
				StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);
				InitSequence(ClientSequence, ServerSequence);

				if (Handler.IsValid())
				{
					Handler->BeginHandshaking();
				}

				bInConnectionlessHandshake = false;
				UE_LOG(LogExtendedSteam, Verbose, TEXT("ExtendedSteamSockets: connectionless handshake complete"));

				// Hand the stateless component over to this connection for the remainder of the session.
				if (StatelessConnectComponent.IsValid())
				{
					StatelessConnectComponent.Pin()->SetDriver(SteamNetDriver);
				}

				StatelessConnect->ResetChallengeData();

				SizeOfData = FMath::DivideAndRoundUp(RawPacket.CountBits, 8);
				if (SizeOfData > 0)
				{
					RecvData = RawPacket.Data;
				}
				else
				{
					return;
				}
			}
		}
	}

	UNetConnection::ReceivedRawPacket(RecvData, SizeOfData);
}

// ---------------------------------------------------------------------------------------------
// UExtendedSteamSocketsNetDriver
// ---------------------------------------------------------------------------------------------

ISocketSubsystem* UExtendedSteamSocketsNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get(EXTENDEDSTEAM_SOCKETS_SUBSYSTEM);
}

bool UExtendedSteamSocketsNetDriver::IsAvailable() const
{
	// Available only when the ExtendedSteamSockets subsystem is actually registered (which the module
	// only does when Steam networking is enabled and the client is up).
	return ISocketSubsystem::Get(EXTENDEDSTEAM_SOCKETS_SUBSYSTEM) != nullptr;
}

bool UExtendedSteamSocketsNetDriver::IsNetResourceValid()
{
	// Valid while we hold a socket (client connect / server listen) or are a pending server driver.
	return Socket != nullptr || ServerConnection == nullptr;
}

bool UExtendedSteamSocketsNetDriver::ArePacketHandlersDisabled() const
{
#if !UE_BUILD_SHIPPING
	return FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler"));
#else
	return false;
#endif
}

bool UExtendedSteamSocketsNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL,
	bool bReuseAddressAndPort, FString& Error)
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	NetConnectionClass = UExtendedSteamSocketsNetConnection::StaticClass();

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == nullptr)
	{
		Error = TEXT("ExtendedSteamSockets socket subsystem is not registered");
		return false;
	}

	// One socket per driver: the client's connect socket or the server's listen socket.
	Socket = static_cast<FExtendedSteamSocket*>(
		SocketSubsystem->CreateSocket(FName(TEXT("SteamNetworkingSockets")), TEXT("ExtendedSteamSockets NetDriver"),
			FExtendedSteamNetworkProtocolTypes::SteamP2P()));

	if (Socket == nullptr)
	{
		Error = TEXT("Failed to create an ExtendedSteamSockets socket");
		return false;
	}

	return true;
}

bool UExtendedSteamSocketsNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	TSharedPtr<FInternetAddr> RemoteAddr = SocketSubsystem->GetAddressFromString(ConnectURL.Host);
	if (!RemoteAddr.IsValid())
	{
		Error = FString::Printf(TEXT("ExtendedSteamSockets: could not parse connect address '%s'"), *ConnectURL.Host);
		return false;
	}
	RemoteAddr->SetPort(ConnectURL.Port);

	if (!Socket->Connect(*RemoteAddr))
	{
		Error = TEXT("ExtendedSteamSockets: ConnectP2P failed");
		return false;
	}

	// The client also runs a connectionless handler so it can complete the stateless handshake.
	InitConnectionlessHandler();

	// Set up the single server-facing connection on the client.
	UExtendedSteamSocketsNetConnection* Connection = NewObject<UExtendedSteamSocketsNetConnection>();
	ServerConnection = Connection;
	Connection->InitLocalConnection(this, Socket, ConnectURL, USOCK_Pending);

	CreateInitialClientChannels();

	// Register the connect handle so connection-status callbacks route back to this driver.
#if WITH_EXTENDEDSTEAM_SDK
	if (FExtendedSteamSocketsSubsystem* SteamSub = static_cast<FExtendedSteamSocketsSubsystem*>(SocketSubsystem))
	{
		SteamSub->RegisterHandle(Socket->GetConnectionHandle(), this);
	}
#endif

	return true;
}

bool UExtendedSteamSocketsNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	// Bind the local virtual port from the URL, then listen for P2P clients.
	TSharedRef<FInternetAddr> BindAddr = GetSocketSubsystem()->CreateInternetAddr(FExtendedSteamNetworkProtocolTypes::SteamP2P());
	BindAddr->SetPort(LocalURL.Port);
	Socket->Bind(*BindAddr);

	if (!Socket->Listen(0))
	{
		Error = TEXT("ExtendedSteamSockets: CreateListenSocketP2P failed");
		return false;
	}

	InitConnectionlessHandler();

	// Register the listen handle so new-connection callbacks route back to this driver.
#if WITH_EXTENDEDSTEAM_SDK
	if (FExtendedSteamSocketsSubsystem* SteamSub = static_cast<FExtendedSteamSocketsSubsystem*>(GetSocketSubsystem()))
	{
		SteamSub->RegisterHandle(Socket->GetListenSocketHandle(), this);
	}
#endif

	return true;
}

void UExtendedSteamSocketsNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	if (Socket == nullptr)
	{
		return;
	}

#if WITH_EXTENDEDSTEAM_SDK
	const bool bIsServer = (ServerConnection == nullptr);

	// Drain every message pending this tick. On the server this pulls from the listen socket's poll
	// group (all accepted connections); on the client it pulls from the single server connection.
	while (Socket != nullptr)
	{
		SteamNetworkingMessage_t* Message = nullptr;
		int32 MessagesRead = 0;
		if (!Socket->RecvRaw(Message, 1, MessagesRead))
		{
			// Invalid / closed handle.
			break;
		}
		if (MessagesRead <= 0 || Message == nullptr)
		{
			// No more data queued.
			break;
		}

		UExtendedSteamSocketsNetConnection* TargetConnection = bIsServer
			? static_cast<UExtendedSteamSocketsNetConnection*>(FindClientConnectionForHandle(Message->m_conn))
			: static_cast<UExtendedSteamSocketsNetConnection*>(ToRawPtr(ServerConnection));

		if (TargetConnection != nullptr)
		{
			// Reconstruct the sender address so connectionless-handshake routing stays consistent.
			FInternetAddrExtendedSteam SenderAddr(FExtendedSteamNetworkProtocolTypes::SteamP2P());
			SenderAddr.SetSteamID64(Message->m_identityPeer.GetSteamID64());
			SenderAddr.SetPort(Message->m_nChannel);

			TargetConnection->HandleRecvMessage(Message->m_pData, Message->m_cbSize, &SenderAddr);
		}
		else
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("ExtendedSteamSockets: received a message for unknown connection handle %u"), Message->m_conn);
		}

		Message->Release();
	}
#endif
}

void UExtendedSteamSocketsNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (!Address.IsValid() || !Address->IsValid() || Socket == nullptr)
	{
		return;
	}

	const uint8* SendData = reinterpret_cast<const uint8*>(Data);

	// Connectionless sends (server-side handshake replies, etc) run through the connectionless handler.
	if (ConnectionlessHandler.IsValid())
	{
		const ProcessedPacket ProcessedData =
			ConnectionlessHandler->OutgoingConnectionless(Address, const_cast<uint8*>(SendData), CountBits, Traits);

		if (!ProcessedData.bError)
		{
			SendData = ProcessedData.Data;
			CountBits = ProcessedData.CountBits;
		}
		else
		{
			CountBits = 0;
		}
	}

	if (CountBits <= 0)
	{
		return;
	}

	const int32 BytesToSend = FMath::DivideAndRoundUp(CountBits, 8);
	int32 BytesSent = 0;

	// Steam is connection-oriented, so route the datagram to the connection that owns this address.
	if (FExtendedSteamSocket* TargetSocket = ResolveSocketForAddress(*Address))
	{
		TargetSocket->SendTo(SendData, BytesToSend, BytesSent, *Address);
	}
	else
	{
		UE_LOG(LogExtendedSteam, Verbose,
			TEXT("ExtendedSteamSockets: LowLevelSend found no connection for %s; packet dropped"), *Address->ToString(true));
	}
}

FExtendedSteamSocket* UExtendedSteamSocketsNetDriver::ResolveSocketForAddress(const FInternetAddr& Address)
{
	// Client: everything goes over the single server-facing socket.
	if (ServerConnection != nullptr)
	{
		return Socket;
	}

	// Server: find the accepted connection whose peer matches this destination. Match on the Steam
	// identity only (the 8-byte SteamID encoded by GetRawIp): a received message's sender port comes
	// from m_nChannel, which is 0 for connection messages, so it will not match the connection's
	// stored virtual port. There is one connection per peer identity, so identity is unambiguous.
	const TArray<uint8> TargetIdentity = Address.GetRawIp();
	for (TObjectPtr<UNetConnection>& ClientConnection : ClientConnections)
	{
		UExtendedSteamSocketsNetConnection* SteamConn = static_cast<UExtendedSteamSocketsNetConnection*>(ClientConnection.Get());
		if (SteamConn != nullptr)
		{
			TSharedPtr<const FInternetAddr> ConnAddr = SteamConn->GetRemoteAddr();
			if (ConnAddr.IsValid() && ConnAddr->GetRawIp() == TargetIdentity)
			{
				return SteamConn->GetConnectionSocket();
			}
		}
	}

	return nullptr;
}

UNetConnection* UExtendedSteamSocketsNetDriver::FindClientConnectionForHandle(uint32 SocketHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	for (TObjectPtr<UNetConnection>& ClientConnection : ClientConnections)
	{
		UExtendedSteamSocketsNetConnection* SteamConn = static_cast<UExtendedSteamSocketsNetConnection*>(ClientConnection.Get());
		if (SteamConn != nullptr)
		{
			const FExtendedSteamSocket* ConnSocket = SteamConn->GetConnectionSocket();
			if (ConnSocket != nullptr && ConnSocket->GetConnectionHandle() == SocketHandle)
			{
				return ClientConnection;
			}
		}
	}
#endif
	return nullptr;
}

void UExtendedSteamSocketsNetDriver::OnConnectionCreated(uint32 ListenParentHandle, uint32 NewConnHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	FExtendedSteamSocketsSubsystem* SocketSub = static_cast<FExtendedSteamSocketsSubsystem*>(GetSocketSubsystem());
	if (Socket == nullptr || SocketSub == nullptr)
	{
		return;
	}

	// Only handle connections arriving on the listen socket THIS driver owns.
	if (Socket->GetListenSocketHandle() != ListenParentHandle)
	{
		return;
	}

	ISteamNetworkingSockets* Interface = SocketSub->GetSteamSocketsInterface();
	if (Interface == nullptr)
	{
		return;
	}

	// The connection MUST be accepted or closed promptly; UE decides which first.
	if (Notify != nullptr && Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept)
	{
		const EResult AcceptResult = Interface->AcceptConnection(NewConnHandle);
		if (AcceptResult != k_EResultOK)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("ExtendedSteamSockets: AcceptConnection failed (result %d)"), static_cast<int32>(AcceptResult));
			// Close the handle rather than leaving it to time out (consistent with the wrapper-alloc path below).
			Interface->CloseConnection(NewConnHandle, 0, "AcceptConnection failed", false);
			return;
		}

		// Wrap the accepted connection in a child socket and route it through the listener's poll group.
		FExtendedSteamSocket* NewSocket = static_cast<FExtendedSteamSocket*>(Socket->Accept(TEXT("ExtendedSteamSockets accepted")));
		if (NewSocket == nullptr)
		{
			Interface->CloseConnection(NewConnHandle, 0, "Accept wrapper allocation failed", false);
			return;
		}
		NewSocket->SetConnectionHandle(NewConnHandle);
		Interface->SetConnectionPollGroup(NewConnHandle, Socket->GetPollGroup());

		// Resolve the peer address for bookkeeping and send routing.
		FInternetAddrExtendedSteam ConnectedAddr(FExtendedSteamNetworkProtocolTypes::SteamP2P());
		NewSocket->GetPeerAddress(ConnectedAddr);

		// We may already have a full route; if so, skip straight to open.
		EConnectionState InitialState = USOCK_Pending;
		SteamNetConnectionInfo_t Info;
		FMemory::Memzero(Info);
		if (Interface->GetConnectionInfo(NewConnHandle, &Info) && Info.m_eState == k_ESteamNetworkingConnectionState_Connected)
		{
			InitialState = USOCK_Open;
		}

		UExtendedSteamSocketsNetConnection* NewConnection = NewObject<UExtendedSteamSocketsNetConnection>();
		check(NewConnection != nullptr);
		NewConnection->InitRemoteConnection(this, NewSocket, World ? World->URL : FURL(), ConnectedAddr, InitialState);
		NewConnection->SetOwnsSocket(true);

		// Route the initial inbound packets through the stateless-connect handshake (default path).
		if (!ArePacketHandlersDisabled() && ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
		{
			NewConnection->FlagForHandshake();
		}

		Notify->NotifyAcceptedConnection(NewConnection);
		AddClientConnection(NewConnection);

		// Route this connection's status callbacks + drained messages back to this driver.
		SocketSub->RegisterHandle(NewConnHandle, this);

		UE_LOG(LogExtendedSteam, Log, TEXT("ExtendedSteamSockets: accepted connection %u from %s"),
			NewConnHandle, *ConnectedAddr.ToString(true));
	}
	else
	{
		Interface->CloseConnection(NewConnHandle, 0, "Connection rejected", false);
		UE_LOG(LogExtendedSteam, Log, TEXT("ExtendedSteamSockets: rejected incoming connection %u"), NewConnHandle);
	}
#endif
}

void UExtendedSteamSocketsNetDriver::OnConnectionUpdated(uint32 SocketHandle, int32 NewState)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (NewState != k_ESteamNetworkingConnectionState_Connected)
	{
		return;
	}

	UNetConnection* SocketConnection = (ServerConnection != nullptr)
		? ToRawPtr(ServerConnection)
		: FindClientConnectionForHandle(SocketHandle);

	if (SocketConnection != nullptr)
	{
		SocketConnection->SetConnectionState(USOCK_Open);
		UE_LOG(LogExtendedSteam, Verbose, TEXT("ExtendedSteamSockets: connection %u is now open"), SocketHandle);
	}
#endif
}

void UExtendedSteamSocketsNetDriver::OnConnectionDisconnected(uint32 SocketHandle)
{
	UNetConnection* SocketConnection = (ServerConnection != nullptr)
		? ToRawPtr(ServerConnection)
		: FindClientConnectionForHandle(SocketHandle);

	if (SocketConnection != nullptr)
	{
		SocketConnection->SetConnectionState(USOCK_Closed);
		UE_LOG(LogExtendedSteam, Verbose, TEXT("ExtendedSteamSockets: connection %u disconnected"), SocketHandle);
	}
}

void UExtendedSteamSocketsNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	if (Socket != nullptr)
	{
		ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
#if WITH_EXTENDEDSTEAM_SDK
		if (FExtendedSteamSocketsSubsystem* SteamSub = static_cast<FExtendedSteamSocketsSubsystem*>(SocketSubsystem))
		{
			SteamSub->UnregisterHandle(Socket->IsListenSocket() ? Socket->GetListenSocketHandle() : Socket->GetConnectionHandle());
		}
#endif
		if (SocketSubsystem != nullptr)
		{
			SocketSubsystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}
}

void UExtendedSteamSocketsNetDriver::Shutdown()
{
	Super::Shutdown();
	LowLevelDestroy();
}

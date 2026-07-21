// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "Networking/ExtendedSteamNetworkingTypes.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steamnetworkingtypes.h"
THIRD_PARTY_INCLUDES_END
#endif

class FExtendedSteamSocketsSubsystem;

/**
 * FSocket implementation backed by a SteamNetworkingSockets connection or listen socket.
 *
 * SteamNetworkingSockets is a connection-oriented, message-oriented API (like TCP, but framed),
 * so the mapping onto FSocket's datagram-ish contract is partial by nature. What is wired to real
 * SDK calls: Connect (ConnectP2P / ConnectByIPAddress), Listen (CreateListenSocketP2P + poll group),
 * Accept (wraps an already-accepted connection handle into a child socket), Send/SendTo
 * (SendMessageToConnection), Recv / RecvRaw (ReceiveMessagesOnConnection / ReceiveMessagesOnPollGroup),
 * Close (CloseConnection / CloseListenSocket) and GetConnectionState (GetConnectionInfo).
 *
 * The actual accept DECISION (whether to admit an incoming connection) is taken by the net driver
 * from the SteamNetConnectionStatusChangedCallback_t path; Accept() here only constructs the
 * FSocket wrapper for a handle the driver has already accepted with AcceptConnection(). The two-arg
 * Accept(FInternetAddr&, ...) is not used by this transport and returns nullptr (documented below).
 */
class FExtendedSteamSocket : public FSocket
{
public:
	FExtendedSteamSocket(ESocketType InSocketType, const FString& InSocketDescription, const FName& InSocketProtocol,
		FExtendedSteamSocketsSubsystem* InSubsystem);
	virtual ~FExtendedSteamSocket();

	//~ Begin FSocket Interface (functional against the SDK)
	virtual bool Close() override;
	virtual bool Connect(const FInternetAddr& Addr) override;
	virtual bool Listen(int32 MaxBacklog) override;
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent) override;
	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination) override;
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None) override;
	virtual bool HasPendingData(uint32& PendingDataSize) override;
	virtual ESocketConnectionState GetConnectionState() override;
	virtual void GetAddress(FInternetAddr& OutAddr) override;
	virtual bool GetPeerAddress(FInternetAddr& OutAddr) override;
	virtual int32 GetPortNo() override { return BindAddress.GetPort(); }
	// Wraps a connection handle (already accepted by the driver) into a child FExtendedSteamSocket.
	virtual class FSocket* Accept(const FString& InSocketDescription) override;
	//~ End FSocket Interface (functional)

	//~ Begin FSocket Interface (honest stubs)
	// Bind records the local address; there is no separate SDK bind step for P2P listen sockets.
	virtual bool Bind(const FInternetAddr& Addr) override;
	// The address-returning accept is unused: the driver already knows the peer from the callback.
	virtual class FSocket* Accept(FInternetAddr& OutAddr, const FString& InSocketDescription) override { return nullptr; }
	virtual bool Shutdown(ESocketShutdownMode Mode) override { return false; }
	virtual bool WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime) override;
	virtual bool Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime) override { return false; }
	virtual bool SetNonBlocking(bool bIsNonBlocking = true) override { return true; } // Always non-blocking.
	virtual bool SetNoDelay(bool bIsNoDelay = true) override { return true; }
	virtual bool SetBroadcast(bool bAllowBroadcast = true) override { return false; }
	virtual bool SetReuseAddr(bool bAllowReuse = true) override { return true; }
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0) override;
	virtual bool SetRecvErr(bool bUseErrorQueue = true) override { return false; }
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize) override { NewSize = Size; return true; }
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize) override { NewSize = Size; return true; }
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress) override { return false; }
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress) override { return false; }
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress) override { return false; }
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress) override { return false; }
	virtual bool SetMulticastLoopback(bool bLoopback) override { return false; }
	virtual bool SetMulticastTtl(uint8 TimeToLive) override { return false; }
	virtual bool SetMulticastInterface(const FInternetAddr& InterfaceAddress) override { return false; }
	//~ End FSocket Interface (honest stubs)

#if WITH_EXTENDEDSTEAM_SDK
	/**
	 * Drains one or more raw Steam messages. On a listen socket this pulls from the poll group (all
	 * accepted connections); on a connection socket it pulls from that single connection. Returns
	 * false only on an invalid/closed handle; a successful call with no data returns true with
	 * MessagesRead == 0. The caller owns each returned message and MUST Release() it.
	 */
	bool RecvRaw(SteamNetworkingMessage_t*& OutMessage, int32 MaxMessages, int32& MessagesRead);

	HSteamNetConnection GetConnectionHandle() const { return Connection; }
	HSteamListenSocket GetListenSocketHandle() const { return ListenSocket; }
	HSteamNetPollGroup GetPollGroup() const { return PollGroup; }
	void SetConnectionHandle(HSteamNetConnection InConnection) { Connection = InConnection; }
#endif

	bool IsListenSocket() const { return bIsListenSocket; }

private:
	/** Records the address passed to Bind/Connect/Listen; also returned by GetAddress. */
	FInternetAddrExtendedSteam BindAddress;

	FExtendedSteamSocketsSubsystem* Subsystem;

	/** True once Listen succeeded; controls which handle is valid and which SDK teardown call to make. */
	bool bIsListenSocket;

	/** k_nSteamNetworkingSend_* flags applied to Send. Unreliable-no-delay by default so the transport
	 *  behaves like the UDP datagram layer UNetConnection expects (UE owns reliability/ordering itself). */
	int32 SendFlags;

	/** Overflow from a Recv() whose caller buffer was smaller than the Steam message. Served first by
	 *  the next Recv() before a new message is pulled, so an oversized message is delivered across
	 *  successive calls instead of being silently truncated and dropped. */
	TArray<uint8> PendingRecvData;

#if WITH_EXTENDEDSTEAM_SDK
	HSteamNetConnection Connection;
	HSteamListenSocket ListenSocket;
	HSteamNetPollGroup PollGroup;
#endif
};

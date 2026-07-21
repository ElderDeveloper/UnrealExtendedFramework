// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "ExtendedSteamSocketsNetDriver.generated.h"

class FNetworkNotify;
class FExtendedSteamSocket;
class FInternetAddr;
class ISocketSubsystem;

/**
 * UNetConnection for the ExtendedSteam SteamNetworkingSockets transport.
 *
 * Wires the address/description accessors, forwards LowLevelSend to the owning socket, and routes
 * received Steam messages into the UE packet path via HandleRecvMessage(). Server-accepted
 * connections own their underlying FExtendedSteamSocket (destroyed in CleanUp); the client's single
 * server-facing connection reuses the driver's socket and does not own it.
 */
UCLASS(transient, config=Engine)
class EXTENDEDSTEAMSOCKETS_API UExtendedSteamSocketsNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	UExtendedSteamSocketsNetConnection() = default;

	//~ Begin UNetConnection Interface
	virtual void CleanUp() override;
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState,
		int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
		const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL,
		EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	//~ End UNetConnection Interface

	/** The socket this connection sends/receives through (owned by the driver or by this connection). */
	FExtendedSteamSocket* GetConnectionSocket() const { return ConnectionSocket; }

	/** Marks whether this connection owns (and must destroy) its socket. True for server-accepted connections. */
	void SetOwnsSocket(bool bInOwnsSocket) { bOwnsSocket = bInOwnsSocket; }

	/** Flags this connection so the first inbound packets run through the connectionless (stateless) handshake. */
	void FlagForHandshake() { bInConnectionlessHandshake = true; }

	/** Feeds one received Steam message into the connection: runs the connectionless handshake if pending,
	 *  otherwise delivers straight to UNetConnection::ReceivedRawPacket. */
	void HandleRecvMessage(void* InData, int32 SizeOfData, const class FInternetAddr* InFormattedAddress);

private:
	/** The socket this connection sends through (owned by the driver / socket subsystem, or by us). */
	FExtendedSteamSocket* ConnectionSocket = nullptr;

	/** True while the connection is still completing the UE connectionless (StatelessConnect) handshake. */
	bool bInConnectionlessHandshake = false;

	/** True when this connection must destroy ConnectionSocket on cleanup (server-accepted connections). */
	bool bOwnsSocket = false;
};

/**
 * UNetDriver that carries Unreal networking traffic over SteamNetworkingSockets via the
 * "ExtendedSteamSockets" socket subsystem.
 *
 * Full transport: InitConnect (client) opens a P2P connection and creates the server-facing connection;
 * InitListen (server) opens a P2P listen socket + poll group. Incoming connections are accepted through
 * the subsystem's SteamNetConnectionStatusChangedCallback_t path (OnConnectionCreated), which creates a
 * UExtendedSteamSocketsNetConnection and adds it as a client connection. TickDispatch drains the poll
 * group (server) or the single connection (client) and routes each message to its net connection.
 * LowLevelSend runs the connectionless handler and sends over the connection matching the destination.
 *
 * Known limitations (documented, not blocking): no SDR relay tuning beyond InitRelayNetworkAccess, no
 * fragmentation handling beyond Steam's own, and connectionless packets are handled per-connection
 * (Steam is connection-oriented) rather than via UNetDriver::ProcessConnectionlessPacket.
 */
UCLASS(transient, config=Engine)
class EXTENDEDSTEAMSOCKETS_API UExtendedSteamSocketsNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:
	UExtendedSteamSocketsNetDriver() = default;

	//~ Begin UNetDriver Interface
	virtual void Shutdown() override;
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool IsNetResourceValid() override;
	//~ End UNetDriver Interface

	/** True when the -NoPacketHandler dev switch is set (skips the stateless-connect handshake). */
	bool ArePacketHandlersDisabled() const;

	//~ Connection-status transitions, invoked by FExtendedSteamSocketsSubsystem from the callback pump.
	/** Accepts a new incoming connection on our listen socket and turns it into a client net connection. */
	void OnConnectionCreated(uint32 ListenParentHandle, uint32 NewConnHandle);
	/** Promotes a connection to open once Steam reports it fully connected. */
	void OnConnectionUpdated(uint32 SocketHandle, int32 NewState);
	/** Marks a connection closed when Steam reports it lost. */
	void OnConnectionDisconnected(uint32 SocketHandle);

protected:
	/** Finds the client net connection whose socket carries the given Steam connection handle. */
	class UNetConnection* FindClientConnectionForHandle(uint32 SocketHandle);

	/** Picks the socket to send a (possibly connectionless) packet over for a destination address. */
	FExtendedSteamSocket* ResolveSocketForAddress(const FInternetAddr& Address);

	/** The listen (server) or connect (client) socket for this driver instance. */
	FExtendedSteamSocket* Socket = nullptr;
};

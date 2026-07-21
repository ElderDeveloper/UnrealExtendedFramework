// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Containers/Ticker.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"
#include "Shared/ExtendedSteamSocketsTypes.h"

class ISteamNetworkingSockets;
class UExtendedSteamSocketsNetDriver;
class FExtendedSteamSocketsEventManager;
struct SteamNetConnectionStatusChangedCallback_t;

/**
 * ISocketSubsystem backed by Valve's SteamNetworkingSockets, registered under
 * EXTENDEDSTEAM_SOCKETS_SUBSYSTEM ("ExtendedSteamSockets"). Pairs with UExtendedSteamSocketsNetDriver.
 *
 * Steamworks lifecycle: this subsystem does NOT own SteamAPI init/shutdown and NEVER pumps the Steam
 * callback queues. FExtendedSteamSharedModule owns SteamAPI_Init/Shutdown and runs SteamAPI_RunCallbacks
 * every frame; that pump also DELIVERS the SteamNetConnectionStatusChangedCallback_t events that this
 * subsystem registers for (via a private FExtendedSteamSocketsEventManager). Because the shared module
 * pumps on the game thread, those callbacks are handled inline on the game thread here.
 *
 * Connection routing: the subsystem keeps a small registry mapping every live Steam handle (listen
 * socket, client connect connection, and each accepted server connection) to the UExtendedSteamSocketsNetDriver
 * that owns it. When a connection-status callback arrives, HandleConnectionStatusChanged() looks up the
 * owning driver and forwards accept / connected / disconnected transitions to it.
 *
 * Functional surface: CreateSocket / DestroySocket, CreateInternetAddr, GetSocketAPIName, the relay
 * toggle and InitRelayNetworkAccess wiring, and the connection-status callback + handle registry.
 * Address resolution (GetAddressInfo) is intentionally an honest no-op: ExtendedSteam addresses are
 * SteamIDs, not DNS names.
 */
class FExtendedSteamSocketsSubsystem : public ISocketSubsystem, public FTSTickerObjectBase
{
public:
	FExtendedSteamSocketsSubsystem() = default;
	// Defined out-of-line so the TUniquePtr<FExtendedSteamSocketsEventManager> can hold an incomplete type here.
	virtual ~FExtendedSteamSocketsSubsystem();

	/** Allocates the singleton (does not Init it). */
	static FExtendedSteamSocketsSubsystem* Create();

	/** Shuts down and frees the singleton if it exists. */
	static void Destroy();

	//~ Begin ISocketSubsystem Interface
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;

	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolName) override;
	virtual void DestroySocket(class FSocket* Socket) override;

	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		const FName ProtocolTypeName = NAME_None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;
	virtual TSharedPtr<FInternetAddr> GetAddressFromString(const FString& InAddress) override;

	virtual bool GetHostName(FString& HostName) override;

	virtual TSharedRef<FInternetAddr> CreateInternetAddr() override;
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(const FName ProtocolType) override;

	virtual TArray<TSharedRef<FInternetAddr>> GetLocalBindAddresses() override;

	virtual const TCHAR* GetSocketAPIName() const override;

	virtual ESocketErrors GetLastErrorCode() override { return static_cast<ESocketErrors>(LastSocketError); }
	virtual ESocketErrors TranslateErrorCode(int32 Code) override { return static_cast<ESocketErrors>(Code); }

	virtual bool HasNetworkDevice() override { return true; }
	virtual bool IsSocketWaitSupported() const override { return false; }
	virtual bool RequiresChatDataBeSeparate() override { return false; }
	virtual bool RequiresEncryptedPackets() override { return false; }
	//~ End ISocketSubsystem Interface

	//~ Begin FTSTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTSTickerObjectBase Interface

	/** True when connections should route through the Steam Datagram Relay network (NAT punch + relays). */
	bool IsUsingRelayNetwork() const { return bUseRelays; }

	/** The SteamNetworkingSockets interface grabbed in Init, or null when the SDK/client is unavailable. */
	ISteamNetworkingSockets* GetSteamSocketsInterface() const { return SteamSocketsInterface; }

	//~ Begin connection-status routing (called from the Steam callback pump / net driver)
	/** Associates a Steam handle (listen socket / connection) with the net driver that owns it. */
	void RegisterHandle(uint32 Handle, UExtendedSteamSocketsNetDriver* Driver);

	/** Drops a handle from the routing registry (on close / disconnect). */
	void UnregisterHandle(uint32 Handle);

	/** Returns the net driver registered for a handle, or null (also prunes stale/dead entries). */
	UExtendedSteamSocketsNetDriver* FindNetDriverForHandle(uint32 Handle);

	/**
	 * Dispatches a SteamNetConnectionStatusChangedCallback_t to the owning net driver: accepts new
	 * incoming connections, promotes connected ones, and tears down closed ones. Invoked by the
	 * private event manager during the shared module's SteamAPI_RunCallbacks pump (game thread).
	 */
	void HandleConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Event);
	//~ End connection-status routing

private:
	/** Reads [OnlineSubsystemExtendedSteam] bAllowP2PPacketRelay from GConfig (defaults to true). */
	static bool ReadRelayEnabledFromConfig();

	/** Single instance, owned by the module. */
	static FExtendedSteamSocketsSubsystem* SocketSingleton;

	/** Cached SteamNetworkingSockets() interface; valid only while the Steam client API is up. */
	ISteamNetworkingSockets* SteamSocketsInterface = nullptr;

	/** Whether the relay network was requested + initialized during Init. */
	bool bUseRelays = true;

	/** Last error recorded by this subsystem or its sockets. */
	int32 LastSocketError = 0;

	/**
	 * Maps every live Steam handle (listen socket handle, client connect connection, and each accepted
	 * server-side connection) to the net driver that owns it. Keyed by uint32 (HSteamNetConnection /
	 * HSteamListenSocket are both uint32) so this Public header stays free of Steamworks SDK types.
	 */
	TMap<uint32, TWeakObjectPtr<UExtendedSteamSocketsNetDriver>> HandleToNetDriver;

	/** Owns the STEAM_CALLBACK for SteamNetConnectionStatusChangedCallback_t; created in Init when the SDK is present. */
	TUniquePtr<FExtendedSteamSocketsEventManager> EventManager;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Subsystem/ExtendedSteamSocketsSubsystem.h"
#include "Socket/ExtendedSteamSocket.h"
#include "Networking/ExtendedSteamNetworkingTypes.h"
#include "NetDriver/ExtendedSteamSocketsNetDriver.h"
#include "AddressInfoTypes.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"
THIRD_PARTY_INCLUDES_END
#endif

FExtendedSteamSocketsSubsystem* FExtendedSteamSocketsSubsystem::SocketSingleton = nullptr;

namespace ExtendedSteamSocketsPrivate
{
	/** Socket type name the ExtendedSteam net driver requests from the subsystem. */
	static const FName SteamNetworkingSocketsType(TEXT("SteamNetworkingSockets"));
}

#if WITH_EXTENDEDSTEAM_SDK
/**
 * Private owner of the SteamNetConnectionStatusChangedCallback_t registration. Kept out of the Public
 * subsystem header so no Steamworks SDK types leak into it. Registration is automatic (the 3-arg
 * STEAM_CALLBACK / STEAM_GAMESERVER_CALLBACK forms register in this object's constructor and unregister
 * in its destructor); the callbacks are delivered by the shared module's SteamAPI_RunCallbacks /
 * SteamGameServer_RunCallbacks pump and forwarded straight to the subsystem on the game thread.
 */
class FExtendedSteamSocketsEventManager
{
public:
	explicit FExtendedSteamSocketsEventManager(FExtendedSteamSocketsSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

private:
	STEAM_CALLBACK(FExtendedSteamSocketsEventManager, OnConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);
	STEAM_GAMESERVER_CALLBACK(FExtendedSteamSocketsEventManager, OnConnectionStatusChangedGS, SteamNetConnectionStatusChangedCallback_t);

	FExtendedSteamSocketsSubsystem* Owner;
};

void FExtendedSteamSocketsEventManager::OnConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Param)
{
	if (Owner != nullptr && Param != nullptr)
	{
		Owner->HandleConnectionStatusChanged(Param);
	}
}

void FExtendedSteamSocketsEventManager::OnConnectionStatusChangedGS(SteamNetConnectionStatusChangedCallback_t* Param)
{
	if (Owner != nullptr && Param != nullptr)
	{
		Owner->HandleConnectionStatusChanged(Param);
	}
}
#else
// Complete (empty) definition so the subsystem's TUniquePtr<FExtendedSteamSocketsEventManager> is
// destructible in SDK-less builds. It is never instantiated (Init only creates it under the SDK).
class FExtendedSteamSocketsEventManager
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

FExtendedSteamSocketsSubsystem::~FExtendedSteamSocketsSubsystem() = default;

FExtendedSteamSocketsSubsystem* FExtendedSteamSocketsSubsystem::Create()
{
	if (SocketSingleton == nullptr)
	{
		SocketSingleton = new FExtendedSteamSocketsSubsystem();
	}
	return SocketSingleton;
}

void FExtendedSteamSocketsSubsystem::Destroy()
{
	if (SocketSingleton != nullptr)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = nullptr;
	}
}

bool FExtendedSteamSocketsSubsystem::ReadRelayEnabledFromConfig()
{
	// Opt-out relay toggle. Missing key keeps the default (true) because GetBool leaves the value alone on failure.
	bool bRelayEnabled = true;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(TEXT("OnlineSubsystemExtendedSteam"), TEXT("bAllowP2PPacketRelay"), bRelayEnabled, GEngineIni);
	}
	return bRelayEnabled;
}

bool FExtendedSteamSocketsSubsystem::Init(FString& Error)
{
	bUseRelays = ReadRelayEnabledFromConfig();

#if WITH_EXTENDEDSTEAM_SDK
	// The interface may be null here if the Steam client API is not up yet; sockets re-query it lazily.
	SteamSocketsInterface = SteamNetworkingSockets();

	if (bUseRelays && SteamNetworkingUtils() != nullptr)
	{
		// Warms up the Steam Datagram Relay network so P2P connections can be established quickly.
		SteamNetworkingUtils()->InitRelayNetworkAccess();
	}

	// Begin acquiring the signing certificates now so authenticated connections are ready promptly.
	if (SteamSocketsInterface != nullptr)
	{
		SteamSocketsInterface->InitAuthentication();
	}

	// Register for connection-status callbacks. Registration requires the Steam client API to be up,
	// which the module guarantees before creating + Init'ing this subsystem.
	EventManager = MakeUnique<FExtendedSteamSocketsEventManager>(this);

	UE_LOG(LogExtendedSteam, Log, TEXT("ExtendedSteamSockets subsystem initialized (relays: %s, interface: %s)"),
		bUseRelays ? TEXT("on") : TEXT("off"),
		SteamSocketsInterface != nullptr ? TEXT("available") : TEXT("pending Steam client init"));
#else
	UE_LOG(LogExtendedSteam, Verbose, TEXT("ExtendedSteamSockets subsystem initialized without Steamworks SDK support"));
#endif

	return true;
}

void FExtendedSteamSocketsSubsystem::Shutdown()
{
	// Tearing down the event manager unregisters the Steam callbacks.
	EventManager.Reset();
	HandleToNetDriver.Empty();
	SteamSocketsInterface = nullptr;
}

FSocket* FExtendedSteamSocketsSubsystem::CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolName)
{
	// Only produce sockets for the SteamNetworkingSockets transport; anything else is not ours.
	if (SocketType != ExtendedSteamSocketsPrivate::SteamNetworkingSocketsType)
	{
		return nullptr;
	}

	const FName ResolvedProtocol = ProtocolName.IsNone() ? FExtendedSteamNetworkProtocolTypes::SteamP2P() : ProtocolName;
	return new FExtendedSteamSocket(SOCKTYPE_Streaming, SocketDescription, ResolvedProtocol, this);
}

void FExtendedSteamSocketsSubsystem::DestroySocket(FSocket* Socket)
{
	// FExtendedSteamSocket::~ closes the underlying connection / listen socket.
	delete Socket;
}

FAddressInfoResult FExtendedSteamSocketsSubsystem::GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName,
	EAddressInfoFlags QueryFlags, const FName ProtocolTypeName, ESocketType SocketType)
{
	// Honest no-op: ExtendedSteam peers are identified by SteamID, not by DNS-resolvable host names.
	// Callers that already hold a SteamID string should use GetAddressFromString instead. The result
	// carries the default SE_NO_DATA return code to signal "nothing resolved".
	return FAddressInfoResult(HostName, ServiceName);
}

TSharedPtr<FInternetAddr> FExtendedSteamSocketsSubsystem::GetAddressFromString(const FString& InAddress)
{
	TSharedRef<FInternetAddrExtendedSteam> Addr = MakeShared<FInternetAddrExtendedSteam>();
	bool bIsValid = false;
	Addr->SetIp(*InAddress, bIsValid);
	return bIsValid ? Addr : TSharedPtr<FInternetAddr>();
}

bool FExtendedSteamSocketsSubsystem::GetHostName(FString& HostName)
{
	// No meaningful host name for a SteamID-addressed transport.
	HostName = TEXT("ExtendedSteamSockets");
	return true;
}

TSharedRef<FInternetAddr> FExtendedSteamSocketsSubsystem::CreateInternetAddr()
{
	return MakeShared<FInternetAddrExtendedSteam>(FExtendedSteamNetworkProtocolTypes::SteamP2P());
}

TSharedRef<FInternetAddr> FExtendedSteamSocketsSubsystem::CreateInternetAddr(const FName ProtocolType)
{
	return MakeShared<FInternetAddrExtendedSteam>(ProtocolType);
}

TArray<TSharedRef<FInternetAddr>> FExtendedSteamSocketsSubsystem::GetLocalBindAddresses()
{
	// Report this machine's own Steam identity as the single bindable address, when the SDK can supply it.
	TArray<TSharedRef<FInternetAddr>> Addresses;
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamNetworkingSockets* Interface = SteamNetworkingSockets())
	{
		SteamNetworkingIdentity Identity;
		if (Interface->GetIdentity(&Identity))
		{
			TSharedRef<FInternetAddrExtendedSteam> Addr = MakeShared<FInternetAddrExtendedSteam>(FExtendedSteamNetworkProtocolTypes::SteamP2P());
			Addr->SetSteamID64(Identity.GetSteamID64());
			Addresses.Add(Addr);
		}
	}
#endif
	return Addresses;
}

const TCHAR* FExtendedSteamSocketsSubsystem::GetSocketAPIName() const
{
	return TEXT("ExtendedSteamSockets");
}

bool FExtendedSteamSocketsSubsystem::Tick(float DeltaTime)
{
	// Intentionally does NOT pump Steam callbacks: FExtendedSteamSharedModule runs SteamAPI_RunCallbacks
	// every frame, which delivers the SteamNetConnectionStatusChangedCallback_t events straight to
	// HandleConnectionStatusChanged (via FExtendedSteamSocketsEventManager). No per-tick work is needed.
	return true;
}

void FExtendedSteamSocketsSubsystem::RegisterHandle(uint32 Handle, UExtendedSteamSocketsNetDriver* Driver)
{
	if (Handle != 0 && Driver != nullptr)
	{
		HandleToNetDriver.Add(Handle, Driver);
	}
}

void FExtendedSteamSocketsSubsystem::UnregisterHandle(uint32 Handle)
{
	HandleToNetDriver.Remove(Handle);
}

UExtendedSteamSocketsNetDriver* FExtendedSteamSocketsSubsystem::FindNetDriverForHandle(uint32 Handle)
{
	if (TWeakObjectPtr<UExtendedSteamSocketsNetDriver>* Found = HandleToNetDriver.Find(Handle))
	{
		if (UExtendedSteamSocketsNetDriver* Driver = Found->Get())
		{
			return Driver;
		}
		// The driver was GC'd/torn down without unregistering; drop the stale entry.
		HandleToNetDriver.Remove(Handle);
	}
	return nullptr;
}

void FExtendedSteamSocketsSubsystem::HandleConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Message)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (Message == nullptr)
	{
		return;
	}

	const ESteamNetworkingConnectionState OldState = Message->m_eOldState;
	const ESteamNetworkingConnectionState NewState = Message->m_info.m_eState;

	// A brand-new incoming connection arrived on a listen socket (must be accepted or closed promptly).
	if (OldState == k_ESteamNetworkingConnectionState_None
		&& NewState == k_ESteamNetworkingConnectionState_Connecting
		&& Message->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid)
	{
		if (UExtendedSteamSocketsNetDriver* Driver = FindNetDriverForHandle(Message->m_info.m_hListenSocket))
		{
			Driver->OnConnectionCreated(Message->m_info.m_hListenSocket, Message->m_hConn);
		}
	}
	// A connection (client connect, or server-accepted) has finished connecting.
	else if ((OldState == k_ESteamNetworkingConnectionState_Connecting || OldState == k_ESteamNetworkingConnectionState_FindingRoute)
		&& NewState == k_ESteamNetworkingConnectionState_Connected)
	{
		if (UExtendedSteamSocketsNetDriver* Driver = FindNetDriverForHandle(Message->m_hConn))
		{
			Driver->OnConnectionUpdated(Message->m_hConn, static_cast<int32>(NewState));
		}
	}
	// A connection was closed by the peer or failed locally.
	else if ((OldState == k_ESteamNetworkingConnectionState_Connecting
			|| OldState == k_ESteamNetworkingConnectionState_FindingRoute
			|| OldState == k_ESteamNetworkingConnectionState_Connected)
		&& (NewState == k_ESteamNetworkingConnectionState_ClosedByPeer
			|| NewState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally))
	{
		if (UExtendedSteamSocketsNetDriver* Driver = FindNetDriverForHandle(Message->m_hConn))
		{
			Driver->OnConnectionDisconnected(Message->m_hConn);
		}
		// Stop routing this handle; the owning net connection / driver will CloseConnection on teardown.
		UnregisterHandle(Message->m_hConn);
	}
#endif
}

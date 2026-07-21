// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Shared identifiers for the ExtendedSteam SteamNetworkingSockets subsystem, net driver and
 * module. Kept in one Public header so the socket subsystem, the FInternetAddr implementation
 * and the net driver all agree on the registered name and protocol names.
 */

/**
 * Name this socket subsystem registers under with FSocketSubsystemModule and is looked up with
 * ISocketSubsystem::Get(). Mirrors the ESTEAM_SUBSYSTEM macro pattern used by the OSS module.
 */
#define EXTENDEDSTEAM_SOCKETS_SUBSYSTEM FName(TEXT("ExtendedSteamSockets"))

namespace FExtendedSteamNetworkProtocolTypes
{
	/**
	 * P2P addressing: the peer is identified by a Steam identity (CSteamID) plus a virtual port.
	 * Connections are established through the Steam Datagram Relay / NAT-punch backend.
	 */
	inline const FName& SteamP2P()
	{
		static const FName Name(TEXT("ExtendedSteamP2P"));
		return Name;
	}

	/**
	 * Direct IP addressing: the peer is identified by an ordinary IPv4/IPv6 address (SteamNetworkingIPAddr).
	 * Used for LAN / direct-connect style sessions over SteamNetworkingSockets.
	 */
	inline const FName& SteamIP()
	{
		static const FName Name(TEXT("ExtendedSteamIP"));
		return Name;
	}
}

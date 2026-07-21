// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * Plugin-specific Ping interface for the "EXTENDEDSTEAM" online subsystem.
 *
 * Not an engine IOnlineSubsystem interface (the framework declares no Ping getter): it is owned by
 * FOnlineSubsystemExtendedSteam and reached through GetPingInterfaceExtended(). It wraps
 * ISteamNetworkingUtils' Steam Datagram Relay (SDR) ping-estimation surface, letting matchmaking
 * rank sessions by estimated latency without any packets flowing between the two clients:
 *
 *  - InitRelayNetworkAccess kicks off the (async) download of the relay network config. Call it
 *    early; ping locations only become meaningful once it completes.
 *  - GetLocalPingLocation snapshots this host's position in the relay network as an opaque, portable
 *    string (advertise it in your session data).
 *  - EstimatePingBetweenLocations estimates round-trip time in milliseconds between two such strings
 *    entirely locally.
 *  - GetRelayNetworkStatus exposes the ESteamNetworkingAvailability of the relay config so callers
 *    can tell "not ready yet" from "ready".
 *
 * Every method is offline-safe: with the SDK compiled out or the Steam client down, InitRelayNetwork-
 * Access is a no-op, GetLocalPingLocation returns a negative age and an empty string,
 * EstimatePingBetweenLocations returns the Steam "failed" sentinel (-1), and GetRelayNetworkStatus
 * reports unavailable. No callbacks and no Tick — all calls are synchronous.
 */
class FOnlinePingExtendedSteam
{
public:
	/** Steam's "ping estimate failed / unavailable" sentinel (k_nSteamNetworkingPing_Failed). */
	static constexpr int32 PingFailed = -1;

	explicit FOnlinePingExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlinePingExtendedSteam() = default;

	FOnlinePingExtendedSteam(const FOnlinePingExtendedSteam&) = delete;
	FOnlinePingExtendedSteam& operator=(const FOnlinePingExtendedSteam&) = delete;

	/** Begins downloading the relay network config (SteamNetworkingUtils::InitRelayNetworkAccess). No-op offline. */
	void InitRelayNetworkAccess();

	/**
	 * Snapshots this host's ping location as a portable string.
	 * @param OutSerialized filled with the serialized location on success (emptied on failure).
	 * @return age of the measurement in seconds (>= 0) when a location was produced; a negative value
	 *         when unavailable (Steam down or relay config not ready yet).
	 */
	float GetLocalPingLocation(FString& OutSerialized) const;

	/**
	 * Estimates round-trip time in milliseconds between two serialized ping locations, locally.
	 * @return the estimate in ms, or PingFailed when either string cannot be parsed or Steam is down.
	 */
	int32 EstimatePingBetweenLocations(const FString& LocationA, const FString& LocationB) const;

	/**
	 * Relay-network readiness surface.
	 * @return the ESteamNetworkingAvailability as an int32 (k_ESteamNetworkingAvailability_Current == 100
	 *         means ready); k_ESteamNetworkingAvailability_CannotTry (-102) when Steam is unavailable.
	 */
	int32 GetRelayNetworkStatus() const;

	/** True while the Steam client API is up and the networking-utils accessor is available. */
	bool IsSteamAvailableForPing() const;

private:
	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;
};

typedef TSharedPtr<FOnlinePingExtendedSteam, ESPMode::ThreadSafe> FOnlinePingExtendedSteamPtr;

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamNetworkingUtilsSubsystem.generated.h"

/** Simplified Steam Datagram Relay network status (maps ESteamNetworkingAvailability). */
UENUM(BlueprintType)
enum class EESteamRelayNetworkStatus : uint8
{
	/** Never tried, not applicable, or Steam unavailable. */
	Unknown,
	/** Failed, or cannot even try (e.g. no Internet / no network config). */
	Broken,
	/** Worked previously but currently has a problem (includes retrying after a failure). */
	Problem,
	/** Waiting on a dependency or actively attempting to connect. */
	Attempting,
	/** Relay network is online and available. */
	Available
};

/**
 * Wraps ISteamNetworkingUtils: Steam Datagram Relay access, ping-location estimation,
 * point-of-presence (POP / data center) enumeration and global config values.
 *
 * Out of scope (use the SDK directly when needed): message allocation, the debug output
 * hook, Fake IP helpers and per-connection/listen-socket config scopes.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamNetworkingUtilsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Starts initializing access to the Steam relay network (idempotent; also retriggered by
	 * any relay operation). Call at startup when P2P connections are anticipated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	void InitRelayNetworkAccess();

	/** Current relay network status, simplified. */
	UFUNCTION(BlueprintPure, Category = "Steam|NetworkingUtils")
	EESteamRelayNetworkStatus GetRelayNetworkStatusSimple() const;

	/**
	 * Detailed relay network status. Returns the summary status and additionally outputs the
	 * English diagnostic message and the two prerequisite availabilities (network config and
	 * any-relay) as raw ESteamNetworkingAvailability ints for diagnostics/UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	EESteamRelayNetworkStatus GetRelayNetworkStatusDetailed(FString& OutDebugMsg, int32& OutAvailNetworkConfig, int32& OutAvailAnyRelay) const;

	/** True once ping data has been measured and is no more than MaxAgeSeconds old. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	bool CheckPingDataUpToDate(float MaxAgeSeconds);

	/**
	 * A monotonically increasing local timestamp in microseconds. Only meaningful within this
	 * process run; subtract two values to measure elapsed time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int64 GetLocalTimestamp() const;

	/**
	 * The local host's serialized ping location (safe to send over the wire; do not parse yourself).
	 * Returns the approximate age of the data in seconds, or -1 when no data is available yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	float GetLocalPingLocation(FString& OutSerializedLocation) const;

	/**
	 * Validates and normalizes a serialized ping location string (round-trips it through the
	 * SDK). Returns false when the string cannot be parsed; OutNormalized holds the canonical form.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	bool ParsePingLocationString(const FString& SerializedLocation, FString& OutNormalized) const;

	/**
	 * Estimated round-trip latency in milliseconds between two serialized ping locations
	 * (from GetLocalPingLocation on each host). Returns -1 when either location fails to
	 * parse or the estimate fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 EstimatePingBetweenTwoLocations(const FString& LocationA, const FString& LocationB) const;

	/**
	 * Estimated round-trip latency in milliseconds from the local host to a serialized ping
	 * location. Returns -1 when the location fails to parse or the estimate fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 EstimatePingTimeFromLocalHost(const FString& SerializedLocation) const;

	// ---- Points of presence (data centers) ----

	/** Number of network points of presence in the current config (0 when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 GetPOPCount() const;

	/** Lists all POP ids (SteamNetworkingPOPID values). Returns the number written to OutPOPIds. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 GetPOPList(TArray<int32>& OutPOPIds) const;

	/**
	 * Best relayed-route ping time in milliseconds from this host to a data center POP, and the
	 * relay POP it routes through. Returns -1 when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 GetPingToDataCenter(int32 POPId, int32& OutViaRelayPOP) const;

	/** Direct ping time in milliseconds to the relays at a data center POP (-1 when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils")
	int32 GetDirectPingToPOP(int32 POPId) const;

	// ---- Global config values ----
	// ConfigValue is an ESteamNetworkingConfigValue id; pass the numeric value of the desired
	// option (see steamnetworkingtypes.h). These operate at global scope only.

	/** Sets a global int32 config value. Returns false when the value/type is rejected. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool SetGlobalConfigValueInt32(int32 ConfigValue, int32 Value);

	/** Sets a global float config value. Returns false when the value/type is rejected. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool SetGlobalConfigValueFloat(int32 ConfigValue, float Value);

	/** Sets a global string config value. Returns false when the value/type is rejected. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool SetGlobalConfigValueString(int32 ConfigValue, const FString& Value);

	/** Reads a global int32 config value. Returns false when it does not exist or is another type. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool GetGlobalConfigValueInt32(int32 ConfigValue, int32& OutValue) const;

	/** Reads a global float config value. Returns false when it does not exist or is another type. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool GetGlobalConfigValueFloat(int32 ConfigValue, float& OutValue) const;

	/** Reads a global string config value. Returns false when it does not exist or is another type. */
	UFUNCTION(BlueprintCallable, Category = "Steam|NetworkingUtils|Config")
	bool GetGlobalConfigValueString(int32 ConfigValue, FString& OutValue) const;
};

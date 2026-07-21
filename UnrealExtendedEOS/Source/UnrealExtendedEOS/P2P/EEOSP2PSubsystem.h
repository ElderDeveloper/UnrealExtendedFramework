// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Containers/Ticker.h"
#include "EEOSP2PSubsystem.generated.h"

// Forward declare EOS SDK product user ID (matches eos_common.h)
typedef struct EOS_ProductUserIdDetails* EOS_ProductUserId;

UENUM(BlueprintType)
enum class EEOSNATType : uint8
{
	Unknown,
	Open,
	Moderate,
	Strict
};

UENUM(BlueprintType)
enum class EEOSPacketReliability : uint8
{
	UnreliableUnordered		UMETA(DisplayName = "Unreliable Unordered"),
	UnreliableOrdered		UMETA(DisplayName = "Unreliable Ordered"),
	ReliableUnordered		UMETA(DisplayName = "Reliable Unordered"),
	ReliableOrdered			UMETA(DisplayName = "Reliable Ordered")
};

UENUM(BlueprintType)
enum class EEOSRelayControl : uint8
{
	NoRelays		UMETA(DisplayName = "No Relays (Direct Only)"),
	AllowRelays		UMETA(DisplayName = "Allow Relays"),
	ForceRelays		UMETA(DisplayName = "Force Relays")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSPacketReceived, const FString&, RemoteUserId, int32, Channel, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectionRequest, const FString&, RemoteUserId, const FString&, SocketName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectionClosed, const FString&, RemoteUserId, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSNATTypeQueried, EEOSNATType, NATType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectionEstablished, const FString&, RemoteUserId, const FString&, SocketName);

/**
 * Manages peer-to-peer connections through EOS relay and NAT traversal.
 * Supports multiple socket IDs, packet reliability modes, and relay control.
 *
 * All P2P SDK calls require a logged-in EOS Connect user (local Product User ID).
 * Notification registration and packet receiving therefore initialize lazily:
 * the receive ticker retries every tick until Connect login completes.
 *
 * IDENTITY REBIND: the EOS P2P notifications are per-local-user filtered, so a registration
 * is only valid for the (platform, PUID) pair it was made with. The ticker re-checks both
 * every tick: on Connect logout, re-login as a different user, or platform churn, it tears
 * down (synthesizing one OnConnectionClosed per tracked peer — the SDK can no longer deliver
 * those closes), clears peer state, and lazily re-registers with the new identity.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSP2PSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Sending ──────────────────────────────────────────────────────────────

	/**
	 * Send a raw byte packet to a remote user.
	 * Uses the socket the peer was accepted/established on, falling back to the current
	 * outgoing socket for peers we initiated to.
	 * @return true if the SDK accepted the packet. Returns false (with a warning log) when
	 * the payload exceeds EOS_P2P_MAX_PACKET_SIZE (1170 bytes), Channel is outside 0-255,
	 * no Connect user is logged in, the remote ID is invalid, or the SDK call failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	bool SendPacket(const FString& RemoteUserId, const TArray<uint8>& Data, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	/** Send a string packet to a remote user (auto-converts to bytes). Same return semantics as SendPacket. */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	bool SendStringPacket(const FString& RemoteUserId, const FString& Message, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	/** Broadcast a packet to all connected peers */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void BroadcastPacket(const TArray<uint8>& Data, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	// ── Connection Management ────────────────────────────────────────────────

	/**
	 * Accept an incoming P2P connection request.
	 * SocketName must be the socket name delivered by OnConnectionRequest — the accept only
	 * matches the pending request when the socket IDs are identical.
	 * @return true if the SDK accepted the request. False when rejected or failed to start:
	 * EOS unavailable, no logged-in Connect user, invalid remote ID, or the SDK call failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	bool AcceptConnection(const FString& RemoteUserId, const FString& SocketName);

	/**
	 * Close a P2P connection to a specific user.
	 * Leave SocketName empty to close the socket tracked for that peer (from accept/established),
	 * falling back to the current outgoing socket.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void CloseConnection(const FString& RemoteUserId, const FString& SocketName = TEXT(""));

	/** Close all P2P connections */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void CloseAllConnections();

	// ── Relay & NAT ──────────────────────────────────────────────────────────

	/** Set the relay control mode for P2P connections */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void SetRelayControl(EEOSRelayControl RelayMode);

	/** Get the current relay control setting */
	UFUNCTION(BlueprintPure, Category = "EOS|P2P")
	EEOSRelayControl GetRelayControl() const;

	/**
	 * Query the local NAT type (async — the result arrives on OnNATTypeQueried).
	 * @return true if the query was started (OnNATTypeQueried will broadcast the result).
	 * False if it could not start (EOS/platform unavailable) — a false return broadcasts
	 * NOTHING: failure-to-start is reported by the return value only, so listeners waiting
	 * on OnNATTypeQueried for a legitimately started query never receive a foreign failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	bool QueryNATType();

	/** Set the port range for P2P connections */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void SetPortRange(int32 Port, int32 MaxAdditionalPorts = 10);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the last queried NAT type */
	UFUNCTION(BlueprintPure, Category = "EOS|P2P")
	EEOSNATType GetNATType() const;

	/** Get list of all connected peer user IDs */
	UFUNCTION(BlueprintPure, Category = "EOS|P2P")
	TArray<FString> GetConnectedPeers() const;

	/** Check if connected to a specific peer */
	UFUNCTION(BlueprintPure, Category = "EOS|P2P")
	bool IsConnectedToPeer(const FString& RemoteUserId) const;

	/** Get the connection count */
	UFUNCTION(BlueprintPure, Category = "EOS|P2P")
	int32 GetConnectionCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSPacketReceived OnPacketReceived;

	/** Incoming connection request. Pass the delivered SocketName to AcceptConnection. */
	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionRequest OnConnectionRequest;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionClosed OnConnectionClosed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSNATTypeQueried OnNATTypeQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionEstablished OnConnectionEstablished;

private:

	/** Resolve the local Product User ID of the first logged-in Connect user (index 0) on the
	 *  given platform. Returns nullptr while no Connect user is logged in (or Platform is null). */
	static EOS_ProductUserId GetLocalProductUserId(EOS_HPlatform PlatformHandle);

	/** Platform lookup WITHOUT the Shared base's unconditional not-available warning —
	 *  the lazy-init/receive ticker runs every frame and must not spam while waiting. */
	static EOS_HPlatform GetPlatformHandleQuiet();

	/** True if the given platform handle is still among the SDK manager's active platforms. */
	static bool IsPlatformStillActive(EOS_HPlatform PlatformHandle);

	/** Lazy init: registers the P2P notifications and applies the default relay mode
	 *  once the EOS platform and a logged-in Connect user are both available.
	 *  Returns true once initialization has completed (now or previously). Re-runs after
	 *  TeardownNotifications so a new login (possibly a different PUID) re-registers. */
	bool TryLazyInitNotifications();

	/** Tear down everything TryLazyInitNotifications set up: remove the notifications from the
	 *  platform they were REGISTERED on (RegisteredPlatform — never the current
	 *  GetPlatformHandle(), which may be a different platform under churn/multi-instance PIE),
	 *  free or intentionally leak the notify context, synthesize one OnConnectionClosed per
	 *  tracked peer (CloseReason), clear peer state, and reset bNotificationsInitialized so
	 *  lazy init can re-register. Safe to call when never initialized (no-op). */
	void TeardownNotifications(const FString& CloseReason);

	EEOSNATType CachedNATType = EEOSNATType::Unknown;
	EEOSRelayControl CurrentRelayControl = EEOSRelayControl::AllowRelays;
	TArray<FString> ConnectedPeers;

	/** Current socket name for outgoing connections we initiate */
	FString CurrentSocketName = TEXT("Default");

	/** Socket name each peer's connection runs on (populated on accept and on ConnectionEstablished) */
	TMap<FString /*PeerId*/, FString /*SocketName*/> PeerSockets;

	/** Whether lazy initialization (notification registration + default relay mode) has completed */
	bool bNotificationsInitialized = false;

	/** The Product User ID the notifications were registered with. The EOS P2P notifications are
	 *  per-local-user filtered, so they die silently when the Connect identity changes (logout,
	 *  re-login as a different user, device-id transfer). The receive ticker compares this against
	 *  the CURRENT local PUID every tick and tears down / re-registers on mismatch. */
	EOS_ProductUserId RegisteredLocalUserId = nullptr;

	/** The EOS platform handle the notifications were registered on. RemoveNotify calls MUST run
	 *  against this exact handle: GetPlatformHandle() returns ActivePlatforms[0] *at call time*,
	 *  which under platform churn / multi-instance PIE can be a different platform — removal there
	 *  would no-op while the original platform still references the notify context. */
	EOS_HPlatform RegisteredPlatform = nullptr;

	/** EOS notification IDs for cleanup */
	uint64 ConnectionRequestNotifId = 0;
	uint64 ConnectionClosedNotifId = 0;
	uint64 ConnectionEstablishedNotifId = 0;

	/** Heap context passed as ClientData to the persistent EOS P2P notifications (holds a weak self pointer) */
	struct FEEOSP2PNotifyContext* NotifyContext = nullptr;

	/** Ticker handle for the lazy-init retry + receive poll loop */
	FTSTicker::FDelegateHandle ReceiveTickerHandle;

	/** Called every tick: retries lazy init until ready, then drains incoming EOS P2P packets */
	bool PollIncomingPackets(float DeltaTime);
};

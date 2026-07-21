// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamNetworkingSubsystem.generated.h"

/** How a P2P packet is delivered (mirrors Steamworks EP2PSend). */
UENUM(BlueprintType)
enum class EESteamP2PSendType : uint8
{
	/** Basic UDP send, max 1200 bytes. Can be lost or arrive out of order. */
	Unreliable,
	/** As Unreliable, but dropped instead of buffered while the connection is being established. Ideal for voice payloads. */
	UnreliableNoDelay,
	/** Reliable, ordered message send with fragmentation/re-assembly; up to 1MB per message. */
	Reliable,
	/** As Reliable, with Nagle-style coalescing of small messages (~200ms or MTU). */
	ReliableWithBuffering
};

/** Reason a legacy P2P session is failing (mirrors EP2PSessionError). */
UENUM(BlueprintType)
enum class EESteamP2PSessionError : uint8
{
	/** No error. */
	None,
	/** Target is not running the same app (historical; Valve removed this code). */
	NotRunningApp,
	/** Local user doesn't own the app that is running. */
	NoRightsToApp,
	/** Target is not logged into Steam (historical; Valve removed this code). */
	DestinationNotLoggedIn,
	/** Target isn't responding — perhaps it never called AcceptP2PSessionWithUser. */
	Timeout
};

/** Snapshot of the underlying connection to a P2P peer (mirrors P2PSessionState_t; debugging aid). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamP2PSessionState
{
	GENERATED_BODY()

	/** True when an open connection to the peer exists. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	bool bConnectionActive = false;

	/** True while a connection is being established. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	bool bConnecting = false;

	/** Last error recorded on this session. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	EESteamP2PSessionError P2PSessionError = EESteamP2PSessionError::None;

	/** True when traffic is going through a Steam relay (TURN) server. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	bool bUsingRelay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	int32 BytesQueuedForSend = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	int32 PacketsQueuedForSend = 0;

	/** Potential IP of the remote host (dotted quad); could be a relay server. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	FString RemoteIP;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Networking")
	int32 RemotePort = 0;
};

/** Fired for every P2P packet read from a polled channel. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamP2PPacketReceived, FESteamId, Sender, const TArray<uint8>&, Data, int32, Channel);

/**
 * Fired when a user wants to open a P2P session with us (first packet from a peer we have
 * not talked to yet). Call AcceptP2PSessionWithUser to accept and receive the data —
 * ignoring the request rejects it (the peer's retries re-fire this periodically).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamP2PSessionRequested, FESteamId, Requester);

/** Fired when packets cannot reach a peer; queued packets are dropped. ErrorCode mirrors EP2PSessionError. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamP2PSessionConnectFailed, FESteamId, User, int32, ErrorCode);

/**
 * Wraps the legacy ISteamNetworking UDP-style P2P API: connectionless packet exchange
 * keyed by Steam id, with automatic NAT traversal and Steam relay fallback.
 *
 * NOTE: Valve has deprecated ISteamNetworking and may remove it from a future SDK;
 * new code should prefer ISteamNetworkingSockets / ISteamNetworkingMessages. Wrapped
 * here for full API coverage and for titles that still rely on it.
 *
 * While the Steam client is up, a core ticker drains incoming packets on channels
 * 0..PolledChannelCount-1 every frame and broadcasts them on OnP2PPacketReceived.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamNetworkingSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Sending ----

	/**
	 * Sends a P2P packet to a user. Unreliable sends are capped at 1200 bytes, reliable at 1MB.
	 * The first packet may be delayed while NAT traversal runs; delivery failures arrive on
	 * OnP2PSessionConnectFailed. The receiver must read with the same channel number.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool SendP2PPacket(FESteamId Target, const TArray<uint8>& Data, EESteamP2PSendType SendType, int32 Channel = 0);

	// ---- Receiving ----

	/**
	 * Number of channels the receive ticker polls each frame (channels 0..Count-1).
	 * Clamped to at least 1. Default 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	void SetPolledChannelCount(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Steam|Networking")
	int32 GetPolledChannelCount() const { return PolledChannelCount; }

	// ---- Sessions ----

	/**
	 * Accepts a P2P session in response to OnP2PSessionRequested. Only needed for peers
	 * we have not sent to yet — sending to a user implicitly accepts their request.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool AcceptP2PSessionWithUser(FESteamId User);

	/** Closes the P2P session with a user and frees its resources. New data from the user re-fires OnP2PSessionRequested. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool CloseP2PSessionWithUser(FESteamId User);

	/** Closes one channel to a user; once all open channels are closed the session itself closes. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool CloseP2PChannelWithUser(FESteamId User, int32 Channel);

	/**
	 * Allows or disallows falling back to Steam relay servers when a direct connection fails
	 * (allowed by default; applies to connections created after the call).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool AllowP2PPacketRelay(bool bAllow);

	/** Fills OutState with connection details for a peer. Returns false when no connection exists. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Networking")
	bool GetP2PSessionState(FESteamId User, FESteamP2PSessionState& OutState);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Networking")
	FOnSteamP2PPacketReceived OnP2PPacketReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Networking")
	FOnSteamP2PSessionRequested OnP2PSessionRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Networking")
	FOnSteamP2PSessionConnectFailed OnP2PSessionConnectFailed;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	/** Core ticker callback draining incoming P2P packets on the polled channels. */
	bool TickPollPackets(float DeltaTime);

	void RemovePollTicker();

	friend class FESteamNetworkingCallbacks;
	TSharedPtr<class FESteamNetworkingCallbacks> Callbacks;

	FTSTicker::FDelegateHandle PollTickerHandle;
	int32 PolledChannelCount = 1;
};

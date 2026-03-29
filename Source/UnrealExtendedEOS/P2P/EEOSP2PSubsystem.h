// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "Containers/Ticker.h"
#include "EEOSP2PSubsystem.generated.h"

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSConnectionRequest, const FString&, RemoteUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectionClosed, const FString&, RemoteUserId, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSNATTypeQueried, EEOSNATType, NATType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSConnectionEstablished, const FString&, RemoteUserId, const FString&, SocketName);

/**
 * Manages peer-to-peer connections through EOS relay and NAT traversal.
 * Supports multiple socket IDs, packet reliability modes, and relay control.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSP2PSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Sending ──────────────────────────────────────────────────────────────

	/** Send a raw byte packet to a remote user */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void SendPacket(const FString& RemoteUserId, const TArray<uint8>& Data, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	/** Send a string packet to a remote user (auto-converts to bytes) */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void SendStringPacket(const FString& RemoteUserId, const FString& Message, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	/** Broadcast a packet to all connected peers */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void BroadcastPacket(const TArray<uint8>& Data, int32 Channel = 0, EEOSPacketReliability Reliability = EEOSPacketReliability::ReliableOrdered);

	// ── Connection Management ────────────────────────────────────────────────

	/** Accept an incoming P2P connection request */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void AcceptConnection(const FString& RemoteUserId, const FString& SocketName = TEXT("Default"));

	/** Close a P2P connection to a specific user */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void CloseConnection(const FString& RemoteUserId, const FString& SocketName = TEXT("Default"));

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

	/** Query the local NAT type */
	UFUNCTION(BlueprintCallable, Category = "EOS|P2P")
	void QueryNATType();

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

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionRequest OnConnectionRequest;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionClosed OnConnectionClosed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSNATTypeQueried OnNATTypeQueried;

	UPROPERTY(BlueprintAssignable, Category = "EOS|P2P")
	FOnEOSConnectionEstablished OnConnectionEstablished;

private:

	EEOSNATType CachedNATType = EEOSNATType::Unknown;
	EEOSRelayControl CurrentRelayControl = EEOSRelayControl::AllowRelays;
	TArray<FString> ConnectedPeers;

	/** Current socket name for outgoing connections */
	FString CurrentSocketName = TEXT("Default");

	/** EOS notification IDs for cleanup */
	uint64 ConnectionRequestNotifId = 0;
	uint64 ConnectionClosedNotifId = 0;
	uint64 ConnectionEstablishedNotifId = 0;

	/** Ticker handle for the receive poll loop */
	FTSTicker::FDelegateHandle ReceiveTickerHandle;

	/** Called every tick to drain incoming EOS P2P packets */
	bool PollIncomingPackets(float DeltaTime);
};


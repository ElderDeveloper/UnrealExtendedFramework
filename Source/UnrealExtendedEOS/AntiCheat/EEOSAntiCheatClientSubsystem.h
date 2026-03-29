// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAntiCheatSubsystem.h"
#include "EEOSAntiCheatClientSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSACClientActionRequired, EEOSAntiCheatAction, Action, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSACClientIntegrityViolated, const FString&, ViolationMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSACClientPeerAuthChanged, const FString&, PeerId, bool, bAuthenticated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSACClientMessageToServer, const TArray<uint8>&, MessageData);

/**
 * Client-side EOS Anti-Cheat protection.
 * Manages client anti-cheat sessions, integrity monitoring, and P2P peer management.
 * Uses EOS_AntiCheatClient_* SDK functions.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatClientSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Session Management ───────────────────────────────────────────────────

	/** Begin a client-side anti-cheat session for the current multiplayer game */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void BeginSession();

	/** End the client-side anti-cheat session */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void EndSession();

	// ── Message Passing ──────────────────────────────────────────────────────

	/** Forward a message received from the server to the anti-cheat client */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void ReceiveMessageFromServer(const TArray<uint8>& MessageData);

	/** Forward a message received from a P2P peer to the anti-cheat client */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void ReceiveMessageFromPeer(const FString& PeerId, const TArray<uint8>& MessageData);

	// ── P2P Peer Management ──────────────────────────────────────────────────

	/** Register a P2P peer (for peer-to-peer anti-cheat mode) */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void RegisterPeer(const FString& PeerId);

	/** Unregister a P2P peer */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client")
	void UnregisterPeer(const FString& PeerId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if client anti-cheat session is active */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Client")
	bool IsSessionActive() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientActionRequired OnClientActionRequired;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientIntegrityViolated OnIntegrityViolated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientPeerAuthChanged OnPeerAuthStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientMessageToServer OnMessageToServer;

private:

	bool bSessionActive = false;

#if WITH_EOS_SDK
	uint64 NotifyMessageToServerId = 0;
	uint64 NotifyIntegrityViolatedId = 0;
	uint64 NotifyPeerAuthChangedId = 0;
	uint64 NotifyPeerActionRequiredId = 0;
#endif
};

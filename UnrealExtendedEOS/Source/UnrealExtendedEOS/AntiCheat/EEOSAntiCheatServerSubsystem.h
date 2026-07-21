// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAntiCheatSubsystem.h"
#include "EEOSAntiCheatServerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSACServerClientAction, const FString&, ClientId, EEOSAntiCheatAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSACServerClientAuthChanged, const FString&, ClientId, bool, bAuthenticated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSACServerMessageToClient, const FString&, ClientId, const TArray<uint8>&, MessageData);

/**
 * DEPRECATED — use UEEOSAntiCheatSubsystem instead.
 *
 * This subsystem is a non-functional shim kept only so existing Blueprint
 * references keep loading. Its previous implementation was broken by design:
 * its ClientHandle bookkeeping parsed Product User ID strings with Atoi64
 * (PUIDs are hex — every id collapsed to the same handle and could never be
 * round-tripped back), it registered clients even when the SDK rejected them,
 * and it reported an ACTIVE session when EOS_AntiCheatServer_BeginSession
 * failed (fail-open).
 *
 * This game is listen-server co-op with NO dedicated servers, so there is no
 * EOS_AntiCheatServer deployment to talk to; the correct integration is the
 * peer-to-peer client mode in UEEOSAntiCheatSubsystem (see Docs/11_AntiCheat.md).
 * If dedicated servers ever ship, a server subsystem must be rebuilt from
 * scratch against eos_anticheatserver.h with proper handle bookkeeping.
 *
 * All methods now log an error and fail closed: no SDK calls are made,
 * IsSessionActive() always returns false, queries return empty, and none of
 * the delegates ever fire.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatServerSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	// ── Session Management ───────────────────────────────────────────────────

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::BeginSession. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::BeginSession — this subsystem is a non-functional shim."))
	void BeginSession();

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::EndSession. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::EndSession — this subsystem is a non-functional shim."))
	void EndSession();

	// ── Client Management ────────────────────────────────────────────────────

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::RegisterPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::RegisterPeer — this subsystem is a non-functional shim."))
	void RegisterClient(const FString& ClientId);

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::UnregisterPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::UnregisterPeer — this subsystem is a non-functional shim."))
	void UnregisterClient(const FString& ClientId);

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::UnregisterAllPeers. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::UnregisterAllPeers — this subsystem is a non-functional shim."))
	void UnregisterAllClients();

	// ── Message Passing ──────────────────────────────────────────────────────

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer — this subsystem is a non-functional shim."))
	void ReceiveMessageFromClient(const FString& ClientId, const TArray<uint8>& MessageData);

	// ── Cerberus Gameplay Events ─────────────────────────────────────────────

	/** DEPRECATED no-op — gameplay-data logging requires a real EOS_AntiCheatServer deployment. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Non-functional shim — gameplay-data logging requires a dedicated-server EOS_AntiCheatServer integration."))
	void SetGameSessionId(const FString& SessionId);

	/** DEPRECATED no-op — gameplay-data logging requires a real EOS_AntiCheatServer deployment. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Non-functional shim — gameplay-data logging requires a dedicated-server EOS_AntiCheatServer integration."))
	void LogPlayerSpawn(const FString& ClientId);

	/** DEPRECATED no-op — gameplay-data logging requires a real EOS_AntiCheatServer deployment. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Non-functional shim — gameplay-data logging requires a dedicated-server EOS_AntiCheatServer integration."))
	void LogPlayerDespawn(const FString& ClientId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** DEPRECATED — always returns false (fail-closed shim). Use UEEOSAntiCheatSubsystem::IsSessionActive. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::IsSessionActive — this subsystem is a non-functional shim."))
	bool IsSessionActive() const;

	/** DEPRECATED — always returns empty. Use UEEOSAntiCheatSubsystem::GetRegisteredPeers. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::GetRegisteredPeers — this subsystem is a non-functional shim."))
	TArray<FString> GetRegisteredClients() const;

	/** DEPRECATED — always returns 0. Use UEEOSAntiCheatSubsystem::GetRegisteredPeerCount. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::GetRegisteredPeerCount — this subsystem is a non-functional shim."))
	int32 GetRegisteredClientCount() const;

	// ── Delegates (kept for Blueprint compatibility — they never fire) ───────

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerClientAction OnClientActionRequired;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerClientAuthChanged OnClientAuthStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerMessageToClient OnMessageToClient;
};

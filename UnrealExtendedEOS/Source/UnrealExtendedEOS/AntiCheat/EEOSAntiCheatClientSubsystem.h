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
 * DEPRECATED — use UEEOSAntiCheatSubsystem instead.
 *
 * This subsystem is a non-functional shim kept only so existing Blueprint
 * references keep loading. Its previous implementation was broken by design:
 * it began sessions in EOS_ACCM_ClientServer mode while registering
 * peer-to-peer-only notifications and exposing peer-to-peer-only APIs that the
 * SDK rejects with EOS_AntiCheat_InvalidMode, and it reported an ACTIVE session
 * even when EOS_AntiCheatClient_BeginSession failed (fail-open).
 *
 * All methods now log an error and fail closed: no SDK calls are made,
 * IsSessionActive() always returns false, and none of the delegates ever fire.
 * The functional peer-to-peer implementation for this game's listen-server
 * co-op lives in UEEOSAntiCheatSubsystem (see Docs/11_AntiCheat.md).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatClientSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	// ── Session Management ───────────────────────────────────────────────────

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::BeginSession. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::BeginSession — this subsystem is a non-functional shim."))
	void BeginSession();

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::EndSession. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::EndSession — this subsystem is a non-functional shim."))
	void EndSession();

	// ── Message Passing ──────────────────────────────────────────────────────

	/** DEPRECATED no-op — there is no game server in this project's listen-server P2P design. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer — this subsystem is a non-functional shim."))
	void ReceiveMessageFromServer(const TArray<uint8>& MessageData);

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::ReceiveMessageFromPeer — this subsystem is a non-functional shim."))
	void ReceiveMessageFromPeer(const FString& PeerId, const TArray<uint8>& MessageData);

	// ── P2P Peer Management ──────────────────────────────────────────────────

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::RegisterPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::RegisterPeer — this subsystem is a non-functional shim."))
	void RegisterPeer(const FString& PeerId);

	/** DEPRECATED no-op — use UEEOSAntiCheatSubsystem::UnregisterPeer. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::UnregisterPeer — this subsystem is a non-functional shim."))
	void UnregisterPeer(const FString& PeerId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** DEPRECATED — always returns false (fail-closed shim). Use UEEOSAntiCheatSubsystem::IsSessionActive. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Client",
		meta = (DeprecatedFunction, DeprecationMessage = "Use UEEOSAntiCheatSubsystem::IsSessionActive — this subsystem is a non-functional shim."))
	bool IsSessionActive() const;

	// ── Delegates (kept for Blueprint compatibility — they never fire) ───────

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientActionRequired OnClientActionRequired;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientIntegrityViolated OnIntegrityViolated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientPeerAuthChanged OnPeerAuthStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Client")
	FOnEOSACClientMessageToServer OnMessageToServer;
};

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
 * Server-side EOS Anti-Cheat protection.
 * Manages server anti-cheat sessions, registered clients, Cerberus gameplay events.
 * Uses EOS_AntiCheatServer_* SDK functions.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatServerSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Session Management ───────────────────────────────────────────────────

	/** Begin a server-side anti-cheat session */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void BeginSession();

	/** End the server-side anti-cheat session */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void EndSession();

	// ── Client Management ────────────────────────────────────────────────────

	/** Register a connected client for anti-cheat monitoring */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void RegisterClient(const FString& ClientId);

	/** Unregister a disconnected client */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void UnregisterClient(const FString& ClientId);

	/** Unregister all clients */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void UnregisterAllClients();

	// ── Message Passing ──────────────────────────────────────────────────────

	/** Forward a message received from a client to the anti-cheat server */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void ReceiveMessageFromClient(const FString& ClientId, const TArray<uint8>& MessageData);

	// ── Cerberus Gameplay Events ─────────────────────────────────────────────

	/** Set the game session ID for analytics */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void SetGameSessionId(const FString& SessionId);

	/** Log a player spawn event */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void LogPlayerSpawn(const FString& ClientId);

	/** Log a player despawn/death event */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat|Server")
	void LogPlayerDespawn(const FString& ClientId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if server anti-cheat session is active */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server")
	bool IsSessionActive() const;

	/** Get registered client IDs */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server")
	TArray<FString> GetRegisteredClients() const;

	/** Get registered client count */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat|Server")
	int32 GetRegisteredClientCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerClientAction OnClientActionRequired;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerClientAuthChanged OnClientAuthStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat|Server")
	FOnEOSACServerMessageToClient OnMessageToClient;

private:

	bool bSessionActive = false;
	TArray<FString> RegisteredClients;

#if WITH_EOS_SDK
	uint64 NotifyMsgToClientId = 0;
	uint64 NotifyClientActionId = 0;
	uint64 NotifyClientAuthId = 0;
#endif
};

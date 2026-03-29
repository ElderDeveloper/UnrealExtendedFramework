// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAntiCheatSubsystem.generated.h"

UENUM(BlueprintType)
enum class EEOSAntiCheatAction : uint8
{
	None,
	RemovePlayer,
	Invalid
};

UENUM(BlueprintType)
enum class EEOSAntiCheatViolationType : uint8
{
	None				UMETA(DisplayName = "None"),
	SpeedHack			UMETA(DisplayName = "Speed Hack"),
	AimBot				UMETA(DisplayName = "Aim Bot"),
	WallHack			UMETA(DisplayName = "Wall Hack"),
	Teleport			UMETA(DisplayName = "Teleport"),
	DamageHack			UMETA(DisplayName = "Damage Hack"),
	ResourceHack		UMETA(DisplayName = "Resource Hack"),
	CustomViolation		UMETA(DisplayName = "Custom Violation")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAntiCheatClientAction, EEOSAntiCheatAction, Action, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAntiCheatPeerStatus, const FString&, PeerId, bool, bAuthenticated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSAntiCheatViolation, const FString&, PlayerId, EEOSAntiCheatViolationType, ViolationType, const FString&, Details);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSAntiCheatIntegrityChanged, bool, bIntegrityValid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSAntiCheatPlayerActionReported, const FString&, PlayerId, const FString&, ActionType, const FString&, ActionData);

/**
 * Manages EOS Anti-Cheat for client and server-side protection.
 * Tracks sessions, peers, and player action integrity.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Session Management ───────────────────────────────────────────────────

	/** Begin an anti-cheat session (call on game start) */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void BeginSession();

	/** End the anti-cheat session (call on game end) */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void EndSession();

	// ── Peer Management (Server) ─────────────────────────────────────────────

	/** Register a peer for server-side anti-cheat */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void RegisterPeer(const FString& PeerId);

	/** Unregister a peer */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void UnregisterPeer(const FString& PeerId);

	/** Unregister all peers */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void UnregisterAllPeers();

	// ── Player Action Reporting ──────────────────────────────────────────────

	/** Report a player action for server validation (e.g., movement, damage dealt) */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void ReportPlayerAction(const FString& PlayerId, const FString& ActionType, const FString& ActionData);

	/** Report a suspected violation */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void ReportViolation(const FString& PlayerId, EEOSAntiCheatViolationType ViolationType, const FString& Details);

	/** Request an integrity check — NOTE: checks LOCAL client only, not remote players */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void RequestIntegrityCheck(const FString& PlayerId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if anti-cheat session is active */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	bool IsSessionActive() const;

	/** Get the list of registered peer IDs */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	TArray<FString> GetRegisteredPeers() const;

	/** Get registered peer count */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	int32 GetRegisteredPeerCount() const;

	/** Check if a specific peer is registered */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	bool IsPeerRegistered(const FString& PeerId) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatClientAction OnClientActionRequired;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatPeerStatus OnPeerAuthStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatViolation OnViolationDetected;

	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatIntegrityChanged OnIntegrityChanged;

	/** Broadcast when a player action is reported for game-side validation */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatPlayerActionReported OnPlayerActionReported;

private:

	bool bSessionActive = false;
	TArray<FString> RegisteredPeers;

	/** Maps PeerId → handle value used for EOS_AntiCheatClient_RegisterPeer */
	TMap<FString, int64> PeerHandleMap;

	/** Running counter for generating unique per-peer client handles */
	int64 NextPeerHandleValue = 0;

	/** EOS notification IDs for cleanup */
	uint64 IntegrityViolatedNotifId = 0;
	uint64 PeerAuthStatusNotifId = 0;
	uint64 PeerActionRequiredNotifId = 0;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSAntiCheatSubsystem.generated.h"

/**
 * Opaque EOS anti-cheat client handle. Re-declared here (identical to
 * eos_anticheatcommon_types.h) so this public header does not need the private
 * SDK include path. Handle values are locally unique tokens minted by this
 * subsystem — they are NEVER derived from or converted to user-id strings.
 */
typedef void* EOS_AntiCheatCommon_ClientHandle;

struct FEEOSAntiCheatNotifyContext;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSAntiCheatSessionStarted, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSAntiCheatSessionEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAntiCheatMessageToPeer, const FString&, TargetPeerPuid, const TArray<uint8>&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSAntiCheatPeerActionRequired, const FString&, PeerPuid, EEOSAntiCheatAction, Action, const FString&, Message);

/**
 * EOS Anti-Cheat integration for listen-server co-op — the SINGLE functional
 * anti-cheat subsystem of this module. Runs EOS_ACCM_PeerToPeer mode on every
 * machine (host and clients alike): each participant begins a local anti-cheat
 * session and registers every OTHER participant as a peer.
 *
 * ── GAME-SIDE MESSAGE RELAY CONTRACT (REQUIRED) ─────────────────────────────
 * The EOS SDK does NOT transport anti-cheat messages. The game must relay them
 * over its own networking (a reliable, ordered channel — e.g. reliable RPCs via
 * PlayerController/PlayerState):
 *
 *   Outbound:  OnAntiCheatMessageToPeer(TargetPeerPuid, Payload) fires here →
 *              the game sends Payload to the machine owned by TargetPeerPuid.
 *   Inbound:   when a relayed payload arrives from the machine owned by
 *              SenderPeerPuid, the game calls
 *              ReceiveMessageFromPeer(SenderPeerPuid, Payload).
 *
 * Payloads are opaque binary, at most 512 bytes each
 * (EOS_ANTICHEATCLIENT_ONMESSAGETOPEERCALLBACK_MAX_MESSAGE_SIZE). Deliver them
 * reliably and in order per peer pair; without this relay every registered peer
 * fails authentication and is flagged for removal after the registration
 * timeout (40 s).
 *
 * ── FAIL-CLOSED ─────────────────────────────────────────────────────────────
 * Any BeginSession failure (setting disabled, no EAC module, no Connect login,
 * notification registration failure, SDK error) broadcasts
 * OnAntiCheatSessionStarted(false) and leaves the session INACTIVE. The game
 * decides the consequence (block matchmaking, return to menu, unprotected
 * mode). This subsystem never reports an active session on error.
 *
 * All peer identifiers crossing this API are Product User ID strings (bare
 * PUIDs; composite "<EAS>|<PUID>" net-id strings are accepted as input and
 * normalized). Delegates always broadcast bare PUIDs — or an EMPTY string when
 * an input id contained no parseable PUID (never the raw unparseable input).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSAntiCheatSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Session Management ───────────────────────────────────────────────────

	/**
	 * Begin the peer-to-peer anti-cheat session. Call when a multiplayer game
	 * session starts (host and clients). Requires bEnableAntiCheat = true and a
	 * logged-in Connect user (Product User ID).
	 * Result is broadcast through OnAntiCheatSessionStarted(bSuccess) — false is
	 * broadcast on EVERY failure path (including a redundant call while a session
	 * is already active: a NEW session requires EndSession first; the active
	 * session itself is left untouched) and the session stays inactive.
	 * Returns true only when a new session actually started.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool BeginSession();

	/**
	 * End the anti-cheat session. Unregisters all peers, removes SDK
	 * notifications and broadcasts OnAntiCheatSessionEnded. Call when leaving a
	 * multiplayer game session; a NEW BeginSession is required for the next one.
	 * Returns false only when no session was active (nothing to end).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool EndSession();

	// ── Peer Management ──────────────────────────────────────────────────────

	/**
	 * Register a remote participant of the current game session for mutual
	 * anti-cheat authentication. Call on EVERY machine for EVERY OTHER player
	 * (full mesh). PeerPuid: the remote player's Product User ID (composite
	 * net-id strings are normalized). On SDK failure the peer is NOT tracked and
	 * OnPeerAuthStatusChanged(PeerPuid, false) is broadcast; when the input
	 * contains no parseable PUID the broadcast carries an EMPTY id (delegates
	 * never echo unparseable input).
	 * Returns true when the peer is registered after the call (including a
	 * redundant call for an already-registered peer); false on any failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool RegisterPeer(const FString& PeerPuid);

	/**
	 * Unregister a remote participant when they leave the session in progress.
	 * Returns true when the peer mapping was removed; false when the peer was
	 * not registered or the SDK refused the unregister (mapping kept).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool UnregisterPeer(const FString& PeerPuid);

	/** Unregister all remote participants. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void UnregisterAllPeers();

	// ── Message Transport (game-side relay — see class comment) ─────────────

	/**
	 * Deliver an anti-cheat payload that another machine relayed to us. The
	 * game MUST call this for every payload received over its replication
	 * channel. SenderPeerPuid: Product User ID of the machine that ORIGINATED
	 * the payload (must be a registered peer, otherwise the payload is dropped
	 * with a warning). Payloads larger than 512 bytes
	 * (EOS_ANTICHEATCLIENT_ONMESSAGETOPEERCALLBACK_MAX_MESSAGE_SIZE) are
	 * rejected before reaching the SDK — legitimate payloads never exceed that,
	 * and this is an attacker-controlled input surface.
	 * Returns true when the payload was accepted by the SDK; false on any drop.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool ReceiveMessageFromPeer(const FString& SenderPeerPuid, const TArray<uint8>& Payload);

	// ── Player Action Reporting (game-side validation surface, no SDK calls) ─

	/** Report a player action for game-side validation (e.g., movement, damage dealt). Broadcast locally via OnPlayerActionReported. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void ReportPlayerAction(const FString& PlayerId, const FString& ActionType, const FString& ActionData);

	/** Report a suspected game-logic violation. Broadcast locally via OnViolationDetected. */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	void ReportViolation(const FString& PlayerId, EEOSAntiCheatViolationType ViolationType, const FString& Details);

	/**
	 * Poll LOCAL client integrity (never remote players). Verdict broadcast via
	 * OnIntegrityChanged (false on every path where integrity cannot be verified —
	 * fail-closed). Returns true when the poll actually ran (regardless of the
	 * verdict); false when the check could not be performed.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|AntiCheat")
	bool RequestIntegrityCheck(const FString& PlayerId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Check if the anti-cheat session is active. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	bool IsSessionActive() const;

	/** Get the Product User IDs of all registered peers. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	TArray<FString> GetRegisteredPeers() const;

	/** Get registered peer count. */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	int32 GetRegisteredPeerCount() const;

	/** Check if a specific peer is registered (accepts bare or composite ids). */
	UFUNCTION(BlueprintPure, Category = "EOS|AntiCheat")
	bool IsPeerRegistered(const FString& PeerPuid) const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Session start result. false on ANY failure path — the session is inactive and the game must react (fail-closed). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatSessionStarted OnAntiCheatSessionStarted;

	/** Session ended (EndSession or subsystem teardown with an active session). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatSessionEnded OnAntiCheatSessionEnded;

	/**
	 * REQUIRED transport hook: the SDK produced a payload that must reach the
	 * machine owned by TargetPeerPuid. Ship it over the game's replication
	 * (reliable, ordered) and call ReceiveMessageFromPeer on the receiving side.
	 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatMessageToPeer OnAntiCheatMessageToPeer;

	/**
	 * An action (e.g. RemovePlayer) must be applied to a registered peer.
	 * PeerPuid identifies the affected player; when the LOCAL client is the
	 * subject (EOS_ANTICHEATCLIENT_PEER_SELF) the local user's PUID is used.
	 * The game should kick/disconnect that player and call UnregisterPeer.
	 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatPeerActionRequired OnPeerActionRequired;

	/** Legacy peer-less action event; fired alongside OnPeerActionRequired. Prefer OnPeerActionRequired. */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatClientAction OnClientActionRequired;

	/** A registered peer's authentication status changed. bAuthenticated = backend validation (RemoteAuthComplete). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatPeerStatus OnPeerAuthStatusChanged;

	/** Game-side violation report echo (ReportViolation). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatViolation OnViolationDetected;

	/** Local client integrity state (SDK integrity-violated notification / RequestIntegrityCheck). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatIntegrityChanged OnIntegrityChanged;

	/** Game-side action report echo (ReportPlayerAction). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|AntiCheat")
	FOnEOSAntiCheatPlayerActionReported OnPlayerActionReported;

	// ── Internal SDK-callback handlers (game thread; do NOT call from game code) ──

	void HandleMessageToPeer(EOS_AntiCheatCommon_ClientHandle TargetHandle, const TArray<uint8>& Payload);
	void HandlePeerActionRequired(EOS_AntiCheatCommon_ClientHandle PeerHandle, EEOSAntiCheatAction Action, const FString& Message);
	void HandlePeerAuthStatusChanged(EOS_AntiCheatCommon_ClientHandle PeerHandle, bool bAuthenticated);

private:

	/** True only between a SUCCESSFUL EOS_AntiCheatClient_BeginSession and the matching EndSession. Never set on failure. */
	bool bSessionActive = false;

	/** Bare PUID of the local user, cached on successful BeginSession (used to resolve EOS_ANTICHEATCLIENT_PEER_SELF). */
	FString CachedLocalPuid;

	/**
	 * Bidirectional peer bookkeeping. Handles are opaque tokens minted from
	 * NextPeerHandleValue — never parsed from or printed as user ids. Entries
	 * are added only after EOS_AntiCheatClient_RegisterPeer succeeds.
	 */
	TMap<FString, EOS_AntiCheatCommon_ClientHandle> PuidToHandle;
	TMap<EOS_AntiCheatCommon_ClientHandle, FString> HandleToPuid;

	/** Monotonically increasing handle source. Starts at 1; never reset while the subsystem lives (stale handles must not be reissued). */
	UPTRINT NextPeerHandleValue = 1;

	/** Heap context passed as ClientData to the persistent notifications; carries a weak self pointer (EOS platform outlives this subsystem). */
	FEEOSAntiCheatNotifyContext* NotifyContext = nullptr;

	/** EOS notification IDs, valid only while the session's registrations are alive. */
	uint64 MessageToPeerNotifId = 0;
	uint64 PeerActionRequiredNotifId = 0;
	uint64 PeerAuthStatusNotifId = 0;
	uint64 IntegrityViolatedNotifId = 0;

	/** Returns true when bEnableAntiCheat is set; logs (once per call site name) when it is not. */
	bool IsAntiCheatEnabled(const TCHAR* CallSite) const;

	/** Remove whatever notifications are registered and free the notify context (safe on partial registration). */
	void RemoveNotifications();

	/** Resolve a handle to its PUID: registered peer, PEER_SELF → local PUID, otherwise empty. */
	FString ResolveHandleToPuid(EOS_AntiCheatCommon_ClientHandle Handle) const;
};

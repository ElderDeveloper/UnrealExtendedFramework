// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "UObject/ObjectKey.h"
#include "OnlineSubsystemTypes.h"
#include "Containers/Ticker.h"
#include "EEOSVoiceSubsystem.generated.h"

class IVoiceChatUser;
class FUniqueNetId;
struct FVoiceChatResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomJoined, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomLeft, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoiceRoomJoinFailed, const FString&, RoomName, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerTalking, const FString&, UserId, bool, bIsTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerJoinedRoom, const FString&, RoomName, const FString&, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerLeftRoom, const FString&, RoomName, const FString&, UserId);

/**
 * Manages EOS voice chat rooms, per-player audio, transmit modes, and device management.
 *
 * DESIGN: voice rides lobby-managed RTC rooms. Lobbies created with CreateLobby(bUseVoiceChat=true)
 * set bUseLobbiesVoiceChatIfAvailable; the engine's EOS Online Subsystem then joins/leaves the
 * lobby's RTC room automatically as lobby membership changes. There is no RTCAdmin token backend,
 * so JoinVoiceRoom cannot create rooms — it only confirms membership of a room the lobby already
 * placed us in. Actual room entry/exit is driven by JoinLobby/LeaveLobby/DestroyLobby.
 *
 * The local voice user is resolved through IOnlineSubsystemEOS::GetVoiceChatUserInterface(), which
 * creates and logs the user in with the local Product User Id once EOS identity login has completed.
 * A standalone IVoiceChat fallback exists but cannot see lobby RTC rooms (it runs its own EOS
 * platform instance) — it is best-effort only.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSVoiceSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Room Management ──────────────────────────────────────────────────────

	/**
	 * Confirm membership of a lobby-managed voice room.
	 * If the room is already an active channel on the voice user (the lobby auto-joined it),
	 * broadcasts OnVoiceRoomJoined synchronously. Otherwise broadcasts OnVoiceRoomJoinFailed:
	 * rooms come from lobby membership (CreateLobby with bUseVoiceChat), not from manual joins.
	 * @return true if the room was confirmed (OnVoiceRoomJoined broadcast). False when rejected
	 * or unconfirmed: voice disabled in settings (log only), or no voice user / room not an
	 * active channel (OnVoiceRoomJoinFailed broadcast — a dedicated failure delegate, so
	 * legitimate OnVoiceRoomJoined waiters never receive a foreign failure).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	bool JoinVoiceRoom(const FString& RoomName);

	/**
	 * Leave a specific voice channel by name. NOTE: lobby-managed RTC rooms cannot be left this
	 * way — the engine rejects LeaveChannel for lobby rooms (NotPermitted); leave the lobby
	 * instead. OnVoiceRoomLeft broadcasts from the channel-exited event, never optimistically.
	 * @return true if a leave was started (completion arrives via OnVoiceRoomLeft) or a stale
	 * mirror entry was cleared synchronously. False when rejected: not in the room (log only,
	 * nothing broadcast).
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	bool LeaveVoiceRoom(const FString& RoomName);

	/** Attempt to leave all currently joined voice rooms (see LeaveVoiceRoom for lobby-room limits). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void LeaveAllVoiceRooms();

	// ── Per-Player Controls ──────────────────────────────────────────────────

	/** Mute a specific player */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void MutePlayer(const FString& UserId);

	/** Unmute a specific player */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void UnmutePlayer(const FString& UserId);

	/** Set the volume for a specific player (0.0 to 2.0). Manual calls may be overridden by
	 *  component proximity aggregation — see SetPlayerVolumeContribution. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetPlayerVolume(const FString& UserId, float Volume);

	/** Get the volume for a specific player */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	float GetPlayerVolume(const FString& UserId) const;

	/** Check if a specific player is currently talking */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsPlayerTalking(const FString& UserId) const;

	/** Check if a specific player is muted */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsPlayerMuted(const FString& UserId) const;

	// ── Volume & Muting ──────────────────────────────────────────────────────

	/** Set the output volume (0.0 to 1.0) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetOutputVolume(float Volume);

	/** Set the input (microphone) volume */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetInputVolume(float Volume);

	/** Get the current input volume */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	float GetInputVolume() const;

	/** Mute/unmute the local microphone */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetLocalMuted(bool bMuted);

	// ── Transmit Modes ───────────────────────────────────────────────────────
	// NOTE: while any voice chat components are registered, the subsystem composes the transmit
	// set from the components' rooms (TransmitToSpecificChannels). Manual calls below apply
	// immediately but are overwritten on the next component recompute.

	/** Transmit voice to all joined rooms */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void TransmitToAllRooms();

	/** Transmit voice only to a specific room */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void TransmitToSelectedRoom(const FString& RoomName);

	/** Stop transmitting voice (listen-only mode) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void TransmitToNoRoom();

	// ── Device Management ────────────────────────────────────────────────────

	/** Get available input (microphone) devices */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FEEOSVoiceDeviceInfo> GetInputDevices() const;

	/** Get available output (speaker) devices */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FEEOSVoiceDeviceInfo> GetOutputDevices() const;

	/** Set the input device by ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetInputDevice(const FString& DeviceId);

	/** Set the output device by ID */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetOutputDevice(const FString& DeviceId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get all voice rooms (channels) the local user is currently in. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetActiveVoiceRooms() const;

	/** Get the RTC room name of the current lobby's voice channel (empty if no voice lobby). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	FString GetLobbyVoiceRoomName() const;

	/** Check if currently in any voice room */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsInVoiceRoom() const;

	/** Check if currently in a specific voice room */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsInRoom(const FString& RoomName) const;

	/** Get the current voice room name (first joined — prefer GetActiveVoiceRooms for multi-room) */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	FString GetCurrentRoomName() const;

	/** Check if the local microphone is muted */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsLocalMuted() const;

	/** Get all players in a specific voice room */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetPlayersInRoom(const FString& RoomName) const;

	/** Get all joined voice room names (alias of GetActiveVoiceRooms, kept for BP compatibility) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetJoinedRooms() const;

	/** Get the cached IVoiceChatUser instance (may be null if voice is not available) */
	IVoiceChatUser* GetCachedVoiceChatUser() const;

	// ── Component Aggregation (C++ API for UEEOSVoiceChatComponent) ──────────

	/**
	 * Register a component as a user of a room. Refcounted: the same room may be used by many
	 * components. bTransmit adds the room to the composed transmit set once the room is joined.
	 * Returns true if the room is currently joined (the caller may treat itself as active now);
	 * false means the room is not joined yet — wait for OnVoiceRoomJoined.
	 */
	bool RegisterVoiceRoomUser(const FString& RoomName, bool bTransmit);

	/**
	 * Unregister a component from a room (must mirror the RegisterVoiceRoomUser call exactly).
	 * When the last user of a room unregisters, the room is removed from the transmit set only —
	 * actual channel membership belongs to the lobby and ends when the lobby is left.
	 */
	void UnregisterVoiceRoomUser(const FString& RoomName, bool bTransmit);

	/**
	 * Report a per-player receive-volume contribution from a source object (a proximity
	 * component). IVoiceChatUser only exposes a GLOBAL per-player volume (no per-channel
	 * volume), so the subsystem applies the MAX across all contributions for each player.
	 * When a player has no contributions left, their volume is restored to 1.0.
	 */
	void SetPlayerVolumeContribution(const UObject* Source, const FString& UserId, float Volume);

	/** Remove one player's volume contribution from a source object and re-apply the aggregate. */
	void ClearPlayerVolumeContribution(const UObject* Source, const FString& UserId);

	/** Remove all volume contributions from a source object and re-apply aggregates. */
	void ClearPlayerVolumeContributions(const UObject* Source);

	// ── Delegates ────────────────────────────────────────────────────────────

	/** A voice room became active: fired from the engine's channel-joined event (lobby RTC
	 *  auto-join) and synchronously from JoinVoiceRoom when the room was already joined. */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceRoomJoined OnVoiceRoomJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceRoomLeft OnVoiceRoomLeft;

	/** JoinVoiceRoom could not confirm the room (not a lobby-managed channel we are in). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceRoomJoinFailed OnVoiceRoomJoinFailed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSPlayerTalking OnPlayerTalking;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerJoinedRoom OnPlayerJoinedRoom;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerLeftRoom OnPlayerLeftRoom;

private:

	// ── Voice user resolution ────────────────────────────────────────────────

	/** Resolve the local voice user (OSS route first, standalone fallback). Safe to call repeatedly. */
	void ResolveVoiceUser();

	/** Standalone IVoiceChat fallback: Initialize + Connect + CreateUser + Login(PUID). */
	void ResolveStandaloneVoiceUser(const FString& ProductUserId);

	/** CreateUser + Login on the standalone IVoiceChat (called once connected). */
	void CreateAndLoginStandaloneUser(const FString& ProductUserId);

	/** Bind talking/joined/left/channel delegates on the resolved voice user (handles stored). */
	void BindVoiceUserDelegates();

	/** Unbind all stored delegate handles from the voice user (safe if never bound). */
	void UnbindVoiceUserDelegates();

	/** Drop the voice user: unbind, and logout/release only if we own it (standalone path). */
	void TearDownVoiceUser();

	// ── Engine event handlers ────────────────────────────────────────────────

	void HandleIdentityLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId);
	void HandleVoiceChatLoggedIn(const FString& PlayerName);
	void HandleVoiceChatLoggedOut(const FString& PlayerName);
	void HandleChannelJoined(const FString& ChannelName);
	void HandleChannelExited(const FString& ChannelName, const FVoiceChatResult& Reason);
	void HandlePlayerAdded(const FString& ChannelName, const FString& PlayerName);
	void HandlePlayerRemoved(const FString& ChannelName, const FString& PlayerName);
	void HandlePlayerTalkingUpdated(const FString& ChannelName, const FString& PlayerName, bool bIsTalking);

	/** Apply configured defaults (volumes, start-muted) through the subsystem's own wrappers. */
	void ApplyVoiceDefaults();

	// ── Transmit composition / volume aggregation ────────────────────────────

	/** Re-apply the transmit set: union of transmit-registered rooms that are actually joined. */
	void RecomputeTransmitChannels();

	/** Purge + re-apply the max-wins aggregate volume for one player (1.0 restore when none remain). */
	void ApplyAggregatedVolumeForPlayer(const FString& UserId);

	/** Compute and push one player's aggregate. Assumes dead sources were just purged. */
	void ApplyAggregatedVolumeForPlayerInternal(const FString& UserId);

	/** Drop contributions from dead sources (components destroyed without EndPlay/unregister)
	 *  and re-apply the aggregate for EVERY player they touched — a stale contribution must not
	 *  pin anyone's volume. Runs before each aggregation and periodically from a sweep ticker
	 *  (the sweep covers the case where no live component remains to drive aggregation). */
	void PurgeStaleVolumeContributions();

	/** Sweep ticker trampoline (FTSTicker signature) for PurgeStaleVolumeContributions. */
	bool HandleStaleContributionSweep(float DeltaTime);

	/** True while any component registration exists — the composed transmit set owns transmission. */
	bool IsTransmitCompositionActive() const;

	// ── State ────────────────────────────────────────────────────────────────

	/** Mirror of joined channels, updated from channel joined/exited events. */
	TSet<FString> JoinedRooms;
	bool bLocalMuted = false;
	float CurrentInputVolume = 1.0f;

	/** Cached voice chat user. OSS route: an engine-owned wrapper (never Login/Logout/Release it).
	 *  Standalone route: owned by this subsystem, released per the orphan-safe pattern below. */
	IVoiceChatUser* CachedVoiceChatUser = nullptr;

	/** True only on the standalone fallback path — we created the user and must release it. */
	bool bOwnsVoiceUser = false;

	/** Whether the voice user has completed login (either route). */
	bool bVoiceUserLoggedIn = false;

	/** Whether an async standalone login is in flight (its completion delegate owns the user's
	 *  release if we die meanwhile — see Deinitialize handoff). */
	bool bVoiceLoginPending = false;

	/** Guard so voice defaults apply once per login session. */
	bool bDefaultsApplied = false;

	// Delegate handles on the voice user
	FDelegateHandle LoggedInHandle;
	FDelegateHandle LoggedOutHandle;
	FDelegateHandle ChannelJoinedHandle;
	FDelegateHandle ChannelExitedHandle;
	FDelegateHandle PlayerAddedHandle;
	FDelegateHandle PlayerRemovedHandle;
	FDelegateHandle PlayerTalkingHandle;

	// Delegate handle on the identity interface (login status → resolve/teardown voice user)
	FDelegateHandle IdentityStatusChangedHandle;

	// Component room refcounts
	TMap<FString, int32> RoomRefCounts;
	TMap<FString, int32> RoomTransmitRefCounts;

	// Per-source, per-player volume contributions (max-wins aggregation)
	TMap<FObjectKey, TMap<FString, float>> VolumeContributions;
	/** Last volume actually pushed per player, so unchanged aggregates skip the SDK call. */
	TMap<FString, float> AppliedPlayerVolumes;

	/** Periodic stale-contribution sweep (see PurgeStaleVolumeContributions). */
	FTSTicker::FDelegateHandle StaleContributionSweepHandle;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "UObject/ObjectKey.h"
#include "OnlineSubsystemTypes.h"
#include "Containers/Ticker.h"
#include "EEOSVoiceSubsystem.generated.h"

class IVoiceChatUser;
class IEOSPlatformHandle;
struct FEEOSVoiceCaptureState;
class FUniqueNetId;
struct FVoiceChatResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomJoined, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomLeft, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoiceRoomJoinFailed, const FString&, RoomName, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerTalking, const FString&, UserId, bool, bIsTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerJoinedRoom, const FString&, RoomName, const FString&, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerLeftRoom, const FString&, RoomName, const FString&, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEOSVoiceAudioDevicesChanged);

/**
	* Manages EOS voice chat rooms, per-player audio, transmit modes, and device management.
	*
	* DESIGN: voice rides lobby-managed RTC rooms. Lobbies created with CreateLobby(bUseVoiceChat=true)
	* set bUseLobbiesVoiceChatIfAvailable; the engine's EOS Online Subsystem then joins/leaves the
	* lobby's RTC room automatically as lobby membership changes. There is no RTCAdmin token backend,
	* so custom JoinVoiceRoom calls must receive credentials from the caller's trusted server.
	* Lobby room entry/exit is driven by JoinLobby/LeaveLobby/DestroyLobby.
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

	/** Confirms an active channel, or joins a custom channel with trusted-server credentials.
		* True means accepted, not completed. Observe OnVoiceRoomJoined/OnVoiceRoomJoinFailed. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	bool JoinVoiceRoom(const FString& RoomName, const FString& ChannelCredentials = TEXT(""));
	/** Leaves a custom channel joined by this subsystem. Lobby-owned channels reject this request. */
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
	 *  component proximity aggregation — see SetPlayerVolumeContribution. For a per-player
	 *  slider use SetPlayerVolumeScale, which proximity does not overwrite. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetPlayerVolume(const FString& UserId, float Volume);

	/**
	 * Local listener gain for one player (0.0 to 2.0), multiplied into the proximity aggregate
	 * (or into full volume when no component contributes). 1.0 removes the scale.
	 */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetPlayerVolumeScale(const FString& UserId, float Scale);

	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	float GetPlayerVolumeScale(const FString& UserId) const;

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

	/** Set the input (microphone) volume: 0 silent, 1 unchanged, 2 the most EOS boosts (the engine's own range). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetInputVolume(float Volume);

	/** Get the current input volume */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	float GetInputVolume() const;

	/** Mute/unmute the local microphone */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void SetLocalMuted(bool bMuted);

	// ── Transmit Modes ───────────────────────────────────────────────────────
	// NOTE: nothing is transmitted until something asks. A voice user starts with an empty transmit
	// set (the engine's own default of every channel never applies). While any voice chat components
	// are registered, the subsystem composes the transmit set from the components' rooms
	// (TransmitToSpecificChannels). Manual calls below apply immediately but are overwritten on the
	// next component recompute.

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

	/** Product User Id the local voice user is logged in as. Empty until the voice user has logged in. */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	FString GetLocalVoicePlayerName() const;

	/** Get all players in a specific voice room */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetPlayersInRoom(const FString& RoomName) const;

	/** Get all joined voice room names (alias of GetActiveVoiceRooms, kept for BP compatibility) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetJoinedRooms() const;

	/** Get the cached IVoiceChatUser instance (may be null if voice is not available) */
	IVoiceChatUser* GetCachedVoiceChatUser() const;

	/**
	 * True when the resolved user is the standalone IVoiceChat fallback.
	 * That user cannot see lobby RTC rooms and must not be treated as a lobby connection.
	 */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsUsingStandaloneVoiceUser() const;

	/** True when the engine-owned OSS voice user is logged in. This is the lobby RTC user. */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool HasLobbyVoiceUser() const;

	/**
	 * Open-mic gate. While enabled, captured samples below the threshold are zeroed so silence
	 * is not transmitted. The EOS SDK has no project-facing noise-gate setting; this is the
	 * capture-path gate. bIsSpeaking from the send callback also holds the gate open.
	 */
	void SetLocalSpeechGate(bool bSilenceWhenInactive, float RmsThreshold, float ReleaseSeconds);
	void RegisterCaptureState(const UObject* Source, const TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>& State);
	void UnregisterCaptureState(const UObject* Source);

	/** Zeros outgoing samples while still measuring the microphone. Test mode cannot leave audio transmitting. */
	void SetMicrophoneTestMode(bool bEnabled);

	bool IsLocalSpeechActive() const;
	float GetLocalCaptureLevel() const;

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
	FOnEOSVoiceRoomJoinFailed OnVoiceRoomLeaveFailed;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSPlayerTalking OnPlayerTalking;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerJoinedRoom OnPlayerJoinedRoom;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerLeftRoom OnPlayerLeftRoom;

	/**
	 * The backend's input/output device lists changed: a device was plugged or unplugged, the system
	 * default moved, or EOS finished its first enumeration after login (the lists are empty before
	 * that). Re-resolve saved device ids here instead of polling. Broadcast on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceAudioDevicesChanged OnAudioDevicesChanged;

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
	void HandleCapturedAudio(const FString& ChannelName, TArrayView<int16> PcmSamples, int SampleRate, int Channels);
	void HandleAudioAboutToSend(const FString& ChannelName, TArrayView<const int16> PcmSamples, int SampleRate, int Channels, bool bIsSpeaking);
	void HandleAvailableAudioDevicesChanged();

	/** Mute state reaches EOS and the log only when it changes; callers may re-assert it freely. */
	void SetPlayerMutedIfChanged(const FString& UserId, bool bMuted);

	/** Apply configured defaults (volumes, start-muted) through the subsystem's own wrappers. */
	void ApplyVoiceDefaults();

	// ── Transmit composition / volume aggregation ────────────────────────────

	/** Re-apply the transmit set: union of transmit-registered rooms that are actually joined. */
	void RecomputeTransmitChannels();

	/**
	 * Re-send the current transmit mode, whatever it is, to every joined channel. The engine calls
	 * EOS_RTCAudio_UpdateSending only when its transmit mode changes, so this steps through another
	 * mode and back. Needed because the SDK turns sending on by itself when it joins a lobby's RTC
	 * room, while the engine goes on believing sending is off and never corrects it.
	 */
	void ReapplyTransmitState();

	/** A lobby RTC room finished connecting for LocalUserId (a Product User Id string). */
	void HandleLobbyRTCRoomConnected(const FString& LocalUserId);

	/** Watch the OSS platform for lobby RTC rooms connecting. OSS voice user only: a standalone user has no lobby rooms. */
	void BindLobbyRTCNotification();
	void UnbindLobbyRTCNotification();

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
	TSet<FString> ManagedRooms;
	TSet<FString> PendingJoins;
	TSet<FString> PendingLeaves;
	uint64 VoiceUserGeneration = 0;
	bool bTearingDownVoiceUser = false;
	TMap<FString, TSet<FString>> TalkingRoomsByPlayer;
	FCriticalSection CaptureStatesMutex;
	TMap<FObjectKey, TSharedPtr<FEEOSVoiceCaptureState, ESPMode::ThreadSafe>> CaptureStates;
	bool bLocalMuted = false;
	bool bLocalMuteApplied = false;
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
	FDelegateHandle CaptureReadHandle;
	FDelegateHandle CaptureSentHandle;
	FDelegateHandle AudioDevicesChangedHandle;

	/** EOS_Lobby_AddNotifyRTCRoomConnectionChanged id, and the platform it lives on (a gone platform took it along). */
	uint64 LobbyRTCConnectionNotifyId = 0;
	TWeakPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> LobbyRTCPlatform;
	friend struct FEEOSVoiceLobbyRTCCallbacks;

	friend class FDOPVoiceRuntimeRegressionTest;

	bool bSilenceWhenInactive = false;
	bool bMicrophoneTest = false;
	float SpeechRmsThreshold = 0.02f;
	float SpeechReleaseSeconds = 0.28f;
	volatile bool bLocalSpeechActive = false;
	volatile float LocalCaptureLevel = 0.0f;
	volatile double SpeechHoldUntilSeconds = 0.0;

	// Delegate handle on the identity interface (login status → resolve/teardown voice user)
	FDelegateHandle IdentityStatusChangedHandle;

	// Component room refcounts
	TMap<FString, int32> RoomRefCounts;
	TMap<FString, int32> RoomTransmitRefCounts;

	// Per-source, per-player volume contributions (max-wins aggregation)
	TMap<FObjectKey, TMap<FString, float>> VolumeContributions;
	/** Listener gain per player, multiplied into the aggregate. Absent means 1.0. */
	TMap<FString, float> PlayerVolumeScales;
	/** Last volume actually pushed per player, so unchanged aggregates skip the SDK call. */
	TMap<FString, float> AppliedPlayerVolumes;

	/** Periodic stale-contribution sweep (see PurgeStaleVolumeContributions). */
	FTSTicker::FDelegateHandle StaleContributionSweepHandle;
};

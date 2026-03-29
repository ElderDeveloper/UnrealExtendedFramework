// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSVoiceSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomJoined, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSVoiceRoomLeft, const FString&, RoomName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerTalking, const FString&, UserId, bool, bIsTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerJoinedRoom, const FString&, RoomName, const FString&, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSVoicePlayerLeftRoom, const FString&, RoomName, const FString&, UserId);

/**
 * Manages EOS voice chat rooms, per-player audio, transmit modes, and device management.
 * Supports joining multiple rooms simultaneously (e.g., team + proximity).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSVoiceSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Room Management ──────────────────────────────────────────────────────

	/** Join a voice chat room. Can be called multiple times for multi-room (e.g., team + proximity). */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void JoinVoiceRoom(const FString& RoomName);

	/** Leave a specific voice chat room by name. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void LeaveVoiceRoom(const FString& RoomName);

	/** Leave all currently joined voice rooms. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void LeaveAllVoiceRooms();

	// ── Per-Player Controls ──────────────────────────────────────────────────

	/** Mute a specific player */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void MutePlayer(const FString& UserId);

	/** Unmute a specific player */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	void UnmutePlayer(const FString& UserId);

	/** Set the volume for a specific player (0.0 to 2.0) */
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

	/** Check if currently in any voice room */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsInVoiceRoom() const;

	/** Check if currently in a specific voice room */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsInRoom(const FString& RoomName) const;

	/** Get the current voice room name (first joined — prefer GetJoinedRooms for multi-room) */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	FString GetCurrentRoomName() const;

	/** Check if the local microphone is muted */
	UFUNCTION(BlueprintPure, Category = "EOS|Voice")
	bool IsLocalMuted() const;

	/** Get all players in a specific voice room */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetPlayersInRoom(const FString& RoomName) const;

	/** Get all joined voice room names */
	UFUNCTION(BlueprintCallable, Category = "EOS|Voice")
	TArray<FString> GetJoinedRooms() const;

	/** Get the cached IVoiceChatUser instance (may be null if voice is not available) */
	class IVoiceChatUser* GetCachedVoiceChatUser() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceRoomJoined OnVoiceRoomJoined;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoiceRoomLeft OnVoiceRoomLeft;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSPlayerTalking OnPlayerTalking;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerJoinedRoom OnPlayerJoinedRoom;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Voice")
	FOnEOSVoicePlayerLeftRoom OnPlayerLeftRoom;

private:

	TSet<FString> JoinedRooms;
	bool bLocalMuted = false;
	float CurrentInputVolume = 1.0f;

	/** Cached voice chat user — created once in Initialize, released in Deinitialize */
	class IVoiceChatUser* CachedVoiceChatUser = nullptr;

	/** Whether the cached voice user has been logged in */
	bool bVoiceUserLoggedIn = false;
};

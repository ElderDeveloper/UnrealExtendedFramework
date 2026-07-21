// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamMusicSubsystem.generated.h"

/** Playback state of the Steam music player (mirrors Steamworks AudioPlayback_Status). */
UENUM(BlueprintType)
enum class EESteamMusicStatus : uint8
{
	Undefined = 0,
	Playing = 1,
	Paused = 2,
	Idle = 3
};

/** Fired when the Steam music player's playback status changed (poll GetPlaybackStatus for the new state). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamMusicPlaybackChanged);

/** Fired when the Steam music player's volume changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamMusicVolumeChanged, float, NewVolume);

/**
 * Wraps ISteamMusic: remote control of the Steam client's music player.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamMusicSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** True when the Steam music player is enabled in the Steam client. */
	UFUNCTION(BlueprintPure, Category = "Steam|Music")
	bool IsEnabled() const;

	/** True when the Steam music player is currently playing. */
	UFUNCTION(BlueprintPure, Category = "Steam|Music")
	bool IsPlaying() const;

	/** Current playback status (Undefined when Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Music")
	EESteamMusicStatus GetPlaybackStatus() const;

	UFUNCTION(BlueprintCallable, Category = "Steam|Music")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "Steam|Music")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "Steam|Music")
	void PlayPrevious();

	UFUNCTION(BlueprintCallable, Category = "Steam|Music")
	void PlayNext();

	/** Music player volume in 0.0 .. 1.0 (0 when Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Music")
	float GetVolume() const;

	/** Sets the music player volume (clamped to 0.0 .. 1.0). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Music")
	void SetVolume(float Volume);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Music")
	FOnSteamMusicPlaybackChanged OnPlaybackChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Music")
	FOnSteamMusicVolumeChanged OnVolumeChanged;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamMusicCallbacks;
	TSharedPtr<class FESteamMusicCallbacks> Callbacks;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamVideoSubsystem.generated.h"

/** Outcome/stop reason of a live broadcast upload (mirrors EBroadcastUploadResult). */
UENUM(BlueprintType)
enum class EESteamBroadcastUploadResult : uint8
{
	None,
	OK,
	InitFailed,
	FrameFailed,
	Timeout,
	BandwidthExceeded,
	LowFPS,
	MissingKeyFrames,
	NoConnection,
	RelayFailed,
	SettingsChanged,
	MissingAudio,
	TooFarBehind,
	TranscodeBehind,
	NotAllowedToPlay,
	Busy,
	Banned,
	AlreadyActive,
	ForcedOff,
	AudioBehind,
	Shutdown,
	Disconnect,
	VideoInitFailed,
	AudioInitFailed
};

/** Fired when a GetVideoURL request completes; URL is empty on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamVideoURLReceived, bool, bSuccess, int32, VideoAppId, const FString&, URL);

/** Fired when a GetOPFSettings request completes (OPF details for 360 video playback). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamOPFSettingsReceived, bool, bSuccess, int32, VideoAppId);

/** Fired when the user starts uploading a live broadcast; bIsRTMP is true for RTMP uploads. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamBroadcastUploadStart, bool, bIsRTMP);

/** Fired when a live broadcast upload stops; Reason describes why. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamBroadcastUploadStop, EESteamBroadcastUploadResult, Reason);

/**
 * Wraps ISteamVideo: streaming URLs for Steam video apps and broadcast state.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamVideoSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Requests a streaming URL for the given video app id; the result arrives on OnVideoURLReceived. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Video")
	void GetVideoURL(int32 VideoAppId);

	/** True while the user is uploading a live broadcast; outputs the current viewer count. */
	UFUNCTION(BlueprintPure, Category = "Steam|Video")
	bool IsBroadcasting(int32& OutNumViewers) const;

	/**
	 * Requests the OPF (Open Projection Format) details for a 360 video app. The request
	 * completes on OnOPFSettingsReceived; read the string with GetOPFStringForApp afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Video")
	void GetOPFSettings(int32 VideoAppId);

	/**
	 * Reads the cached OPF string for a video app (populated after a successful GetOPFSettings).
	 * Returns false when the OPF details are not yet available.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Video")
	bool GetOPFStringForApp(int32 VideoAppId, FString& OutOPF);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Video")
	FOnSteamVideoURLReceived OnVideoURLReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Video")
	FOnSteamOPFSettingsReceived OnOPFSettingsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Video")
	FOnSteamBroadcastUploadStart OnBroadcastUploadStart;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Video")
	FOnSteamBroadcastUploadStop OnBroadcastUploadStop;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamVideoCallbacks;
	TSharedPtr<class FESteamVideoCallbacks> Callbacks;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamVoiceSubsystem.generated.h"

class UESteamVoiceSoundWave;

/**
 * Fired each time a chunk of compressed voice data is captured while recording.
 * Transmit the payload to peers (e.g. over P2P) and decode it there with DecompressVoice.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamVoiceCaptured, const TArray<uint8>&, CompressedData);

/**
 * Wraps the ISteamUser voice API: microphone capture into compressed voice packets,
 * decompression to raw PCM, and playback helpers.
 *
 * While recording, a core ticker polls GetAvailableVoice/GetVoice every frame and
 * broadcasts each captured chunk on OnVoiceCaptured. Because push-to-talk keys are
 * often released early, Steam keeps capturing briefly after StopVoiceRecording; this
 * wrapper stops polling immediately on Stop, so that short tail is dropped by design.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamVoiceSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Capture ----

	/**
	 * Starts microphone capture; captured chunks arrive on OnVoiceCaptured.
	 * Also flags the local user as speaking for the Steam UI (SetInGameVoiceSpeaking).
	 * Safe no-op when Steam is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	void StartVoiceRecording();

	/** Stops microphone capture and clears the in-game speaking flag. Safe to call when not recording. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	void StopVoiceRecording();

	/**
	 * True while recording was started through this subsystem. Local flag —
	 * the Steamworks SDK exposes no recording-state query.
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|Voice")
	bool IsRecording() const { return bRecording; }

	// ---- Decode / playback ----

	/**
	 * Decompresses a captured voice chunk into raw mono 16-bit PCM at the optimal
	 * sample rate (grows the output buffer on k_EVoiceResultBufferTooSmall).
	 * Returns false when Steam is unavailable or the data is empty/corrupted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	bool DecompressVoice(const TArray<uint8>& CompressedData, TArray<uint8>& OutPcm);

	/** Native sample rate of the Steam voice decompressor (0 when Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Voice")
	int32 GetVoiceOptimalSampleRate() const;

	/**
	 * Creates a transient procedural sound wave configured for Steam voice playback
	 * (mono, 16-bit, optimal sample rate). Returns null when Steam is unavailable.
	 * Play it through an audio component and feed it via PlayVoiceData.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	UESteamVoiceSoundWave* CreateVoiceSoundWave();

	/**
	 * Convenience: decompresses a captured voice chunk and queues the PCM on the
	 * given sound wave. The wave must already be playing (or virtualized) to be heard.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	bool PlayVoiceData(UESteamVoiceSoundWave* Target, const TArray<uint8>& CompressedData);

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice")
	FOnSteamVoiceCaptured OnVoiceCaptured;

protected:
	virtual void HandleSteamClientShutdown() override;

private:
	/** Core ticker callback polling GetAvailableVoice/GetVoice while recording. */
	bool TickVoiceCapture(float DeltaTime);

	void RemoveVoiceTicker();

	FTSTicker::FDelegateHandle VoiceTickerHandle;
	bool bRecording = false;
};

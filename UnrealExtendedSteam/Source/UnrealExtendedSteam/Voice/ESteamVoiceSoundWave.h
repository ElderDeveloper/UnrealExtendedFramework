// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "ESteamVoiceSoundWave.generated.h"

/**
 * Procedural sound wave preconfigured for Steam voice playback: mono, 16-bit PCM,
 * non-looping, virtualized as PlayWhenSilent so gaps between voice packets do not
 * kill the active sound. The sample rate is set at creation time from
 * ISteamUser::GetVoiceOptimalSampleRate — create instances through
 * UESteamVoiceSubsystem::CreateVoiceSoundWave and feed them decompressed voice
 * data (see DecompressVoice / PlayVoiceData).
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDSTEAM_API UESteamVoiceSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UESteamVoiceSoundWave(const FObjectInitializer& ObjectInitializer);

	/**
	 * Sets the playback sample rate. Called once at creation by
	 * UESteamVoiceSubsystem::CreateVoiceSoundWave with the Steam-optimal rate;
	 * the queued PCM must have been decompressed at the same rate.
	 */
	void InitializeSteamVoice(int32 InSampleRate);

	/** Queues raw mono 16-bit PCM data (as produced by DecompressVoice) for playback. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice")
	void QueueDecompressedAudio(const TArray<uint8>& Pcm);
};

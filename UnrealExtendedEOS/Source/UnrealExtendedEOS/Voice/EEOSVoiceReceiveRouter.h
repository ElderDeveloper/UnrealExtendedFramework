// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include <atomic>

/**
 * One remote player's received voice on its way into the audio engine. EOS's render thread pushes its 16-bit
 * frames in; Output takes mono float audio at the audio engine's sample rate (an audio bus patch in practice).
 * Push runs on one thread at a time. Gain may be set from any thread.
 */
class UNREALEXTENDEDEOS_API FEEOSVoiceReceiveStream
{
public:
	/** Returns how many samples were taken: fewer than offered means the receiver is full, and the rest is dropped. */
	using FOutput = TFunction<int32(const float* Samples, int32 NumSamples)>;

	FEEOSVoiceReceiveStream(int32 InOutputSampleRate, int32 InBufferFrames, FOutput&& InOutput);

	/**
	 * Mixes to mono, scales by Gain times MasterGain, resamples to the output rate and hands the result on.
	 * Nothing reaches Output until BufferFrames have gathered, and again after a pause long enough for the
	 * receiver to have run dry, so uneven delivery does not leave holes in the playback. A player heard in
	 * two rooms delivers the same voice twice; only one room is taken at a time.
	 */
	void Push(TArrayView<const int16> Samples, int32 SampleRate, int32 Channels, float MasterGain, const FString& Room, double NowSeconds);

	/** The listener's own gain for this player (0..2). */
	std::atomic<float> Gain{1.0f};

	int32 GetOutputSampleRate() const { return OutputSampleRate; }
	int32 GetBufferFrames() const { return BufferFrames; }

private:
	const int32 OutputSampleRate;
	const int32 BufferFrames;
	FOutput Output;

	// Push-thread state.
	TArray<float> Mono;
	TArray<float> Resampled;
	/** The cushion gathered before playback starts. */
	TArray<float> Pending;
	/** Where the next output sample is read, in input samples from the start of the next chunk (-1 is the previous chunk's last sample). */
	double ResamplePosition = 0.0;
	float PreviousSample = 0.0f;
	int32 InputSampleRate = 0;
	FString SourceRoom;
	double LastPushSeconds = -1.0;
	bool bFlowing = false;
};

/**
 * Takes EOS's rendered voice (the voice user's mixed and unmixed render delegates) and feeds each player's
 * stream. Held through a thread-safe shared pointer, so EOS's render thread never touches a UObject.
 */
class UNREALEXTENDEDEOS_API FEEOSVoiceReceiveRouter : public TSharedFromThis<FEEOSVoiceReceiveRouter, ESPMode::ThreadSafe>
{
public:
	/** While set, EOS's own playback of the mix is silenced: the audio engine plays the voices instead. */
	std::atomic<bool> bSilenceMix{false};
	/** Scales every stream: the voice output volume. */
	std::atomic<float> MasterGain{1.0f};

	void AddStream(const FString& PlayerId, const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>& Stream);
	bool HasStream(const FString& PlayerId) const;
	void RemoveAllStreams();
	/** The listener's own gain for one player, also kept for a stream added later. */
	void SetPlayerGain(const FString& PlayerId, float Gain);

	/** EOS render thread. */
	void HandleMixedAudio(TArrayView<int16> Samples, int SampleRate, int Channels, bool bIsSilence, const FString& ChannelName, const FString& PlayerName);
	void HandleUnmixedAudio(TArrayView<int16> Samples, int SampleRate, int Channels, bool bIsSilence, const FString& ChannelName, const FString& PlayerName);

private:
	mutable FCriticalSection Mutex;
	TMap<FString, TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>> Streams;
	TMap<FString, float> PlayerGains;
};

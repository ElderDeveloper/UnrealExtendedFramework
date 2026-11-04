// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSVoiceReceiveRouter.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

FEEOSVoiceReceiveStream::FEEOSVoiceReceiveStream(const int32 InOutputSampleRate, const int32 InBufferFrames, FOutput&& InOutput)
	: OutputSampleRate(FMath::Max(1, InOutputSampleRate))
	, BufferFrames(FMath::Max(1, InBufferFrames))
	, Output(MoveTemp(InOutput))
{
}

void FEEOSVoiceReceiveStream::Push(const TArrayView<const int16> Samples, const int32 SampleRate, const int32 Channels,
	const float MasterGain, const FString& Room, const double NowSeconds)
{
	if (Channels <= 0 || SampleRate <= 0 || !Output)
	{
		return;
	}
	const int32 Frames = Samples.Num() / Channels;
	if (Frames <= 0)
	{
		return;
	}

	// What the receiver holds when playback starts; past that much silence it has played it all.
	const double BufferSeconds = static_cast<double>(BufferFrames) / static_cast<double>(OutputSampleRate);
	const bool bPaused = LastPushSeconds >= 0.0 && NowSeconds - LastPushSeconds > BufferSeconds;
	if (!Room.IsEmpty() && Room != SourceRoom)
	{
		if (!SourceRoom.IsEmpty() && !bPaused)
		{
			return;
		}
		SourceRoom = Room;
	}
	if (bPaused)
	{
		// Gather a fresh cushion. A start shorter than the cushion, cut off by the pause, is dropped.
		bFlowing = false;
		Pending.Reset();
	}
	LastPushSeconds = NowSeconds;

	const float Scale = FMath::Clamp(Gain.load(std::memory_order_relaxed) * MasterGain, 0.0f, 4.0f) / 32768.0f;
	Mono.SetNumUninitialized(Frames, EAllowShrinking::No);
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		int32 Sum = 0;
		for (int32 Channel = 0; Channel < Channels; ++Channel)
		{
			Sum += Samples[Frame * Channels + Channel];
		}
		Mono[Frame] = static_cast<float>(Sum) / static_cast<float>(Channels) * Scale;
	}

	if (SampleRate != InputSampleRate)
	{
		InputSampleRate = SampleRate;
		ResamplePosition = 0.0;
		PreviousSample = 0.0f;
	}

	const TArray<float>* Converted = &Mono;
	if (SampleRate != OutputSampleRate)
	{
		// Linear interpolation: enough for speech, and it carries its position across chunks without a seam.
		const double Step = static_cast<double>(SampleRate) / static_cast<double>(OutputSampleRate);
		const double LastFrame = static_cast<double>(Frames - 1);
		Resampled.Reset();
		while (ResamplePosition <= LastFrame)
		{
			const int32 Index = FMath::FloorToInt32(ResamplePosition);
			const float Fraction = static_cast<float>(ResamplePosition - Index);
			const float From = Index < 0 ? PreviousSample : Mono[Index];
			const float To = Index + 1 < Frames ? Mono[Index + 1] : From;
			Resampled.Add(From + (To - From) * Fraction);
			ResamplePosition += Step;
		}
		ResamplePosition -= static_cast<double>(Frames);
		PreviousSample = Mono[Frames - 1];
		Converted = &Resampled;
	}

	if (bFlowing)
	{
		Output(Converted->GetData(), Converted->Num());
		return;
	}

	Pending.Append(*Converted);
	if (Pending.Num() >= BufferFrames)
	{
		Output(Pending.GetData(), Pending.Num());
		Pending.Reset();
		bFlowing = true;
	}
}

void FEEOSVoiceReceiveRouter::AddStream(const FString& PlayerId, const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>& Stream)
{
	FScopeLock Lock(&Mutex);
	if (const float* PlayerGain = PlayerGains.Find(PlayerId))
	{
		Stream->Gain.store(*PlayerGain);
	}
	Streams.Add(PlayerId, Stream);
}

bool FEEOSVoiceReceiveRouter::HasStream(const FString& PlayerId) const
{
	FScopeLock Lock(&Mutex);
	return Streams.Contains(PlayerId);
}

void FEEOSVoiceReceiveRouter::RemoveAllStreams()
{
	FScopeLock Lock(&Mutex);
	Streams.Empty();
}

void FEEOSVoiceReceiveRouter::SetPlayerGain(const FString& PlayerId, const float Gain)
{
	FScopeLock Lock(&Mutex);
	const float Clamped = FMath::Clamp(Gain, 0.0f, 2.0f);
	PlayerGains.Add(PlayerId, Clamped);
	if (const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>* Stream = Streams.Find(PlayerId))
	{
		(*Stream)->Gain.store(Clamped);
	}
}

void FEEOSVoiceReceiveRouter::HandleMixedAudio(TArrayView<int16> Samples, int /*SampleRate*/, int /*Channels*/, bool /*bIsSilence*/,
	const FString& /*ChannelName*/, const FString& /*PlayerName*/)
{
	// The SDK renders this buffer after the callback, so zeroing it is what keeps EOS from playing the voices a second time in 2D.
	if (bSilenceMix.load(std::memory_order_relaxed) && Samples.Num() > 0)
	{
		FMemory::Memzero(Samples.GetData(), Samples.Num() * sizeof(int16));
	}
}

void FEEOSVoiceReceiveRouter::HandleUnmixedAudio(TArrayView<int16> Samples, const int SampleRate, const int Channels, bool /*bIsSilence*/,
	const FString& ChannelName, const FString& PlayerName)
{
	TSharedPtr<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe> Stream;
	{
		FScopeLock Lock(&Mutex);
		if (const TSharedRef<FEEOSVoiceReceiveStream, ESPMode::ThreadSafe>* Found = Streams.Find(PlayerName))
		{
			Stream = *Found;
		}
	}
	if (Stream)
	{
		Stream->Push(Samples, SampleRate, Channels, MasterGain.load(std::memory_order_relaxed), ChannelName, FPlatformTime::Seconds());
	}
}

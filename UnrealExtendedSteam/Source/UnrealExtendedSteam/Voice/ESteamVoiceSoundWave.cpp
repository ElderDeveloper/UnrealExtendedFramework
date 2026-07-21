// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Voice/ESteamVoiceSoundWave.h"
#include "Sound/SoundGroups.h"

UESteamVoiceSoundWave::UESteamVoiceSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Steam voice is single-channel 16-bit PCM.
	NumChannels = 1;
	SampleByteSize = 2;
	bLooping = false;
	bProcedural = true;
	SoundGroup = SOUNDGROUP_Voice;

	// Keep the sound alive through the silent gaps between voice packets.
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

	// Procedural waves have no fixed length; mirrors INDEFINITELY_LOOPING_DURATION
	// (AudioMixerCore/AudioDefines.h) without pulling in the extra include.
	Duration = 10000.0f;

	// Lowest rate the Steam voice decoder supports; overwritten by InitializeSteamVoice.
	SetSampleRate(11025);
}

void UESteamVoiceSoundWave::InitializeSteamVoice(int32 InSampleRate)
{
	SetSampleRate(static_cast<uint32>(FMath::Max(InSampleRate, 1)));
}

void UESteamVoiceSoundWave::QueueDecompressedAudio(const TArray<uint8>& Pcm)
{
	if (Pcm.Num() > 0)
	{
		QueueAudio(Pcm.GetData(), Pcm.Num());
	}
}

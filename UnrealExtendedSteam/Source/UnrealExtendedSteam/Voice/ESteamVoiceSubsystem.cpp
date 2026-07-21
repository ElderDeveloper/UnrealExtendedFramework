// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Voice/ESteamVoiceSubsystem.h"
#include "Voice/ESteamVoiceSoundWave.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

void UESteamVoiceSubsystem::Deinitialize()
{
	Super::Deinitialize();
	// HandleSteamClientShutdown already ran when Steam was up; this covers the Steam-down path.
	RemoveVoiceTicker();
	bRecording = false;
}

void UESteamVoiceSubsystem::HandleSteamClientShutdown()
{
	RemoveVoiceTicker();
	bRecording = false;
}

void UESteamVoiceSubsystem::StartVoiceRecording()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bRecording)
	{
		return;
	}

	if (!IsSteamAvailable() || !SteamUser())
	{
		LogSteamUnavailable(TEXT("StartVoiceRecording"));
		return;
	}

	SteamUser()->StartVoiceRecording();
	if (SteamFriends())
	{
		SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), true);
	}

	bRecording = true;

	if (!VoiceTickerHandle.IsValid())
	{
		VoiceTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UESteamVoiceSubsystem::TickVoiceCapture));
	}
#else
	LogSteamUnavailable(TEXT("StartVoiceRecording"));
#endif
}

void UESteamVoiceSubsystem::StopVoiceRecording()
{
	RemoveVoiceTicker();

#if WITH_EXTENDEDSTEAM_SDK
	if (bRecording && IsSteamAvailable() && SteamUser())
	{
		SteamUser()->StopVoiceRecording();
		if (SteamFriends())
		{
			SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), false);
		}
	}
#endif

	bRecording = false;
}

bool UESteamVoiceSubsystem::TickVoiceCapture(float /*DeltaTime*/)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (bRecording && IsSteamAvailable() && SteamUser())
	{
		uint32 CompressedSize = 0;
		while (SteamUser()->GetAvailableVoice(&CompressedSize) == k_EVoiceResultOK && CompressedSize > 0)
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(static_cast<int32>(CompressedSize));

			uint32 BytesWritten = 0;
			const EVoiceResult Result = SteamUser()->GetVoice(true, Buffer.GetData(), CompressedSize, &BytesWritten);
			if (Result != k_EVoiceResultOK || BytesWritten == 0)
			{
				break;
			}

			Buffer.SetNum(static_cast<int32>(BytesWritten), EAllowShrinking::No);
			OnVoiceCaptured.Broadcast(Buffer);
		}
	}
#endif
	return true;
}

void UESteamVoiceSubsystem::RemoveVoiceTicker()
{
	if (VoiceTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VoiceTickerHandle);
		VoiceTickerHandle.Reset();
	}
}

bool UESteamVoiceSubsystem::DecompressVoice(const TArray<uint8>& CompressedData, TArray<uint8>& OutPcm)
{
	OutPcm.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser())
	{
		LogSteamUnavailable(TEXT("DecompressVoice"));
		return false;
	}

	if (CompressedData.Num() == 0)
	{
		return false;
	}

	const uint32 SampleRate = SteamUser()->GetVoiceOptimalSampleRate();
	if (SampleRate == 0)
	{
		return false;
	}

	// Valve suggests starting with a 20kb buffer; on k_EVoiceResultBufferTooSmall the SDK
	// reports the required size in BytesWritten, so grow and retry.
	uint32 BufferSize = 20 * 1024;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		OutPcm.SetNumUninitialized(static_cast<int32>(BufferSize));

		uint32 BytesWritten = 0;
		const EVoiceResult Result = SteamUser()->DecompressVoice(
			CompressedData.GetData(), static_cast<uint32>(CompressedData.Num()),
			OutPcm.GetData(), BufferSize, &BytesWritten, SampleRate);

		if (Result == k_EVoiceResultOK)
		{
			OutPcm.SetNum(static_cast<int32>(BytesWritten), EAllowShrinking::No);
			return BytesWritten > 0;
		}

		if (Result != k_EVoiceResultBufferTooSmall)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("DecompressVoice failed (EVoiceResult %d)"), static_cast<int32>(Result));
			break;
		}

		BufferSize = FMath::Max<uint32>(BytesWritten, BufferSize * 2);
	}

	OutPcm.Reset();
	return false;
#else
	LogSteamUnavailable(TEXT("DecompressVoice"));
	return false;
#endif
}

int32 UESteamVoiceSubsystem::GetVoiceOptimalSampleRate() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		return static_cast<int32>(SteamUser()->GetVoiceOptimalSampleRate());
	}
#endif
	return 0;
}

UESteamVoiceSoundWave* UESteamVoiceSubsystem::CreateVoiceSoundWave()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser())
	{
		LogSteamUnavailable(TEXT("CreateVoiceSoundWave"));
		return nullptr;
	}

	const int32 SampleRate = GetVoiceOptimalSampleRate();
	if (SampleRate <= 0)
	{
		return nullptr;
	}

	UESteamVoiceSoundWave* Wave = NewObject<UESteamVoiceSoundWave>(GetTransientPackage());
	Wave->InitializeSteamVoice(SampleRate);
	return Wave;
#else
	LogSteamUnavailable(TEXT("CreateVoiceSoundWave"));
	return nullptr;
#endif
}

bool UESteamVoiceSubsystem::PlayVoiceData(UESteamVoiceSoundWave* Target, const TArray<uint8>& CompressedData)
{
	if (!Target)
	{
		return false;
	}

	TArray<uint8> Pcm;
	if (!DecompressVoice(CompressedData, Pcm))
	{
		return false;
	}

	Target->QueueDecompressedAudio(Pcm);
	return true;
}

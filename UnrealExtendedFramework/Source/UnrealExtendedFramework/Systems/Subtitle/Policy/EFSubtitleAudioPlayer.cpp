// EFSubtitleAudioPlayer.cpp
#include "EFSubtitleAudioPlayer.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

USoundBase* UEFSubtitleAudioPlayer::ResolveCultureSound(const FEFSubtitleEntry& Entry) const
{
	if (Entry.CultureSounds.Num() > 0)
	{
		const FString CurrentCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		const FString CurrentLanguage = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();

		for (const FEFCultureSound& CS : Entry.CultureSounds)
		{
			if (CS.CultureCode == CurrentCulture)
			{
				if (USoundBase* Sound = CS.Sound.LoadSynchronous())
				{
					return Sound;
				}
			}
		}

		for (const FEFCultureSound& CS : Entry.CultureSounds)
		{
			if (CS.CultureCode == CurrentLanguage)
			{
				if (USoundBase* Sound = CS.Sound.LoadSynchronous())
				{
					return Sound;
				}
			}
		}
	}

	if (!Entry.VoiceSound.IsNull())
	{
		return Entry.VoiceSound.LoadSynchronous();
	}

	return nullptr;
}

void UEFSubtitleAudioPlayer_Default::PlaySubtitleAudio(UObject* WorldContextObject, const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
{
	USoundBase* Sound = ResolveCultureSound(Entry);
	if (!Sound || !WorldContextObject)
	{
		return;
	}

	switch (Request.ExecutionType)
	{
	case EEFSubtitleExecutionType::Boundless:
	case EEFSubtitleExecutionType::PlayerOnly:
		UGameplayStatics::PlaySound2D(WorldContextObject, Sound);
		break;

	case EEFSubtitleExecutionType::Location:
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Request.WorldLocation);
		break;

	case EEFSubtitleExecutionType::AttachedToActor:
		if (Request.AttachActor.IsValid())
		{
			UGameplayStatics::SpawnSoundAttached(Sound, Request.AttachActor->GetRootComponent());
		}
		else
		{
			UGameplayStatics::PlaySound2D(WorldContextObject, Sound);
		}
		break;
	}
}

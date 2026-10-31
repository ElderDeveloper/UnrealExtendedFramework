// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleAudioPlayer.cpp
#include "EFSubtitleAudioPlayer.h"

#include "Assets/EFAssetLoading.h"
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
				if (USoundBase* Sound = EFAssets::Resolve(CS.Sound, TEXT("UEFSubtitleAudioPlayer")))
				{
					return Sound;
				}
			}
		}

		for (const FEFCultureSound& CS : Entry.CultureSounds)
		{
			if (CS.CultureCode == CurrentLanguage)
			{
				if (USoundBase* Sound = EFAssets::Resolve(CS.Sound, TEXT("UEFSubtitleAudioPlayer")))
				{
					return Sound;
				}
			}
		}
	}

	if (!Entry.VoiceSound.IsNull())
	{
		return EFAssets::Resolve(Entry.VoiceSound, TEXT("UEFSubtitleAudioPlayer"));
	}

	return nullptr;
}

TSharedPtr<FStreamableHandle> UEFSubtitleAudioPlayer::PreloadSubtitleAudio(const FEFSubtitleEntry& Entry) const
{
	TArray<FSoftObjectPath> Paths;
	GatherSubtitleSoundPaths(Entry, Paths);
	return EFAssets::Preload(MoveTemp(Paths));
}

void UEFSubtitleAudioPlayer::GatherSubtitleSoundPaths(const FEFSubtitleEntry& Entry, TArray<FSoftObjectPath>& OutPaths)
{
	if (Entry.CultureSounds.Num() > 0)
	{
		const FString CurrentCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		const FString CurrentLanguage = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();

		// Same order as ResolveCultureSound: the full culture first, then the language.
		const auto AddCultureSounds = [&Entry, &OutPaths](const FString& CultureCode)
		{
			bool bAdded = false;
			for (const FEFCultureSound& CS : Entry.CultureSounds)
			{
				if (CS.CultureCode == CultureCode && !CS.Sound.IsNull())
				{
					OutPaths.Add(CS.Sound.ToSoftObjectPath());
					bAdded = true;
				}
			}
			return bAdded;
		};

		if (AddCultureSounds(CurrentCulture) || AddCultureSounds(CurrentLanguage))
		{
			return;
		}
	}

	if (!Entry.VoiceSound.IsNull())
	{
		OutPaths.Add(Entry.VoiceSound.ToSoftObjectPath());
	}
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

// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleAudioPlayer.h - Swappable subtitle voice playback
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "UObject/Object.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleData.h"
#include "EFSubtitleAudioPlayer.generated.h"

class USoundBase;

/**
 * Abstract audio player for subtitle voiceovers.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleAudioPlayer : public UObject
{
	GENERATED_BODY()

public:
	virtual void PlaySubtitleAudio(UObject* WorldContextObject, const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request)
	PURE_VIRTUAL(UEFSubtitleAudioPlayer::PlaySubtitleAudio, );

	virtual USoundBase* ResolveCultureSound(const FEFSubtitleEntry& Entry) const;

	/**
	 * Starts loading the sound Entry will play, so it is in memory when the entry starts. Called
	 * when the entry is queued; keep the handle until it has finished. Null when there is nothing
	 * to load.
	 */
	virtual TSharedPtr<FStreamableHandle> PreloadSubtitleAudio(const FEFSubtitleEntry& Entry) const;

	/**
	 * Adds the sounds ResolveCultureSound looks at first for Entry in the current culture: the
	 * culture's own sounds, else the language's, else the default VoiceSound.
	 */
	static void GatherSubtitleSoundPaths(const FEFSubtitleEntry& Entry, TArray<FSoftObjectPath>& OutPaths);
};

/**
 * Default 2D / location / attached playback with culture sound resolution.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Default Subtitle Audio Player"))
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleAudioPlayer_Default : public UEFSubtitleAudioPlayer
{
	GENERATED_BODY()

public:
	virtual void PlaySubtitleAudio(UObject* WorldContextObject, const FEFSubtitleEntry& Entry, const FEFSubtitleRequest& Request) override;
};

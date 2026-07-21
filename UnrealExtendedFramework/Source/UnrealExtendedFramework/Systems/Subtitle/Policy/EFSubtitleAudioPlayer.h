// EFSubtitleAudioPlayer.h - Swappable subtitle voice playback
#pragma once

#include "CoreMinimal.h"
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

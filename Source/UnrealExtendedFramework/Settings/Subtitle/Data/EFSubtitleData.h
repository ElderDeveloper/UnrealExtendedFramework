// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "EFSubtitleData.generated.h"

UENUM(BlueprintType)
enum class ESubtitleExecutionType : uint8
{
	Boundless,
	Location,
	AttachedToActor
};


USTRUCT(BlueprintType)
struct FExtendedSubtitle : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FText Subtitle;

	// This value will be used if the duration does not directly defined by the FExtendedSubtitleDurationSettings
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float Duration;
	
	// Default sound asset for the subtitle, this sound will be played if the subtitle has no culture sound
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	USoundBase* SubtitleSound;
	
	/*
	 * Key: Culture Name, e.g. "en", "fr", "de" , "tr"
	 * Value: Sound Asset
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FString, TSoftObjectPtr<USoundBase>> CultureSoundMap;
	
	FExtendedSubtitle()
	{
		Subtitle = FText::FromString("");
		Duration = 0;
		SubtitleSound = nullptr;
	}
};


USTRUCT(BlueprintType)
struct FExtendedSubtitleBorderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float BorderSize;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FLinearColor BorderColor;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float BorderOpacity;
	
	FExtendedSubtitleBorderSettings()
	{
		BorderSize = 0;
		BorderColor = FLinearColor::White;
		BorderOpacity = 1;
	}
};


USTRUCT(BlueprintType)
struct FExtendedSubtitleBackgroundSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FLinearColor BackgroundColor;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float BackgroundOpacity;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FVector2D BackgroundSize;
	
	FExtendedSubtitleBackgroundSettings()
	{
		BackgroundColor = FLinearColor::White;
		BackgroundOpacity = 1;
		BackgroundSize = FVector2D(10,10);
	}
};


USTRUCT(BlueprintType)
struct FExtendedSubtitleDurationSettings
{
	GENERATED_BODY()

	// If true, the duration will be sum of TimeForEachLetterCount and TimeForAfterLetterCount. This duration will be forced to the value of SubtitleDuration
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	bool bForceSubtitleDuration = false;
	
	/*
	 * If true ,the subtitle will be displayed for each letter of the subtitle
	 * Based on the duration type of the subtitle, the subtitle animation behavior will change
	 * If subtitle has a duration, the animation will match the duration of the subtitle with minus TimeForAfterLetterCount
	 * If subtitle duration is less than TimeForAfterLetterCount, the animation will be finished with %70 of the duration
	 * If subtitle duration is 0, the animation will be finished with the sum of TimeForEachLetterCount
	 * With Animation, each letter will be displayed one by one with a delay of TimeForEachLetterCount
	 * After the last letter is displayed, the subtitle will be displayed for TimeForAfterLetterCount
	 */
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	bool AnimateSubtitleLetters = false;

	// Added to the duration of the subtitle for each letter, this value will be used if the Subtitle Duration is 0
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float TimeForEachLetterCount = 0.1;

	// Additional time added to the duration of the subtitle after the last letter
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	float TimeForAfterLetterCount = 1;
	
};

// EFSubtitleUserSettings.h — Per-player subtitle preferences via ModularSettings
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFSubtitleUserSettings.generated.h"


/**
 * Subtitles Enabled — user toggle for subtitle display
 */
UCLASS(Blueprintable, DisplayName = "Subtitle Enabled Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleEnabledSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFSubtitleEnabledSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitlesEnabled")), false);
		DisplayName = NSLOCTEXT("Settings", "SubtitlesEnabled", "Subtitles Enabled");
		ConfigCategory = TEXT("Accessibility");
		DefaultValue = true;
		Value = true;
	}

	virtual void Apply_Implementation() override
	{
		UE_LOG(LogTemp, Log, TEXT("Applied Subtitles Enabled: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};


/**
 * Subtitle Text Size — scales subtitle text
 */
UCLASS(Blueprintable, DisplayName = "Subtitle Text Size Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleTextSizeSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFSubtitleTextSizeSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleTextSize")), false);
		DisplayName = NSLOCTEXT("Settings", "SubtitleTextSize", "Subtitle Text Size");
		ConfigCategory = TEXT("Accessibility");
		DefaultValue = 1.0f;
		Value = 1.0f;
		Min = 0.5f;
		Max = 3.0f;
	}

	virtual void Apply_Implementation() override
	{
		UE_LOG(LogTemp, Log, TEXT("Applied Subtitle Text Size: %.2f"), Value);
	}
};


/**
 * Subtitle Background Opacity — controls background transparency
 */
UCLASS(Blueprintable, DisplayName = "Subtitle Background Opacity Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleBgOpacitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFSubtitleBgOpacitySetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleBgOpacity")), false);
		DisplayName = NSLOCTEXT("Settings", "SubtitleBgOpacity", "Subtitle Background Opacity");
		ConfigCategory = TEXT("Accessibility");
		DefaultValue = 0.7f;
		Value = 0.7f;
		Min = 0.0f;
		Max = 1.0f;
	}

	virtual void Apply_Implementation() override
	{
		UE_LOG(LogTemp, Log, TEXT("Applied Subtitle Background Opacity: %.2f"), Value);
	}
};


/**
 * Speaker Labels Enabled — toggle speaker name display
 */
UCLASS(Blueprintable, DisplayName = "Subtitle Speaker Labels Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleSpeakerLabelsSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFSubtitleSpeakerLabelsSetting()
	{
		SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleSpeakerLabels")), false);
		DisplayName = NSLOCTEXT("Settings", "SubtitleSpeakerLabels", "Show Speaker Labels");
		ConfigCategory = TEXT("Accessibility");
		DefaultValue = true;
		Value = true;
	}

	virtual void Apply_Implementation() override
	{
		UE_LOG(LogTemp, Log, TEXT("Applied Subtitle Speaker Labels: %s"), Value ? TEXT("Enabled") : TEXT("Disabled"));
	}
};

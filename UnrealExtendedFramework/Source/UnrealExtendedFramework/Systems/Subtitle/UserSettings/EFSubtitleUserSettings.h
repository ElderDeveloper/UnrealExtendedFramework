// EFSubtitleUserSettings.h — Per-player subtitle preferences via ModularSettings
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFSubtitleUserSettings.generated.h"

UCLASS(Blueprintable, DisplayName = "Subtitle Enabled Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleEnabledSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFSubtitleEnabledSetting();
	virtual void Apply_Implementation() override;
};

UCLASS(Blueprintable, DisplayName = "Subtitle Text Size Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleTextSizeSetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFSubtitleTextSizeSetting();
	virtual void Apply_Implementation() override;
};

UCLASS(Blueprintable, DisplayName = "Subtitle Background Opacity Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleBgOpacitySetting : public UEFModularSettingsFloat
{
	GENERATED_BODY()

public:
	UEFSubtitleBgOpacitySetting();
	virtual void Apply_Implementation() override;
};

UCLASS(Blueprintable, DisplayName = "Subtitle Speaker Labels Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleSpeakerLabelsSetting : public UEFModularSettingsBool
{
	GENERATED_BODY()

public:
	UEFSubtitleSpeakerLabelsSetting();
	virtual void Apply_Implementation() override;
};

/**
 * Style profile multi-select. Values should match UEFSubtitleStyleProfile::ProfileId
 * entries listed in UEFSubtitleProjectSettings::AvailableStyleProfiles.
 * Empty / "Default" clears the profile override.
 */
UCLASS(Blueprintable, DisplayName = "Subtitle Style Profile Setting")
class UNREALEXTENDEDFRAMEWORK_API UEFSubtitleStyleProfileSetting : public UEFModularSettingsMultiSelect
{
	GENERATED_BODY()

public:
	UEFSubtitleStyleProfileSetting();
	virtual void Apply_Implementation() override;
	virtual void RefreshValues_Implementation() override;
};

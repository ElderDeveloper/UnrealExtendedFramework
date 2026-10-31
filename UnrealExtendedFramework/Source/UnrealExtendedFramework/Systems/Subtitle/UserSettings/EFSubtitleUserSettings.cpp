// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleUserSettings.cpp
#include "EFSubtitleUserSettings.h"
#include "Internationalization/StringTableRegistry.h"

#include "Assets/EFAssetLoading.h"
#include "GameplayTagsManager.h"

#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleProjectSettings.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Data/EFSubtitleStyleProfile.h"
#include "UnrealExtendedFramework/Systems/Subtitle/Subsystem/EFSubtitleLocalSubsystem.h"
#include "UnrealExtendedFramework/Systems/Subtitle/UserSettings/EFSubtitleSettingsHelpers.h"

UEFSubtitleEnabledSetting::UEFSubtitleEnabledSetting()
{
	SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitlesEnabled")), false);
	DisplayName = LOCTABLE("ExtendedFramework", "Settings.SubtitlesEnabled");
	ConfigCategory = TEXT("Accessibility");
	DefaultValue = true;
	Value = true;
}

void UEFSubtitleEnabledSetting::Apply_Implementation()
{
	if (UEFSubtitleLocalSubsystem* Local = EFSubtitleSettingsHelpers::ResolveLocalSubsystem(this))
	{
		Local->SetSubtitlesEnabled(Value);
	}
}

UEFSubtitleTextSizeSetting::UEFSubtitleTextSizeSetting()
{
	SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleTextSize")), false);
	DisplayName = LOCTABLE("ExtendedFramework", "Settings.SubtitleTextSize");
	ConfigCategory = TEXT("Accessibility");
	DefaultValue = 1.0f;
	Value = 1.0f;
	Min = 0.5f;
	Max = 3.0f;
	DisplayMin = 0.5f;
	DisplayMax = 3.0f;
}

void UEFSubtitleTextSizeSetting::Apply_Implementation()
{
	if (UEFSubtitleLocalSubsystem* Local = EFSubtitleSettingsHelpers::ResolveLocalSubsystem(this))
	{
		Local->SetSubtitleTextScale(Value);
	}
}

UEFSubtitleBgOpacitySetting::UEFSubtitleBgOpacitySetting()
{
	SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleBgOpacity")), false);
	DisplayName = LOCTABLE("ExtendedFramework", "Settings.SubtitleBgOpacity");
	ConfigCategory = TEXT("Accessibility");
	DefaultValue = 0.7f;
	Value = 0.7f;
	Min = 0.0f;
	Max = 1.0f;
	DisplayMin = 0.0f;
	DisplayMax = 1.0f;
}

void UEFSubtitleBgOpacitySetting::Apply_Implementation()
{
	if (UEFSubtitleLocalSubsystem* Local = EFSubtitleSettingsHelpers::ResolveLocalSubsystem(this))
	{
		Local->SetSubtitleBackgroundOpacity(Value);
	}
}

UEFSubtitleSpeakerLabelsSetting::UEFSubtitleSpeakerLabelsSetting()
{
	SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleSpeakerLabels")), false);
	DisplayName = LOCTABLE("ExtendedFramework", "Settings.SubtitleSpeakerLabels");
	ConfigCategory = TEXT("Accessibility");
	DefaultValue = true;
	Value = true;
}

void UEFSubtitleSpeakerLabelsSetting::Apply_Implementation()
{
	if (UEFSubtitleLocalSubsystem* Local = EFSubtitleSettingsHelpers::ResolveLocalSubsystem(this))
	{
		Local->SetShowSpeakerLabels(Value);
	}
}

UEFSubtitleStyleProfileSetting::UEFSubtitleStyleProfileSetting()
{
	SettingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Settings.Accessibility.SubtitleStyleProfile")), false);
	DisplayName = LOCTABLE("ExtendedFramework", "Settings.SubtitleStyleProfile");
	ConfigCategory = TEXT("Accessibility");
	DefaultValue = TEXT("Default");
	Values = { TEXT("Default") };
	DisplayNames = { LOCTABLE("ExtendedFramework", "Settings.SubtitleStyleDefault") };
	SelectedIndex = 0;
}

void UEFSubtitleStyleProfileSetting::OnRegistered()
{
	Super::OnRegistered();

	// UEFSubtitleSubsystem starts the same load when the game instance starts, so the profiles are
	// usually in memory already when this setting builds its options.
	if (!StyleProfilesHandle.IsValid())
	{
		StyleProfilesHandle = EFSubtitleSettingsHelpers::PreloadStyleProfiles();
	}
}

void UEFSubtitleStyleProfileSetting::RefreshValues_Implementation()
{
	Values.Reset();
	DisplayNames.Reset();

	Values.Add(TEXT("Default"));
	DisplayNames.Add(LOCTABLE("ExtendedFramework", "Settings.SubtitleStyleDefault"));

	if (const UEFSubtitleProjectSettings* Settings = GetDefault<UEFSubtitleProjectSettings>())
	{
		for (const TSoftObjectPtr<UEFSubtitleStyleProfile>& SoftProfile : Settings->AvailableStyleProfiles)
		{
			if (const UEFSubtitleStyleProfile* Profile = EFAssets::Resolve(SoftProfile, TEXT("UEFSubtitleStyleProfileSetting")))
			{
				if (Profile->ProfileId.IsNone())
				{
					continue;
				}

				Values.Add(Profile->ProfileId.ToString());
				DisplayNames.Add(Profile->DisplayName.IsEmpty()
					? FText::FromName(Profile->ProfileId)
					: Profile->DisplayName);
			}
		}
	}

	// Keep selection stable after refresh.
	const FString Current = GetValueAsString();
	const int32 Found = Values.Find(Current.IsEmpty() ? DefaultValue : Current);
	SelectedIndex = Found != INDEX_NONE ? Found : 0;
}

void UEFSubtitleStyleProfileSetting::Apply_Implementation()
{
	UEFSubtitleLocalSubsystem* Local = EFSubtitleSettingsHelpers::ResolveLocalSubsystem(this);
	if (!Local)
	{
		return;
	}

	const FString Selected = GetValueAsString();
	if (Selected.IsEmpty() || Selected.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
	{
		Local->SetStyleProfileId(NAME_None);
		return;
	}

	Local->SetStyleProfileId(FName(*Selected));
}

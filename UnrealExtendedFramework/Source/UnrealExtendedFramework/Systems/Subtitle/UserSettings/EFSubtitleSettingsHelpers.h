// EFSubtitleSettingsHelpers.h - Resolve LocalSubsystem from ModularSettings
#pragma once

#include "CoreMinimal.h"

class UEFModularSettingsBase;
class UEFSubtitleLocalSubsystem;
class UEFSubtitleStyleProfile;

namespace EFSubtitleSettingsHelpers
{
	/** Resolve the local subtitle subsystem for a ModularSetting instance. */
	UEFSubtitleLocalSubsystem* ResolveLocalSubsystem(const UEFModularSettingsBase* Setting);

	/** Find a style profile by id from project settings. */
	UEFSubtitleStyleProfile* ResolveStyleProfile(FName ProfileId);
}

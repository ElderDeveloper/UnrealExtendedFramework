// Copyright Moon Punch Games. All Rights Reserved.

// EFSubtitleSettingsHelpers.h - Resolve LocalSubsystem from ModularSettings
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"

class UEFModularSettingsBase;
class UEFSubtitleLocalSubsystem;
class UEFSubtitleStyleProfile;

namespace EFSubtitleSettingsHelpers
{
	/** Resolve the local subtitle subsystem for a ModularSetting instance. */
	UEFSubtitleLocalSubsystem* ResolveLocalSubsystem(const UEFModularSettingsBase* Setting);

	/** Find a style profile by id from project settings. */
	UEFSubtitleStyleProfile* ResolveStyleProfile(FName ProfileId);

	/**
	 * Starts loading every style profile listed in project settings, so ResolveStyleProfile and the
	 * style setting's option list find them in memory. Keep the handle while they are needed.
	 */
	TSharedPtr<FStreamableHandle> PreloadStyleProfiles();
}

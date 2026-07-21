// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class ULocalizationTarget;

/**
 * B-1 zero-state onboarding: detects an unconfigured localization pipeline and performs the
 * setup steps the Localization Dashboard would normally require by hand. All changes go through
 * the same objects the dashboard edits (ULocalizationTarget::Settings synced back into
 * ULocalizationSettings' config), so the two tools stay interchangeable.
 */
namespace EELocSetup
{
	enum class EEESetupState : uint8
	{
		/** No game localization targets exist. */
		NoTargets,
		/** Target exists but has no valid native culture ("Target 'Game' has no valid native culture."). */
		NoCultures,
		/** Cultures are set but nothing is configured to be gathered. */
		NoGatherPaths,
		/** Configured, but gather has never produced a manifest. */
		NeverGathered,
		/** Pipeline is usable; the normal grid applies. */
		Ready
	};

	/** State for the named target (empty name = first game target, NoTargets when none exist). */
	UNREALEXTENDEDFRAMEWORKEDITOR_API EEESetupState DetectState(const FString& TargetName);

	UNREALEXTENDEDFRAMEWORKEDITOR_API ULocalizationTarget* CreateGameTarget(const FString& TargetName, FString& OutError);

	/** Sets native (index 0) + foreign cultures on the target and persists. */
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool ConfigureCultures(const FString& TargetName,
		const FString& NativeCulture, const TArray<FString>& ForeignCultures, FString& OutError);

	/**
	 * Adds the standard gather configuration: C++ source ("Source" directory, default text-file
	 * extensions) and/or Content packages ("Content/*", excluding "Content/Localization/*").
	 */
	UNREALEXTENDEDFRAMEWORKEDITOR_API bool AddDefaultGatherPaths(const FString& TargetName,
		bool bGatherSource, bool bGatherContent, FString& OutError);

	/** Human-readable description of what a setup state means and what the next step does. */
	UNREALEXTENDEDFRAMEWORKEDITOR_API FText DescribeState(EEESetupState State);
}

#endif // WITH_EDITOR

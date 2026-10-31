// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EFModularSettingsDeviceRules.generated.h"

UNREALEXTENDEDFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogEFDeviceSettings, Log, All);

/**
 * Whether a local setting can be seen and changed on this device.
 *
 * Locked and Hidden force the setting to its device default; the player's own saved value stays in
 * the save file, so it comes back on a device that allows it.
 */
UENUM(BlueprintType)
enum class EEFSettingAvailability : uint8
{
	Visible,
	/** Shown, greyed out, not changeable. */
	Locked,
	/** Not shown at all. */
	Hidden
};

/** Which save bucket a local setting belongs to. */
UENUM(BlueprintType)
enum class EEFSettingScope : uint8
{
	/** Decided by UEFModularProjectSettings::DeviceScopedSettings. */
	Auto,
	/** Follows the player across devices (audio, controls, gameplay). */
	User,
	/** Kept per device family (graphics, display). */
	Device
};

/**
 * One line of a `[<Name> ModularSettingsDeviceRules]` section in DeviceProfiles.ini:
 *
 *   +Rules=(Setting="Settings.Graphics.ShadowQuality", Default="Low", HiddenOptions=("High","Ultra"))
 *
 * A rule replaces any rule for the same setting from an earlier section, as a whole, the way a child
 * profile's cvar replaces its parent's. Values are written as the setting saves them: the option id for
 * a list, "true" / "false" for a switch, a number for a slider.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFModularSettingDeviceRule
{
	GENERATED_BODY()

	/** The setting's gameplay tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FName Setting;

	/** The device default. Empty keeps the default the setting would have without this rule. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FString Default;

	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	EEFSettingAvailability Availability = EEFSettingAvailability::Visible;

	/** Multi-select: only these options are offered. Applied before HiddenOptions. Empty = all. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	TArray<FString> AllowedOptions;

	/** Multi-select: these options are never offered. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	TArray<FString> HiddenOptions;

	/** Float: narrowed range, in the setting's own units. Empty keeps the authored bound. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FString Min;

	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FString Max;
};

/**
 * One `+HardwareRules=(...)` line. Every condition that is set must hold; a rule with no condition
 * never matches. A matching rule applies the `[<Apply> ModularSettingsDeviceRules]` section after the
 * device profile chain.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFModularSettingHardwareRule
{
	GENERATED_BODY()

	/** Dedicated video memory below this many MB. 0 = no condition. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	int32 VideoMemoryBelowMB = 0;

	/** Dedicated video memory at least this many MB. 0 = no condition. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	int32 VideoMemoryAtLeastMB = 0;

	/** Installed system memory below this many GB. 0 = no condition. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	int32 SystemMemoryBelowGB = 0;

	/** Installed system memory at least this many GB. 0 = no condition. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	int32 SystemMemoryAtLeastGB = 0;

	/** Regular expression matched against the graphics adapter name (case-insensitive). Empty = any. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FString GPUName;

	/** The ModularSettingsDeviceRules section to apply when every condition holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Device Rules")
	FName Apply;
};

/** The hardware facts hardware rules are evaluated against. */
struct UNREALEXTENDEDFRAMEWORK_API FEFDeviceHardwareInfo
{
	FString GPUName;
	int64 VideoMemoryMB = 0;
	int32 SystemMemoryGB = 0;

	/** Reads the current machine. Video memory is 0 until the RHI has been initialised. */
	static FEFDeviceHardwareInfo Gather();

	FString ToString() const;
};

/** Everything the device rules resolved to, plus where each piece came from (Settings.Device.Dump). */
struct UNREALEXTENDEDFRAMEWORK_API FEFResolvedDeviceRules
{
	/** The active device profile, as the device profile manager reports it. */
	FString ActiveProfile;

	/** The profile and its parents, root first. */
	TArray<FString> ProfileChain;

	/** Device profile fragments the engine matched for this device. */
	TArray<FString> MatchedFragments;

	/** Sections applied because a hardware rule matched, in order. */
	TArray<FString> HardwareSections;

	/** Which chain entry supplied the hardware rules (the most specific one with a rules section). */
	FString HardwareRulesSource;

	/** Save bucket for device-scoped settings. Never empty. */
	FString DeviceFamily;

	FEFDeviceHardwareInfo Hardware;

	/** Merged rules, keyed by setting tag name. */
	TMap<FName, FEFModularSettingDeviceRule> Rules;

	/** The section each merged rule came from. */
	TMap<FName, FString> RuleSources;

	/** Problems found while reading: unparsable lines, unknown tags. Logged as warnings. */
	TArray<FString> Warnings;

	const FEFModularSettingDeviceRule* FindRule(const FGameplayTag& Tag) const
	{
		return Tag.IsValid() ? Rules.Find(Tag.GetTagName()) : nullptr;
	}

	/** True when the profile chain or a matched fragment has this name. */
	bool ContainsProfile(const FString& ProfileName) const;
};

namespace EFDeviceRules
{
	/** Name of the config section holding a rules set: "<Name> ModularSettingsDeviceRules". */
	UNREALEXTENDEDFRAMEWORK_API FString MakeSectionName(const FString& Name);

	/**
	 * Reads the device rules for the active device profile from DeviceProfiles.ini.
	 *
	 * Order, later replacing earlier for the same setting: every profile of the chain root first, then
	 * the matched fragments, then the sections picked by hardware rules. Hardware rules come from the
	 * most specific profile in the chain that has a rules section, so a profile can opt out of its
	 * parent's hardware rules by having a section of its own without any.
	 */
	UNREALEXTENDEDFRAMEWORK_API FEFResolvedDeviceRules Resolve();

	/** The active device profile's name, or the platform's when the manager has none yet. */
	UNREALEXTENDEDFRAMEWORK_API FString GetActiveProfileName();

	/** The profile chain for a profile name, root first. */
	UNREALEXTENDEDFRAMEWORK_API TArray<FString> BuildProfileChain(const FString& ProfileName);

	/** Whether every condition of a hardware rule holds for this hardware. */
	UNREALEXTENDEDFRAMEWORK_API bool Matches(const FEFModularSettingHardwareRule& Rule, const FEFDeviceHardwareInfo& Hardware);
}

// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"

EXTENDEDDEVICEPROFILESELECTOR_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedDeviceProfileSelector, Log, All);

/**
 * Chooses the device profile for this session.
 *
 * Order:
 *  1. In the editor, the platform's own answer (WindowsEditor); preview other devices with dp.Override.
 *  2. A remembered profile: [ExtendedDeviceProfileSelector] SelectedProfile in GameUserSettings.ini,
 *     when DeviceProfiles.ini has a section for it. That file is machine-local (cloud saves sync
 *     SaveGames, not Saved/Config), which is what a device choice needs. Whatever runs later in the
 *     session and knows better (the Steam client's own Steam Deck check) writes it, so from the second
 *     launch the right profile is active before the first frame.
 *  3. The first [ExtendedDeviceProfileSelector] ProfileRules entry that matches:
 *       +ProfileRules=(Profile="SteamDeck", GPUDescription="VANGOGH|Custom GPU 0405")
 *       +ProfileRules=(Profile="SteamDeck", Environment="SteamDeck=1")
 *     Conditions (all that are set must hold; regular expressions are case-insensitive):
 *       GPUDescription  regex against the primary GPU's name and driver description
 *       CPUBrand        regex against the CPU brand string
 *       Environment     "NAME=VALUE" (exact) or "NAME" (set and not empty)
 *       CommandLine     a token that must appear on the command line
 *       MinMemoryGB / MaxMemoryGB  installed system memory, inclusive
 *  4. The platform's own selector (FallbackSelectorModule, default WindowsDeviceProfileSelector on
 *     Windows), else the platform name. Never an empty string: the engine uses the answer as is.
 *
 * It also answers the engine's matching rules with SRC_EnvSteamDeck, SRC_TotalPhysicalGB and
 * SRC_CPUBrand. Every decision is logged with the strings it compared.
 */
class EXTENDEDDEVICEPROFILESELECTOR_API FExtendedDeviceProfileSelectorModule : public IDeviceProfileSelectorModule
{
public:
	//~ IDeviceProfileSelectorModule
	virtual const FString GetRuntimeDeviceProfileName() override;
	virtual bool GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT) override;

	//~ IModuleInterface
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}

	/** Config section (engine ini) for the rules, and (GameUserSettings.ini) for the remembered profile. */
	static const TCHAR* GetConfigSection() { return TEXT("ExtendedDeviceProfileSelector"); }

	/** Key of the remembered profile in GameUserSettings.ini. Other modules may write it directly. */
	static const TCHAR* GetRememberedProfileKey() { return TEXT("SelectedProfile"); }

	/** Remembers a profile for the next launches on this machine (GameUserSettings.ini, flushed now). */
	static void RememberProfile(const FString& ProfileName);

	/** Forgets the remembered profile, so the rules decide again on the next launch. */
	static void ForgetRememberedProfile();

	/** The remembered profile, or empty. */
	static FString GetRememberedProfile();

private:
	FString ChooseProfile() const;
	FString GetFallbackProfile() const;
};

// Copyright Moon Punch Games. All Rights Reserved.

#include "ExtendedDeviceProfileSelector.h"

#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Regex.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogExtendedDeviceProfileSelector);

IMPLEMENT_MODULE(FExtendedDeviceProfileSelectorModule, ExtendedDeviceProfileSelector);

namespace ExtendedDeviceProfileSelectorPrivate
{
	struct FMachineFacts
	{
		FString GPUBrand;
		FString GPUDescription;
		FString CPUBrand;
		FString SteamDeckVariable;
		int32 TotalPhysicalGB = 0;

		static FMachineFacts Gather()
		{
			FMachineFacts Facts;
			Facts.GPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
			Facts.GPUDescription = FPlatformMisc::GetGPUDriverInfo(Facts.GPUBrand).DeviceDescription;
			Facts.CPUBrand = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
			Facts.SteamDeckVariable = FPlatformMisc::GetEnvironmentVariable(TEXT("SteamDeck"));
			Facts.TotalPhysicalGB = static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB);
			return Facts;
		}

		FString ToString() const
		{
			return FString::Printf(TEXT("GPU brand '%s', GPU description '%s', CPU '%s', SteamDeck='%s', %d GB"),
				*GPUBrand, *GPUDescription, *CPUBrand, *SteamDeckVariable, TotalPhysicalGB);
		}
	};

	bool MatchesRegex(const FString& Pattern, const FString& Subject)
	{
		if (Subject.IsEmpty())
		{
			return false;
		}
		const FRegexPattern Regex(Pattern, ERegexPatternFlags::CaseInsensitive);
		FRegexMatcher Matcher(Regex, Subject);
		return Matcher.FindNext();
	}

	bool HasProfileSection(const FString& ProfileName)
	{
		const FConfigFile* File = GConfig ? GConfig->FindConfigFile(GDeviceProfilesIni) : nullptr;
		return !ProfileName.IsEmpty() && File && File->FindSection(ProfileName + TEXT(" DeviceProfile")) != nullptr;
	}

	/** One ProfileRules line. Returns true when every condition it sets holds; OutProfile is its target. */
	bool EvaluateRule(const FString& Line, const FMachineFacts& Facts, FString& OutProfile, FString& OutWhy)
	{
		OutProfile.Reset();
		if (!FParse::Value(*Line, TEXT("Profile="), OutProfile) || OutProfile.IsEmpty())
		{
			OutWhy = TEXT("no Profile");
			return false;
		}

		bool bHasCondition = false;
		TArray<FString> Matched;

		FString GPUPattern;
		if (FParse::Value(*Line, TEXT("GPUDescription="), GPUPattern, /*bShouldStopOnSeparator*/ false) && !GPUPattern.IsEmpty())
		{
			bHasCondition = true;
			if (!MatchesRegex(GPUPattern, Facts.GPUDescription) && !MatchesRegex(GPUPattern, Facts.GPUBrand))
			{
				OutWhy = FString::Printf(TEXT("GPU does not match '%s'"), *GPUPattern);
				return false;
			}
			Matched.Add(TEXT("GPU"));
		}

		FString CPUPattern;
		if (FParse::Value(*Line, TEXT("CPUBrand="), CPUPattern, false) && !CPUPattern.IsEmpty())
		{
			bHasCondition = true;
			if (!MatchesRegex(CPUPattern, Facts.CPUBrand))
			{
				OutWhy = FString::Printf(TEXT("CPU does not match '%s'"), *CPUPattern);
				return false;
			}
			Matched.Add(TEXT("CPU"));
		}

		FString Environment;
		if (FParse::Value(*Line, TEXT("Environment="), Environment, false) && !Environment.IsEmpty())
		{
			bHasCondition = true;
			FString Name, Expected;
			const bool bHasValue = Environment.Split(TEXT("="), &Name, &Expected);
			if (!bHasValue)
			{
				Name = Environment;
			}
			const FString Actual = FPlatformMisc::GetEnvironmentVariable(*Name.TrimStartAndEnd());
			if (bHasValue ? !Actual.Equals(Expected.TrimStartAndEnd(), ESearchCase::IgnoreCase) : Actual.IsEmpty())
			{
				OutWhy = FString::Printf(TEXT("environment %s is '%s'"), *Name, *Actual);
				return false;
			}
			Matched.Add(TEXT("environment"));
		}

		FString CommandLineToken;
		if (FParse::Value(*Line, TEXT("CommandLine="), CommandLineToken, false) && !CommandLineToken.IsEmpty())
		{
			bHasCondition = true;
			if (!FString(FCommandLine::Get()).Contains(CommandLineToken))
			{
				OutWhy = FString::Printf(TEXT("command line lacks '%s'"), *CommandLineToken);
				return false;
			}
			Matched.Add(TEXT("command line"));
		}

		int32 MinMemoryGB = 0;
		if (FParse::Value(*Line, TEXT("MinMemoryGB="), MinMemoryGB) && MinMemoryGB > 0)
		{
			bHasCondition = true;
			if (Facts.TotalPhysicalGB < MinMemoryGB)
			{
				OutWhy = FString::Printf(TEXT("%d GB is below %d"), Facts.TotalPhysicalGB, MinMemoryGB);
				return false;
			}
			Matched.Add(TEXT("memory"));
		}

		int32 MaxMemoryGB = 0;
		if (FParse::Value(*Line, TEXT("MaxMemoryGB="), MaxMemoryGB) && MaxMemoryGB > 0)
		{
			bHasCondition = true;
			if (Facts.TotalPhysicalGB > MaxMemoryGB)
			{
				OutWhy = FString::Printf(TEXT("%d GB is above %d"), Facts.TotalPhysicalGB, MaxMemoryGB);
				return false;
			}
			Matched.AddUnique(TEXT("memory"));
		}

		if (!bHasCondition)
		{
			OutWhy = TEXT("no condition (a rule needs at least one)");
			return false;
		}

		OutWhy = FString::Printf(TEXT("matched on %s"), *FString::Join(Matched, TEXT(", ")));
		return true;
	}
}

FString FExtendedDeviceProfileSelectorModule::GetFallbackProfile() const
{
	FString FallbackModule;
	if (!GConfig || !GConfig->GetString(GetConfigSection(), TEXT("FallbackSelectorModule"), FallbackModule, GEngineIni) || FallbackModule.IsEmpty())
	{
#if PLATFORM_WINDOWS
		FallbackModule = TEXT("WindowsDeviceProfileSelector");
#elif PLATFORM_LINUX
		FallbackModule = TEXT("LinuxDeviceProfileSelector");
#endif
	}

	// The platform's own selector keeps its behaviour (RHI-specific profiles, ConfigRules overrides).
	if (!FallbackModule.IsEmpty() && FallbackModule != TEXT("ExtendedDeviceProfileSelector"))
	{
		if (IDeviceProfileSelectorModule* Fallback = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>(*FallbackModule))
		{
			const FString Name = Fallback->GetRuntimeDeviceProfileName();
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
	}

	return FPlatformProperties::PlatformName();
}

FString FExtendedDeviceProfileSelectorModule::ChooseProfile() const
{
	using namespace ExtendedDeviceProfileSelectorPrivate;

	const FString Fallback = GetFallbackProfile();

	// Previews of other devices in the editor go through dp.Override, never through this selector.
	if (GIsEditor)
	{
		return Fallback;
	}

	const FMachineFacts Facts = FMachineFacts::Gather();
	UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("Choosing a device profile for %s"), *Facts.ToString());

	const FString Remembered = GetRememberedProfile();
	if (!Remembered.IsEmpty())
	{
		if (HasProfileSection(Remembered))
		{
			UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("Using remembered profile '%s' (GameUserSettings.ini [%s] %s)"),
				*Remembered, GetConfigSection(), GetRememberedProfileKey());
			return Remembered;
		}
		UE_LOG(LogExtendedDeviceProfileSelector, Warning, TEXT("Remembered profile '%s' has no [%s DeviceProfile] section; ignoring it"), *Remembered, *Remembered);
	}

	TArray<FString> Rules;
	if (GConfig)
	{
		GConfig->GetArray(GetConfigSection(), TEXT("ProfileRules"), Rules, GEngineIni);
	}

	for (const FString& Rule : Rules)
	{
		FString Profile;
		FString Why;
		if (!EvaluateRule(Rule, Facts, Profile, Why))
		{
			UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("Rule %s: %s"), *Rule, *Why);
			continue;
		}

		if (!HasProfileSection(Profile))
		{
			UE_LOG(LogExtendedDeviceProfileSelector, Warning, TEXT("Rule %s matched, but there is no [%s DeviceProfile] section"), *Rule, *Profile);
			continue;
		}

		UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("Rule %s %s: using profile '%s'"), *Rule, *Why, *Profile);
		return Profile;
	}

	UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("No rule matched; using the platform profile '%s'"), *Fallback);
	return Fallback;
}

const FString FExtendedDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	// The engine asks more than once; decide once per process.
	static FString ProfileName;
	if (ProfileName.IsEmpty())
	{
		ProfileName = ChooseProfile();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}
	}
	return ProfileName;
}

bool FExtendedDeviceProfileSelectorModule::GetSelectorPropertyValue(const FName& PropertyType, FString& PropertyValueOUT)
{
	static const FName EnvSteamDeck(TEXT("SRC_EnvSteamDeck"));
	static const FName TotalPhysicalGB(TEXT("SRC_TotalPhysicalGB"));
	static const FName CPUBrand(TEXT("SRC_CPUBrand"));

	if (PropertyType == EnvSteamDeck)
	{
		PropertyValueOUT = FPlatformMisc::GetEnvironmentVariable(TEXT("SteamDeck"));
		return true;
	}
	if (PropertyType == TotalPhysicalGB)
	{
		PropertyValueOUT = FString::FromInt(static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB));
		return true;
	}
	if (PropertyType == CPUBrand)
	{
		PropertyValueOUT = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
		return true;
	}

	PropertyValueOUT.Reset();
	return false;
}

void FExtendedDeviceProfileSelectorModule::RememberProfile(const FString& ProfileName)
{
	if (!GConfig || ProfileName.IsEmpty())
	{
		return;
	}
	GConfig->SetString(GetConfigSection(), GetRememberedProfileKey(), *ProfileName, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	UE_LOG(LogExtendedDeviceProfileSelector, Log, TEXT("Remembered device profile '%s' for the next launches"), *ProfileName);
}

void FExtendedDeviceProfileSelectorModule::ForgetRememberedProfile()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->RemoveKey(GetConfigSection(), GetRememberedProfileKey(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString FExtendedDeviceProfileSelectorModule::GetRememberedProfile()
{
	FString Profile;
	if (GConfig)
	{
		GConfig->GetString(GetConfigSection(), GetRememberedProfileKey(), Profile, GGameUserSettingsIni);
	}
	return Profile.TrimStartAndEnd();
}

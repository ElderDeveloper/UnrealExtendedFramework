// Fill out your copyright notice in the Description page of Project Settings.

#include "EFModularSettingsDeviceRules.h"

#include "Algo/Reverse.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "HAL/PlatformMemory.h"
#include "Internationalization/Regex.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/StringOutputDevice.h"
#include "RHIGlobals.h"

DEFINE_LOG_CATEGORY(LogEFDeviceSettings);

namespace EFDeviceRulesPrivate
{
	const TCHAR* const RulesSectionSuffix = TEXT(" ModularSettingsDeviceRules");
	const TCHAR* const ProfileSectionSuffix = TEXT(" DeviceProfile");
	const TCHAR* const DefaultFamily = TEXT("Default");

	bool HasSection(const FString& SectionName)
	{
		const FConfigFile* File = GConfig ? GConfig->FindConfigFile(GDeviceProfilesIni) : nullptr;
		return File && File->FindSection(SectionName) != nullptr;
	}

	/** Parses one "(...)" line into a struct the way the engine reads its fragment lines. */
	template <typename TStruct>
	bool ParseLine(const FString& Line, TStruct& OutValue, FString& OutError)
	{
		FStringOutputDevice Errors;
		UScriptStruct* Struct = TStruct::StaticStruct();
		const TCHAR* Result = Struct->ImportText(*Line, &OutValue, nullptr, PPF_None, &Errors, Struct->GetName());
		if (!Result || !Errors.IsEmpty())
		{
			OutError = Errors.IsEmpty() ? FString(TEXT("could not be parsed")) : FString(Errors).TrimStartAndEnd();
			return false;
		}
		return true;
	}

	void ReadRulesSection(const FString& Name, FEFResolvedDeviceRules& Out)
	{
		const FString Section = EFDeviceRules::MakeSectionName(Name);
		if (!HasSection(Section))
		{
			return;
		}

		FString Family;
		if (GConfig->GetString(*Section, TEXT("DeviceFamily"), Family, GDeviceProfilesIni) && !Family.TrimStartAndEnd().IsEmpty())
		{
			Out.DeviceFamily = Family.TrimStartAndEnd();
		}

		TArray<FString> Lines;
		GConfig->GetArray(*Section, TEXT("Rules"), Lines, GDeviceProfilesIni);
		for (const FString& Line : Lines)
		{
			FEFModularSettingDeviceRule Rule;
			FString Error;
			if (!ParseLine(Line, Rule, Error))
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] rule %s: %s"), *Section, *Line, *Error));
				continue;
			}
			if (Rule.Setting.IsNone())
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] rule %s names no Setting"), *Section, *Line));
				continue;
			}

			// The tag must exist, or the rule could never reach a setting. Requested without an error so
			// a typo is one warning here rather than an ensure.
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Rule.Setting, false);
			if (!Tag.IsValid())
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] rule for '%s': no such gameplay tag"), *Section, *Rule.Setting.ToString()));
				continue;
			}

			Rule.Setting = Tag.GetTagName();
			Out.Rules.Add(Rule.Setting, Rule);
			Out.RuleSources.Add(Rule.Setting, Section);
		}
	}

	bool MatchesRegex(const FString& Pattern, const FString& Subject)
	{
		const FRegexPattern Regex(Pattern, ERegexPatternFlags::CaseInsensitive);
		FRegexMatcher Matcher(Regex, Subject);
		return Matcher.FindNext();
	}
}

FString FEFDeviceHardwareInfo::ToString() const
{
	return FString::Printf(TEXT("GPU '%s', %lld MB video memory, %d GB system memory"), *GPUName, VideoMemoryMB, SystemMemoryGB);
}

FEFDeviceHardwareInfo FEFDeviceHardwareInfo::Gather()
{
	FEFDeviceHardwareInfo Info;
	Info.GPUName = GRHIAdapterName;
	if (Info.GPUName.IsEmpty())
	{
		Info.GPUName = FPlatformMisc::GetPrimaryGPUBrand();
	}
	Info.VideoMemoryMB = static_cast<int64>(GRHIGlobals.GpuInfo.DedicatedVideoMemory / (1024ull * 1024ull));
	Info.SystemMemoryGB = static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB);
	return Info;
}

bool FEFResolvedDeviceRules::ContainsProfile(const FString& ProfileName) const
{
	auto Equals = [&ProfileName](const FString& Candidate) { return Candidate.Equals(ProfileName, ESearchCase::IgnoreCase); };
	return ProfileChain.ContainsByPredicate(Equals) || MatchedFragments.ContainsByPredicate(Equals);
}

FString EFDeviceRules::MakeSectionName(const FString& Name)
{
	return Name + EFDeviceRulesPrivate::RulesSectionSuffix;
}

FString EFDeviceRules::GetActiveProfileName()
{
	FString Name;
	if (UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		Name = UDeviceProfileManager::Get().GetActiveDeviceProfileName();
	}
	if (Name.IsEmpty())
	{
		Name = UDeviceProfileManager::GetPlatformDeviceProfileName();
	}
	return Name;
}

TArray<FString> EFDeviceRules::BuildProfileChain(const FString& ProfileName)
{
	// UDeviceProfileManager::GetDPChain does the same walk but is not exported, so read the
	// BaseProfileName links straight from the config, as the engine itself does.
	TArray<FString> LeafFirst;
	FString Current = ProfileName;
	while (!Current.IsEmpty() && !LeafFirst.ContainsByPredicate([&Current](const FString& Seen) { return Seen.Equals(Current, ESearchCase::IgnoreCase); }))
	{
		LeafFirst.Add(Current);

		FString Parent;
		if (!GConfig || !GConfig->GetString(*(Current + EFDeviceRulesPrivate::ProfileSectionSuffix), TEXT("BaseProfileName"), Parent, GDeviceProfilesIni))
		{
			break;
		}
		Current = Parent.TrimStartAndEnd();
	}

	Algo::Reverse(LeafFirst);
	return LeafFirst;
}

bool EFDeviceRules::Matches(const FEFModularSettingHardwareRule& Rule, const FEFDeviceHardwareInfo& Hardware)
{
	bool bHasCondition = false;

	if (Rule.VideoMemoryBelowMB > 0)
	{
		bHasCondition = true;
		// Unknown video memory (no RHI yet) is not "below": the rule must not fire on missing data.
		if (Hardware.VideoMemoryMB <= 0 || Hardware.VideoMemoryMB >= Rule.VideoMemoryBelowMB)
		{
			return false;
		}
	}
	if (Rule.VideoMemoryAtLeastMB > 0)
	{
		bHasCondition = true;
		if (Hardware.VideoMemoryMB < Rule.VideoMemoryAtLeastMB)
		{
			return false;
		}
	}
	if (Rule.SystemMemoryBelowGB > 0)
	{
		bHasCondition = true;
		if (Hardware.SystemMemoryGB <= 0 || Hardware.SystemMemoryGB >= Rule.SystemMemoryBelowGB)
		{
			return false;
		}
	}
	if (Rule.SystemMemoryAtLeastGB > 0)
	{
		bHasCondition = true;
		if (Hardware.SystemMemoryGB < Rule.SystemMemoryAtLeastGB)
		{
			return false;
		}
	}
	if (!Rule.GPUName.IsEmpty())
	{
		bHasCondition = true;
		if (!EFDeviceRulesPrivate::MatchesRegex(Rule.GPUName, Hardware.GPUName))
		{
			return false;
		}
	}

	return bHasCondition;
}

FEFResolvedDeviceRules EFDeviceRules::Resolve()
{
	using namespace EFDeviceRulesPrivate;

	FEFResolvedDeviceRules Out;
	Out.DeviceFamily = DefaultFamily;
	Out.Hardware = FEFDeviceHardwareInfo::Gather();

	if (!GConfig)
	{
		Out.Warnings.Add(TEXT("No config system: device rules not read"));
		return Out;
	}

	Out.ActiveProfile = GetActiveProfileName();
	Out.ProfileChain = BuildProfileChain(Out.ActiveProfile);

	if (UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		const FString Fragments = UDeviceProfileManager::Get().GetActiveDeviceProfileMatchedFragmentsString(/*bEnabledOnly*/ true, /*bIncludeTags*/ false, /*bAlphaSort*/ false);
		Fragments.ParseIntoArray(Out.MatchedFragments, TEXT(","), /*bCullEmpty*/ true);
		for (FString& Fragment : Out.MatchedFragments)
		{
			Fragment.TrimStartAndEndInline();
		}
	}

	for (const FString& Profile : Out.ProfileChain)
	{
		ReadRulesSection(Profile, Out);
	}
	for (const FString& Fragment : Out.MatchedFragments)
	{
		ReadRulesSection(Fragment, Out);
	}

	// Hardware rules belong to the most specific profile that has a rules section: WindowsEditor (no
	// section) uses Windows', while SteamDeck (a section without hardware rules) uses none, so a
	// handheld's shared-memory number never reaches the desktop's thresholds.
	for (int32 Index = Out.ProfileChain.Num() - 1; Index >= 0; --Index)
	{
		const FString Section = MakeSectionName(Out.ProfileChain[Index]);
		if (!HasSection(Section))
		{
			continue;
		}

		Out.HardwareRulesSource = Out.ProfileChain[Index];
		TArray<FString> Lines;
		GConfig->GetArray(*Section, TEXT("HardwareRules"), Lines, GDeviceProfilesIni);
		for (const FString& Line : Lines)
		{
			FEFModularSettingHardwareRule HardwareRule;
			FString Error;
			if (!ParseLine(Line, HardwareRule, Error))
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] hardware rule %s: %s"), *Section, *Line, *Error));
				continue;
			}
			if (HardwareRule.Apply.IsNone())
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] hardware rule %s names no section to Apply"), *Section, *Line));
				continue;
			}
			if (!HasSection(MakeSectionName(HardwareRule.Apply.ToString())))
			{
				Out.Warnings.Add(FString::Printf(TEXT("[%s] hardware rule %s applies '%s', which has no section"), *Section, *Line, *MakeSectionName(HardwareRule.Apply.ToString())));
				continue;
			}
			if (Matches(HardwareRule, Out.Hardware))
			{
				Out.HardwareSections.Add(HardwareRule.Apply.ToString());
				ReadRulesSection(HardwareRule.Apply.ToString(), Out);
			}
		}
		break;
	}

	return Out;
}

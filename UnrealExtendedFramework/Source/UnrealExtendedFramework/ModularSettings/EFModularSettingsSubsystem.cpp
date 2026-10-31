// Copyright Moon Punch Games. All Rights Reserved.

#include "EFModularSettingsSubsystem.h"
#include "EFModularSettingsLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/EFSettingsSaveGame.h"
#include "Settings/EFModularSettingsBase.h"
#include "Settings/Examples/EFGraphicsSettings.h"
#include "Async/Async.h"
#include "Algo/StableSort.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Interfaces/IPluginManager.h"
#include "UnrealExtendedFramework/Libraries/Monitor/EFMonitorLibrary.h"

static const FString SettingsSaveSlotName = TEXT("ModularSettingsSave");
static const FString SettingsVersion1BackupSlotName = TEXT("ModularSettingsSave_v1");
static const int32 SettingsUserIndex = 0;

namespace EFModularSettingsConsole
{
	static FString JoinArgs(const TArray<FString>& Args, const int32 StartIndex = 0)
	{
		FString JoinedValue;
		for (int32 Index = StartIndex; Index < Args.Num(); ++Index)
		{
			if (!JoinedValue.IsEmpty())
			{
				JoinedValue += TEXT(" ");
			}

			JoinedValue += Args[Index];
		}

		return JoinedValue;
	}

	static bool TryParseBool(const FString& ValueString, bool& bOutValue)
	{
		if (ValueString.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("1"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("on"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("enable"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("enabled"), ESearchCase::IgnoreCase))
		{
			bOutValue = true;
			return true;
		}

		if (ValueString.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("0"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("off"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("no"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("disable"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("disabled"), ESearchCase::IgnoreCase))
		{
			bOutValue = false;
			return true;
		}

		return false;
	}

	static FString DescribeOptions(const UEFModularSettingsMultiSelect* Setting)
	{
		return Setting && Setting->Values.Num() > 0
			? FString::Join(Setting->Values, TEXT(", "))
			: TEXT("<none>");
	}

	static FGameplayTag MakeTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	static bool TryParseAspectRatio(const FString& ValueString, int32& OutWidthPart, int32& OutHeightPart)
	{
		FString LeftPart;
		FString RightPart;
		if (!ValueString.Split(TEXT(":"), &LeftPart, &RightPart) && !ValueString.Split(TEXT("/"), &LeftPart, &RightPart))
		{
			return false;
		}

		OutWidthPart = FCString::Atoi(*LeftPart);
		OutHeightPart = FCString::Atoi(*RightPart);
		return OutWidthPart > 0 && OutHeightPart > 0;
	}

	static bool TryParseWindowMode(const FString& ValueString, EWindowMode::Type& OutWindowMode, FString& OutModeName)
	{
		if (ValueString.Equals(TEXT("windowed"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::Windowed;
			OutModeName = TEXT("Windowed");
			return true;
		}

		if (ValueString.Equals(TEXT("borderless"), ESearchCase::IgnoreCase)
			|| ValueString.Equals(TEXT("windowedfullscreen"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::WindowedFullscreen;
			OutModeName = TEXT("Borderless");
			return true;
		}

		if (ValueString.Equals(TEXT("fullscreen"), ESearchCase::IgnoreCase))
		{
			OutWindowMode = EWindowMode::Fullscreen;
			OutModeName = TEXT("Fullscreen");
			return true;
		}

		return false;
	}

	static int32 AlignDimensionForAspectTest(const int32 Value)
	{
		return Value > 1 ? Value - (Value % 2) : Value;
	}

	static FIntPoint ComputeLargestFittingResolution(const int32 MaxWidth, const int32 MaxHeight, const float TargetAspectRatio)
	{
		if (MaxWidth <= 0 || MaxHeight <= 0 || TargetAspectRatio <= 0.0f)
		{
			return FIntPoint::ZeroValue;
		}

		const int32 WidthLimitedHeight = AlignDimensionForAspectTest(FMath::FloorToInt(static_cast<float>(MaxWidth) / TargetAspectRatio));
		const int32 HeightLimitedWidth = AlignDimensionForAspectTest(FMath::FloorToInt(static_cast<float>(MaxHeight) * TargetAspectRatio));

		FIntPoint BestResolution = FIntPoint::ZeroValue;
		int64 BestPixelCount = -1;

		auto TryCandidate = [&](const int32 CandidateWidth, const int32 CandidateHeight)
		{
			if (CandidateWidth <= 0 || CandidateHeight <= 0)
			{
				return;
			}

			if (CandidateWidth > MaxWidth || CandidateHeight > MaxHeight)
			{
				return;
			}

			const int64 PixelCount = static_cast<int64>(CandidateWidth) * static_cast<int64>(CandidateHeight);
			if (PixelCount > BestPixelCount)
			{
				BestResolution = FIntPoint(CandidateWidth, CandidateHeight);
				BestPixelCount = PixelCount;
			}
		};

		TryCandidate(MaxWidth, WidthLimitedHeight);
		TryCandidate(HeightLimitedWidth, MaxHeight);
		return BestResolution;
	}
}

namespace EFModularSettingsDevice
{
	static bool IsPluginPhaseComplete(const ELoadingPhase::Type Phase)
	{
		// ELoadingPhase::None is the starting value and sorts after every real phase.
		const ELoadingPhase::Type Completed = IPluginManager::Get().GetLastCompletedLoadingPhase();
		return Completed != ELoadingPhase::None && Completed >= Phase;
	}

	static FString DescribeStored(const TMap<FGameplayTag, FString>& Values)
	{
		TArray<FString> Parts;
		for (const TPair<FGameplayTag, FString>& Pair : Values)
		{
			Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key.ToString(), *Pair.Value));
		}
		Parts.Sort();
		return FString::Join(Parts, TEXT(", "));
	}
}

void UEFModularSettingsSubsystem::Initialize(FSubsystemCollectionBase& SubsystemCollectionBase)
{
	Super::Initialize(SubsystemCollectionBase);

	if (const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>())
	{
		for (const TSoftObjectPtr<UEFModularSettingsContainer>& ContainerPtr : ProjectSettings->LocalSettingsContainers)
		{
			if (UEFModularSettingsContainer* Container = ContainerPtr.LoadSynchronous())
			{
				for (UEFModularSettingsBase* SettingTemplate : Container->Settings)
				{
					if (SettingTemplate)
					{
						UEFModularSettingsBase* RuntimeSetting = DuplicateObject<UEFModularSettingsBase>(SettingTemplate, this);
						RegisterSetting(RuntimeSetting);
					}
				}
			}
		}

		for (const FString& Entry : ProjectSettings->DeviceScopedSettings)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Entry.TrimStartAndEnd()), false);
			if (Tag.IsValid())
			{
				DeviceScopedTags.AddUnique(Tag);
			}
			else
			{
				// A project may list a setting it does not register on every platform (HDR).
				UE_LOG(LogEFDeviceSettings, Verbose, TEXT("DeviceScopedSettings entry '%s' is not a gameplay tag; ignored"), *Entry);
			}
		}
	}

	// Device rules before the save: defaults decide what the sparse save leaves out, and the device
	// family decides which bucket device-scoped values come from.
	ResolveDeviceRules();
	ApplyDeviceRulesToSettings();

	LoadFromDisk();

	// Force apply all settings to ensure engine state matches loaded settings. Overall Quality first,
	// so its preset can never overwrite an individual setting the player changed on its own.
	ApplyAllSettingsInOrder();

	if (UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		DeviceProfileChangedHandle = UDeviceProfileManager::Get().OnActiveDeviceProfileChanged().AddUObject(
			this, &UEFModularSettingsSubsystem::HandleActiveDeviceProfileChanged);
	}

	// In a packaged game the vendor upscaler plugins load in PostEngineInit, after this subsystem has
	// started, so whether this machine supports DLSS or XeSS is only known then. Capability decides
	// availability and options, so the rules are resolved again once that phase completes.
	if (!EFModularSettingsDevice::IsPluginPhaseComplete(ELoadingPhase::PostEngineInit))
	{
		LoadingPhaseHandle = IPluginManager::Get().OnLoadingPhaseComplete().AddWeakLambda(this,
			[this](ELoadingPhase::Type Phase, bool /*bSuccess*/)
			{
				if (Phase == ELoadingPhase::PostEngineInit)
				{
					// Never remove a delegate from inside its own broadcast; the reapply runs next tick and
					// unbinds there.
					ScheduleDeviceRulesReapply(TEXT("machine capabilities known (PostEngineInit)"), /*bSaveCurrentFamilyFirst*/ false);
				}
			});
	}

	RegisterConsoleCommands();
}


void UEFModularSettingsSubsystem::Deinitialize()
{
	if (DeviceProfileChangedHandle.IsValid() && UDeviceProfileManager::DeviceProfileManagerSingleton)
	{
		UDeviceProfileManager::Get().OnActiveDeviceProfileChanged().Remove(DeviceProfileChangedHandle);
	}
	DeviceProfileChangedHandle.Reset();

	if (LoadingPhaseHandle.IsValid())
	{
		IPluginManager::Get().OnLoadingPhaseComplete().Remove(LoadingPhaseHandle);
		LoadingPhaseHandle.Reset();
	}

	if (PendingReapplyHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingReapplyHandle);
		PendingReapplyHandle.Reset();
	}

	UnregisterConsoleCommands();
	Super::Deinitialize();
}


bool UEFModularSettingsSubsystem::IsDeviceScoped(const UEFModularSettingsBase* Setting) const
{
	if (!Setting)
	{
		return false;
	}

	switch (Setting->Scope)
	{
	case EEFSettingScope::Device:
		return true;
	case EEFSettingScope::User:
		return false;
	case EEFSettingScope::Auto:
	default:
		return DeviceScopedTags.ContainsByPredicate([Setting](const FGameplayTag& Entry)
		{
			return Setting->SettingTag.MatchesTag(Entry);
		});
	}
}


void UEFModularSettingsSubsystem::ResolveDeviceRules()
{
	const UEFModularProjectSettings* ProjectSettings = GetDefault<UEFModularProjectSettings>();
	if (ProjectSettings && !ProjectSettings->bApplyDeviceRules)
	{
		DeviceRules = FEFResolvedDeviceRules();
		DeviceRules.DeviceFamily = TEXT("Default");
		DeviceRules.Hardware = FEFDeviceHardwareInfo::Gather();
		DeviceRules.ActiveProfile = EFDeviceRules::GetActiveProfileName();
		UE_LOG(LogEFDeviceSettings, Log, TEXT("Device rules are off (bApplyDeviceRules=False); every setting keeps its authored default"));
		return;
	}

	DeviceRules = EFDeviceRules::Resolve();

	// Rules that name a tag this subsystem never registered reach nothing. Player and world settings are
	// left out on purpose (a host's device must not decide them for the party), so they count too.
	for (const TPair<FName, FEFModularSettingDeviceRule>& Pair : DeviceRules.Rules)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Pair.Key, false);
		if (!Settings.Contains(Tag))
		{
			DeviceRules.Warnings.Add(FString::Printf(TEXT("[%s] rule for '%s': no local setting has this tag"),
				*DeviceRules.RuleSources.FindRef(Pair.Key), *Pair.Key.ToString()));
		}
	}

	UE_LOG(LogEFDeviceSettings, Log, TEXT("Device rules: profile '%s' (chain %s%s), family '%s', hardware rules from '%s' matched [%s]; %s"),
		*DeviceRules.ActiveProfile,
		*FString::Join(DeviceRules.ProfileChain, TEXT(" > ")),
		DeviceRules.MatchedFragments.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", fragments %s"), *FString::Join(DeviceRules.MatchedFragments, TEXT(", "))),
		*DeviceRules.DeviceFamily,
		DeviceRules.HardwareRulesSource.IsEmpty() ? TEXT("<none>") : *DeviceRules.HardwareRulesSource,
		*FString::Join(DeviceRules.HardwareSections, TEXT(", ")),
		*DeviceRules.Hardware.ToString());
}


void UEFModularSettingsSubsystem::ApplyDeviceRulesToSettings()
{
	TArray<FString> Warnings;
	for (const auto& SettingPair : Settings)
	{
		if (UEFModularSettingsBase* Setting = SettingPair.Value)
		{
			Setting->ApplyDeviceRule(DeviceRules.FindRule(Setting->SettingTag), Warnings);
		}
	}

	ResolveSettingDefaults();

	DeviceRules.Warnings.Append(Warnings);
	for (const FString& Warning : DeviceRules.Warnings)
	{
		UE_LOG(LogEFDeviceSettings, Warning, TEXT("Device rules: %s"), *Warning);
	}
}


void UEFModularSettingsSubsystem::ResolveSettingDefaults()
{
	UEFOverallQualitySetting* Overall = nullptr;
	for (const auto& SettingPair : Settings)
	{
		if (UEFOverallQualitySetting* Candidate = Cast<UEFOverallQualitySetting>(SettingPair.Value))
		{
			Overall = Candidate;
			break;
		}
	}

	// The preset every quality setting starts from: an Overall rule's Default, or the device profile's
	// level when every quality group agrees on one. Settings the preset manages but that drive no group
	// (texture filtering, reflections, bloom) follow it.
	int32 BasePreset = INDEX_NONE;
	if (Overall)
	{
		const FEFModularSettingDeviceRule* OverallRule = DeviceRules.FindRule(Overall->SettingTag);
		if (OverallRule && !OverallRule->Default.IsEmpty())
		{
			BasePreset = Overall->FindPresetIndex(OverallRule->Default);
		}

		if (BasePreset == INDEX_NONE)
		{
			TOptional<int32> CommonLevel;
			bool bAgree = true;
			for (const FGameplayTag& Tag : UEFOverallQualitySetting::GetGraphicsSettingTags())
			{
				const UEFModularSettingsBase* Setting = Settings.FindRef(Tag);
				const FName Group = Setting ? Setting->GetEffectiveScalabilityGroup() : NAME_None;
				int32 Level = 0;
				if (Group.IsNone() || !UDeviceProfileManager::GetScalabilityCVar(Group.ToString(), Level))
				{
					continue;
				}
				if (CommonLevel.IsSet() && CommonLevel.GetValue() != Level)
				{
					bAgree = false;
					break;
				}
				CommonLevel = Level;
			}
			if (bAgree && CommonLevel.IsSet())
			{
				BasePreset = FMath::Clamp(CommonLevel.GetValue(), 0, 3);
			}
		}
	}

	auto ResolveOne = [this, Overall, BasePreset](UEFModularSettingsBase* Setting)
	{
		FString Default;
		const TCHAR* Source = TEXT("authored");
		const FEFModularSettingDeviceRule* Rule = DeviceRules.FindRule(Setting->SettingTag);
		UEFModularSettingsMultiSelect* MultiSelect = Cast<UEFModularSettingsMultiSelect>(Setting);

		if (Rule && !Rule->Default.IsEmpty())
		{
			if (Setting->IsKnownValue(Rule->Default))
			{
				Default = Rule->Default;
				Source = TEXT("rule");
			}
			else
			{
				DeviceRules.Warnings.Add(FString::Printf(TEXT("[%s] %s: Default '%s' is not a value of this setting"),
					*DeviceRules.RuleSources.FindRef(Setting->SettingTag.GetTagName()), *Setting->SettingTag.ToString(), *Rule->Default));
			}
		}

		if (Default.IsEmpty() && Setting != Overall && Overall && BasePreset != INDEX_NONE && MultiSelect
			&& UEFOverallQualitySetting::GetGraphicsSettingTags().Contains(Setting->SettingTag))
		{
			const int32 PresetIndex = Overall->GetExpectedSubSettingIndex(Setting->SettingTag, BasePreset);
			if (MultiSelect->Values.IsValidIndex(PresetIndex))
			{
				Default = MultiSelect->Values[PresetIndex];
				Source = TEXT("preset");
			}
		}

		if (Default.IsEmpty() && MultiSelect && MultiSelect->Values.Num() > 0)
		{
			const FName Group = Setting->GetEffectiveScalabilityGroup();
			int32 Level = 0;
			if (!Group.IsNone() && UDeviceProfileManager::GetScalabilityCVar(Group.ToString(), Level))
			{
				Default = MultiSelect->Values[FMath::Clamp(Level, 0, MultiSelect->Values.Num() - 1)];
				Source = TEXT("device profile level");
			}
		}

		if (Default.IsEmpty() && Setting != Overall)
		{
			Default = Setting->ComputeDeviceDefault();
			Source = TEXT("computed");
		}

		if (Default.IsEmpty() && Setting == Overall)
		{
			Default = Overall->DeriveDefaultFromSubSettings();
			Source = TEXT("derived from the quality settings");
		}

		if (Default.IsEmpty())
		{
			Default = Setting->GetAuthoredDefaultValue();
			Source = TEXT("authored");
		}

		if (!Setting->SetDefaultValueFromString(Default))
		{
			Setting->SetDefaultValueFromString(Setting->GetAuthoredDefaultValue());
			Source = TEXT("authored (fallback)");
		}

		// A default must be something the device offers, or a reset would be refused.
		if (MultiSelect && MultiSelect->Values.Num() > 0)
		{
			int32 DefaultIndex = MultiSelect->Values.Find(MultiSelect->DefaultValue);
			if (DefaultIndex == INDEX_NONE)
			{
				DefaultIndex = 0;
			}
			if (!MultiSelect->IsIndexOffered(DefaultIndex))
			{
				const int32 Nearest = MultiSelect->FindNearestOfferedIndex(DefaultIndex);
				if (Nearest != INDEX_NONE)
				{
					if (FCString::Strcmp(Source, TEXT("rule")) == 0)
					{
						DeviceRules.Warnings.Add(FString::Printf(TEXT("%s: Default '%s' is hidden on this device; using '%s'"),
							*Setting->SettingTag.ToString(), *MultiSelect->DefaultValue, *MultiSelect->Values[Nearest]));
					}
					MultiSelect->DefaultValue = MultiSelect->Values[Nearest];
				}
			}
		}

		UE_LOG(LogEFDeviceSettings, Verbose, TEXT("Default %s = %s (%s), %s"), *Setting->SettingTag.ToString(),
			*Setting->GetDefaultValueAsString(), Source, *UEnum::GetValueAsString(Setting->GetAvailability()));
	};

	for (const auto& SettingPair : Settings)
	{
		// A view of another setting takes its default from the source.
		if (SettingPair.Value && SettingPair.Value != Overall && SettingPair.Value->IsPersisted())
		{
			ResolveOne(SettingPair.Value);
		}
	}

	// Last: Overall Quality's derived default reads the others' defaults.
	if (Overall)
	{
		ResolveOne(Overall);
	}
}


TArray<UEFModularSettingsBase*> UEFModularSettingsSubsystem::GetSettingsInApplyOrder() const
{
	TArray<UEFModularSettingsBase*> Ordered;
	Ordered.Reserve(Settings.Num());
	for (const auto& SettingPair : Settings)
	{
		if (SettingPair.Value)
		{
			Ordered.Add(SettingPair.Value);
		}
	}

	Algo::StableSortBy(Ordered, [](const UEFModularSettingsBase* Setting) { return Setting->GetApplyPriority(); });
	return Ordered;
}


void UEFModularSettingsSubsystem::ApplyAllSettingsInOrder()
{
	for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
	{
		// Ensure the "PreviousValue" matches what we just loaded
		Setting->SaveCurrentValue();

		if (UEFOverallQualitySetting* Overall = Cast<UEFOverallQualitySetting>(Setting))
		{
			// The quality settings apply their own values right after; syncing them from the engine now
			// would replace a saved value with the preset's before it is applied.
			Overall->SetSubSettingSyncSuppressed(true);
			Overall->Apply();
			Overall->SetSubSettingSyncSuppressed(false);
		}
		else
		{
			Setting->Apply();
		}

		Setting->ClearDirty();
	}
}


const FString* UEFModularSettingsSubsystem::FindStoredValue(const UEFModularSettingsBase* Setting) const
{
	if (!Setting)
	{
		return nullptr;
	}

	if (IsDeviceScoped(Setting))
	{
		const TMap<FGameplayTag, FString>* Bucket = LoadedDeviceBuckets.Find(DeviceRules.DeviceFamily);
		return Bucket ? Bucket->Find(Setting->SettingTag) : nullptr;
	}

	return LoadedUserValues.Find(Setting->SettingTag);
}


void UEFModularSettingsSubsystem::ApplyStoredValues()
{
	HeldValues.Reset();
	TGuardValue<bool> ApplyingGuard(bApplyingStoredValues, true);

	// Read from a copy: nothing that runs while values go in may change what is still to be read.
	const TMap<FGameplayTag, FString> UserValues = LoadedUserValues;
	const TMap<FGameplayTag, FString>* LoadedFamily = LoadedDeviceBuckets.Find(DeviceRules.DeviceFamily);
	const TMap<FGameplayTag, FString> FamilyValues = LoadedFamily ? *LoadedFamily : TMap<FGameplayTag, FString>();

	for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
	{
		if (!Setting->IsPersisted())
		{
			continue;
		}

		const FString* Stored = IsDeviceScoped(Setting) ? FamilyValues.Find(Setting->SettingTag) : UserValues.Find(Setting->SettingTag);

		if (!Setting->IsEditableOnThisDevice())
		{
			// Locked and Hidden settings hold the device default. The player's own value stays in the
			// save for a device that allows it.
			Setting->ResetToDefault();
			if (Stored)
			{
				HeldValues.Add(Setting->SettingTag, *Stored);
			}
			continue;
		}

		if (!Stored)
		{
			Setting->ResetToDefault();
			continue;
		}

		if (Setting->CanTakeValueOnThisDevice(*Stored))
		{
			Setting->SetValueFromString(*Stored);
			continue;
		}

		// A hidden option or a value outside this device's range: keep the player's choice in the save
		// and use the nearest value this device can take (or its default).
		HeldValues.Add(Setting->SettingTag, *Stored);
		const FString Adjusted = Setting->AdjustValueToDevice(*Stored);
		if (Adjusted.IsEmpty())
		{
			Setting->ResetToDefault();
		}
		else
		{
			Setting->SetValueFromString(Adjusted);
		}
		UE_LOG(LogEFDeviceSettings, Log, TEXT("%s: saved value '%s' is not available on this device; using '%s' and keeping the saved value"),
			*Setting->SettingTag.ToString(), **Stored, *Setting->GetValueAsString());
	}
}


void UEFModularSettingsSubsystem::RefreshDeviceRules()
{
	ReapplyDeviceRules(TEXT("requested"), /*bSaveCurrentFamilyFirst*/ true);
}


void UEFModularSettingsSubsystem::HandleActiveDeviceProfileChanged()
{
	// Broadcast from inside SetActiveDeviceProfile, before an override's cvars are applied: wait a frame
	// so the new profile's sg.* levels are the ones read.
	ScheduleDeviceRulesReapply(TEXT("device profile changed"), /*bSaveCurrentFamilyFirst*/ true);
}


void UEFModularSettingsSubsystem::ScheduleDeviceRulesReapply(const TCHAR* Reason, bool bSaveCurrentFamilyFirst)
{
	bPendingReapplySavesFirst |= bSaveCurrentFamilyFirst;
	PendingReapplyReason = PendingReapplyReason.IsEmpty() ? FString(Reason) : PendingReapplyReason + TEXT(", ") + Reason;

	if (PendingReapplyHandle.IsValid())
	{
		return;
	}

	PendingReapplyHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		PendingReapplyHandle.Reset();

		if (LoadingPhaseHandle.IsValid() && EFModularSettingsDevice::IsPluginPhaseComplete(ELoadingPhase::PostEngineInit))
		{
			IPluginManager::Get().OnLoadingPhaseComplete().Remove(LoadingPhaseHandle);
			LoadingPhaseHandle.Reset();
		}

		const FString Reason = MoveTemp(PendingReapplyReason);
		const bool bSaveFirst = bPendingReapplySavesFirst;
		PendingReapplyReason.Reset();
		bPendingReapplySavesFirst = false;
		ReapplyDeviceRules(*Reason, bSaveFirst);
		return false;
	}), 0.0f);
}


void UEFModularSettingsSubsystem::ReapplyDeviceRules(const TCHAR* Reason, bool bSaveCurrentFamilyFirst)
{
	const FString PreviousProfile = DeviceRules.ActiveProfile;
	const FString PreviousFamily = DeviceRules.DeviceFamily;

	// The device-scoped values in memory belong to the family that is about to be left. Store them under
	// it first, or switching to another family and back would lose them.
	if (bSaveCurrentFamilyFirst)
	{
		SaveToDisk();
	}

	struct FSettingState
	{
		FString Value;
		FString Default;
		EEFSettingAvailability Availability = EEFSettingAvailability::Visible;
		int32 OfferedCount = 0;
	};
	auto Capture = [](const UEFModularSettingsBase* Setting)
	{
		FSettingState State;
		State.Value = Setting->GetValueAsString();
		State.Default = Setting->GetDefaultValueAsString();
		State.Availability = Setting->GetAvailability();
		if (const UEFModularSettingsMultiSelect* MultiSelect = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			State.OfferedCount = MultiSelect->GetOfferedIndices().Num();
		}
		return State;
	};

	TMap<const UEFModularSettingsBase*, FSettingState> Before;
	for (const auto& SettingPair : Settings)
	{
		if (SettingPair.Value)
		{
			Before.Add(SettingPair.Value, Capture(SettingPair.Value));
		}
	}

	ResolveDeviceRules();
	ApplyDeviceRulesToSettings();
	ApplyStoredValues();

	TArray<UEFModularSettingsBase*> Changed;
	for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
	{
		const FSettingState* Old = Before.Find(Setting);
		const FSettingState New = Capture(Setting);
		if (!Old || Old->Value != New.Value || Old->Default != New.Default || Old->Availability != New.Availability || Old->OfferedCount != New.OfferedCount)
		{
			Changed.Add(Setting);
		}
	}

	if (bSaveCurrentFamilyFirst)
	{
		// A device profile switch applies the new profile's sg.* levels over the player's, so every
		// setting is applied again, not only the ones whose value moved.
		ApplyAllSettingsInOrder();
	}
	else
	{
		// Capabilities only: re-applying unchanged display settings would resize the window for nothing.
		// Overall Quality's preset rewrites every scalability group, so when it is applied again each
		// quality setting is applied after it too, or one whose value differs from the preset (or that
		// maps its options to other engine levels, such as a game's shadow floor) would be lost.
		const bool bOverallReapplied = Changed.ContainsByPredicate([](const UEFModularSettingsBase* Setting)
		{
			return Setting->IsA<UEFOverallQualitySetting>();
		});

		for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
		{
			if (!Changed.Contains(Setting) && !(bOverallReapplied && Setting->GetEffectiveScalabilityGroup() != NAME_None))
			{
				continue;
			}

			Setting->SaveCurrentValue();
			if (UEFOverallQualitySetting* Overall = Cast<UEFOverallQualitySetting>(Setting))
			{
				Overall->SetSubSettingSyncSuppressed(true);
				Overall->Apply();
				Overall->SetSubSettingSyncSuppressed(false);
			}
			else
			{
				Setting->Apply();
			}
			Setting->ClearDirty();
		}
	}

	UE_LOG(LogEFDeviceSettings, Log, TEXT("Device rules re-applied (%s): profile '%s' -> '%s', family '%s' -> '%s', %d setting(s) changed"),
		Reason, *PreviousProfile, *DeviceRules.ActiveProfile, *PreviousFamily, *DeviceRules.DeviceFamily, Changed.Num());

	for (UEFModularSettingsBase* Setting : Changed)
	{
		OnSettingsChanged.Broadcast(Setting);
	}
	OnDeviceRulesChanged.Broadcast();
}


void UEFModularSettingsSubsystem::RegisterConsoleCommands()
{
	RegisterConsoleCommand(
		TEXT("Settings.Set"),
		TEXT("Set a modular setting value. Usage: Settings.Set <Tag> <Value>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleSetCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.Set"),
		TEXT("Set the active upscaler. Usage: Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerSetCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.DLSS.Mode"),
		TEXT("Set the DLSS quality mode. Usage: Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerDLSSModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.DLSS.RayReconstruction"),
		TEXT("Enable or disable DLSS ray reconstruction. Usage: Settings.Upscaler.DLSS.RayReconstruction <true|false>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerDLSSRayReconstructionCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FSR.Mode"),
		TEXT("Set the FSR quality mode. Usage: Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFSRModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FSR.Sharpness"),
		TEXT("Set the FSR sharpness. Usage: Settings.Upscaler.FSR.Sharpness <0.0-1.0>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFSRSharpnessCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.XeSS.QualityMode"),
		TEXT("Set the XeSS quality mode. Usage: Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerXeSSQualityModeCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.FrameGeneration"),
		TEXT("Enable or disable frame generation. Usage: Settings.Upscaler.FrameGeneration <true|false>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerFrameGenerationCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.ResolutionScale"),
		TEXT("Set the manual resolution scale. Usage: Settings.Upscaler.ResolutionScale <25-200>"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerResolutionScaleCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Upscaler.Status"),
		TEXT("Log the current upscaler-related modular settings."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleUpscalerStatusCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Device.Dump"),
		TEXT("Log what the device rules resolved to: profile chain, hardware, matched hardware rules, family, every rule and its section, and each local setting's default, availability and hidden options. Usage: Settings.Device.Dump [Tag filter]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleDeviceDumpCommand));

	RegisterConsoleCommand(
		TEXT("Settings.Device.Refresh"),
		TEXT("Resolve the device rules again and re-apply every local setting (after editing DeviceProfiles.ini in a running editor, or a dp.Override)."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>&) { RefreshDeviceRules(); }));

	RegisterConsoleCommand(
		TEXT("Settings.Display.TestAspect"),
		TEXT("Apply the largest resolution that fits the current monitor for a target aspect ratio. Usage: Settings.Display.TestAspect <16:9|19:9|21:9|32:9|Native> [Windowed|Fullscreen|Borderless]"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleDisplayTestAspectCommand));
}


void UEFModularSettingsSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (const FString& CommandName : RegisteredConsoleCommands)
	{
		if (ConsoleManager.FindConsoleObject(*CommandName) != nullptr)
		{
			ConsoleManager.UnregisterConsoleObject(*CommandName, false);
		}
	}

	RegisteredConsoleCommands.Reset();
}


void UEFModularSettingsSubsystem::RegisterConsoleCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Delegate)
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	if (ConsoleManager.FindConsoleObject(Name) != nullptr)
	{
		ConsoleManager.UnregisterConsoleObject(Name, false);
	}

	ConsoleManager.RegisterConsoleCommand(Name, Help, Delegate, ECVF_Default);
	RegisteredConsoleCommands.AddUnique(Name);
}


bool UEFModularSettingsSubsystem::ApplyConsoleSettingValue(FGameplayTag Tag, const FString& ValueString, const TCHAR* CommandLabel)
{
	UEFModularSettingsBase* Setting = GetLocalSettingByTag(Tag);
	if (!Setting)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not find setting '%s'"), CommandLabel, *Tag.ToString());
		return false;
	}

	const FString TrimmedValue = ValueString.TrimStartAndEnd();
	if (TrimmedValue.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s requires a non-empty value for '%s'"), CommandLabel, *Tag.ToString());
		return false;
	}

	if (!Setting->IsEditableOnThisDevice())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s '%s' is %s on this device (profile '%s'); it holds its device default '%s'. Try another profile with dp.Override."),
			CommandLabel, *Tag.ToString(), *UEnum::GetDisplayValueAsText(Setting->GetAvailability()).ToString(),
			*DeviceRules.ActiveProfile, *Setting->GetDefaultValueAsString());
		return false;
	}

	// Parse the console text into the setting's canonical string form with
	// friendlier diagnostics than RequestChange gives. Refresh first so the
	// option list and range being reported are current.
	Setting->EnsureFresh();

	FString CanonicalValue = TrimmedValue;
	if (UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
	{
		const int32 ValueIndex = MultiSelectSetting->Values.IndexOfByPredicate([&TrimmedValue](const FString& Candidate)
		{
			return Candidate.Equals(TrimmedValue, ESearchCase::IgnoreCase);
		});

		if (!MultiSelectSetting->Values.IsValidIndex(ValueIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid value '%s' for '%s'. Valid options: %s"),
				CommandLabel,
				*TrimmedValue,
				*Tag.ToString(),
				*EFModularSettingsConsole::DescribeOptions(MultiSelectSetting));
			return false;
		}

		if (MultiSelectSetting->IsIndexLocked(ValueIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s option '%s' is currently locked for '%s'"), CommandLabel, *MultiSelectSetting->Values[ValueIndex], *Tag.ToString());
			return false;
		}

		if (!MultiSelectSetting->IsIndexOffered(ValueIndex))
		{
			TArray<FString> Offered;
			for (const int32 OfferedIndex : MultiSelectSetting->GetOfferedIndices())
			{
				Offered.Add(MultiSelectSetting->Values[OfferedIndex]);
			}
			UE_LOG(LogTemp, Warning, TEXT("%s option '%s' is not offered on this device for '%s'. Offered: %s"),
				CommandLabel, *MultiSelectSetting->Values[ValueIndex], *Tag.ToString(), *FString::Join(Offered, TEXT(", ")));
			return false;
		}

		CanonicalValue = MultiSelectSetting->Values[ValueIndex];
	}
	else if (Setting->IsA<UEFModularSettingsBool>())
	{
		bool bParsedValue = false;
		if (!EFModularSettingsConsole::TryParseBool(TrimmedValue, bParsedValue))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid bool value '%s' for '%s'. Expected one of: true, false, 1, 0, on, off"),
				CommandLabel,
				*TrimmedValue,
				*Tag.ToString());
			return false;
		}

		CanonicalValue = bParsedValue ? TEXT("true") : TEXT("false");
	}
	else if (const UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(Setting))
	{
		float ParsedValue = 0.0f;
		if (!FDefaultValueHelper::ParseFloat(TrimmedValue, ParsedValue))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s invalid float value '%s' for '%s'"), CommandLabel, *TrimmedValue, *Tag.ToString());
			return false;
		}

		if (ParsedValue < FloatSetting->Min || ParsedValue > FloatSetting->Max)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s value %.3f is out of range for '%s' on this device. Expected %.3f to %.3f"),
				CommandLabel,
				ParsedValue,
				*Tag.ToString(),
				FloatSetting->Min,
				FloatSetting->Max);
			return false;
		}

		CanonicalValue = FString::SanitizeFloat(ParsedValue);
	}

	if (!Setting->RequestChange(CanonicalValue))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s rejected value '%s' for '%s'"), CommandLabel, *TrimmedValue, *Tag.ToString());
		return false;
	}

	Setting->Apply();
	Setting->SaveCurrentValue();
	Setting->ClearDirty();
	OnSettingsChanged.Broadcast(Setting);
	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("%s set %s to %s"), CommandLabel, *Tag.ToString(), *Setting->GetValueAsString());
	return true;
}


void UEFModularSettingsSubsystem::LogSettingValue(FGameplayTag Tag, const TCHAR* Label) const
{
	const UEFModularSettingsBase* Setting = GetLocalSettingByTag(Tag);
	if (!Setting)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: setting '%s' is not registered"), Label, *Tag.ToString());
		return;
	}

	if (const UEFModularSettingsMultiSelect* MultiSelectSetting = Cast<UEFModularSettingsMultiSelect>(Setting))
	{
		UE_LOG(LogTemp, Log, TEXT("%s: %s [Options: %s]"), Label, *MultiSelectSetting->GetValueAsString(), *EFModularSettingsConsole::DescribeOptions(MultiSelectSetting));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("%s: %s"), Label, *Setting->GetValueAsString());
}


void UEFModularSettingsSubsystem::LogUpscalerStatus() const
{
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), TEXT("Upscaler"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), TEXT("DLSS Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), TEXT("DLSS Ray Reconstruction"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), TEXT("FSR Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), TEXT("FSR Sharpness"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), TEXT("XeSS Quality Mode"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), TEXT("Frame Generation"));
	LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), TEXT("Resolution Scale"));
}



void UEFModularSettingsSubsystem::ApplyAllChanges()
{
	bool bAnyChanged = false;

	// Populate snapshot BEFORE applying changes so we can revert to the state just before this apply
	PreviousSettingsSnapshot.Empty();
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting)
		{
			PreviousSettingsSnapshot.Add(Setting->SettingTag, Setting->GetSavedValueAsString());
		}
	}

	// Overall Quality first (GetApplyPriority), so its preset is in place before the individual quality
	// settings apply their own values on top. Its preset rewrites every scalability group, so once it has
	// been applied each quality setting is applied again, changed or not: a setting that maps its options
	// to other engine levels (a game's shadow floor) would otherwise be left on the preset's level.
	bool bOverallApplied = false;
	for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
	{
		if (Setting->IsDirty())
		{
			Setting->SaveCurrentValue();
			Setting->Apply();
			Setting->ClearDirty();
			bAnyChanged = true;
			bOverallApplied |= Setting->IsA<UEFOverallQualitySetting>();
			OnSettingsChanged.Broadcast(Setting);
		}
		else if (bOverallApplied && Setting->GetEffectiveScalabilityGroup() != NAME_None)
		{
			Setting->Apply();
		}
	}
    
	if (bAnyChanged)
	{
		SaveToDisk();
		UE_LOG(LogTemp, Log, TEXT("Applied and saved changed settings. Snapshot created."));
	}
}


void UEFModularSettingsSubsystem::RevertPendingChanges()
{
	int32 RevertedCount = 0;
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting && Setting->IsDirty())
		{
			Setting->RevertToSavedValue();
			OnSettingsChanged.Broadcast(Setting);
			++RevertedCount;
		}
	}
    
	UE_LOG(LogTemp, Log, TEXT("Reverted %d unapplied setting(s) to saved values."), RevertedCount);
}


void UEFModularSettingsSubsystem::RevertToPreviousSettings()
{
	if (PreviousSettingsSnapshot.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No previous settings snapshot exists to revert to."));
		return;
	}

	// In apply order, so Overall Quality's preset never lands on top of the quality settings.
	for (UEFModularSettingsBase* Setting : GetSettingsInApplyOrder())
	{
		if (const FString* SnapshotValue = PreviousSettingsSnapshot.Find(Setting->SettingTag))
		{
			Setting->SetValueFromString(*SnapshotValue);
			Setting->SaveCurrentValue();
			Setting->Apply();

			// Broadcast change so UI updates
			OnSettingsChanged.Broadcast(Setting);
		}
	}
	
	SaveToDisk();
	UE_LOG(LogTemp, Log, TEXT("Reverted to previous settings snapshot and saved."));
}


void UEFModularSettingsSubsystem::RefreshAllSettings()
{
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		Setting->RefreshValues();
		OnSettingsChanged.Broadcast(Setting);
	}
}


bool UEFModularSettingsSubsystem::HasPendingChanges() const
{
	for (const auto& SettingPair : Settings)
	{
		const UEFModularSettingsBase* Setting = SettingPair.Value;
		// A value changed and then restored to its saved selection no longer needs Apply.
		if (Setting && Setting->IsDirty() && Setting->GetValueAsString() != Setting->GetSavedValueAsString())
		{
			return true;
		}
	}
	return false;
}


void UEFModularSettingsSubsystem::ResetToDefaults(FGameplayTag CategoryTag)
{
	int32 ResetCount = 0;
	for (const auto& SettingPair : Settings)
	{
		UEFModularSettingsBase* Setting = SettingPair.Value;
		if (Setting && (!CategoryTag.IsValid() || Setting->SettingTag.MatchesTag(CategoryTag)))
		{
			Setting->ResetToDefault();
			OnSettingsChanged.Broadcast(Setting);
			++ResetCount;
		}
	}
	
	ApplyAllChanges(); // Apply and save
	UE_LOG(LogTemp, Log, TEXT("Reset %d setting(s) in category '%s' to defaults."), ResetCount, *CategoryTag.ToString());
}


void UEFModularSettingsSubsystem::SaveToDisk()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UEFModularSettingsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UEFModularSettingsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->SaveToDisk();
			}
		});
		return;
	}

	if (bApplyingStoredValues)
	{
		UE_LOG(LogEFDeviceSettings, Verbose, TEXT("Save skipped: saved values are still being applied"));
		return;
	}

	if (UEFSettingsSaveGame* SaveGameInstance = BuildSaveGame())
	{
		UGameplayStatics::SaveGameToSlot(SaveGameInstance, SettingsSaveSlotName, SettingsUserIndex);
		UE_LOG(LogTemp, Log, TEXT("Modular settings saved to slot: %s"), *SettingsSaveSlotName);
	}
}


bool UEFModularSettingsSubsystem::DetermineValueToWrite(const UEFModularSettingsBase* Setting, FString& OutValue)
{
	if (!Setting->IsPersisted())
	{
		return false;
	}

	const FString Current = Setting->GetValueToSave();
	const FString Default = Setting->GetDefaultValueAsString();

	if (const FString* Held = HeldValues.Find(Setting->SettingTag))
	{
		// The player chose something this device cannot honour. Keep it while the setting is fixed here
		// or still sits on the device default; a different choice made here replaces it.
		if (!Setting->IsEditableOnThisDevice() || Setting->AreValuesEqual(Current, Default))
		{
			OutValue = *Held;
			return true;
		}
		HeldValues.Remove(Setting->SettingTag);
	}

	// A value equal to the default is not written, so it follows the default from then on, including
	// after a patch changes it.
	if (Setting->AreValuesEqual(Current, Default))
	{
		return false;
	}

	OutValue = Current;
	return true;
}


UEFSettingsSaveGame* UEFModularSettingsSubsystem::BuildSaveGame()
{
	UEFSettingsSaveGame* SaveGameInstance = Cast<UEFSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(UEFSettingsSaveGame::StaticClass()));
	if (!SaveGameInstance)
	{
		return nullptr;
	}

	SaveGameInstance->SaveVersion = UEFSettingsSaveGame::SparseSaveVersion;

	// Values for tags this build does not register (a newer build's settings) are carried over as they
	// were read, so running an older build in between loses nothing.
	TMap<FGameplayTag, FString> UserValues;
	for (const TPair<FGameplayTag, FString>& Pair : LoadedUserValues)
	{
		if (!Settings.Contains(Pair.Key))
		{
			UserValues.Add(Pair.Key, Pair.Value);
		}
	}

	TMap<FGameplayTag, FString> FamilyValues;
	if (const TMap<FGameplayTag, FString>* LoadedFamily = LoadedDeviceBuckets.Find(DeviceRules.DeviceFamily))
	{
		for (const TPair<FGameplayTag, FString>& Pair : *LoadedFamily)
		{
			if (!Settings.Contains(Pair.Key))
			{
				FamilyValues.Add(Pair.Key, Pair.Value);
			}
		}
	}

	for (const auto& SettingPair : Settings)
	{
		const UEFModularSettingsBase* Setting = SettingPair.Value;
		FString Value;
		if (!Setting || !DetermineValueToWrite(Setting, Value))
		{
			continue;
		}

		if (IsDeviceScoped(Setting))
		{
			FamilyValues.Add(Setting->SettingTag, Value);
		}
		else
		{
			UserValues.Add(Setting->SettingTag, Value);
		}
	}

	// Every other family's bucket goes back unchanged: the file is shared by every device the player's
	// cloud save reaches.
	LoadedUserValues = UserValues;
	LoadedDeviceBuckets.Add(DeviceRules.DeviceFamily, FamilyValues);

	SaveGameInstance->StoredSettings = MoveTemp(UserValues);
	for (const TPair<FString, TMap<FGameplayTag, FString>>& Bucket : LoadedDeviceBuckets)
	{
		if (!Bucket.Value.IsEmpty())
		{
			SaveGameInstance->DeviceScopedSettings.Add(Bucket.Key).Values = Bucket.Value;
		}
	}

	return SaveGameInstance;
}


void UEFModularSettingsSubsystem::LoadFromDisk()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UEFModularSettingsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UEFModularSettingsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->LoadFromDisk();
			}
		});
		return;
	}

	UEFSettingsSaveGame* LoadedGame = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SettingsSaveSlotName, SettingsUserIndex))
	{
		LoadedGame = Cast<UEFSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSaveSlotName, SettingsUserIndex));
		if (!LoadedGame)
		{
			UE_LOG(LogEFDeviceSettings, Warning, TEXT("Settings save '%s' exists but could not be read; using device defaults"), *SettingsSaveSlotName);
		}
	}

	ProcessLoadedSave(LoadedGame);
}


void UEFModularSettingsSubsystem::SaveToDiskAsync()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UEFModularSettingsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UEFModularSettingsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->SaveToDiskAsync();
			}
		});
		return;
	}

	if (bApplyingStoredValues)
	{
		UE_LOG(LogEFDeviceSettings, Verbose, TEXT("Async save skipped: saved values are still being applied"));
		return;
	}

	UEFSettingsSaveGame* SaveGameInstance = BuildSaveGame();
	if (!SaveGameInstance)
	{
		HandleAsyncSaveComplete(SettingsSaveSlotName, SettingsUserIndex, false);
		return;
	}

	// AsyncSaveGameToSlot serializes the UObject immediately on the game thread,
	// then dispatches only the platform file write.
	UGameplayStatics::AsyncSaveGameToSlot(
		SaveGameInstance,
		SettingsSaveSlotName,
		SettingsUserIndex,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleAsyncSaveComplete));
}


void UEFModularSettingsSubsystem::LoadFromDiskAsync()
{
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UEFModularSettingsSubsystem> WeakThis(this);
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UEFModularSettingsSubsystem* Subsystem = WeakThis.Get())
			{
				Subsystem->LoadFromDiskAsync();
			}
		});
		return;
	}

	// The engine performs the platform read asynchronously and constructs the
	// SaveGame UObject on the game thread before invoking this delegate.
	UGameplayStatics::AsyncLoadGameFromSlot(
		SettingsSaveSlotName,
		SettingsUserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &UEFModularSettingsSubsystem::HandleAsyncLoadComplete));
}


void UEFModularSettingsSubsystem::ProcessLoadedSave(UEFSettingsSaveGame* LoadedGame)
{
	LoadedUserValues.Reset();
	LoadedDeviceBuckets.Reset();

	// No save (a new player, or a deleted file): every setting starts on this device's default. There is
	// no hardware benchmark: the device profile and its rules decide the starting point, and a sparse
	// save lets a later config patch move every player who never changed a setting.
	if (!LoadedGame)
	{
		ApplyStoredValues();
		UE_LOG(LogEFDeviceSettings, Log, TEXT("No settings save; using the defaults for device profile '%s' (family '%s')"),
			*DeviceRules.ActiveProfile, *DeviceRules.DeviceFamily);
		return;
	}

	if (LoadedGame->SaveVersion < UEFSettingsSaveGame::SparseSaveVersion)
	{
		MigrateVersion1Save(*LoadedGame);
		ApplyStoredValues();

		// Written at once, so the migration never runs twice on the same file.
		SaveToDisk();
		return;
	}

	LoadedUserValues = LoadedGame->StoredSettings;
	for (const TPair<FString, FEFDeviceScopedValues>& Bucket : LoadedGame->DeviceScopedSettings)
	{
		LoadedDeviceBuckets.Add(Bucket.Key, Bucket.Value.Values);
	}

	ApplyStoredValues();
	UE_LOG(LogTemp, Log, TEXT("Modular settings loaded from slot: %s (%d user value(s), %d device famil%s)"),
		*SettingsSaveSlotName, LoadedUserValues.Num(), LoadedDeviceBuckets.Num(), LoadedDeviceBuckets.Num() == 1 ? TEXT("y") : TEXT("ies"));
}


void UEFModularSettingsSubsystem::BackupVersion1Save() const
{
	if (UGameplayStatics::DoesSaveGameExist(SettingsVersion1BackupSlotName, SettingsUserIndex))
	{
		return;
	}

	// A byte copy through the save system, so it works on any platform and the cloud keeps it too.
	TArray<uint8> Bytes;
	if (UGameplayStatics::LoadDataFromSlot(Bytes, SettingsSaveSlotName, SettingsUserIndex)
		&& UGameplayStatics::SaveDataToSlot(Bytes, SettingsVersion1BackupSlotName, SettingsUserIndex))
	{
		UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration: kept the version 1 save as '%s'"), *SettingsVersion1BackupSlotName);
	}
	else
	{
		UE_LOG(LogEFDeviceSettings, Warning, TEXT("Settings migration: could not back up the version 1 save as '%s'"), *SettingsVersion1BackupSlotName);
	}
}


void UEFModularSettingsSubsystem::MigrateVersion1Save(const UEFSettingsSaveGame& LoadedGame)
{
	BackupVersion1Save();

	// A version 1 save holds every value, many of them picked by the old first-launch benchmark rather
	// than the player, from builds whose graphics tiers meant something else, and possibly from another
	// device through the cloud. So device-scoped values start over on this device's recommendation, and
	// only the player's own (audio, controls, gameplay) are kept, when they are still valid.
	TMap<FGameplayTag, FString> Kept;
	TMap<FGameplayTag, FString> DroppedDevice;
	TMap<FGameplayTag, FString> DroppedInvalid;
	TMap<FGameplayTag, FString> DroppedUnknown;

	for (const TPair<FGameplayTag, FString>& Pair : LoadedGame.StoredSettings)
	{
		const UEFModularSettingsBase* Setting = Settings.FindRef(Pair.Key);
		if (!Setting)
		{
			DroppedUnknown.Add(Pair.Key, Pair.Value);
		}
		else if (IsDeviceScoped(Setting))
		{
			DroppedDevice.Add(Pair.Key, Pair.Value);
		}
		else if (!Setting->IsKnownValue(Pair.Value))
		{
			DroppedInvalid.Add(Pair.Key, Pair.Value);
		}
		else
		{
			Kept.Add(Pair.Key, Pair.Value);
		}
	}

	LoadedUserValues = Kept;

	UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration from version %d: %d kept, %d device-scoped reset to the defaults for '%s', %d invalid dropped, %d unknown dropped"),
		LoadedGame.SaveVersion, Kept.Num(), DroppedDevice.Num(), *DeviceRules.DeviceFamily, DroppedInvalid.Num(), DroppedUnknown.Num());
	UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration kept: %s"), *EFModularSettingsDevice::DescribeStored(Kept));
	UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration reset (device-scoped): %s"), *EFModularSettingsDevice::DescribeStored(DroppedDevice));
	UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration dropped (invalid): %s"), *EFModularSettingsDevice::DescribeStored(DroppedInvalid));
	UE_LOG(LogEFDeviceSettings, Log, TEXT("Settings migration dropped (no such setting): %s"), *EFModularSettingsDevice::DescribeStored(DroppedUnknown));
}


void UEFModularSettingsSubsystem::HandleAsyncSaveComplete(const FString& SlotName, int32 UserIndex, bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("Modular settings saved asynchronously to slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save modular settings asynchronously to slot: %s (user %d)"), *SlotName, UserIndex);
	}
	OnSettingsSaved.Broadcast();
}


void UEFModularSettingsSubsystem::HandleAsyncLoadComplete(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGame)
{
	UEFSettingsSaveGame* SettingsSave = Cast<UEFSettingsSaveGame>(LoadedGame);
	if (!SettingsSave)
	{
		UE_LOG(LogTemp, Verbose, TEXT("No modular settings data loaded from slot '%s' for user %d."), *SlotName, UserIndex);
	}
	ProcessLoadedSave(SettingsSave);
	OnSettingsLoaded.Broadcast();
}


bool UEFModularSettingsSubsystem::HasSetting(FGameplayTag Tag) const
{
	return GetSettingByTag(Tag) != nullptr;
}


UEFModularSettingsBase* UEFModularSettingsSubsystem::GetSettingByTag(FGameplayTag Tag) const
{
	if (UEFModularSettingsBase* LocalSetting = Settings.FindRef(Tag))
	{
		return LocalSetting;
	}

	// Player and World settings live on components, not in this map. Resolve
	// them through the world so callers get one answer regardless of scope.
	return UEFModularSettingsLibrary::GetModularSetting(this, Tag, EEFSettingsSource::Auto);
}


void UEFModularSettingsSubsystem::RegisterSetting(UEFModularSettingsBase* Setting)
{
	if (Setting && Setting->SettingTag.IsValid())
	{
		Settings.Add(Setting->SettingTag, Setting);
		Setting->ModularSettingsSubsystem = this;
		Setting->OnRegistered();

		// After OnRegistered: a setting that builds its option list there (resolutions, devices) may pick
		// a valid default as it does, and that is the default device rules start from.
		Setting->CaptureAuthoredDefault();
		UE_LOG(LogTemp, Log, TEXT("Registered setting: %s"), *Setting->SettingTag.ToString());
	}
}


TArray<UEFModularSettingsBase*> UEFModularSettingsSubsystem::GetSettingsByCategory(FName Category) const
{
	TArray<UEFModularSettingsBase*> Result;
	
	for (const auto& SettingPair : Settings)
	{
		if (SettingPair.Value->ConfigCategory == Category)
		{
			Result.Add(SettingPair.Value);
		}
	}
	
	return Result;
}


void UEFModularSettingsSubsystem::HandleSetCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Set <Tag> <Value>"));
		return;
	}

	const FString TagString = Args[0];
	const FString ValueString = EFModularSettingsConsole::JoinArgs(Args, 1);

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Tag: %s"), *TagString);
		return;
	}

	ApplyConsoleSettingValue(Tag, ValueString, TEXT("Settings.Set"));
}


void UEFModularSettingsSubsystem::HandleUpscalerSetCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), TEXT("Upscaler"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.Set"));
}


void UEFModularSettingsSubsystem::HandleUpscalerDLSSModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), TEXT("DLSS Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.Mode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.DLSS.Mode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerDLSSRayReconstructionCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.DLSS.RayReconstruction <true|false>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), TEXT("DLSS Ray Reconstruction"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.DLSS.RayReconstruction")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.DLSS.RayReconstruction"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFSRModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), TEXT("FSR Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Mode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FSR.Mode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFSRSharpnessCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FSR.Sharpness <0.0-1.0>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), TEXT("FSR Sharpness"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FSR.Sharpness")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FSR.Sharpness"));
}


void UEFModularSettingsSubsystem::HandleUpscalerXeSSQualityModeCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), TEXT("XeSS Quality Mode"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.XeSS.QualityMode")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.XeSS.QualityMode"));
}


void UEFModularSettingsSubsystem::HandleUpscalerFrameGenerationCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.FrameGeneration <true|false>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), TEXT("Frame Generation"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.FrameGeneration")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.FrameGeneration"));
}


void UEFModularSettingsSubsystem::HandleUpscalerResolutionScaleCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Upscaler.ResolutionScale <25-200>"));
		LogSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), TEXT("Resolution Scale"));
		return;
	}

	ApplyConsoleSettingValue(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Upscaler.ResolutionScale")), EFModularSettingsConsole::JoinArgs(Args), TEXT("Settings.Upscaler.ResolutionScale"));
}


void UEFModularSettingsSubsystem::HandleUpscalerStatusCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0 && Args[0].Equals(TEXT("help"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.Set <None|DLSS|FSR|XeSS>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.DLSS.Mode <Auto|DLAA|UltraQuality|Quality|Balanced|Performance|UltraPerformance>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.DLSS.RayReconstruction <true|false>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FSR.Mode <NativeAA|Quality|Balanced|Performance|UltraPerformance>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FSR.Sharpness <0.0-1.0>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.XeSS.QualityMode <UltraPerformance|Performance|Balanced|Quality|UltraQuality|UltraQualityPlus|AntiAliasing>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.FrameGeneration <true|false>"));
		UE_LOG(LogTemp, Log, TEXT("Settings.Upscaler.ResolutionScale <25-200>"));
	}

	LogUpscalerStatus();
}


void UEFModularSettingsSubsystem::HandleDisplayTestAspectCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: Settings.Display.TestAspect <16:9|19:9|21:9|32:9|Native> [Windowed|Fullscreen|Borderless]"));
		return;
	}

	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect requires a valid engine instance."));
		return;
	}

	UGameUserSettings* UserSettings = GEngine->GetGameUserSettings();
	if (!UserSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect requires GameUserSettings."));
		return;
	}

	const FString AspectArgument = Args[0].TrimStartAndEnd();
	EWindowMode::Type WindowMode = EWindowMode::Windowed;
	FString WindowModeName = TEXT("Windowed");

	if (Args.Num() >= 2 && !EFModularSettingsConsole::TryParseWindowMode(Args[1], WindowMode, WindowModeName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect invalid window mode '%s'. Expected Windowed, Fullscreen, or Borderless."), *Args[1]);
		return;
	}

	int32 MonitorWidth = 0;
	int32 MonitorHeight = 0;
	UEFMonitorLibrary::GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);
	if (MonitorWidth <= 0 || MonitorHeight <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect could not determine the current monitor resolution."));
		return;
	}

	FIntPoint TargetResolution = FIntPoint(MonitorWidth, MonitorHeight);
	FString AspectLabel = TEXT("Native");

	if (!AspectArgument.Equals(TEXT("native"), ESearchCase::IgnoreCase)
		&& !AspectArgument.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		int32 AspectWidth = 0;
		int32 AspectHeight = 0;
		if (!EFModularSettingsConsole::TryParseAspectRatio(AspectArgument, AspectWidth, AspectHeight))
		{
			UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect invalid aspect ratio '%s'. Expected values like 16:9, 19:9, 21:9, 32:9, or Native."), *AspectArgument);
			return;
		}

		AspectLabel = FString::Printf(TEXT("%d:%d"), AspectWidth, AspectHeight);
		const float TargetAspectRatio = static_cast<float>(AspectWidth) / static_cast<float>(AspectHeight);
		TargetResolution = EFModularSettingsConsole::ComputeLargestFittingResolution(MonitorWidth, MonitorHeight, TargetAspectRatio);
		if (TargetResolution.X <= 0 || TargetResolution.Y <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect could not compute a valid resolution for aspect %s on monitor %dx%d."),
				*AspectLabel,
				MonitorWidth,
				MonitorHeight);
			return;
		}
	}

	const float NativeAspect = static_cast<float>(MonitorWidth) / static_cast<float>(MonitorHeight);
	const float RequestedAspect = static_cast<float>(TargetResolution.X) / static_cast<float>(TargetResolution.Y);
	if (WindowMode == EWindowMode::WindowedFullscreen && !FMath::IsNearlyEqual(NativeAspect, RequestedAspect, 0.01f))
	{
		UE_LOG(LogTemp, Warning, TEXT("Settings.Display.TestAspect switched Borderless to Windowed because borderless always uses the monitor's native aspect ratio."));
		WindowMode = EWindowMode::Windowed;
		WindowModeName = TEXT("Windowed");
	}

	UserSettings->SetFullscreenMode(WindowMode);
	UserSettings->SetScreenResolution(TargetResolution);
	UserSettings->ApplySettings(false);
	UserSettings->SaveSettings();

	const FString ResolutionString = FString::Printf(TEXT("%dx%d"), TargetResolution.X, TargetResolution.Y);

	if (UEFModularSettingsMultiSelect* DisplayModeSetting = Cast<UEFModularSettingsMultiSelect>(GetLocalSettingByTag(EFModularSettingsConsole::MakeTag(TEXT("Settings.Display.DisplayMode")))))
	{
		const int32 DisplayModeIndex = DisplayModeSetting->Values.IndexOfByPredicate([&WindowModeName](const FString& Candidate)
		{
			return Candidate.Equals(WindowModeName, ESearchCase::IgnoreCase);
		});

		if (DisplayModeSetting->Values.IsValidIndex(DisplayModeIndex))
		{
			DisplayModeSetting->SetSelectedIndex(DisplayModeIndex);
			DisplayModeSetting->SaveCurrentValue();
			DisplayModeSetting->ClearDirty();
			OnSettingsChanged.Broadcast(DisplayModeSetting);
		}
	}

	if (UEFModularSettingsMultiSelect* ResolutionSetting = Cast<UEFModularSettingsMultiSelect>(GetLocalSettingByTag(EFModularSettingsConsole::MakeTag(TEXT("Settings.Graphics.Resolution")))))
	{
		ResolutionSetting->RefreshValues();
		if (!ResolutionSetting->Values.Contains(ResolutionString))
		{
			ResolutionSetting->Values.Add(ResolutionString);
			ResolutionSetting->DisplayNames.Add(FText::FromString(ResolutionString));
		}

		ResolutionSetting->SetValueFromString(ResolutionString);
		ResolutionSetting->SaveCurrentValue();
		ResolutionSetting->ClearDirty();
		OnSettingsChanged.Broadcast(ResolutionSetting);
	}

	SaveToDisk();

	UE_LOG(LogTemp, Log, TEXT("Settings.Display.TestAspect applied %s as %s on monitor %dx%d using %s mode."),
		*AspectLabel,
		*ResolutionString,
		MonitorWidth,
		MonitorHeight,
		*WindowModeName);
}


void UEFModularSettingsSubsystem::HandleDeviceDumpCommand(const TArray<FString>& Args)
{
	const FString Filter = Args.Num() > 0 ? Args[0] : FString();

	UE_LOG(LogEFDeviceSettings, Display, TEXT("=== Settings.Device.Dump ==="));
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Profile: %s"), *DeviceRules.ActiveProfile);
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Chain (root first): %s"), *FString::Join(DeviceRules.ProfileChain, TEXT(" > ")));
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Matched fragments: %s"), DeviceRules.MatchedFragments.IsEmpty() ? TEXT("<none>") : *FString::Join(DeviceRules.MatchedFragments, TEXT(", ")));
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Hardware: %s"), *DeviceRules.Hardware.ToString());
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Hardware rules from: %s; matched: %s"),
		DeviceRules.HardwareRulesSource.IsEmpty() ? TEXT("<none>") : *DeviceRules.HardwareRulesSource,
		DeviceRules.HardwareSections.IsEmpty() ? TEXT("<none>") : *FString::Join(DeviceRules.HardwareSections, TEXT(", ")));
	UE_LOG(LogEFDeviceSettings, Display, TEXT("Device family: %s"), *DeviceRules.DeviceFamily);

	TArray<FName> RuleNames;
	DeviceRules.Rules.GetKeys(RuleNames);
	RuleNames.Sort(FNameLexicalLess());
	for (const FName& Name : RuleNames)
	{
		if (!Filter.IsEmpty() && !Name.ToString().Contains(Filter))
		{
			continue;
		}
		const FEFModularSettingDeviceRule& Rule = DeviceRules.Rules[Name];
		FString Line;
		FEFModularSettingDeviceRule::StaticStruct()->ExportText(Line, &Rule, nullptr, nullptr, PPF_None, nullptr);
		UE_LOG(LogEFDeviceSettings, Display, TEXT("Rule [%s] %s"), *DeviceRules.RuleSources.FindRef(Name), *Line);
	}

	TArray<UEFModularSettingsBase*> Ordered = GetSettingsInApplyOrder();
	Ordered.Sort([](const UEFModularSettingsBase& A, const UEFModularSettingsBase& B) { return A.SettingTag.ToString() < B.SettingTag.ToString(); });
	for (const UEFModularSettingsBase* Setting : Ordered)
	{
		if (!Filter.IsEmpty() && !Setting->SettingTag.ToString().Contains(Filter))
		{
			continue;
		}

		FString Extra;
		if (const UEFModularSettingsMultiSelect* MultiSelect = Cast<UEFModularSettingsMultiSelect>(Setting))
		{
			TArray<FString> Hidden;
			for (int32 Index = 0; Index < MultiSelect->Values.Num(); ++Index)
			{
				if (!MultiSelect->IsIndexOffered(Index))
				{
					Hidden.Add(MultiSelect->Values[Index]);
				}
			}
			if (!Hidden.IsEmpty())
			{
				Extra += FString::Printf(TEXT(", hidden options [%s]"), *FString::Join(Hidden, TEXT(", ")));
			}
		}
		else if (const UEFModularSettingsFloat* FloatSetting = Cast<UEFModularSettingsFloat>(Setting))
		{
			Extra += FString::Printf(TEXT(", range %g..%g"), FloatSetting->Min, FloatSetting->Max);
		}
		if (const FString* Held = HeldValues.Find(Setting->SettingTag))
		{
			Extra += FString::Printf(TEXT(", holding saved '%s'"), **Held);
		}
		if (const FName Group = Setting->GetEffectiveScalabilityGroup(); !Group.IsNone())
		{
			Extra += FString::Printf(TEXT(", group %s"), *Group.ToString());
		}

		UE_LOG(LogEFDeviceSettings, Display, TEXT("%s = %s (default %s, authored %s, %s, %s)%s"),
			*Setting->SettingTag.ToString(),
			*Setting->GetValueAsString(),
			*Setting->GetDefaultValueAsString(),
			*Setting->GetAuthoredDefaultValue(),
			*UEnum::GetDisplayValueAsText(Setting->GetAvailability()).ToString(),
			IsDeviceScoped(Setting) ? TEXT("device-scoped") : TEXT("user-scoped"),
			*Extra);
	}

	for (const FString& Warning : DeviceRules.Warnings)
	{
		UE_LOG(LogEFDeviceSettings, Warning, TEXT("Device rules: %s"), *Warning);
	}
}

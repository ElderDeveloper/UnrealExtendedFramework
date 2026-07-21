// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelSettings.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PerfSentinelSettings"

UPerfSentinelSettings::UPerfSentinelSettings()
{
	TraceOutputDirectory = TEXT("PerfSentinel");
	TraceFilePrefix = TEXT("PerfSentinel");
	ReportOutputDirectory = TEXT("PerfSentinel");
	BaselineDirectory = TEXT("PerfSentinel/Baselines");

	TraceChannels = {
		TEXT("cpu"),
		TEXT("frame"),
		TEXT("gpu"),
		TEXT("bookmark"),
		TEXT("loadtime"),
		TEXT("file")
	};
	ApplyCaptureProfile();
}

FName UPerfSentinelSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

FName UPerfSentinelSettings::GetSectionName() const
{
	return FName(TEXT("PerfSentinel"));
}

#if WITH_EDITOR
FText UPerfSentinelSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "PerfSentinel");
}

FText UPerfSentinelSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "Configure PerfSentinel trace capture, analysis, spike screenshots, and performance budgets.");
}

void UPerfSentinelSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPerfSentinelSettings, BudgetProfile))
	{
		ApplyBudgetProfile();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPerfSentinelSettings, CaptureProfile))
	{
		ApplyCaptureProfile();
	}
}
#endif

const UPerfSentinelSettings* UPerfSentinelSettings::Get()
{
	return GetDefault<UPerfSentinelSettings>();
}

void UPerfSentinelSettings::ApplyBudgetProfile()
{
	switch (BudgetProfile)
	{
	case EPerfSentinelBudgetProfile::Custom:
		break;

	case EPerfSentinelBudgetProfile::Editor60:
		FrameBudgetMs = 16.67f;
		HitchThresholdMs = 33.33f;
		SevereFrameBudgetMs = 50.0f;
		break;

	case EPerfSentinelBudgetProfile::Packaged30:
		FrameBudgetMs = 33.33f;
		HitchThresholdMs = 50.0f;
		SevereFrameBudgetMs = 66.66f;
		break;

	case EPerfSentinelBudgetProfile::Packaged60:
		FrameBudgetMs = 16.67f;
		HitchThresholdMs = 25.0f;
		SevereFrameBudgetMs = 33.33f;
		break;

	case EPerfSentinelBudgetProfile::LowEnd:
		FrameBudgetMs = 50.0f;
		HitchThresholdMs = 100.0f;
		SevereFrameBudgetMs = 150.0f;
		break;

	default:
		break;
	}
}

void UPerfSentinelSettings::ApplyCaptureProfile()
{
	if (CaptureProfile != EPerfSentinelCaptureProfile::Custom)
	{
		TraceChannels = GetChannelsForCaptureProfile();
	}
}

TArray<FString> UPerfSentinelSettings::GetChannelsForCaptureProfile() const
{
	if (CaptureProfile == EPerfSentinelCaptureProfile::Custom)
	{
		return TraceChannels;
	}

	TArray<FString> Channels = { TEXT("cpu"), TEXT("frame"), TEXT("gpu"), TEXT("bookmark"), TEXT("region"), TEXT("counters"), TEXT("stats") };
	switch (CaptureProfile)
	{
	case EPerfSentinelCaptureProfile::HitchDiagnosis:
		Channels.Append({ TEXT("task"), TEXT("contextswitch"), TEXT("stacksampling") });
		break;
	case EPerfSentinelCaptureProfile::LoadingStreaming:
		Channels.Append({ TEXT("loadtime"), TEXT("file"), TEXT("metadata"), TEXT("assetmetadata") });
		break;
	case EPerfSentinelCaptureProfile::UIAnimation:
		Channels.Append({ TEXT("slate"), TEXT("animation") });
		break;
	case EPerfSentinelCaptureProfile::MemoryLeak:
		Channels.Append({ TEXT("memory"), TEXT("metadata"), TEXT("assetmetadata"), TEXT("callstack"), TEXT("module") });
		break;
	case EPerfSentinelCaptureProfile::Multiplayer:
		Channels.Add(TEXT("net"));
		break;
	default:
		break;
	}

	return Channels;
}

TArray<FString> UPerfSentinelSettings::GetRequiredLaunchArguments() const
{
	switch (CaptureProfile)
	{
	case EPerfSentinelCaptureProfile::MemoryLeak:
		return { TEXT("-trace=default,memory,metadata,assetmetadata,callstack,module") };
	case EPerfSentinelCaptureProfile::Multiplayer:
		return { TEXT("-trace=default,net"), TEXT("-NetTrace=1") };
	case EPerfSentinelCaptureProfile::HitchDiagnosis:
		return { TEXT("-trace=default,task,contextswitch,stacksampling") };
	default:
		return {};
	}
}

bool UPerfSentinelSettings::CaptureProfileRequiresRelaunch() const
{
	return GetRequiredLaunchArguments().Num() > 0;
}

bool UPerfSentinelSettings::AreRequiredLaunchArgumentsPresent() const
{
	const TArray<FString> RequiredArguments = GetRequiredLaunchArguments();
	if (RequiredArguments.Num() == 0)
	{
		return true;
	}

	const FString CommandLine = FCommandLine::Get();
	for (const FString& Required : RequiredArguments)
	{
		if (Required.StartsWith(TEXT("-trace="), ESearchCase::IgnoreCase))
		{
			FString ActiveTraceChannels;
			if (!FParse::Value(*CommandLine, TEXT("-trace="), ActiveTraceChannels))
			{
				return false;
			}
			ActiveTraceChannels.TrimQuotesInline();
			TArray<FString> Active;
			TArray<FString> Needed;
			ActiveTraceChannels.ParseIntoArray(Active, TEXT(","), true);
			Required.RightChop(Required.Find(TEXT("=")) + 1).ParseIntoArray(Needed, TEXT(","), true);
			for (const FString& NeededChannel : Needed)
			{
				if (!Active.ContainsByPredicate([&](const FString& Value) { return Value.Equals(NeededChannel, ESearchCase::IgnoreCase); }))
				{
					return false;
				}
			}
		}
		else if (!CommandLine.Contains(Required, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

FString UPerfSentinelSettings::GetResolvedTraceOutputDirectory() const
{
	FString Dir = TraceOutputDirectory;
	if (FPaths::IsRelative(Dir))
	{
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), Dir);
	}

	Dir = FPaths::ConvertRelativePathToFull(Dir);
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}

FString UPerfSentinelSettings::GetResolvedReportOutputDirectory() const
{
	FString Dir = ReportOutputDirectory;
	if (FPaths::IsRelative(Dir))
	{
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), Dir);
	}

	Dir = FPaths::ConvertRelativePathToFull(Dir);
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}

FString UPerfSentinelSettings::GetResolvedBaselineDirectory() const
{
	FString Dir = BaselineDirectory;
	if (FPaths::IsRelative(Dir))
	{
		Dir = FPaths::Combine(FPaths::ProjectSavedDir(), Dir);
	}

	Dir = FPaths::ConvertRelativePathToFull(Dir);
	FPaths::NormalizeDirectoryName(Dir);
	return Dir;
}

FString UPerfSentinelSettings::GetResolvedAnalyzerScriptPath() const
{
	FString ScriptPath = AnalyzerScript.FilePath;
	if (ScriptPath.IsEmpty())
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedPerfSentinel")))
		{
			ScriptPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts"), TEXT("perf_sentinel_analyze.py"));
		}
		else
		{
			ScriptPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ExtendedFramework"), TEXT("UnrealExtendedPerfSentinel"), TEXT("Scripts"), TEXT("perf_sentinel_analyze.py"));
		}
	}
	else if (FPaths::IsRelative(ScriptPath))
	{
		ScriptPath = FPaths::Combine(FPaths::ProjectDir(), ScriptPath);
	}

	ScriptPath = FPaths::ConvertRelativePathToFull(ScriptPath);
	FPaths::NormalizeFilename(ScriptPath);
	return ScriptPath;
}

FString UPerfSentinelSettings::GetResolvedPythonExecutable() const
{
	FString PythonPath = PythonExecutable.FilePath;
	if (PythonPath.IsEmpty())
	{
		return TEXT("python");
	}

	const bool bLooksLikePath = PythonPath.Contains(TEXT("/")) || PythonPath.Contains(TEXT("\\")) || PythonPath.Contains(TEXT(":"));
	if (bLooksLikePath)
	{
		if (FPaths::IsRelative(PythonPath))
		{
			PythonPath = FPaths::Combine(FPaths::ProjectDir(), PythonPath);
		}

		PythonPath = FPaths::ConvertRelativePathToFull(PythonPath);
		FPaths::NormalizeFilename(PythonPath);
	}

	return PythonPath;
}

#undef LOCTEXT_NAMESPACE

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfSentinelSettings.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerfSentinelCaptureProfilesTest,
	"PerfSentinel.Settings.CaptureProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPerfSentinelCaptureProfilesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPerfSentinelSettings* Settings = NewObject<UPerfSentinelSettings>();

	Settings->CaptureProfile = EPerfSentinelCaptureProfile::Standard;
	Settings->ApplyCaptureProfile();
	TestTrue(TEXT("Standard captures CPU"), Settings->TraceChannels.Contains(TEXT("cpu")));
	TestTrue(TEXT("Standard captures frames"), Settings->TraceChannels.Contains(TEXT("frame")));
	TestFalse(TEXT("Standard avoids memory provider"), Settings->TraceChannels.Contains(TEXT("memory")));
	TestFalse(TEXT("Standard needs no relaunch"), Settings->CaptureProfileRequiresRelaunch());

	Settings->CaptureProfile = EPerfSentinelCaptureProfile::HitchDiagnosis;
	Settings->ApplyCaptureProfile();
	TestTrue(TEXT("Hitch profile captures tasks"), Settings->TraceChannels.Contains(TEXT("task")));
	TestTrue(TEXT("Hitch profile captures context switches"), Settings->TraceChannels.Contains(TEXT("contextswitch")));
	TestTrue(TEXT("Hitch profile advertises startup arguments"), Settings->CaptureProfileRequiresRelaunch());

	Settings->CaptureProfile = EPerfSentinelCaptureProfile::LoadingStreaming;
	Settings->ApplyCaptureProfile();
	TestTrue(TEXT("Loading profile captures file activity"), Settings->TraceChannels.Contains(TEXT("file")));
	TestTrue(TEXT("Loading profile captures asset metadata"), Settings->TraceChannels.Contains(TEXT("assetmetadata")));

	Settings->CaptureProfile = EPerfSentinelCaptureProfile::MemoryLeak;
	Settings->ApplyCaptureProfile();
	TestTrue(TEXT("Memory profile captures memory"), Settings->TraceChannels.Contains(TEXT("memory")));
	TestTrue(TEXT("Memory profile captures callstacks"), Settings->TraceChannels.Contains(TEXT("callstack")));
	TestTrue(TEXT("Memory profile needs relaunch"), Settings->CaptureProfileRequiresRelaunch());

	Settings->CaptureProfile = EPerfSentinelCaptureProfile::Multiplayer;
	Settings->ApplyCaptureProfile();
	TestTrue(TEXT("Multiplayer profile captures network"), Settings->TraceChannels.Contains(TEXT("net")));
	TestTrue(TEXT("Multiplayer profile includes NetTrace level"), Settings->GetRequiredLaunchArguments().Contains(TEXT("-NetTrace=1")));

	Settings->TraceChannels = { TEXT("custom-channel") };
	Settings->CaptureProfile = EPerfSentinelCaptureProfile::Custom;
	Settings->ApplyCaptureProfile();
	TestEqual(TEXT("Custom profile preserves explicit channels"), Settings->TraceChannels.Num(), 1);
	TestEqual(TEXT("Custom profile channel"), Settings->TraceChannels[0], FString(TEXT("custom-channel")));

	const FString AnalyzerScriptPath = Settings->GetResolvedAnalyzerScriptPath();
	TestTrue(
		TEXT("Analyzer resolves through the UnrealExtendedPerfSentinel plugin"),
		AnalyzerScriptPath.Contains(TEXT("UnrealExtendedPerfSentinel/Scripts/perf_sentinel_analyze.py")));
	TestTrue(TEXT("Resolved analyzer script exists"), IFileManager::Get().FileExists(*AnalyzerScriptPath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPerfSentinelBudgetProfilesTest,
	"PerfSentinel.Settings.BudgetProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPerfSentinelBudgetProfilesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UPerfSentinelSettings* Settings = NewObject<UPerfSentinelSettings>();
	Settings->BudgetProfile = EPerfSentinelBudgetProfile::Packaged60;
	Settings->ApplyBudgetProfile();
	TestTrue(TEXT("60 FPS frame budget"), FMath::IsNearlyEqual(Settings->FrameBudgetMs, 16.67f));
	TestTrue(TEXT("60 FPS hitch threshold"), FMath::IsNearlyEqual(Settings->HitchThresholdMs, 25.0f));
	TestTrue(TEXT("60 FPS severe threshold"), FMath::IsNearlyEqual(Settings->SevereFrameBudgetMs, 33.33f));
	return true;
}

#endif

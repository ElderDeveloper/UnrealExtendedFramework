// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelTraceController.h"

#include "PerfSentinelRuntimeMonitor.h"
#include "PerfSentinelSettings.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Trace/Trace.h"

DEFINE_LOG_CATEGORY(LogPerfSentinel);

FPerfSentinelTraceController::FPerfSentinelTraceController()
{
	RuntimeMonitor = MakeUnique<FPerfSentinelRuntimeMonitor>();
}

FPerfSentinelTraceController::~FPerfSentinelTraceController()
{
	CancelAutoStop();
}

bool FPerfSentinelTraceController::HasCompletedTrace() const
{
	return !LastCompletedSession.TracePath.IsEmpty() && LastCompletedSession.IsCompleted();
}

bool FPerfSentinelTraceController::StartCapture(const FString& ScenarioName)
{
	if (CaptureState != EPerfSentinelCaptureState::Idle)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("StartCapture: Cannot start while state is %d."), static_cast<int32>(CaptureState));
		return false;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StartCapture: PerfSentinel settings are unavailable."));
		return false;
	}

	const FString TracePath = BuildTraceFilePath();
	const FString OutputDir = FPaths::GetPath(TracePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*OutputDir))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StartCapture: Failed to create trace output directory: %s"), *OutputDir);
		return false;
	}

	const FString MetadataPath = FPaths::ChangeExtension(TracePath, TEXT(".metadata.json"));
	const TArray<FString> SafeChannels = BuildSafeTraceChannels();
	const FString ChannelsArg = FString::Join(SafeChannels, TEXT(","));
	if (SafeChannels.Num() == 0)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StartCapture: No safe trace channels are configured."));
		return false;
	}

	UE_LOG(LogPerfSentinel, Log, TEXT("StartCapture: Starting trace file: %s"), *TracePath);
	UE_LOG(LogPerfSentinel, Log, TEXT("StartCapture: Requested channels: %s"), *ChannelsArg);

	CurrentSession = FPerfSentinelTraceSession();
	CurrentSession.ScenarioName = ScenarioName;
	CurrentSession.StartedAt = FDateTime::UtcNow();
	CurrentSession.TracePath = TracePath;
	CurrentSession.MetadataPath = MetadataPath;
	CurrentSession.Channels = SafeChannels;
	CurrentSession.CaptureProfile = Settings->CaptureProfile;
	CurrentSession.RequiredLaunchArguments = Settings->GetRequiredLaunchArguments();
	CurrentSession.bLaunchRequirementsSatisfied = Settings->AreRequiredLaunchArgumentsPresent();
	CurrentSession.ReportDirectory = FPaths::Combine(Settings->GetResolvedReportOutputDirectory(), ExtractSessionBaseName(TracePath));
	CurrentSession.ReportDirectory = FPaths::ConvertRelativePathToFull(CurrentSession.ReportDirectory);
	FPaths::NormalizeDirectoryName(CurrentSession.ReportDirectory);
	if (!CurrentSession.bLaunchRequirementsSatisfied)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("StartCapture: The selected profile requires process launch arguments that are not active: %s"), *FString::Join(CurrentSession.RequiredLaunchArguments, TEXT(" ")));
	}

	if (!StartTraceFile(TracePath, CurrentSession.Channels))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StartCapture: Trace writer failed to start for: %s"), *TracePath);
		CurrentSession = FPerfSentinelTraceSession();
		return false;
	}

	CaptureState = EPerfSentinelCaptureState::Capturing;

	if (RuntimeMonitor)
	{
		RuntimeMonitor->Activate(ExtractSessionBaseName(TracePath), FPaths::GetPath(TracePath));
	}

	CancelAutoStop();
	if (Settings->bAutoStopCapture && Settings->DefaultCaptureDurationSeconds > 0)
	{
		AutoStopHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FPerfSentinelTraceController::HandleAutoStop),
			static_cast<float>(Settings->DefaultCaptureDurationSeconds));
	}

	UE_LOG(LogPerfSentinel, Log, TEXT("StartCapture: Trace capture started."));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Scenario: %s"), *CurrentSession.ScenarioName);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace:    %s"), *CurrentSession.TracePath);

	return true;
}

bool FPerfSentinelTraceController::StopCapture()
{
	if (CaptureState != EPerfSentinelCaptureState::Capturing)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("StopCapture: Cannot stop while state is %d."), static_cast<int32>(CaptureState));
		return false;
	}

	CaptureState = EPerfSentinelCaptureState::Stopping;
	CancelAutoStop();

	UE_LOG(LogPerfSentinel, Log, TEXT("StopCapture: Stopping trace writer."));
	if (!StopTraceFile())
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StopCapture: Trace writer did not report an active output to stop."));
		CaptureState = EPerfSentinelCaptureState::Capturing;
		return false;
	}

	if (RuntimeMonitor)
	{
		RuntimeMonitor->Deactivate();
	}

	CurrentSession.StoppedAt = FDateTime::UtcNow();

	if (!WaitForTraceFile(CurrentSession.TracePath))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StopCapture: Expected trace file was not written: %s"), *CurrentSession.TracePath);
		CaptureState = EPerfSentinelCaptureState::Idle;
		return false;
	}

	if (RuntimeMonitor && RuntimeMonitor->GetSpikeCount() > 0)
	{
		CurrentSession.SpikeEventsPath = RuntimeMonitor->GetSpikeEventsPath();
		UE_LOG(LogPerfSentinel, Log, TEXT("StopCapture: Attached %d spike event(s): %s"), RuntimeMonitor->GetSpikeCount(), *CurrentSession.SpikeEventsPath);
	}
	if (RuntimeMonitor)
	{
		if (FPaths::FileExists(RuntimeMonitor->GetFrameSamplesPath()))
		{
			CurrentSession.FrameSamplesPath = RuntimeMonitor->GetFrameSamplesPath();
			CurrentSession.FallbackStatsPath = CurrentSession.FrameSamplesPath;
		}
		if (FPaths::FileExists(RuntimeMonitor->GetRuntimeContextPath()))
		{
			CurrentSession.RuntimeContextPath = RuntimeMonitor->GetRuntimeContextPath();
		}
		if (FPaths::FileExists(RuntimeMonitor->GetRuntimeCountersPath()))
		{
			CurrentSession.RuntimeCountersPath = RuntimeMonitor->GetRuntimeCountersPath();
		}
		if (FPaths::FileExists(RuntimeMonitor->GetGameStatsPath()))
		{
			CurrentSession.GameStatsPath = RuntimeMonitor->GetGameStatsPath();
		}
	}

	WriteMetadataSidecar();
	LastCompletedSession = CurrentSession;
	CaptureState = EPerfSentinelCaptureState::Idle;

	UE_LOG(LogPerfSentinel, Log, TEXT("StopCapture: Trace capture stopped."));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace:    %s"), *LastCompletedSession.TracePath);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Duration: %.1f seconds"), (LastCompletedSession.StoppedAt - LastCompletedSession.StartedAt).GetTotalSeconds());

	return true;
}

bool FPerfSentinelTraceController::HandleAutoStop(float DeltaTime)
{
	(void)DeltaTime;
	AutoStopHandle.Reset();
	if (CaptureState == EPerfSentinelCaptureState::Capturing)
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("Auto-stop duration reached; stopping PerfSentinel capture."));
		StopCapture();
	}
	return false;
}

void FPerfSentinelTraceController::CancelAutoStop()
{
	if (AutoStopHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AutoStopHandle);
		AutoStopHandle.Reset();
	}
}

FString FPerfSentinelTraceController::BuildTraceFilePath() const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	check(Settings);

	const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H%M%S"));
	const FString SessionBaseName = FString::Printf(TEXT("%s_%s"), *Settings->TraceFilePrefix, *Timestamp);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString CandidateBaseName = SessionBaseName;
	FString TracePath;
	for (int32 Attempt = 1; Attempt < 1000; ++Attempt)
	{
		if (Attempt > 1)
		{
			CandidateBaseName = FString::Printf(TEXT("%s_%03d"), *SessionBaseName, Attempt);
		}

		const FString SessionDir = FPaths::Combine(Settings->GetResolvedTraceOutputDirectory(), CandidateBaseName);
		TracePath = FPaths::Combine(SessionDir, FString::Printf(TEXT("%s.utrace"), *CandidateBaseName));
		TracePath = FPaths::ConvertRelativePathToFull(TracePath);
		FPaths::NormalizeFilename(TracePath);

		if (!PlatformFile.DirectoryExists(*SessionDir) && !PlatformFile.FileExists(*TracePath))
		{
			break;
		}
	}

	TracePath = FPaths::ConvertRelativePathToFull(TracePath);
	FPaths::NormalizeFilename(TracePath);
	return TracePath;
}

TArray<FString> FPerfSentinelTraceController::BuildSafeTraceChannels() const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	check(Settings);

	const TArray<FString> ProfileChannels = Settings->GetChannelsForCaptureProfile();
	const TArray<FString> SourceChannels = ProfileChannels.Num() > 0
		? ProfileChannels
		: TArray<FString>{ TEXT("cpu"), TEXT("frame"), TEXT("gpu"), TEXT("bookmark"), TEXT("loadtime"), TEXT("file") };

	TArray<FString> SafeChannels;
	for (const FString& Channel : SourceChannels)
	{
		FString TrimmedChannel = Channel;
		TrimmedChannel.TrimStartAndEndInline();
		if (TrimmedChannel.IsEmpty())
		{
			continue;
		}

		SafeChannels.AddUnique(TrimmedChannel);
	}

	return SafeChannels;
}

FString FPerfSentinelTraceController::BuildChannelsArg() const
{
	const TArray<FString> SafeChannels = BuildSafeTraceChannels();
	return FString::Join(SafeChannels, TEXT(","));
}

FString FPerfSentinelTraceController::ExtractSessionBaseName(const FString& TracePath) const
{
	return FPaths::GetBaseFilename(TracePath);
}

bool FPerfSentinelTraceController::StartTraceFile(const FString& TracePath, const TArray<FString>& Channels) const
{
	for (const FString& Channel : Channels)
	{
		FString TrimmedChannel = Channel;
		TrimmedChannel.TrimStartAndEndInline();
		if (TrimmedChannel.IsEmpty())
		{
			continue;
		}

		if (TrimmedChannel.Equals(TEXT("memory"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogPerfSentinel, Log, TEXT("StartTraceFile: Memory tracing is launch-only; preserving it in coverage metadata without late toggling."));
			continue;
		}

		if (!UE::Trace::IsChannel(*TrimmedChannel))
		{
			UE_LOG(LogPerfSentinel, Warning, TEXT("StartTraceFile: Trace channel '%s' is not registered in this runtime."), *TrimmedChannel);
			continue;
		}

		const bool bEnabled = UE::Trace::ToggleChannel(*TrimmedChannel, true);
		UE_LOG(LogPerfSentinel, Verbose, TEXT("StartTraceFile: Channel '%s' enabled state: %s"), *TrimmedChannel, bEnabled ? TEXT("true") : TEXT("false"));
	}

	const bool bWriteStarted = UE::Trace::WriteTo(*TracePath);
	if (!bWriteStarted)
	{
		return false;
	}

	return UE::Trace::IsTracing();
}

bool FPerfSentinelTraceController::StopTraceFile() const
{
	return UE::Trace::Stop();
}

bool FPerfSentinelTraceController::ExecTraceCommand(const FString& Command)
{
	if (!GEngine)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("ExecTraceCommand: GEngine is null, cannot execute: %s"), *Command);
		return false;
	}

	UWorld* World = nullptr;
	if (GEngine->GetWorldContexts().Num() > 0)
	{
		World = GEngine->GetWorldContexts()[0].World();
	}

	return GEngine->Exec(World, *Command);
}

bool FPerfSentinelTraceController::WaitForTraceFile(const FString& TracePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const double StartSeconds = FPlatformTime::Seconds();
	constexpr double TimeoutSeconds = 2.0;

	while ((FPlatformTime::Seconds() - StartSeconds) < TimeoutSeconds)
	{
		if (PlatformFile.FileExists(*TracePath))
		{
			return true;
		}

		FPlatformProcess::Sleep(0.05f);
	}

	return PlatformFile.FileExists(*TracePath);
}

void FPerfSentinelTraceController::WriteMetadataSidecar() const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("WriteMetadataSidecar: PerfSentinel settings are unavailable."));
		return;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("trace_file"), CurrentSession.TracePath);
	Root->SetStringField(TEXT("scenario"), CurrentSession.ScenarioName);
	Root->SetStringField(TEXT("started_at"), CurrentSession.StartedAt.ToIso8601());
	Root->SetStringField(TEXT("stopped_at"), CurrentSession.StoppedAt.ToIso8601());
	Root->SetNumberField(TEXT("duration_seconds"), (CurrentSession.StoppedAt - CurrentSession.StartedAt).GetTotalSeconds());
	Root->SetStringField(TEXT("project"), FApp::GetProjectName());
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("report_directory"), CurrentSession.ReportDirectory);
	Root->SetNumberField(TEXT("capture_profile"), static_cast<uint8>(CurrentSession.CaptureProfile));
	if (RuntimeMonitor.IsValid())
	{
		// Anchor used by the offline analyzer to convert wall-clock spike times into trace-relative time.
		Root->SetNumberField(TEXT("capture_start_platform_seconds"), RuntimeMonitor->GetCaptureStartPlatformSeconds());
	}

	TArray<TSharedPtr<FJsonValue>> ChannelValues;
	for (const FString& Channel : CurrentSession.Channels)
	{
		ChannelValues.Add(MakeShared<FJsonValueString>(Channel));
	}
	Root->SetArrayField(TEXT("channels"), ChannelValues);
	TArray<TSharedPtr<FJsonValue>> RequiredLaunchValues;
	for (const FString& Argument : CurrentSession.RequiredLaunchArguments)
	{
		RequiredLaunchValues.Add(MakeShared<FJsonValueString>(Argument));
	}
	Root->SetArrayField(TEXT("required_launch_arguments"), RequiredLaunchValues);
	Root->SetBoolField(TEXT("profile_requires_relaunch"), Settings->CaptureProfileRequiresRelaunch());
	Root->SetBoolField(TEXT("launch_requirements_satisfied"), CurrentSession.bLaunchRequirementsSatisfied);
	if (RuntimeMonitor.IsValid())
	{
		Root->SetNumberField(TEXT("suppressed_spike_count"), RuntimeMonitor->GetSuppressedSpikeCount());
		Root->SetNumberField(TEXT("worst_suppressed_frame_ms"), RuntimeMonitor->GetWorstSuppressedFrameMs());
	}

	if (!CurrentSession.FallbackStatsPath.IsEmpty())
	{
		Root->SetStringField(TEXT("fallback_stats_file"), CurrentSession.FallbackStatsPath);
	}
	if (!CurrentSession.SpikeEventsPath.IsEmpty())
	{
		Root->SetStringField(TEXT("spike_events_file"), CurrentSession.SpikeEventsPath);
	}
	if (!CurrentSession.FrameSamplesPath.IsEmpty())
	{
		Root->SetStringField(TEXT("frame_samples_file"), CurrentSession.FrameSamplesPath);
	}
	if (!CurrentSession.RuntimeContextPath.IsEmpty())
	{
		Root->SetStringField(TEXT("runtime_context_file"), CurrentSession.RuntimeContextPath);
	}
	if (!CurrentSession.RuntimeCountersPath.IsEmpty())
	{
		Root->SetStringField(TEXT("runtime_counters_file"), CurrentSession.RuntimeCountersPath);
	}
	if (!CurrentSession.GameStatsPath.IsEmpty())
	{
		Root->SetStringField(TEXT("game_stats_file"), CurrentSession.GameStatsPath);
	}

	TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
	Budgets->SetNumberField(TEXT("frame_budget_ms"), Settings->FrameBudgetMs);
	Budgets->SetNumberField(TEXT("hitch_threshold_ms"), Settings->HitchThresholdMs);
	Budgets->SetNumberField(TEXT("severe_frame_budget_ms"), Settings->SevereFrameBudgetMs);
	Budgets->SetNumberField(TEXT("screenshot_spike_threshold_ms"), Settings->ScreenshotSpikeThresholdMs);
	Root->SetObjectField(TEXT("budgets"), Budgets);

	TSharedRef<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
	SettingsObject->SetStringField(TEXT("trace_output_directory"), Settings->GetResolvedTraceOutputDirectory());
	SettingsObject->SetStringField(TEXT("report_output_directory"), CurrentSession.ReportDirectory);
	SettingsObject->SetStringField(TEXT("baseline_directory"), Settings->GetResolvedBaselineDirectory());
	SettingsObject->SetStringField(TEXT("python_executable"), Settings->GetResolvedPythonExecutable());
	SettingsObject->SetStringField(TEXT("analyzer_script"), Settings->GetResolvedAnalyzerScriptPath());
	SettingsObject->SetBoolField(TEXT("capture_screenshot_on_spike"), Settings->bCaptureScreenshotOnSpike);
	SettingsObject->SetNumberField(TEXT("screenshot_cooldown_seconds"), Settings->ScreenshotCooldownSeconds);
	SettingsObject->SetNumberField(TEXT("spike_event_cooldown_seconds"), Settings->SpikeEventCooldownSeconds);
	SettingsObject->SetNumberField(TEXT("max_spike_events"), Settings->MaxSpikeEvents);
	SettingsObject->SetBoolField(TEXT("write_full_object_inventory"), Settings->bWriteFullObjectInventory);
	SettingsObject->SetBoolField(TEXT("native_trace_extraction"), Settings->bEnableNativeTraceExtraction);
	SettingsObject->SetNumberField(TEXT("default_capture_duration_seconds"), Settings->DefaultCaptureDurationSeconds);
	SettingsObject->SetBoolField(TEXT("verify_stat_coverage"), Settings->bVerifyStatUnitAndGpuTraceCoverage);
	SettingsObject->SetBoolField(TEXT("collect_stat_unit_fallback"), Settings->bCollectStatUnitFallback);
	SettingsObject->SetBoolField(TEXT("collect_stat_gpu_fallback"), Settings->bCollectStatGpuFallback);
	SettingsObject->SetBoolField(TEXT("collect_frame_samples"), Settings->bCollectFrameSamples);
	SettingsObject->SetBoolField(TEXT("collect_runtime_counters"), Settings->bCollectRuntimeCounters);
	SettingsObject->SetNumberField(TEXT("runtime_counter_interval_seconds"), Settings->RuntimeCounterIntervalSeconds);
	SettingsObject->SetBoolField(TEXT("write_spike_window_files"), Settings->bWriteSpikeWindows);
	SettingsObject->SetBoolField(TEXT("write_game_stats"), Settings->bWriteGameStats);
	SettingsObject->SetNumberField(TEXT("spike_window_pre_seconds"), Settings->SpikeWindowPreSeconds);
	SettingsObject->SetNumberField(TEXT("spike_window_post_seconds"), Settings->SpikeWindowPostSeconds);
	SettingsObject->SetBoolField(TEXT("async_screenshot_compression"), Settings->bAsyncScreenshotCompression);
	SettingsObject->SetNumberField(TEXT("screenshot_max_dimension"), Settings->ScreenshotMaxDimension);
	SettingsObject->SetBoolField(TEXT("harvest_runtime_stats"), Settings->bHarvestRuntimeStats);
	SettingsObject->SetNumberField(TEXT("runtime_stat_top_n"), Settings->RuntimeStatTopN);
	SettingsObject->SetBoolField(TEXT("collect_per_class_breakdown"), Settings->bCollectPerClassBreakdown);
	SettingsObject->SetNumberField(TEXT("per_class_top_n"), Settings->PerClassTopN);
	SettingsObject->SetNumberField(TEXT("leak_snapshot_interval_seconds"), Settings->LeakSnapshotIntervalSeconds);
	Root->SetObjectField(TEXT("settings"), SettingsObject);

	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(Root, Writer);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(CurrentSession.MetadataPath));

	if (FFileHelper::SaveStringToFile(OutputString, *CurrentSession.MetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("WriteMetadataSidecar: Written -> %s"), *CurrentSession.MetadataPath);
	}
	else
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("WriteMetadataSidecar: Failed to write -> %s"), *CurrentSession.MetadataPath);
	}
}

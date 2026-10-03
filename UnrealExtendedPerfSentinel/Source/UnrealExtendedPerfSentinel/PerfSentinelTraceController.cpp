// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelTraceController.h"

#include "PerfSentinelRuntimeMonitor.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTelemetry.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Net/Core/Trace/NetTrace.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Trace/Trace.h"

DEFINE_LOG_CATEGORY(LogPerfSentinel);

namespace
{
FString InferProcessRole()
{
	TSet<FString> Roles;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const UWorld* World = Context.World();
			if (!World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)) { continue; }
			switch (World->GetNetMode())
			{
			case NM_DedicatedServer: Roles.Add(TEXT("server")); break;
			case NM_ListenServer: Roles.Add(TEXT("listen_server")); break;
			case NM_Client: Roles.Add(TEXT("client")); break;
			default: Roles.Add(TEXT("standalone")); break;
			}
		}
	}
	if (Roles.Num() == 1) { return *Roles.CreateConstIterator(); }
	if (Roles.Num() > 1) { return TEXT("mixed"); }
	return GIsEditor ? TEXT("editor") : TEXT("unknown");
}

FString CanonicalChannelName(FString Name)
{
	Name.RemoveFromEnd(TEXT("Channel"), ESearchCase::IgnoreCase);
	Name.ToLowerInline();
	return Name;
}

TArray<FString> GetEnabledTraceChannels()
{
	TArray<FString> Channels;
	UE::Trace::EnumerateChannels([](const UE::Trace::FChannelInfo& Info, void* User)
	{
		if (Info.bIsEnabled)
		{
			static_cast<TArray<FString>*>(User)->AddUnique(CanonicalChannelName(UTF8_TO_TCHAR(Info.Name)));
		}
		return true;
	}, &Channels);
	return Channels;
}
}


FPerfSentinelTraceController::FPerfSentinelTraceController()
{
	RuntimeMonitor = MakeUnique<FPerfSentinelRuntimeMonitor>();
}

FPerfSentinelTraceController::~FPerfSentinelTraceController()
{
	CancelAutoStop();
	if (bOwnsTrace) { StopCapture(); }
}

bool FPerfSentinelTraceController::HasCompletedTrace() const
{
	return !LastCompletedSession.TracePath.IsEmpty() && LastCompletedSession.IsCompleted() && FPaths::FileExists(LastCompletedSession.TracePath);
}

bool FPerfSentinelTraceController::StartCapture(const FString& ScenarioName)
{
	if (CaptureState != EPerfSentinelCaptureState::Idle)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("StartCapture: Cannot start while state is %d."), static_cast<int32>(CaptureState));
		return false;
	}

	if (UE::Trace::IsTracing() || FTraceAuxiliary::IsConnected())
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("StartCapture: Another trace recorder is active. PerfSentinel did not stop or redirect it. Explicitly stop the existing recording before starting PerfSentinel; network tracing itself needs no relaunch."));
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
	CurrentSession.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	CurrentSession.RunId = Settings->SharedRunId.TrimStartAndEnd().Left(128);
	if (CurrentSession.RunId.IsEmpty()) { CurrentSession.RunId = CurrentSession.SessionId; }
	CurrentSession.ProcessRole = Settings->ProcessRole.TrimStartAndEnd().Left(64).ToLower();
	if (CurrentSession.ProcessRole.IsEmpty()) { CurrentSession.ProcessRole = InferProcessRole(); }
	CurrentSession.BuildId = Settings->BuildId.TrimStartAndEnd().Left(256);
	CurrentSession.StartedPlatformSeconds = FPlatformTime::Seconds();
	CurrentSession.ClockOffsetMilliseconds = FMath::IsFinite(Settings->ClockOffsetMilliseconds) ? Settings->ClockOffsetMilliseconds : 0.0;
	CurrentSession.ClockUncertaintyMilliseconds = FMath::IsFinite(Settings->ClockUncertaintyMilliseconds) ? Settings->ClockUncertaintyMilliseconds : -1.0;
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
	FPerfSentinelTelemetry::Get().SetCaptureContext(CurrentSession.SessionId, CurrentSession.RunId, CurrentSession.ProcessRole,
		CurrentSession.ClockOffsetMilliseconds, CurrentSession.ClockUncertaintyMilliseconds);

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
		FPerfSentinelTelemetry::Get().ClearCaptureContext();
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
		if (FPaths::FileExists(RuntimeMonitor->GetNetworkSamplesPath()))
		{
			CurrentSession.NetworkSamplesPath = RuntimeMonitor->GetNetworkSamplesPath();
		}
		if (FPaths::FileExists(RuntimeMonitor->GetTelemetryEventsPath()))
		{
			CurrentSession.TelemetryEventsPath = RuntimeMonitor->GetTelemetryEventsPath();
		}
		if (FPaths::FileExists(RuntimeMonitor->GetCollectorCatalogPath()))
		{
			CurrentSession.CollectorCatalogPath = RuntimeMonitor->GetCollectorCatalogPath();
		}
		if (FPaths::FileExists(RuntimeMonitor->GetGameStatsPath()))
		{
			CurrentSession.GameStatsPath = RuntimeMonitor->GetGameStatsPath();
		}
	}

	WriteMetadataSidecar();
	LastCompletedSession = CurrentSession;
	FPerfSentinelTelemetry::Get().ClearCaptureContext();
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

	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d_%H%M%S"));
	FString SafePrefix = FPaths::MakeValidFileName(Settings->TraceFilePrefix);
	if (SafePrefix.IsEmpty()) { SafePrefix = TEXT("PerfSentinel"); }
	const FString SessionBaseName = FString::Printf(TEXT("%s_%s_%s"), *SafePrefix.Left(64), *Timestamp, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12));

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

bool FPerfSentinelTraceController::StartTraceFile(const FString& TracePath, const TArray<FString>& Channels)
{
	const TArray<FString> PreviouslyEnabled = GetEnabledTraceChannels();
	bPreviousNetChannelEnabled = PreviouslyEnabled.Contains(TEXT("net"));
	NewlyEnabledChannels.Reset();
	AppliedNetTraceVerbosity = 0;
	bChangedNetTraceVerbosity = false;
	TArray<FString> StartChannels;
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

		StartChannels.AddUnique(TrimmedChannel);
	}

#if UE_NET_TRACE_ENABLED
	PreviousNetTraceVerbosity = FNetTrace::GetTraceVerbosity();
	const bool bRequestsNet = Channels.ContainsByPredicate([](const FString& Channel) { return CanonicalChannelName(Channel.TrimStartAndEnd()).Equals(TEXT("net")); });
	if (bRequestsNet)
	{
		const uint32 Desired = FMath::Clamp(UPerfSentinelSettings::Get()->NetworkTraceVerbosity, 1, 3);
		FNetTrace::SetTraceVerbosity(Desired);
		AppliedNetTraceVerbosity = FNetTrace::GetTraceVerbosity();
		bChangedNetTraceVerbosity = AppliedNetTraceVerbosity != PreviousNetTraceVerbosity;
	}
#endif
	const FString ChannelArg = FString::Join(StartChannels, TEXT(","));
	// TraceAuxiliary publishes connection notifications needed by NetTrace initialization and object metadata.
	bOwnsTrace = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *TracePath, *ChannelArg);
	const TArray<FString> Enabled = GetEnabledTraceChannels();
	CurrentSession.ActiveChannels = Enabled;
	for (const FString& Channel : Enabled) { if (!PreviouslyEnabled.Contains(Channel)) { NewlyEnabledChannels.Add(Channel); } }
	CurrentSession.EnabledChannels.Reset();
	CurrentSession.UnavailableChannels.Reset();
	for (const FString& Channel : Channels)
	{
		(Enabled.Contains(CanonicalChannelName(Channel)) ? CurrentSession.EnabledChannels : CurrentSession.UnavailableChannels).Add(Channel);
	}
	if (!bOwnsTrace)
	{
		RestoreTraceConfiguration();
		return false;
	}
	return bOwnsTrace;
}

bool FPerfSentinelTraceController::StopTraceFile()
{
	if (!bOwnsTrace) { return false; }
	FString Destination = FPaths::ConvertRelativePathToFull(FTraceAuxiliary::GetTraceDestinationString());
	FPaths::NormalizeFilename(Destination);
	if (FTraceAuxiliary::IsConnected() && !Destination.Equals(CurrentSession.TracePath, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("StopCapture: Trace output changed externally; refusing to stop another recording."));
		return false;
	}
	const bool bWasStopped = FTraceAuxiliary::Stop();
	if (!bWasStopped && UE::Trace::IsTracing()) { return false; }
	bOwnsTrace = false;
	RestoreTraceConfiguration();
	return true;
}

void FPerfSentinelTraceController::RestoreTraceConfiguration()
{
#if UE_NET_TRACE_ENABLED
	if (bChangedNetTraceVerbosity && FNetTrace::GetTraceVerbosity() == AppliedNetTraceVerbosity)
	{
		FNetTrace::SetTraceVerbosity(PreviousNetTraceVerbosity);
	}
#endif
	for (const FString& Channel : NewlyEnabledChannels) { UE::Trace::ToggleChannel(*Channel, false); }
	// NetTrace's verbosity setter toggles NetChannel itself; restore its separately recorded state too.
	// This covers an enabled channel with verbosity zero and a disabled channel with nonzero verbosity.
	UE::Trace::ToggleChannel(TEXT("net"), bPreviousNetChannelEnabled);
	NewlyEnabledChannels.Reset();
	bChangedNetTraceVerbosity = false;
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
	// The file writer opens its destination before capture starts. Do not block the game thread
	// on a missing file during StopCapture; report the failed capture explicitly.
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*TracePath);
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
	Root->SetNumberField(TEXT("schema_version"), 4);
	Root->SetStringField(TEXT("session_id"), CurrentSession.SessionId);
	Root->SetStringField(TEXT("run_id"), CurrentSession.RunId);
	Root->SetStringField(TEXT("process_role"), CurrentSession.ProcessRole);
	Root->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
	Root->SetStringField(TEXT("build_id"), CurrentSession.BuildId);
	Root->SetStringField(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("build_target"), LexToString(FApp::GetBuildTargetType()));
	Root->SetStringField(TEXT("engine_build_version"), FApp::GetBuildVersion());
	Root->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
	Root->SetStringField(TEXT("local_clock_utc"), CurrentSession.StartedAt.ToIso8601());
	Root->SetNumberField(TEXT("local_clock_monotonic_seconds"), CurrentSession.StartedPlatformSeconds);
	const bool bKnownClock = CurrentSession.ClockUncertaintyMilliseconds >= 0.0;
	Root->SetBoolField(TEXT("clock_alignment_known"), bKnownClock);
	Root->SetField(TEXT("clock_offset_ms"), bKnownClock ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueNumber>(CurrentSession.ClockOffsetMilliseconds)) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
	Root->SetField(TEXT("clock_uncertainty_ms"), bKnownClock ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueNumber>(CurrentSession.ClockUncertaintyMilliseconds)) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
	Root->SetStringField(TEXT("clock_alignment_source"), bKnownClock ? TEXT("caller_supplied_observation") : TEXT("unknown"));
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
	TArray<TSharedPtr<FJsonValue>> EnabledChannelValues, UnavailableChannelValues;
	for (const FString& Channel : CurrentSession.EnabledChannels) { EnabledChannelValues.Add(MakeShared<FJsonValueString>(Channel)); }
	for (const FString& Channel : CurrentSession.UnavailableChannels) { UnavailableChannelValues.Add(MakeShared<FJsonValueString>(Channel)); }
	Root->SetArrayField(TEXT("enabled_channels"), EnabledChannelValues);
	TArray<TSharedPtr<FJsonValue>> ActiveChannelValues;
	for (const FString& Channel : CurrentSession.ActiveChannels) { ActiveChannelValues.Add(MakeShared<FJsonValueString>(Channel)); }
	Root->SetArrayField(TEXT("active_channels"), ActiveChannelValues);
	Root->SetArrayField(TEXT("unavailable_channels"), UnavailableChannelValues);
	Root->SetNumberField(TEXT("network_trace_verbosity"), AppliedNetTraceVerbosity);
	Root->SetBoolField(TEXT("network_trace_compiled"), UE_NET_TRACE_ENABLED != 0);
	Root->SetStringField(TEXT("network_trace_status"), AppliedNetTraceVerbosity > 0 ? TEXT("enabled_verify_data_in_analysis") : (UE_NET_TRACE_ENABLED ? TEXT("not_requested") : TEXT("not_compiled")));
	TArray<TSharedPtr<FJsonValue>> RequiredLaunchValues;
	for (const FString& Argument : CurrentSession.RequiredLaunchArguments)
	{
		RequiredLaunchValues.Add(MakeShared<FJsonValueString>(Argument));
	}
	Root->SetArrayField(TEXT("required_launch_arguments"), RequiredLaunchValues);
	Root->SetBoolField(TEXT("profile_requires_relaunch"), CurrentSession.RequiredLaunchArguments.Num() > 0);
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
	if (!CurrentSession.NetworkSamplesPath.IsEmpty())
	{
		Root->SetStringField(TEXT("network_samples_file"), CurrentSession.NetworkSamplesPath);
	}
	if (!CurrentSession.TelemetryEventsPath.IsEmpty()) { Root->SetStringField(TEXT("telemetry_events_file"), CurrentSession.TelemetryEventsPath); }
	if (!CurrentSession.CollectorCatalogPath.IsEmpty()) { Root->SetStringField(TEXT("collector_catalog_file"), CurrentSession.CollectorCatalogPath); }
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
	TSharedRef<FJsonObject> NetworkBudgets = MakeShared<FJsonObject>();
	NetworkBudgets->SetNumberField(TEXT("max_outgoing_kib_per_second"), Settings->MaxOutgoingKiBPerSecond);
	NetworkBudgets->SetNumberField(TEXT("max_incoming_kib_per_second"), Settings->MaxIncomingKiBPerSecond);
	NetworkBudgets->SetNumberField(TEXT("max_rtt_ms"), Settings->MaxNetworkRttMs);
	NetworkBudgets->SetNumberField(TEXT("max_loss_percent"), Settings->MaxNetworkLossPercent);
	NetworkBudgets->SetNumberField(TEXT("max_reliable_backlog"), Settings->MaxReliableBacklog);
	NetworkBudgets->SetNumberField(TEXT("max_network_regression_percent"), Settings->MaxNetworkRegressionPercent);
	Root->SetObjectField(TEXT("network_budgets"), NetworkBudgets);

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
	SettingsObject->SetBoolField(TEXT("collect_network_samples"), Settings->bCollectNetworkSamples);
	SettingsObject->SetBoolField(TEXT("enable_telemetry_history"), Settings->bEnableTelemetryHistory);
	SettingsObject->SetNumberField(TEXT("telemetry_history_capacity"), Settings->TelemetryHistoryCapacity);
	SettingsObject->SetNumberField(TEXT("telemetry_history_seconds"), Settings->TelemetryHistorySeconds);
	SettingsObject->SetBoolField(TEXT("enable_detailed_collectors"), Settings->bEnableDetailedCollectors);
	SettingsObject->SetNumberField(TEXT("collector_interval_seconds"), Settings->CollectorIntervalSeconds);
	SettingsObject->SetNumberField(TEXT("detailed_collector_interval_seconds"), Settings->DetailedCollectorIntervalSeconds);
	SettingsObject->SetNumberField(TEXT("max_collector_time_ms"), Settings->MaxCollectorTimeMilliseconds);
	SettingsObject->SetNumberField(TEXT("max_collector_rows"), Settings->MaxCollectorRows);
	SettingsObject->SetNumberField(TEXT("network_sample_interval_seconds"), Settings->NetworkSampleIntervalSeconds);
	SettingsObject->SetNumberField(TEXT("network_trace_verbosity_requested"), Settings->NetworkTraceVerbosity);
	SettingsObject->SetNumberField(TEXT("max_network_worlds"), Settings->MaxNetworkWorlds);
	SettingsObject->SetNumberField(TEXT("max_network_drivers"), Settings->MaxNetworkDrivers);
	SettingsObject->SetNumberField(TEXT("max_network_connections"), Settings->MaxNetworkConnections);
	SettingsObject->SetNumberField(TEXT("max_network_channels_per_connection"), Settings->MaxNetworkChannelsPerConnection);
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

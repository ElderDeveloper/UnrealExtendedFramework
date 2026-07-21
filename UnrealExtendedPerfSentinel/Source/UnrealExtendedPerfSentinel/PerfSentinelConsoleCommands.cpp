// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelConsoleCommands.h"

#include "UnrealExtendedPerfSentinel.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelPythonRunner.h"
#include "PerfSentinelRuntimeMonitor.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
FString CaptureStateToString(EPerfSentinelCaptureState State)
{
	switch (State)
	{
	case EPerfSentinelCaptureState::Idle:
		return TEXT("Idle");
	case EPerfSentinelCaptureState::Capturing:
		return TEXT("Capturing");
	case EPerfSentinelCaptureState::Stopping:
		return TEXT("Stopping");
	case EPerfSentinelCaptureState::Analyzing:
		return TEXT("Analyzing");
	default:
		return TEXT("Unknown");
	}
}

FString BudgetProfileToString(EPerfSentinelBudgetProfile Profile)
{
	switch (Profile)
	{
	case EPerfSentinelBudgetProfile::Custom:
		return TEXT("Custom");
	case EPerfSentinelBudgetProfile::Editor60:
		return TEXT("Editor60");
	case EPerfSentinelBudgetProfile::Packaged30:
		return TEXT("Packaged30");
	case EPerfSentinelBudgetProfile::Packaged60:
		return TEXT("Packaged60");
	case EPerfSentinelBudgetProfile::LowEnd:
		return TEXT("LowEnd");
	default:
		return TEXT("Unknown");
	}
}

FString JoinScenarioArgs(const TArray<FString>& Args)
{
	return Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString(TEXT("ManualCapture"));
}

void RunAnalysisRequest(const TCHAR* CommandName, const FPerfSentinelAnalysisRequest& Request)
{
	const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	if (!Manager)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("%s: Analysis manager is unavailable."), CommandName);
		return;
	}
	FString Error;
	if (!Manager->StartAnalysis(Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("%s: %s"), CommandName, *Error);
		return;
	}
	UE_LOG(LogPerfSentinel, Log, TEXT("%s: Analysis started asynchronously. Use PerfSentinel.AnalysisStatus or PerfSentinel.CancelAnalysis."), CommandName);
}
}

FPerfSentinelConsoleCommands::FPerfSentinelConsoleCommands()
{
	RegisterCommands();
}

FPerfSentinelConsoleCommands::~FPerfSentinelConsoleCommands()
{
	UnregisterCommands();
}

void FPerfSentinelConsoleCommands::RegisterCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.StartCapture"),
		TEXT("Start PerfSentinel trace capture. Usage: PerfSentinel.StartCapture [ScenarioName]"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::StartCapture),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.StopCapture"),
		TEXT("Stop the active PerfSentinel trace capture."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::StopCapture),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.AnalyzeLastTrace"),
		TEXT("Analyze the last completed PerfSentinel trace."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::AnalyzeLastTrace),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.AnalyzeTrace"),
		TEXT("Analyze an existing trace. Usage: PerfSentinel.AnalyzeTrace <TracePath>"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::AnalyzeTrace),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.CancelAnalysis"),
		TEXT("Cancel the active PerfSentinel analysis process."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::CancelAnalysis),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.AnalysisStatus"),
		TEXT("Log the current PerfSentinel analysis state."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::AnalysisStatus),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.OpenReportsFolder"),
		TEXT("Open the resolved PerfSentinel reports folder."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::OpenReportsFolder),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.DumpSettings"),
		TEXT("Log resolved PerfSentinel settings and capture state."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::DumpSettings),
		ECVF_Default));

	RegisteredCommands.Add(ConsoleManager.RegisterConsoleCommand(
		TEXT("PerfSentinel.CaptureSpikeScreenshot"),
		TEXT("Record a manual spike event during an active capture."),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FPerfSentinelConsoleCommands::CaptureSpikeScreenshot),
		ECVF_Default));

	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel console commands registered."));
}

void FPerfSentinelConsoleCommands::UnregisterCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	for (IConsoleObject* Command : RegisteredCommands)
	{
		if (Command)
		{
			ConsoleManager.UnregisterConsoleObject(Command, false);
		}
	}

	RegisteredCommands.Reset();
}

void FPerfSentinelConsoleCommands::StartCapture(const TArray<FString>& Args)
{
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.StartCapture: Trace controller is unavailable."));
		return;
	}

	Controller->StartCapture(JoinScenarioArgs(Args));
}

void FPerfSentinelConsoleCommands::StopCapture(const TArray<FString>& Args)
{
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.StopCapture: Trace controller is unavailable."));
		return;
	}

	Controller->StopCapture();
}

void FPerfSentinelConsoleCommands::AnalyzeLastTrace(const TArray<FString>& Args)
{
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.AnalyzeLastTrace: Trace controller is unavailable."));
		return;
	}

	if (!Controller->HasCompletedTrace())
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("PerfSentinel.AnalyzeLastTrace: No completed trace is available."));
		return;
	}

	const FPerfSentinelTraceSession& Session = Controller->GetLastCompletedSession();
	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.AnalyzeLastTrace: Trace: %s"), *Session.TracePath);
	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.AnalyzeLastTrace: Metadata: %s"), *Session.MetadataPath);

	FPerfSentinelPythonRunner Runner;
	FPerfSentinelAnalysisRequest Request;
	FString Error;
	if (!Runner.BuildRequestForSession(Session, Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.AnalyzeLastTrace: %s"), *Error);
		return;
	}

	RunAnalysisRequest(TEXT("PerfSentinel.AnalyzeLastTrace"), Request);
}

void FPerfSentinelConsoleCommands::AnalyzeTrace(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("PerfSentinel.AnalyzeTrace: Usage: PerfSentinel.AnalyzeTrace <TracePath>"));
		return;
	}

	FString TracePath = Args[0];
	TracePath = FPaths::ConvertRelativePathToFull(TracePath);
	FPaths::NormalizeFilename(TracePath);

	const FString MetadataPath = FPaths::ChangeExtension(TracePath, TEXT(".metadata.json"));
	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.AnalyzeTrace: Trace: %s"), *TracePath);
	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.AnalyzeTrace: Metadata: %s"), *MetadataPath);

	if (!FPaths::FileExists(MetadataPath))
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("PerfSentinel.AnalyzeTrace: Metadata is missing. The Python runner will generate report-local fallback metadata."));
	}

	FPerfSentinelPythonRunner Runner;
	FPerfSentinelAnalysisRequest Request;
	FString Error;
	if (!Runner.BuildRequestForTrace(TracePath, Request, Error))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.AnalyzeTrace: %s"), *Error);
		return;
	}

	RunAnalysisRequest(TEXT("PerfSentinel.AnalyzeTrace"), Request);
}

void FPerfSentinelConsoleCommands::CancelAnalysis(const TArray<FString>& Args)
{
	(void)Args;
	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		Manager->CancelAnalysis();
		UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.CancelAnalysis: Cancellation requested."));
	}
}

void FPerfSentinelConsoleCommands::AnalysisStatus(const TArray<FString>& Args)
{
	(void)Args;
	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel analysis state: %d, last error: %s"), static_cast<int32>(Manager->GetState()), *Manager->GetLastError());
	}
}

void FPerfSentinelConsoleCommands::OpenReportsFolder(const TArray<FString>& Args)
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.OpenReportsFolder: PerfSentinel settings are unavailable."));
		return;
	}

	const FString ReportsDir = Settings->GetResolvedReportOutputDirectory();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ReportsDir);

#if WITH_EDITOR
	FPlatformProcess::ExploreFolder(*ReportsDir);
#else
	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel.OpenReportsFolder: Trace report root: %s"), *ReportsDir);
#endif
}

void FPerfSentinelConsoleCommands::DumpSettings(const TArray<FString>& Args)
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.DumpSettings: PerfSentinel settings are unavailable."));
		return;
	}

	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	const EPerfSentinelCaptureState CaptureState = Controller ? Controller->GetCaptureState() : EPerfSentinelCaptureState::Idle;

	UE_LOG(LogPerfSentinel, Log, TEXT("PerfSentinel Settings"));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Capture State: %s"), *CaptureStateToString(CaptureState));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace Output Directory: %s"), *Settings->TraceOutputDirectory);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace Output Resolved:  %s"), *Settings->GetResolvedTraceOutputDirectory());
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace File Prefix:      %s"), *Settings->TraceFilePrefix);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Trace Channels:         %s"), *FString::Join(Settings->TraceChannels, TEXT(",")));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Reports Directory:      %s"), *Settings->ReportOutputDirectory);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Reports Resolved:       %s"), *Settings->GetResolvedReportOutputDirectory());
	UE_LOG(LogPerfSentinel, Log, TEXT("  Python Executable:      %s"), *Settings->GetResolvedPythonExecutable());
	UE_LOG(LogPerfSentinel, Log, TEXT("  Analyzer Script:        %s"), *Settings->GetResolvedAnalyzerScriptPath());
	UE_LOG(LogPerfSentinel, Log, TEXT("  Budget Profile:         %s"), *BudgetProfileToString(Settings->BudgetProfile));
	UE_LOG(LogPerfSentinel, Log, TEXT("  Frame Budget:           %.2f ms"), Settings->FrameBudgetMs);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Hitch Threshold:        %.2f ms"), Settings->HitchThresholdMs);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Severe Frame Budget:    %.2f ms"), Settings->SevereFrameBudgetMs);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Spike Threshold:        %.2f ms"), Settings->ScreenshotSpikeThresholdMs);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Screenshot Cooldown:    %.2f seconds"), Settings->ScreenshotCooldownSeconds);
	UE_LOG(LogPerfSentinel, Log, TEXT("  Screenshot On Spike:    %s"), Settings->bCaptureScreenshotOnSpike ? TEXT("true") : TEXT("false"));

	if (Controller && Controller->HasCompletedTrace())
	{
		const FPerfSentinelTraceSession& Session = Controller->GetLastCompletedSession();
		UE_LOG(LogPerfSentinel, Log, TEXT("  Last Trace:             %s"), *Session.TracePath);
		UE_LOG(LogPerfSentinel, Log, TEXT("  Last Metadata:          %s"), *Session.MetadataPath);
	}
	else
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("  Last Trace:             <none>"));
	}
}

void FPerfSentinelConsoleCommands::CaptureSpikeScreenshot(const TArray<FString>& Args)
{
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller)
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("PerfSentinel.CaptureSpikeScreenshot: Trace controller is unavailable."));
		return;
	}

	FPerfSentinelRuntimeMonitor* Monitor = Controller->GetRuntimeMonitor();
	if (!Monitor || !Monitor->IsActive())
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("PerfSentinel.CaptureSpikeScreenshot: Runtime monitor is not active. Start a capture first."));
		return;
	}

	Monitor->CaptureManualSpike();
}

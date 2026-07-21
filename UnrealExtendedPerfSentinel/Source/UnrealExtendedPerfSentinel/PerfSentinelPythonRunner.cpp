// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelPythonRunner.h"

#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString QuoteArg(const FString& Value)
{
	return FString::Printf(TEXT("\"%s\""), *Value);
}

bool LooksLikePath(const FString& Value)
{
	return Value.Contains(TEXT("/")) || Value.Contains(TEXT("\\")) || Value.Contains(TEXT(":"));
}
}

bool FPerfSentinelPythonRunner::BuildRequestForSession(const FPerfSentinelTraceSession& Session, FPerfSentinelAnalysisRequest& OutRequest, FString& OutError) const
{
	if (Session.TracePath.IsEmpty())
	{
		OutError = TEXT("No trace path is available for the completed session.");
		return false;
	}

	if (!BuildCommonRequest(Session.TracePath, Session.MetadataPath, Session.FallbackStatsPath, Session.SpikeEventsPath, OutRequest, OutError))
	{
		return false;
	}
	if (!Session.ReportDirectory.IsEmpty())
	{
		OutRequest.OutputReportDirectory = Session.ReportDirectory;
	}
	OutRequest.BaselineKey = FPaths::MakeValidFileName(Session.ScenarioName.IsEmpty() ? FPaths::GetBaseFilename(Session.TracePath) : Session.ScenarioName);
	return true;
}

bool FPerfSentinelPythonRunner::BuildRequestForTrace(const FString& TracePath, FPerfSentinelAnalysisRequest& OutRequest, FString& OutError) const
{
	if (TracePath.IsEmpty())
	{
		OutError = TEXT("Trace path is empty.");
		return false;
	}

	FString ResolvedTracePath = FPaths::ConvertRelativePathToFull(TracePath);
	FPaths::NormalizeFilename(ResolvedTracePath);

	const FString MetadataPath = FPaths::ChangeExtension(ResolvedTracePath, TEXT(".metadata.json"));
	return BuildCommonRequest(ResolvedTracePath, MetadataPath, FString(), FString(), OutRequest, OutError);
}

bool FPerfSentinelPythonRunner::RunAnalysis(
	const FPerfSentinelAnalysisRequest& Request,
	FPerfSentinelProcessResult& OutResult,
	FString& OutError,
	const FPerfSentinelCancellationToken* CancellationToken) const
{
	OutResult = FPerfSentinelProcessResult();
	OutError.Reset();

	if (!FPaths::FileExists(Request.InputTracePath))
	{
		OutError = FString::Printf(TEXT("Trace file does not exist: %s"), *Request.InputTracePath);
		return false;
	}

	if (!ValidatePythonExecutable(Request.PythonExecutablePath, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(Request.AnalyzerScriptPath))
	{
		OutError = FString::Printf(TEXT("Analyzer script does not exist: %s"), *Request.AnalyzerScriptPath);
		return false;
	}

	FPerfSentinelAnalysisRequest MutableRequest = Request;
	if (!FPaths::FileExists(MutableRequest.InputMetadataPath) && !GenerateFallbackMetadata(MutableRequest, OutError))
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*MutableRequest.OutputReportDirectory))
	{
		OutError = FString::Printf(TEXT("Failed to create report directory: %s"), *MutableRequest.OutputReportDirectory);
		return false;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (Settings && Settings->bEnableNativeTraceExtraction)
	{
		FString NativeDiagnostic;
		if (!RunNativeExtraction(MutableRequest, CancellationToken, NativeDiagnostic))
		{
			if (CancellationToken && CancellationToken->IsCancelled())
			{
				OutError = TEXT("Analysis cancelled during native trace extraction.");
				return false;
			}
			UE_LOG(LogPerfSentinel, Warning, TEXT("PythonRunner: Native extraction unavailable; continuing with Unreal Insights CLI fallback. %s"), *NativeDiagnostic);
		}
	}

	FString InsightsExecutable;
	TryFindUnrealInsightsExecutable(InsightsExecutable);

	const FString Arguments = BuildAnalyzerArguments(MutableRequest, InsightsExecutable);
	UE_LOG(LogPerfSentinel, Log, TEXT("PythonRunner: Launching analyzer: %s %s"), *MutableRequest.PythonExecutablePath, *Arguments);

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	if (!FPlatformProcess::CreatePipe(PipeRead, PipeWrite))
	{
		OutError = TEXT("Failed to create analyzer stdout pipe.");
		return false;
	}

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*MutableRequest.PythonExecutablePath,
		*Arguments,
		false,
		true,
		true,
		nullptr,
		0,
		*FPaths::GetPath(MutableRequest.AnalyzerScriptPath),
		PipeWrite,
		PipeRead);

	if (!ProcHandle.IsValid())
	{
		FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
		OutError = FString::Printf(TEXT("Failed to launch Python analyzer: %s"), *MutableRequest.PythonExecutablePath);
		return false;
	}

	const double StartedAtSeconds = FPlatformTime::Seconds();
	const double TimeoutSeconds = FMath::Max(30, Request.AnalysisTimeoutSeconds);
	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		OutResult.StdOut += FPlatformProcess::ReadPipe(PipeRead);
		if (CancellationToken && CancellationToken->IsCancelled())
		{
			FPlatformProcess::TerminateProc(ProcHandle, true);
			OutError = TEXT("Analysis cancelled.");
			break;
		}
		if ((FPlatformTime::Seconds() - StartedAtSeconds) >= TimeoutSeconds)
		{
			FPlatformProcess::TerminateProc(ProcHandle, true);
			OutError = FString::Printf(TEXT("Analysis timed out after %d seconds."), Request.AnalysisTimeoutSeconds);
			break;
		}
		FPlatformProcess::Sleep(0.05f);
	}

	OutResult.StdOut += FPlatformProcess::ReadPipe(PipeRead);

	int32 ReturnCode = -1;
	FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
	FPlatformProcess::CloseProc(ProcHandle);
	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

	OutResult.ExitCode = ReturnCode;
	if (!OutError.IsEmpty())
	{
		return false;
	}

	const FString FindingsJson = FPaths::Combine(MutableRequest.OutputReportDirectory, TEXT("findings.json"));
	const FString FindingsMarkdown = FPaths::Combine(MutableRequest.OutputReportDirectory, TEXT("findings.md"));
	const FString EvidenceDatabase = FPaths::Combine(MutableRequest.OutputReportDirectory, TEXT("evidence.sqlite"));
	const FString AnalysisLog = FPaths::Combine(MutableRequest.OutputReportDirectory, TEXT("analysis.log"));
	if (FPaths::FileExists(FindingsJson))
	{
		OutResult.GeneratedFiles.Add(FindingsJson);
	}
	if (FPaths::FileExists(FindingsMarkdown))
	{
		OutResult.GeneratedFiles.Add(FindingsMarkdown);
	}
	if (FPaths::FileExists(EvidenceDatabase))
	{
		OutResult.GeneratedFiles.Add(EvidenceDatabase);
	}
	if (FPaths::FileExists(AnalysisLog))
	{
		OutResult.GeneratedFiles.Add(AnalysisLog);
	}
	if (!MutableRequest.NativeEvidencePath.IsEmpty() && FPaths::FileExists(MutableRequest.NativeEvidencePath))
	{
		OutResult.GeneratedFiles.Add(MutableRequest.NativeEvidencePath);
	}

	if (ReturnCode != 0)
	{
		OutError = FString::Printf(TEXT("Analyzer exited with code %d."), ReturnCode);
		return false;
	}

	if (!FPaths::FileExists(FindingsJson) || !FPaths::FileExists(FindingsMarkdown))
	{
		OutError = FString::Printf(TEXT("Analyzer completed but did not write expected findings files in: %s"), *MutableRequest.OutputReportDirectory);
		return false;
	}

	UE_LOG(LogPerfSentinel, Log, TEXT("PythonRunner: Analysis complete: %s"), *MutableRequest.OutputReportDirectory);
	return true;
}

bool FPerfSentinelPythonRunner::BuildCommonRequest(
	const FString& TracePath,
	const FString& MetadataPath,
	const FString& FallbackStatsPath,
	const FString& SpikeEventsPath,
	FPerfSentinelAnalysisRequest& OutRequest,
	FString& OutError) const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		OutError = TEXT("PerfSentinel settings are unavailable.");
		return false;
	}

	FString ResolvedTracePath = FPaths::ConvertRelativePathToFull(TracePath);
	FPaths::NormalizeFilename(ResolvedTracePath);

	FString ResolvedMetadataPath = MetadataPath.IsEmpty() ? FPaths::ChangeExtension(ResolvedTracePath, TEXT(".metadata.json")) : FPaths::ConvertRelativePathToFull(MetadataPath);
	FPaths::NormalizeFilename(ResolvedMetadataPath);

	FString ResolvedFallbackStatsPath = FallbackStatsPath;
	if (!ResolvedFallbackStatsPath.IsEmpty())
	{
		ResolvedFallbackStatsPath = FPaths::ConvertRelativePathToFull(ResolvedFallbackStatsPath);
		FPaths::NormalizeFilename(ResolvedFallbackStatsPath);
	}

	FString ResolvedSpikeEventsPath = SpikeEventsPath;
	if (!ResolvedSpikeEventsPath.IsEmpty())
	{
		ResolvedSpikeEventsPath = FPaths::ConvertRelativePathToFull(ResolvedSpikeEventsPath);
		FPaths::NormalizeFilename(ResolvedSpikeEventsPath);
	}

	OutRequest = FPerfSentinelAnalysisRequest();
	OutRequest.InputTracePath = ResolvedTracePath;
	OutRequest.InputMetadataPath = ResolvedMetadataPath;
	OutRequest.OutputReportDirectory = FPaths::Combine(Settings->GetResolvedReportOutputDirectory(), FPaths::GetBaseFilename(ResolvedTracePath));
	OutRequest.OutputReportDirectory = FPaths::ConvertRelativePathToFull(OutRequest.OutputReportDirectory);
	FPaths::NormalizeDirectoryName(OutRequest.OutputReportDirectory);
	OutRequest.AnalyzerScriptPath = Settings->GetResolvedAnalyzerScriptPath();
	OutRequest.PythonExecutablePath = Settings->GetResolvedPythonExecutable();
	OutRequest.FallbackStatsPath = ResolvedFallbackStatsPath;
	OutRequest.SpikeEventsPath = ResolvedSpikeEventsPath;
	OutRequest.FrameBudgetMs = Settings->FrameBudgetMs;
	OutRequest.HitchThresholdMs = Settings->HitchThresholdMs;
	OutRequest.SevereFrameBudgetMs = Settings->SevereFrameBudgetMs;
	OutRequest.BaselineDirectory = (Settings->bCompareAgainstBaseline || Settings->bUpdateBaselineAfterSuccessfulAnalysis || Settings->bCiMode)
		? Settings->GetResolvedBaselineDirectory()
		: FString();
	OutRequest.BaselineKey = FPaths::MakeValidFileName(FPaths::GetBaseFilename(ResolvedTracePath));
	OutRequest.bUpdateBaseline = Settings->bUpdateBaselineAfterSuccessfulAnalysis;
	OutRequest.bCiMode = Settings->bCiMode;
	OutRequest.MaxP99RegressionPercent = Settings->MaxP99RegressionPercent;
	OutRequest.MaxHitchesPerMinute = Settings->MaxHitchesPerMinute;
	OutRequest.MaxMemoryGrowthMB = Settings->MaxMemoryGrowthMB;
	OutRequest.AnalysisTimeoutSeconds = Settings->AnalysisTimeoutSeconds;
	return true;
}

bool FPerfSentinelPythonRunner::ValidatePythonExecutable(const FString& PythonExecutable, FString& OutError) const
{
	if (PythonExecutable.IsEmpty())
	{
		OutError = TEXT("Python executable is empty.");
		return false;
	}

	if (!LooksLikePath(PythonExecutable))
	{
		return true;
	}

	if (!FPaths::FileExists(PythonExecutable))
	{
		OutError = FString::Printf(TEXT("Python executable path does not exist: %s"), *PythonExecutable);
		return false;
	}

	return true;
}

bool FPerfSentinelPythonRunner::GenerateFallbackMetadata(FPerfSentinelAnalysisRequest& InOutRequest, FString& OutError) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*InOutRequest.OutputReportDirectory))
	{
		OutError = FString::Printf(TEXT("Failed to create report directory for fallback metadata: %s"), *InOutRequest.OutputReportDirectory);
		return false;
	}

	const FString FallbackMetadataPath = FPaths::Combine(InOutRequest.OutputReportDirectory, TEXT("fallback.metadata.json"));

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("trace_file"), InOutRequest.InputTracePath);
	Root->SetStringField(TEXT("scenario"), FPaths::GetBaseFilename(InOutRequest.InputTracePath));
	Root->SetStringField(TEXT("started_at"), TEXT(""));
	Root->SetStringField(TEXT("stopped_at"), TEXT(""));
	Root->SetNumberField(TEXT("duration_seconds"), 0.0);
	Root->SetStringField(TEXT("project"), FApp::GetProjectName());
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
	Budgets->SetNumberField(TEXT("frame_budget_ms"), InOutRequest.FrameBudgetMs);
	Budgets->SetNumberField(TEXT("hitch_threshold_ms"), InOutRequest.HitchThresholdMs);
	Budgets->SetNumberField(TEXT("severe_frame_budget_ms"), InOutRequest.SevereFrameBudgetMs);
	Root->SetObjectField(TEXT("budgets"), Budgets);

	if (!InOutRequest.SpikeEventsPath.IsEmpty())
	{
		Root->SetStringField(TEXT("spike_events_file"), InOutRequest.SpikeEventsPath);
	}
	if (!InOutRequest.FallbackStatsPath.IsEmpty())
	{
		Root->SetStringField(TEXT("fallback_stats_file"), InOutRequest.FallbackStatsPath);
	}

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);

	if (!FFileHelper::SaveStringToFile(Output, *FallbackMetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write fallback metadata: %s"), *FallbackMetadataPath);
		return false;
	}

	InOutRequest.InputMetadataPath = FallbackMetadataPath;
	UE_LOG(LogPerfSentinel, Warning, TEXT("PythonRunner: Metadata sidecar missing. Wrote fallback metadata: %s"), *FallbackMetadataPath);
	return true;
}

bool FPerfSentinelPythonRunner::RunNativeExtraction(
	FPerfSentinelAnalysisRequest& InOutRequest,
	const FPerfSentinelCancellationToken* CancellationToken,
	FString& OutDiagnostic) const
{
	OutDiagnostic.Reset();
#if WITH_EDITOR
	if (IsRunningCommandlet())
	{
		OutDiagnostic = TEXT("Native extraction is already running inside a commandlet.");
		return false;
	}

	FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FPaths::NormalizeFilename(ProjectFile);
	if (!FPaths::FileExists(ProjectFile))
	{
		OutDiagnostic = FString::Printf(TEXT("Project file was not found: %s"), *ProjectFile);
		return false;
	}

	const FString NativeEvidencePath = FPaths::Combine(InOutRequest.OutputReportDirectory, TEXT("native_evidence.json"));
	const FString Executable = FPlatformProcess::ExecutablePath();
	const FString Arguments = FString::Printf(
		TEXT("%s -run=PerfSentinelAnalyze -Trace=%s -Out=%s -HitchThresholdMs=%s -unattended -nop4 -nosplash -NoLogTimes"),
		*QuoteArg(ProjectFile),
		*QuoteArg(InOutRequest.InputTracePath),
		*QuoteArg(NativeEvidencePath),
		*FString::SanitizeFloat(InOutRequest.HitchThresholdMs));

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	if (!FPlatformProcess::CreatePipe(PipeRead, PipeWrite))
	{
		OutDiagnostic = TEXT("Could not create native extractor output pipe.");
		return false;
	}

	FProcHandle Process = FPlatformProcess::CreateProc(
		*Executable, *Arguments, false, true, true, nullptr, 0, *FPaths::ProjectDir(), PipeWrite, PipeRead);
	if (!Process.IsValid())
	{
		FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
		OutDiagnostic = FString::Printf(TEXT("Could not launch native extractor: %s"), *Executable);
		return false;
	}

	FString Output;
	const double StartedAt = FPlatformTime::Seconds();
	const double Timeout = FMath::Max(30, InOutRequest.AnalysisTimeoutSeconds);
	while (FPlatformProcess::IsProcRunning(Process))
	{
		Output += FPlatformProcess::ReadPipe(PipeRead);
		if ((CancellationToken && CancellationToken->IsCancelled()) || (FPlatformTime::Seconds() - StartedAt) >= Timeout)
		{
			FPlatformProcess::TerminateProc(Process, true);
			break;
		}
		FPlatformProcess::Sleep(0.05f);
	}
	Output += FPlatformProcess::ReadPipe(PipeRead);
	int32 ExitCode = -1;
	FPlatformProcess::GetProcReturnCode(Process, &ExitCode);
	FPlatformProcess::CloseProc(Process);
	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

	bool bValidNativeEvidence = false;
	if (FPaths::FileExists(NativeEvidencePath))
	{
		FString NativeJson;
		if (FFileHelper::LoadFileToString(NativeJson, *NativeEvidencePath))
		{
			TSharedPtr<FJsonObject> NativeRoot;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NativeJson);
			bValidNativeEvidence = FJsonSerializer::Deserialize(Reader, NativeRoot)
				&& NativeRoot.IsValid()
				&& NativeRoot->HasTypedField<EJson::Number>(TEXT("schema_version"))
				&& NativeRoot->HasTypedField<EJson::String>(TEXT("extractor"));
		}
	}

	if (!bValidNativeEvidence)
	{
		OutDiagnostic = FString::Printf(TEXT("Native extractor did not produce valid evidence (exit code %d). %s"), ExitCode, *Output.Right(4000));
		return false;
	}
	if (ExitCode != 0)
	{
		// Commandlet hosts can inherit recoverable startup/provider errors from the
		// project and still produce a complete evidence document. The document is
		// the contract; preserve the diagnostic without discarding valid evidence.
		UE_LOG(LogPerfSentinel, Warning, TEXT("PythonRunner: Native extractor returned %d after producing valid evidence."), ExitCode);
	}

	InOutRequest.NativeEvidencePath = NativeEvidencePath;
	UE_LOG(LogPerfSentinel, Log, TEXT("PythonRunner: Native trace evidence extracted: %s"), *NativeEvidencePath);
	return true;
#else
	OutDiagnostic = TEXT("Native extraction requires an editor build.");
	return false;
#endif
}

bool FPerfSentinelPythonRunner::TryFindUnrealInsightsExecutable(FString& OutInsightsPath) const
{
#if PLATFORM_WINDOWS
	const FString PlatformBinaryDir = TEXT("Win64");
	const FString ExecutableName = TEXT("UnrealInsights.exe");
#elif PLATFORM_MAC
	const FString PlatformBinaryDir = TEXT("Mac");
	const FString ExecutableName = TEXT("UnrealInsights");
#else
	const FString PlatformBinaryDir = TEXT("Linux");
	const FString ExecutableName = TEXT("UnrealInsights");
#endif

	OutInsightsPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries"), PlatformBinaryDir, ExecutableName);
	OutInsightsPath = FPaths::ConvertRelativePathToFull(OutInsightsPath);
	FPaths::NormalizeFilename(OutInsightsPath);

	if (!FPaths::FileExists(OutInsightsPath))
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("PythonRunner: UnrealInsights executable was not found at: %s"), *OutInsightsPath);
		OutInsightsPath.Reset();
		return false;
	}

	return true;
}

FString FPerfSentinelPythonRunner::BuildAnalyzerArguments(const FPerfSentinelAnalysisRequest& Request, const FString& InsightsExecutable) const
{
	TArray<FString> Args;
	Args.Add(QuoteArg(Request.AnalyzerScriptPath));
	Args.Add(TEXT("--trace"));
	Args.Add(QuoteArg(Request.InputTracePath));
	Args.Add(TEXT("--metadata"));
	Args.Add(QuoteArg(Request.InputMetadataPath));
	Args.Add(TEXT("--out"));
	Args.Add(QuoteArg(Request.OutputReportDirectory));
	Args.Add(TEXT("--frame-budget-ms"));
	Args.Add(FString::SanitizeFloat(Request.FrameBudgetMs));
	Args.Add(TEXT("--hitch-threshold-ms"));
	Args.Add(FString::SanitizeFloat(Request.HitchThresholdMs));

	if (!Request.SpikeEventsPath.IsEmpty() && FPaths::FileExists(Request.SpikeEventsPath))
	{
		Args.Add(TEXT("--spikes"));
		Args.Add(QuoteArg(Request.SpikeEventsPath));
	}

	if (!Request.FallbackStatsPath.IsEmpty() && FPaths::FileExists(Request.FallbackStatsPath))
	{
		Args.Add(TEXT("--fallback-stats"));
		Args.Add(QuoteArg(Request.FallbackStatsPath));
	}

	if (!InsightsExecutable.IsEmpty())
	{
		Args.Add(TEXT("--insights-exe"));
		Args.Add(QuoteArg(InsightsExecutable));
	}

	if (!Request.NativeEvidencePath.IsEmpty() && FPaths::FileExists(Request.NativeEvidencePath))
	{
		Args.Add(TEXT("--native-evidence"));
		Args.Add(QuoteArg(Request.NativeEvidencePath));
	}
	if (!Request.BaselineDirectory.IsEmpty())
	{
		Args.Add(TEXT("--baseline-dir"));
		Args.Add(QuoteArg(Request.BaselineDirectory));
		Args.Add(TEXT("--baseline-key"));
		Args.Add(QuoteArg(Request.BaselineKey));
	}
	if (Request.bUpdateBaseline)
	{
		Args.Add(TEXT("--update-baseline"));
	}
	if (Request.bCiMode)
	{
		Args.Add(TEXT("--ci"));
	}
	Args.Add(TEXT("--max-p99-regression-percent"));
	Args.Add(FString::SanitizeFloat(Request.MaxP99RegressionPercent));
	Args.Add(TEXT("--max-hitches-per-minute"));
	Args.Add(FString::SanitizeFloat(Request.MaxHitchesPerMinute));
	Args.Add(TEXT("--max-memory-growth-mb"));
	Args.Add(FString::SanitizeFloat(Request.MaxMemoryGrowthMB));

	return FString::Join(Args, TEXT(" "));
}

FString FPerfSentinelPythonRunner::BuildReportDirectory(const FString& TracePath) const
{
	FString ReportDirectory = FPaths::GetPath(TracePath);
	ReportDirectory = FPaths::ConvertRelativePathToFull(ReportDirectory);
	FPaths::NormalizeDirectoryName(ReportDirectory);
	return ReportDirectory;
}

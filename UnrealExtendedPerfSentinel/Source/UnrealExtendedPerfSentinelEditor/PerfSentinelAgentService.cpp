// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "PerfSentinelAgentService.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelPythonRunner.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"
#include "PerfSentinelTelemetry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealExtendedPerfSentinel.h"

namespace
{
constexpr int32 MaxStoredJobs = 32;
constexpr int32 MaxActiveJobs = 4;
constexpr int32 MaxRequestBytes = 16384;
constexpr int64 MaxResponseBytes = 512 * 1024;
constexpr double QueryTimeoutSeconds = 40.0;

FString Json(const TSharedRef<FJsonObject>& Object)
{
	FString Result;
	FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result));
	return Result;
}
FPerfSentinelAgentResponse Failed(const FString& Operation, const FString& Error)
{
	FPerfSentinelAgentResponse Result; Result.Operation = Operation; Result.Error = Error; return Result;
}
FPerfSentinelAgentResponse Immediate(const FString& Operation, const TSharedRef<FJsonObject>& Data)
{
	FPerfSentinelAgentResponse Result; Result.Operation = Operation; Result.State = TEXT("completed"); Result.bSuccess = true;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetBoolField(TEXT("ok"), true); Root->SetStringField(TEXT("operation"), Operation); Root->SetObjectField(TEXT("data"), Data);
	Result.ResultJson = Json(Root); return Result;
}
FPerfSentinelAgentResponse WithOperation(FPerfSentinelAgentResponse Response, const FString& Operation)
{
	Response.Operation = Operation;
	TSharedPtr<FJsonObject> Root;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Response.ResultJson), Root) && Root.IsValid())
	{
		Root->SetStringField(TEXT("operation"), Operation); Response.ResultJson = Json(Root.ToSharedRef());
	}
	return Response;
}
bool SafeText(const FString& Value, int32 MaxLength)
{
	return Value.Len() <= MaxLength && !Value.Contains(TEXT("\r")) && !Value.Contains(TEXT("\n")) && !Value.Contains(TEXT("\""));
}
FString Quote(const FString& Value) { return FString::Printf(TEXT("\"%s\""), *Value); }

struct FAgentJob
{
	FCriticalSection Mutex;
	FPerfSentinelAgentResponse Response;
	TAtomic<bool> Cancelled{ false };
	double CreatedSeconds = 0.0;
	TFuture<void> Future;
};
TMap<FString, TSharedPtr<FAgentJob, ESPMode::ThreadSafe>> Jobs;
bool bServiceAvailable = false;

void SetFailure(const TSharedRef<FAgentJob, ESPMode::ThreadSafe>& Job, const FString& Error, bool bCancelled = false)
{
	FScopeLock Lock(&Job->Mutex);
	Job->Response.State = bCancelled ? TEXT("cancelled") : TEXT("failed");
	Job->Response.bSuccess = false; Job->Response.Error = Error;
}

void RunQuery(const TSharedRef<FAgentJob, ESPMode::ThreadSafe>& Job, const FString& Python, const FString& Script,
	const FString& ReportRoot, const FString& JobDirectory, const FString& RequestJson)
{
	if (Job->Cancelled.Load()) { SetFailure(Job, TEXT("Query cancelled"), true); return; }
	const FString RequestPath = FPaths::Combine(JobDirectory, Job->Response.RequestId + TEXT(".request.json"));
	const FString OutputPath = FPaths::Combine(JobDirectory, Job->Response.RequestId + TEXT(".response.json"));
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.CreateDirectoryTree(*JobDirectory) || !FFileHelper::SaveStringToFile(RequestJson, *RequestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetFailure(Job, TEXT("Cannot create bounded query request")); return;
	}
	const FString Args = Quote(Script) + TEXT(" --root ") + Quote(ReportRoot) + TEXT(" --request ") + Quote(RequestPath) + TEXT(" --out ") + Quote(OutputPath);
	FProcHandle Process = FPlatformProcess::CreateProc(*Python, *Args, false, true, true, nullptr, 0, *FPaths::ProjectDir(), nullptr, nullptr);
	if (!Process.IsValid()) { File.DeleteFile(*RequestPath); SetFailure(Job, TEXT("Cannot start Python query process; configure a valid Python executable")); return; }
	{
		FScopeLock Lock(&Job->Mutex); Job->Response.State = TEXT("running");
	}
	const double Deadline = FPlatformTime::Seconds() + QueryTimeoutSeconds;
	while (FPlatformProcess::IsProcRunning(Process) && !Job->Cancelled.Load() && FPlatformTime::Seconds() < Deadline)
	{
		// This loop executes only on a worker. No UObject/editor API or game-thread wait.
		FPlatformProcess::Sleep(0.05f);
	}
	const bool bCancelled = Job->Cancelled.Load();
	const bool bTimedOut = FPlatformProcess::IsProcRunning(Process);
	if (bTimedOut) { FPlatformProcess::TerminateProc(Process, true); }
	int32 ExitCode = -1; FPlatformProcess::GetProcReturnCode(Process, &ExitCode); FPlatformProcess::CloseProc(Process);
	FString ResponseJson;
	const int64 Size = File.FileSize(*OutputPath);
	const bool bLoaded = Size >= 0 && Size <= MaxResponseBytes && FFileHelper::LoadFileToString(ResponseJson, *OutputPath);
	File.DeleteFile(*RequestPath); File.DeleteFile(*OutputPath); File.DeleteFile(*(OutputPath + TEXT(".tmp")));
	if (bCancelled || bTimedOut) { SetFailure(Job, bCancelled ? TEXT("Query cancelled") : TEXT("Query timed out; narrow the requested evidence scope"), bCancelled); return; }
	TSharedPtr<FJsonObject> Payload;
	if (!bLoaded || !FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(ResponseJson), Payload) || !Payload.IsValid())
	{
		SetFailure(Job, TEXT("Query returned no valid bounded JSON response")); return;
	}
	bool bOk = false; Payload->TryGetBoolField(TEXT("ok"), bOk);
	FScopeLock Lock(&Job->Mutex);
	Job->Response.State = bOk && ExitCode == 0 ? TEXT("completed") : TEXT("failed");
	Job->Response.bSuccess = bOk && ExitCode == 0; Job->Response.ResultJson = MoveTemp(ResponseJson);
	if (!Job->Response.bSuccess) { Payload->TryGetStringField(TEXT("error"), Job->Response.Error); }
}

void PruneJobs()
{
	// Submitted/polled on the game thread. Workers modify only their own locked response.
	TArray<FString> Remove;
	for (const auto& Pair : Jobs)
	{
		if (Pair.Value->Future.IsValid() && Pair.Value->Future.IsReady() && FPlatformTime::Seconds() - Pair.Value->CreatedSeconds > 600.0) { Remove.Add(Pair.Key); }
	}
	for (const FString& Key : Remove) { Jobs.Remove(Key); }
	while (Jobs.Num() >= MaxStoredJobs)
	{
		FString Oldest; double Time = DBL_MAX;
		for (const auto& Pair : Jobs)
		{
			if (Pair.Value->Future.IsValid() && Pair.Value->Future.IsReady() && Pair.Value->CreatedSeconds < Time) { Oldest = Pair.Key; Time = Pair.Value->CreatedSeconds; }
		}
		if (Oldest.IsEmpty()) { break; }
		Jobs.Remove(Oldest);
	}
}
}

void FPerfSentinelAgentService::Initialize() { bServiceAvailable = true; }
void FPerfSentinelAgentService::Shutdown()
{
	bServiceAvailable = false;
	for (const auto& Pair : Jobs) { Pair.Value->Cancelled.Store(true); }
	// Module teardown must finish workers before their native code can unload.
	for (const auto& Pair : Jobs) { if (Pair.Value->Future.IsValid()) { Pair.Value->Future.Wait(); } }
	Jobs.Empty();
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::Submit(const FString& Operation, const TSharedRef<FJsonObject>& Parameters)
{
	if (!IsInGameThread() || !bServiceAvailable) { return Failed(Operation, TEXT("Agent query service is unavailable on this thread")); }
	PruneJobs();
	int32 Active = 0;
	for (const auto& Pair : Jobs) { if (Pair.Value->Future.IsValid() && !Pair.Value->Future.IsReady()) { ++Active; } }
	if (Active >= MaxActiveJobs || Jobs.Num() >= MaxStoredJobs) { return Failed(Operation, TEXT("Bounded agent query capacity is full; wait for a current request")); }
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedPerfSentinel"));
	if (!Settings || !Plugin) { return Failed(Operation, TEXT("Perf Sentinel settings or plugin directory is unavailable")); }
	const FString Python = Settings->GetResolvedPythonExecutable();
	const FString Root = Settings->GetResolvedReportOutputDirectory();
	const FString Script = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts/perf_sentinel_query.py"));
	const FString JobDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PerfSentinel/AgentQueries")));
	if (!SafeText(Python, 2048) || !SafeText(Root, 4096) || !SafeText(Script, 4096) || !SafeText(JobDirectory, 4096)) { return Failed(Operation, TEXT("Configured query paths contain invalid characters")); }
	Parameters->SetStringField(TEXT("operation"), Operation);
	const FString Request = Json(Parameters);
	if (FTCHARToUTF8(*Request).Length() > MaxRequestBytes) { return Failed(Operation, TEXT("Query exceeds bounded input size")); }
	const TSharedRef<FAgentJob, ESPMode::ThreadSafe> Job = MakeShared<FAgentJob, ESPMode::ThreadSafe>();
	Job->CreatedSeconds = FPlatformTime::Seconds(); Job->Response.RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Job->Response.Operation = Operation; Job->Response.State = TEXT("queued");
	Job->Response.bSuccess = true;
	Jobs.Add(Job->Response.RequestId, Job);
	Job->Future = Async(EAsyncExecution::ThreadPool, [Job, Python, Script, Root, JobDirectory, Request]() { RunQuery(Job, Python, Script, Root, JobDirectory, Request); });
	FScopeLock Lock(&Job->Mutex); return Job->Response;
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::Status(const FString& RequestId)
{
	if (!IsInGameThread()) { return Failed(TEXT("query_status"), TEXT("Status must be read on the game thread")); }
	if (!SafeText(RequestId, 64)) { return Failed(TEXT("query_status"), TEXT("Invalid bounded query ID")); }
	const auto* Job = Jobs.Find(RequestId);
	if (!Job) { return Failed(TEXT("query_status"), TEXT("Query ID is expired or not found")); }
	FScopeLock Lock(&(*Job)->Mutex); return (*Job)->Response;
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::Cancel(const FString& RequestId)
{
	if (!IsInGameThread()) { return Failed(TEXT("cancel_query"), TEXT("Cancellation must run on the game thread")); }
	if (!SafeText(RequestId, 64)) { return Failed(TEXT("cancel_query"), TEXT("Invalid bounded query ID")); }
	const auto* Job = Jobs.Find(RequestId);
	if (!Job) { return Failed(TEXT("cancel_query"), TEXT("Query ID is expired or not found")); }
	if (!(*Job)->Future.IsReady()) { (*Job)->Cancelled.Store(true); }
	FScopeLock Lock(&(*Job)->Mutex); return (*Job)->Response;
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::CaptureStatus()
{
	if (!IsInGameThread()) { return Failed(TEXT("capture_status"), TEXT("Capture status requires the game thread")); }
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller) { return Failed(TEXT("capture_status"), TEXT("Runtime capture controller is unavailable")); }
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("capturing"), Controller->IsCapturing()); Data->SetBoolField(TEXT("idle"), Controller->IsIdle());
	Data->SetStringField(TEXT("state"), StaticEnum<EPerfSentinelCaptureState>()->GetNameStringByValue(static_cast<int64>(Controller->GetCaptureState())));
	TSharedPtr<FJsonObject> Session = FJsonObjectConverter::UStructToJsonObject(Controller->GetCurrentSession());
	TSharedPtr<FJsonObject> Last = FJsonObjectConverter::UStructToJsonObject(Controller->GetLastCompletedSession());
	if (Session) { Data->SetObjectField(TEXT("current_session"), Session); } if (Last) { Data->SetObjectField(TEXT("last_completed_session"), Last); }
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (Settings)
	{
		Data->SetStringField(TEXT("profile"), StaticEnum<EPerfSentinelCaptureProfile>()->GetNameStringByValue(static_cast<int64>(Settings->CaptureProfile)));
		Data->SetBoolField(TEXT("requires_relaunch"), Settings->CaptureProfileRequiresRelaunch());
		Data->SetBoolField(TEXT("launch_requirements_satisfied"), Settings->AreRequiredLaunchArgumentsPresent());
		TArray<TSharedPtr<FJsonValue>> Flags; for (const FString& Flag : Settings->GetRequiredLaunchArguments()) { Flags.Add(MakeShared<FJsonValueString>(Flag)); }
		Data->SetArrayField(TEXT("required_launch_arguments"), Flags);
	}
	return Immediate(TEXT("capture_status"), Data);
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::AnalysisStatus()
{
	if (!IsInGameThread()) { return Failed(TEXT("analysis_status"), TEXT("Analysis status requires the game thread")); }
	const auto Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	if (!Manager) { return Failed(TEXT("analysis_status"), TEXT("Runtime analysis manager is unavailable")); }
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>(); Data->SetBoolField(TEXT("running"), Manager->IsRunning());
	Data->SetStringField(TEXT("state"), StaticEnum<EPerfSentinelAnalysisState>()->GetNameStringByValue(static_cast<int64>(Manager->GetState())));
	Data->SetStringField(TEXT("last_error"), Manager->GetLastError().Left(4096));
	Data->SetNumberField(TEXT("last_exit_code"), Manager->GetLastResult().ExitCode);
	return Immediate(TEXT("analysis_status"), Data);
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::ConfigureContext(const FString& RunId, const FString& ProcessRole, const FString& BuildId)
{
	if (!IsInGameThread()) { return Failed(TEXT("configure_context"), TEXT("Context changes require the game thread")); }
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller || !Controller->IsIdle()) { return Failed(TEXT("configure_context"), TEXT("Capture context can change only while idle")); }
	if (!SafeText(RunId, 256) || !SafeText(ProcessRole, 128) || !SafeText(BuildId, 256)) { return Failed(TEXT("configure_context"), TEXT("Capture context exceeds bounded text limits")); }
	UPerfSentinelSettings* Settings = GetMutableDefault<UPerfSentinelSettings>();
	if (!FPerfSentinelTelemetry::Get().ConfigureDefaultContext(RunId, ProcessRole, Settings->ClockOffsetMilliseconds, Settings->ClockUncertaintyMilliseconds))
	{
		return Failed(TEXT("configure_context"), TEXT("Telemetry context cannot change during an active capture"));
	}
	Settings->SharedRunId = RunId; Settings->ProcessRole = ProcessRole; Settings->BuildId = BuildId;
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>(); Data->SetStringField(TEXT("run_id"), RunId); Data->SetStringField(TEXT("process_role"), ProcessRole); Data->SetStringField(TEXT("build_id"), BuildId);
	Data->SetBoolField(TEXT("persisted"), false); return Immediate(TEXT("configure_context"), Data);
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::StartCapture(const FString& ScenarioName)
{
	if (!IsInGameThread() || !SafeText(ScenarioName, 256)) { return Failed(TEXT("start_capture"), TEXT("Invalid capture request")); }
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller || !Controller->IsIdle() || !Controller->StartCapture(ScenarioName)) { return Failed(TEXT("start_capture"), TEXT("Owned capture could not start; inspect current state, external recorder and launch requirements")); }
	return WithOperation(CaptureStatus(), TEXT("start_capture"));
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::StopCapture()
{
	if (!IsInGameThread()) { return Failed(TEXT("stop_capture"), TEXT("Capture operations require the game thread")); }
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	if (!Controller || !Controller->IsCapturing() || !Controller->StopCapture()) { return Failed(TEXT("stop_capture"), TEXT("No owned active capture was stopped")); }
	return WithOperation(CaptureStatus(), TEXT("stop_capture"));
}
FPerfSentinelAgentResponse FPerfSentinelAgentService::AnalyzeLastCapture()
{
	if (!IsInGameThread()) { return Failed(TEXT("analyze_last_capture"), TEXT("Analysis requests require the game thread")); }
	FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController();
	const auto Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager();
	if (!Controller || !Controller->IsIdle() || !Controller->HasCompletedTrace() || !Manager || Manager->IsRunning()) { return Failed(TEXT("analyze_last_capture"), TEXT("A completed idle owned capture and idle analysis manager are required")); }
	FPerfSentinelAnalysisRequest Request; FString Error; FPerfSentinelPythonRunner Runner;
	if (!Runner.BuildRequestForSession(Controller->GetLastCompletedSession(), Request, Error) || !Manager->StartAnalysis(Request, Error)) { return Failed(TEXT("analyze_last_capture"), Error); }
	return WithOperation(AnalysisStatus(), TEXT("analyze_last_capture"));
}

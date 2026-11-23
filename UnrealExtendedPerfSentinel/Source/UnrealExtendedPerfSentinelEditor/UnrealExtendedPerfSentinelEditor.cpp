// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedPerfSentinelEditor.h"

#include "UnrealExtendedPerfSentinel.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelEditorCommands.h"
#include "PerfSentinelEditorMenu.h"
#include "PerfSentinelReportView.h"
#include "PerfSentinelAgentService.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#if PERFSENTINEL_WITH_TOOLSET_REGISTRY
#include "PerfSentinelAgentToolset.h"
#endif

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedPerfSentinelEditorModule"

void FUnrealExtendedPerfSentinelEditorModule::StartupModule()
{
	// The module loads during the Default phase so Unreal can discover the
	// analyzer commandlet. Commandlets do not need any editor UI registration.
	if (IsRunningCommandlet())
	{
		return;
	}

	FPerfSentinelEditorCommands::Register();
	FPerfSentinelAgentService::Initialize();
#if PERFSENTINEL_WITH_TOOLSET_REGISTRY
	PerfSentinelAgentToolset::Register();
#endif
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PerfSentinelReportTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealExtendedPerfSentinelEditorModule::SpawnReportTab))
		.SetDisplayName(LOCTEXT("ReportTabName", "PerfSentinel Report"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		AnalysisCompletedHandle = Manager->OnCompleted().AddRaw(this, &FUnrealExtendedPerfSentinelEditorModule::HandleAnalysisCompleted);
	}

	if (FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController())
	{
		CaptureActiveChangedHandle = Controller->OnCaptureActiveChanged().AddRaw(this, &FUnrealExtendedPerfSentinelEditorModule::HandleCaptureActiveChanged);
	}
	EditorPreExitHandle = FEditorDelegates::OnEditorPreExit.AddRaw(this, &FUnrealExtendedPerfSentinelEditorModule::HandleEditorPreExit);

	EditorMenu = MakeUnique<FPerfSentinelEditorMenu>();
	EditorMenu->Register();
}

void FUnrealExtendedPerfSentinelEditorModule::ShutdownModule()
{
	if (IsRunningCommandlet())
	{
		return;
	}
#if PERFSENTINEL_WITH_TOOLSET_REGISTRY
	PerfSentinelAgentToolset::Unregister();
#endif
	FPerfSentinelAgentService::Shutdown();

	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		Manager->OnCompleted().Remove(AnalysisCompletedHandle);
	}
	if (FPerfSentinelTraceController* Controller = FUnrealExtendedPerfSentinelModule::GetTraceController())
	{
		Controller->OnCaptureActiveChanged().Remove(CaptureActiveChangedHandle);
	}
	FEditorDelegates::OnEditorPreExit.Remove(EditorPreExitHandle);
	// No settings object here: modules shut down after the UObject system, so the setting went back in HandleEditorPreExit.
	// (A module unloaded while the editor keeps running still has its UObjects and puts it back now.)
	if (bRestoreBackgroundThrottle && UObjectInitialized() && !IsEngineExitRequested())
	{
		HandleCaptureActiveChanged(false);
	}
	bRestoreBackgroundThrottle = false;
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PerfSentinelReportTabName);

	if (EditorMenu)
	{
		EditorMenu->Unregister();
	}
	EditorMenu.Reset();
	FPerfSentinelEditorCommands::Unregister();
}

TSharedRef<SDockTab> FUnrealExtendedPerfSentinelEditorModule::SpawnReportTab(const FSpawnTabArgs& Args)
{
	(void)Args;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPerfSentinelReportView)
		];
}

void FUnrealExtendedPerfSentinelEditorModule::HandleAnalysisCompleted(bool bSucceeded, const FPerfSentinelProcessResult& Result, const FString& Error)
{
	(void)Result;
	(void)Error;
	if (bSucceeded)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(PerfSentinelReportTabName);
	}
}

void FUnrealExtendedPerfSentinelEditorModule::HandleCaptureActiveChanged(bool bCapturing)
{
	// A stop with nothing to put back touches no UObject.
	if (!bCapturing && !bRestoreBackgroundThrottle)
	{
		return;
	}
	UEditorPerformanceSettings* PerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();
	if (!PerformanceSettings)
	{
		return;
	}
	if (bCapturing)
	{
		const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
		if (Settings && Settings->bKeepFullSpeedWhenUnfocused && PerformanceSettings->bThrottleCPUWhenNotForeground)
		{
			// Changed in memory only and never saved, so a crash mid-capture leaves the user's setting as it was.
			PerformanceSettings->bThrottleCPUWhenNotForeground = false;
			bRestoreBackgroundThrottle = true;
			UE_LOG(LogPerfSentinel, Log, TEXT("Capture: 'Use Less CPU when in Background' is off until the capture stops."));
		}
	}
	else if (bRestoreBackgroundThrottle)
	{
		PerformanceSettings->bThrottleCPUWhenNotForeground = true;
		bRestoreBackgroundThrottle = false;
	}
}

void FUnrealExtendedPerfSentinelEditorModule::HandleEditorPreExit()
{
	// A capture still running at exit: the setting goes back before the editor writes its settings file.
	HandleCaptureActiveChanged(false);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedPerfSentinelEditorModule, UnrealExtendedPerfSentinelEditor)

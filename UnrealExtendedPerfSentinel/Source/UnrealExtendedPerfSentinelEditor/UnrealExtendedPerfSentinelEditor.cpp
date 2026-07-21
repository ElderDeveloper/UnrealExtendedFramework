// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedPerfSentinelEditor.h"

#include "UnrealExtendedPerfSentinel.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelEditorCommands.h"
#include "PerfSentinelEditorMenu.h"
#include "PerfSentinelReportView.h"

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
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PerfSentinelReportTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealExtendedPerfSentinelEditorModule::SpawnReportTab))
		.SetDisplayName(LOCTEXT("ReportTabName", "PerfSentinel Report"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		AnalysisCompletedHandle = Manager->OnCompleted().AddRaw(this, &FUnrealExtendedPerfSentinelEditorModule::HandleAnalysisCompleted);
	}

	EditorMenu = MakeUnique<FPerfSentinelEditorMenu>();
	EditorMenu->Register();
}

void FUnrealExtendedPerfSentinelEditorModule::ShutdownModule()
{
	if (IsRunningCommandlet())
	{
		return;
	}

	if (const TSharedPtr<FPerfSentinelAnalysisManager> Manager = FUnrealExtendedPerfSentinelModule::GetAnalysisManager())
	{
		Manager->OnCompleted().Remove(AnalysisCompletedHandle);
	}
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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedPerfSentinelEditorModule, UnrealExtendedPerfSentinelEditor)

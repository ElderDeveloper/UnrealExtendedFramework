// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelEditorCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PerfSentinelEditorCommands"

FPerfSentinelEditorCommands::FPerfSentinelEditorCommands()
	: TCommands<FPerfSentinelEditorCommands>(
		TEXT("PerfSentinelEditor"),
		LOCTEXT("ContextDescription", "PerfSentinel"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FPerfSentinelEditorCommands::RegisterCommands()
{
	UI_COMMAND(StartTraceCapture, "Start Trace Capture", "Start a PerfSentinel trace capture.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopTraceCapture, "Stop Trace Capture", "Stop the active PerfSentinel trace capture.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnalyzeLastTrace, "Analyze Last Trace", "Run the analyzer for the last completed PerfSentinel trace.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnalyzeExistingTrace, "Analyze Existing Trace...", "Choose and analyze an existing .utrace file.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CancelAnalysis, "Cancel Analysis", "Cancel the active PerfSentinel analysis job.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenLastReport, "Open Last Report", "Open the newest findings report in the PerfSentinel report viewer.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LaunchProfileSession, "Launch Profile Session", "Launch a standalone game process with the selected capture profile's startup-only trace providers.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CopyProfileLaunchArguments, "Copy Profile Launch Arguments", "Copy the selected capture profile's required process launch arguments.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenReportsFolder, "Open Reports Folder", "Open the PerfSentinel reports folder.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenPerfSentinelSettings, "Open PerfSentinel Settings", "Open PerfSentinel project settings.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

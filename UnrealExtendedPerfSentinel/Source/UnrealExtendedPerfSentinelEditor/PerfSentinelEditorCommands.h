// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FPerfSentinelEditorCommands : public TCommands<FPerfSentinelEditorCommands>
{
public:
	FPerfSentinelEditorCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> StartTraceCapture;
	TSharedPtr<FUICommandInfo> StopTraceCapture;
	TSharedPtr<FUICommandInfo> AnalyzeLastTrace;
	TSharedPtr<FUICommandInfo> AnalyzeExistingTrace;
	TSharedPtr<FUICommandInfo> CancelAnalysis;
	TSharedPtr<FUICommandInfo> OpenLastReport;
	TSharedPtr<FUICommandInfo> LaunchProfileSession;
	TSharedPtr<FUICommandInfo> CopyProfileLaunchArguments;
	TSharedPtr<FUICommandInfo> OpenReportsFolder;
	TSharedPtr<FUICommandInfo> OpenPerfSentinelSettings;
};

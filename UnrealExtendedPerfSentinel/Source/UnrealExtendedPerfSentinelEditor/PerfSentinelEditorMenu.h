// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUICommandList;
class FPerfSentinelTraceController;
enum class EPerfSentinelCaptureProfile : uint8;

class FPerfSentinelEditorMenu
{
public:
	void Register();
	void Unregister();

private:
	void RegisterMenus();
	void BindCommands();

	void StartTraceCapture();
	void StopTraceCapture();
	void AnalyzeLastTrace();
	void AnalyzeExistingTrace();
	void CancelAnalysis();
	void OpenLastReport();
	void LaunchProfileSession();
	void CopyProfileLaunchArguments();
	void SetCaptureProfile(EPerfSentinelCaptureProfile Profile);
	void OpenReportsFolder();
	void OpenPerfSentinelSettings();

	bool CanStartTraceCapture() const;
	bool CanStopTraceCapture() const;
	bool CanAnalyzeLastTrace() const;
	bool CanCancelAnalysis() const;

	FPerfSentinelTraceController* GetController() const;
	void RunAnalysisForSession() const;
	void RunAnalysisForTrace(const FString& TracePath) const;

	TSharedPtr<FUICommandList> CommandList;
};

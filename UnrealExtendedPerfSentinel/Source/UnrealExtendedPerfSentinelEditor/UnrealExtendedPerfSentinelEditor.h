// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FPerfSentinelEditorMenu;
struct FPerfSentinelProcessResult;
class SDockTab;
class FSpawnTabArgs;

class FUnrealExtendedPerfSentinelEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> SpawnReportTab(const FSpawnTabArgs& Args);
	void HandleAnalysisCompleted(bool bSucceeded, const FPerfSentinelProcessResult& Result, const FString& Error);
	/** Switches the editor's "Use Less CPU when in Background" off for a capture and back afterwards (in memory only). */
	void HandleCaptureActiveChanged(bool bCapturing);
	/** The editor is exiting: puts the setting back while UObjects exist and before the editor saves its settings. */
	void HandleEditorPreExit();

	TUniquePtr<FPerfSentinelEditorMenu> EditorMenu;
	FDelegateHandle AnalysisCompletedHandle;
	FDelegateHandle CaptureActiveChangedHandle;
	FDelegateHandle EditorPreExitHandle;
	bool bRestoreBackgroundThrottle = false;
};

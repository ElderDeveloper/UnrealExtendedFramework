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

	TUniquePtr<FPerfSentinelEditorMenu> EditorMenu;
	FDelegateHandle AnalysisCompletedHandle;
};

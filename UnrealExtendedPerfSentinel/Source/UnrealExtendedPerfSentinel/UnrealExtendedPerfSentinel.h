// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FPerfSentinelTraceController;
class FPerfSentinelConsoleCommands;
class FPerfSentinelAnalysisManager;

class UNREALEXTENDEDPERFSENTINEL_API FUnrealExtendedPerfSentinelModule : public IModuleInterface
{
public:
	virtual ~FUnrealExtendedPerfSentinelModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FPerfSentinelTraceController* GetTraceController();
	static TSharedPtr<FPerfSentinelAnalysisManager> GetAnalysisManager();

private:
	TUniquePtr<FPerfSentinelTraceController> TraceController;
	TUniquePtr<FPerfSentinelConsoleCommands> ConsoleCommands;
	TSharedPtr<FPerfSentinelAnalysisManager> AnalysisManager;

	static FUnrealExtendedPerfSentinelModule* Instance;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedPerfSentinel.h"

#include "PerfSentinelConsoleCommands.h"
#include "PerfSentinelAnalysisManager.h"
#include "PerfSentinelSettings.h"
#include "PerfSentinelTraceController.h"
#include "PerfSentinelTelemetry.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedPerfSentinelModule"

FUnrealExtendedPerfSentinelModule* FUnrealExtendedPerfSentinelModule::Instance = nullptr;

FUnrealExtendedPerfSentinelModule::~FUnrealExtendedPerfSentinelModule() = default;

void FUnrealExtendedPerfSentinelModule::StartupModule()
{
	Instance = this;
	UPerfSentinelSettings::ApplyCommandLineOverrides();
	FPerfSentinelTelemetry::Get().Initialize();
	TraceController = MakeUnique<FPerfSentinelTraceController>();
	AnalysisManager = MakeShared<FPerfSentinelAnalysisManager>();

#if !WITH_EDITOR
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings || !Settings->bEnablePackagedBuildConsoleCommands)
	{
		return;
	}
#endif

	ConsoleCommands = MakeUnique<FPerfSentinelConsoleCommands>();
}

void FUnrealExtendedPerfSentinelModule::ShutdownModule()
{
	ConsoleCommands.Reset();
	if (AnalysisManager)
	{
		AnalysisManager->Shutdown();
	}
	AnalysisManager.Reset();
	TraceController.Reset();
	FPerfSentinelTelemetry::Get().Shutdown();
	Instance = nullptr;
}

TSharedPtr<FPerfSentinelAnalysisManager> FUnrealExtendedPerfSentinelModule::GetAnalysisManager()
{
	return Instance ? Instance->AnalysisManager : nullptr;
}

FPerfSentinelTraceController* FUnrealExtendedPerfSentinelModule::GetTraceController()
{
	return Instance ? Instance->TraceController.Get() : nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedPerfSentinelModule, UnrealExtendedPerfSentinel)

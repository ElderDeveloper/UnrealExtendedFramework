// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IConsoleObject;

/** Console command registration for PerfSentinel capture and analysis workflows. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelConsoleCommands
{
public:
	FPerfSentinelConsoleCommands();
	~FPerfSentinelConsoleCommands();

private:
	void RegisterCommands();
	void UnregisterCommands();

	void StartCapture(const TArray<FString>& Args);
	void StopCapture(const TArray<FString>& Args);
	void AnalyzeLastTrace(const TArray<FString>& Args);
	void AnalyzeTrace(const TArray<FString>& Args);
	void CancelAnalysis(const TArray<FString>& Args);
	void AnalysisStatus(const TArray<FString>& Args);
	void OpenReportsFolder(const TArray<FString>& Args);
	void DumpSettings(const TArray<FString>& Args);
	void CaptureSpikeScreenshot(const TArray<FString>& Args);

	TArray<IConsoleObject*> RegisteredCommands;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerfSentinelTypes.h"

class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelCancellationToken
{
public:
	void Cancel() { bCancelled.Store(true); }
	bool IsCancelled() const { return bCancelled.Load(); }

private:
	TAtomic<bool> bCancelled{ false };
};

/** Process-level analyzer launcher. Normally invoked by FPerfSentinelAnalysisManager off the game thread. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelPythonRunner
{
public:
	bool BuildRequestForSession(const FPerfSentinelTraceSession& Session, FPerfSentinelAnalysisRequest& OutRequest, FString& OutError) const;
	bool BuildRequestForTrace(const FString& TracePath, FPerfSentinelAnalysisRequest& OutRequest, FString& OutError) const;
	bool RunAnalysis(
		const FPerfSentinelAnalysisRequest& Request,
		FPerfSentinelProcessResult& OutResult,
		FString& OutError,
		const FPerfSentinelCancellationToken* CancellationToken = nullptr) const;

private:
	bool BuildCommonRequest(const FString& TracePath, const FString& MetadataPath, const FString& FallbackStatsPath, const FString& SpikeEventsPath, FPerfSentinelAnalysisRequest& OutRequest, FString& OutError) const;
	bool ValidatePythonExecutable(const FString& PythonExecutable, FString& OutError) const;
	bool GenerateFallbackMetadata(FPerfSentinelAnalysisRequest& InOutRequest, FString& OutError) const;
	bool RunNativeExtraction(FPerfSentinelAnalysisRequest& InOutRequest, const FPerfSentinelCancellationToken* CancellationToken, FString& OutDiagnostic) const;
	bool TryFindUnrealInsightsExecutable(FString& OutInsightsPath) const;
	FString BuildAnalyzerArguments(const FPerfSentinelAnalysisRequest& Request, const FString& InsightsExecutable) const;
	FString BuildReportDirectory(const FString& TracePath) const;
};

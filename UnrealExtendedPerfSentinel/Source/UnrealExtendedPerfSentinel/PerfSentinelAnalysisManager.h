// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "PerfSentinelPythonRunner.h"
#include "PerfSentinelTypes.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPerfSentinelAnalysisCompleted, bool, const FPerfSentinelProcessResult&, const FString&);

/** Owns one asynchronous, cancellable analyzer job and marshals completion back to the game thread. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelAnalysisManager : public TSharedFromThis<FPerfSentinelAnalysisManager>
{
public:
	~FPerfSentinelAnalysisManager();

	bool StartAnalysis(const FPerfSentinelAnalysisRequest& Request, FString& OutError);
	void CancelAnalysis();
	void Shutdown();

	EPerfSentinelAnalysisState GetState() const { return State; }
	bool IsRunning() const { return State == EPerfSentinelAnalysisState::Running || State == EPerfSentinelAnalysisState::Cancelling; }
	const FPerfSentinelProcessResult& GetLastResult() const { return LastResult; }
	const FString& GetLastError() const { return LastError; }
	FOnPerfSentinelAnalysisCompleted& OnCompleted() { return CompletedEvent; }

private:
	void CompleteOnGameThread(bool bSucceeded, FPerfSentinelProcessResult Result, FString Error);

	EPerfSentinelAnalysisState State = EPerfSentinelAnalysisState::Idle;
	TSharedPtr<FPerfSentinelCancellationToken, ESPMode::ThreadSafe> CancellationToken;
	TFuture<void> WorkerFuture;
	FPerfSentinelProcessResult LastResult;
	FString LastError;
	FOnPerfSentinelAnalysisCompleted CompletedEvent;
};

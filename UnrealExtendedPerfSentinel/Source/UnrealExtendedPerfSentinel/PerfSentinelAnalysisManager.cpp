// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelAnalysisManager.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "PerfSentinelTraceController.h"

FPerfSentinelAnalysisManager::~FPerfSentinelAnalysisManager()
{
	Shutdown();
}

bool FPerfSentinelAnalysisManager::StartAnalysis(const FPerfSentinelAnalysisRequest& Request, FString& OutError)
{
	if (IsRunning())
	{
		OutError = TEXT("A PerfSentinel analysis job is already running.");
		return false;
	}

	State = EPerfSentinelAnalysisState::Running;
	LastResult = FPerfSentinelProcessResult();
	LastError.Reset();
	CancellationToken = MakeShared<FPerfSentinelCancellationToken, ESPMode::ThreadSafe>();

	const TWeakPtr<FPerfSentinelAnalysisManager> WeakSelf = AsShared();
	const TSharedPtr<FPerfSentinelCancellationToken, ESPMode::ThreadSafe> Token = CancellationToken;
	WorkerFuture = Async(EAsyncExecution::ThreadPool, [WeakSelf, Token, Request]()
	{
		FPerfSentinelPythonRunner Runner;
		FPerfSentinelProcessResult Result;
		FString Error;
		const bool bSucceeded = Runner.RunAnalysis(Request, Result, Error, Token.Get());
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, bSucceeded, Result = MoveTemp(Result), Error = MoveTemp(Error)]() mutable
		{
			if (const TSharedPtr<FPerfSentinelAnalysisManager> Pinned = WeakSelf.Pin())
			{
				Pinned->CompleteOnGameThread(bSucceeded, MoveTemp(Result), MoveTemp(Error));
			}
		});
	});

	UE_LOG(LogPerfSentinel, Log, TEXT("AnalysisManager: Started asynchronous analysis for %s"), *Request.InputTracePath);
	return true;
}

void FPerfSentinelAnalysisManager::CancelAnalysis()
{
	if (!IsRunning() || !CancellationToken)
	{
		return;
	}
	State = EPerfSentinelAnalysisState::Cancelling;
	CancellationToken->Cancel();
}

void FPerfSentinelAnalysisManager::Shutdown()
{
	CancelAnalysis();
	if (WorkerFuture.IsValid())
	{
		WorkerFuture.Wait();
	}
	CancellationToken.Reset();
}

void FPerfSentinelAnalysisManager::CompleteOnGameThread(bool bSucceeded, FPerfSentinelProcessResult Result, FString Error)
{
	LastResult = MoveTemp(Result);
	LastError = MoveTemp(Error);
	const bool bCancelled = CancellationToken && CancellationToken->IsCancelled();
	State = bSucceeded
		? EPerfSentinelAnalysisState::Succeeded
		: (bCancelled ? EPerfSentinelAnalysisState::Cancelled : EPerfSentinelAnalysisState::Failed);
	CancellationToken.Reset();
	if (bSucceeded)
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("AnalysisManager: Analysis succeeded with %d generated artifact(s)."), LastResult.GeneratedFiles.Num());
	}
	else if (bCancelled)
	{
		UE_LOG(LogPerfSentinel, Log, TEXT("AnalysisManager: Analysis cancelled."));
	}
	else
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AnalysisManager: Analysis failed: %s"), *LastError);
	}
	CompletedEvent.Broadcast(bSucceeded, LastResult, LastError);
}

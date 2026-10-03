// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "PerfSentinelAgentToolset.h"

class FJsonObject;

/** Bounded background workers for read-only report queries. Never waits for Python on the game thread. */
class FPerfSentinelAgentService
{
public:
	static void Initialize();
	static void Shutdown();
	static FPerfSentinelAgentResponse Submit(const FString& Operation, const TSharedRef<FJsonObject>& Parameters);
	static FPerfSentinelAgentResponse Status(const FString& RequestId);
	static FPerfSentinelAgentResponse Cancel(const FString& RequestId);
	static FPerfSentinelAgentResponse CaptureStatus();
	static FPerfSentinelAgentResponse AnalysisStatus();
	static FPerfSentinelAgentResponse ConfigureContext(const FString& RunId, const FString& ProcessRole, const FString& BuildId);
	static FPerfSentinelAgentResponse StartCapture(const FString& ScenarioName);
	static FPerfSentinelAgentResponse StopCapture();
	static FPerfSentinelAgentResponse AnalyzeLastCapture();
};

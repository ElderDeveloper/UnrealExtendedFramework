// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "PerfSentinelTypes.h"

class UPerfSentinelSettings;
class FPerfSentinelRuntimeMonitor;

UNREALEXTENDEDPERFSENTINEL_API DECLARE_LOG_CATEGORY_EXTERN(LogPerfSentinel, Log, All);

/** Shared trace capture backend used by future editor UI and console commands. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelTraceController
{
public:
	FPerfSentinelTraceController();
	~FPerfSentinelTraceController();

	EPerfSentinelCaptureState GetCaptureState() const { return CaptureState; }
	bool IsIdle() const { return CaptureState == EPerfSentinelCaptureState::Idle; }
	bool IsCapturing() const { return CaptureState == EPerfSentinelCaptureState::Capturing; }

	const FPerfSentinelTraceSession& GetCurrentSession() const { return CurrentSession; }
	const FPerfSentinelTraceSession& GetLastCompletedSession() const { return LastCompletedSession; }
	FPerfSentinelRuntimeMonitor* GetRuntimeMonitor() const { return RuntimeMonitor.Get(); }
	bool HasCompletedTrace() const;

	bool StartCapture(const FString& ScenarioName = TEXT("ManualCapture"));
	bool StopCapture();

private:
	bool HandleAutoStop(float DeltaTime);
	void CancelAutoStop();
	FString BuildTraceFilePath() const;
	TArray<FString> BuildSafeTraceChannels() const;
	FString BuildChannelsArg() const;
	FString ExtractSessionBaseName(const FString& TracePath) const;

	bool StartTraceFile(const FString& TracePath, const TArray<FString>& Channels) const;
	bool StopTraceFile() const;
	static bool ExecTraceCommand(const FString& Command);
	static bool WaitForTraceFile(const FString& TracePath);

	void WriteMetadataSidecar() const;

	EPerfSentinelCaptureState CaptureState = EPerfSentinelCaptureState::Idle;
	FPerfSentinelTraceSession CurrentSession;
	FPerfSentinelTraceSession LastCompletedSession;
	TUniquePtr<FPerfSentinelRuntimeMonitor> RuntimeMonitor;
	FTSTicker::FDelegateHandle AutoStopHandle;
};

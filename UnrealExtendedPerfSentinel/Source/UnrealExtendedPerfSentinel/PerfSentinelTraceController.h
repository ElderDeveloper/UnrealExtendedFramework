// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Misc/Optional.h"
#include "PerfSentinelTypes.h"

class UPerfSentinelSettings;
class FPerfSentinelRuntimeMonitor;

UNREALEXTENDEDPERFSENTINEL_API DECLARE_LOG_CATEGORY_EXTERN(LogPerfSentinel, Log, All);

/** Broadcast with true once a capture is recording and with false once it has stopped. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPerfSentinelCaptureActiveChanged, bool /*bCapturing*/);

/** Shared trace capture backend used by future editor UI and console commands. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelTraceController
{
public:
	/** The editor module uses this to switch the editor's background CPU throttle off while a capture runs. */
	FOnPerfSentinelCaptureActiveChanged& OnCaptureActiveChanged() { return CaptureActiveChanged; }

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

	bool StartTraceFile(const FString& TracePath, const TArray<FString>& Channels);
	bool StopTraceFile();
	void RestoreTraceConfiguration();
	static bool ExecTraceCommand(const FString& Command);
	static bool WaitForTraceFile(const FString& TracePath);

	void WriteMetadataSidecar() const;
	/** Switches t.IdleWhenNotForeground off for the capture and restores the value it had. */
	void SetKeepFullSpeedWhenUnfocused(bool bEnable);

	FOnPerfSentinelCaptureActiveChanged CaptureActiveChanged;
	TOptional<int32> PreviousIdleWhenNotForeground;
	EPerfSentinelCaptureState CaptureState = EPerfSentinelCaptureState::Idle;
	FPerfSentinelTraceSession CurrentSession;
	FPerfSentinelTraceSession LastCompletedSession;
	TUniquePtr<FPerfSentinelRuntimeMonitor> RuntimeMonitor;
	FTSTicker::FDelegateHandle AutoStopHandle;
	TArray<FString> NewlyEnabledChannels;
	uint32 PreviousNetTraceVerbosity = 0;
	uint32 AppliedNetTraceVerbosity = 0;
	bool bPreviousNetChannelEnabled = false;
	bool bChangedNetTraceVerbosity = false;
	bool bOwnsTrace = false;
};

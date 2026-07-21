// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerfSentinelTypes.h"

class FArchive;
class UPerfSentinelSettings;

/** Captures frame-time spike observations while a trace session is active. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelRuntimeMonitor
{
public:
	FPerfSentinelRuntimeMonitor();
	~FPerfSentinelRuntimeMonitor();

	void Activate(const FString& InSessionBaseName, const FString& InSessionOutputDirectory);
	void Deactivate();

	bool IsActive() const { return bActive; }
	const FString& GetSpikeEventsPath() const { return SpikeEventsPath; }
	const FString& GetFrameSamplesPath() const { return FrameSamplesPath; }
	const FString& GetRuntimeContextPath() const { return RuntimeContextPath; }
	const FString& GetRuntimeCountersPath() const { return RuntimeCountersPath; }
	const FString& GetGameStatsPath() const { return GameStatsPath; }
	const TArray<FPerfSentinelSpikeSnapshot>& GetSpikeSnapshots() const { return SpikeSnapshots; }
	int32 GetSpikeCount() const { return SpikeSnapshots.Num(); }
	int32 GetSuppressedSpikeCount() const { return SuppressedSpikeCount; }
	float GetWorstSuppressedFrameMs() const { return WorstSuppressedFrameMs; }

	bool CaptureManualSpike();

	/** Platform-time (FPlatformTime::Seconds) captured when the session was activated; used as the offline timebase anchor. */
	double GetCaptureStartPlatformSeconds() const { return CaptureStartPlatformSeconds; }

private:
	struct FFrameSample
	{
		FDateTime Timestamp = FDateTime::MinValue();
		double PlatformSeconds = 0.0;
		double AppSeconds = 0.0;
		int64 FrameNumber = 0;
		float FrameTimeMs = 0.0f;
		float GameThreadMs = -1.0f;
		float RenderThreadMs = -1.0f;
		float RhiThreadMs = -1.0f;
		float GpuMs = -1.0f;
	};

	/** Per-class CPU cost accumulated across spike frames from the engine stats system. */
	struct FAggregatedScope
	{
		double TotalInclusiveMs = 0.0;
		double MaxInclusiveMs = 0.0;
		double TotalExclusiveMs = 0.0;
		int64 CallCount = 0;
		int32 SpikeCount = 0;
	};

	struct FPendingSpikeWindow
	{
		int64 SpikeFrameNumber = 0;
		double StartSeconds = 0.0;
		double EndSeconds = 0.0;
		FString Path;
	};

	void HandleEndFrame();
	void RecordFrameSample(const UPerfSentinelSettings& Settings);
	static void FillThreadTimings(FFrameSample& Sample);
	void AppendFrameSample(const FFrameSample& Sample) const;
	void AppendRuntimeCounters(const FFrameSample& Sample);
	void AddPerClassBreakdownFields(const UWorld* World, const TSharedRef<class FJsonObject>& Root) const;
	bool CaptureSpike(float FrameTimeMs, bool bManual);
	void HarvestSpikeStats();
	bool RequestSpikeScreenshot(const FString& TargetPath, bool bAsync);
	void HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);
	void HandlePostGarbageCollect();
	bool AppendSpikeEvent(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs, const FString& SpikeDirectory) const;
	void QueueSpikeWindow(const FFrameSample& SpikeSample, const UPerfSentinelSettings& Settings, const FString& SpikeDirectory);
	void FlushSpikeWindows(bool bForce);
	void WriteSpikeWindow(const FPendingSpikeWindow& Window) const;
	void PruneFrameSampleBuffer(double CurrentPlatformSeconds, float RetentionSeconds);
	void WriteRuntimeContext() const;
	void WriteGameStats() const;
	void WriteSpikeEventFile(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs, const FString& SpikeDirectory) const;
	TSharedRef<FJsonObject> BuildFrameSampleObject(const FFrameSample& Sample) const;
	TSharedRef<FJsonObject> BuildSpikeEventObject(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs) const;
	static void WriteJsonLine(FArchive* Writer, const TSharedRef<FJsonObject>& Object);
	FString BuildSpikeDirectory(int32 SpikeIndex, int64 SpikeFrameNumber) const;
	FString BuildScreenshotPath(const FString& SpikeDirectory) const;
	FString BuildSpikeWindowPath(const FString& SpikeDirectory) const;
	FString MakeProjectSavedRelativePath(const FString& AbsolutePath) const;

	FDelegateHandle EndFrameHandle;
	bool bActive = false;
	FString SessionBaseName;
	FString SpikeOutputDirectory;
	FString SpikesDirectory;
	FString SpikeEventsPath;
	FString FrameSamplesPath;
	FString RuntimeContextPath;
	FString RuntimeCountersPath;
	FString GameStatsPath;
	TArray<FPerfSentinelSpikeSnapshot> SpikeSnapshots;
	TArray<FFrameSample> FrameSampleBuffer;
	TArray<FPendingSpikeWindow> PendingSpikeWindows;
	TUniquePtr<FArchive> FrameSamplesWriter;
	TUniquePtr<FArchive> RuntimeCountersWriter;
	double LastScreenshotTimeSeconds = -TNumericLimits<double>::Max();
	double LastSpikeEventTimeSeconds = -TNumericLimits<double>::Max();
	int32 SuppressedSpikeCount = 0;
	float WorstSuppressedFrameMs = 0.0f;
	double LastRuntimeCounterTimeSeconds = -TNumericLimits<double>::Max();
	double LastLeakSnapshotTimeSeconds = -TNumericLimits<double>::Max();
	double CaptureStartPlatformSeconds = 0.0;

	/** Aggregated per-class CPU cost harvested across spike frames (key = scope short name). */
	TMap<FString, FAggregatedScope> AggregatedScopes;
	bool bStatHarvestEnabled = false;

	/** FIFO of screenshot target paths awaiting the next OnScreenshotCaptured broadcast. */
	TArray<FString> PendingScreenshotPaths;
	FDelegateHandle ScreenshotCapturedHandle;

	/** Garbage-collection tracking for leak analysis. */
	FDelegateHandle PostGarbageCollectHandle;
	int32 GarbageCollectionCount = 0;
	double LastGarbageCollectPlatformSeconds = -1.0;
};

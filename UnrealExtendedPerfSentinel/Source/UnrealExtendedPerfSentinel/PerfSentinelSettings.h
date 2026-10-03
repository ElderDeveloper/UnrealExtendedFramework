// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PerfSentinelTypes.h"
#include "UObject/NoExportTypes.h"
#include "PerfSentinelSettings.generated.h"

/** Project settings shared by PerfSentinel runtime, console commands, and editor UI. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Extended Perf Sentinel"))
class UNREALEXTENDEDPERFSENTINEL_API UPerfSentinelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPerfSentinelSettings();

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static const UPerfSentinelSettings* Get();

	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	FString TraceOutputDirectory;

	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	FString TraceFilePrefix;

	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	TArray<FString> TraceChannels;

	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	EPerfSentinelCaptureProfile CaptureProfile = EPerfSentinelCaptureProfile::Standard;

	/** Shared caller-supplied identity for related server/client captures. Empty creates an independent run. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture | Identity")
	FString SharedRunId;

	/** Optional explicit process role; empty infers the role from captured gameplay worlds. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture | Identity")
	FString ProcessRole;

	/** Build/commit identity supplied by the launcher or CI. Empty means unknown, never the current checkout's guessed revision. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture | Identity")
	FString BuildId;

	/** Observed adjustment to the local clock in milliseconds; only meaningful with a known uncertainty. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture | Identity")
	double ClockOffsetMilliseconds = 0.0;

	/** Negative means clocks have not been synchronized. No cross-process ordering precision is implied. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture | Identity", meta = (ClampMin = "-1"))
	double ClockUncertaintyMilliseconds = -1.0;

	/** Keep bounded telemetry history without opening a trace recorder. */
	UPROPERTY(EditAnywhere, Config, Category = "Telemetry")
	bool bEnableTelemetryHistory = true;

	UPROPERTY(EditAnywhere, Config, Category = "Telemetry", meta = (ClampMin = "1", ClampMax = "16384"))
	int32 TelemetryHistoryCapacity = 2048;

	UPROPERTY(EditAnywhere, Config, Category = "Telemetry", meta = (ClampMin = "0", ClampMax = "300"))
	float TelemetryHistorySeconds = 30.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Collectors", meta = (ClampMin = "0.1", ClampMax = "60"))
	float CollectorIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Collectors", meta = (ClampMin = "1", ClampMax = "300"))
	float DetailedCollectorIntervalSeconds = 5.0f;

	/** Detailed inventories are opt-in; counts are never interpreted as execution cost. */
	UPROPERTY(EditAnywhere, Config, Category = "Collectors")
	bool bEnableDetailedCollectors = false;

	/** Stop scheduling more collectors after this observed budget. A callback cannot be preempted. */
	UPROPERTY(EditAnywhere, Config, Category = "Collectors", meta = (ClampMin = "0.1", ClampMax = "50"))
	float MaxCollectorTimeMilliseconds = 2.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Collectors", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 MaxCollectorRows = 256;

	/** Apply PerfSentinel-specific launcher overrides in memory, without changing project config. */
	static void ApplyCommandLineOverrides();

	/** Automatically stop captures after DefaultCaptureDurationSeconds. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	bool bAutoStopCapture = true;

	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = "1", UIMin = "1"))
	int32 DefaultCaptureDurationSeconds = 60;

	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	bool bEnablePackagedBuildConsoleCommands = true;

	UPROPERTY(EditAnywhere, Config, Category = "Stats")
	bool bVerifyStatUnitAndGpuTraceCoverage = true;

	UPROPERTY(EditAnywhere, Config, Category = "Stats")
	bool bCollectStatUnitFallback = true;

	UPROPERTY(EditAnywhere, Config, Category = "Stats")
	bool bCollectStatGpuFallback = true;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bCollectFrameSamples = true;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bCollectRuntimeCounters = true;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RuntimeCounterIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bWriteSpikeWindows = true;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bWriteGameStats = true;

	/** Lightweight gameplay transport counters are independent of full object inventories and trace verbosity. */
	UPROPERTY(EditAnywhere, Config, Category = "Networking")
	bool bCollectNetworkSamples = true;

	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float NetworkSampleIntervalSeconds = 1.0f;

	/** Adds native packet / actor / property / RPC trace data. Unsupported builds remain explicit in metadata. */
	UPROPERTY(EditAnywhere, Config, Category = "Networking")
	bool bEnableNetworkTrace = true;

	/** 1 = packet content, 2 = verbose, 3 = very verbose (higher capture overhead). */
	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "1", ClampMax = "3", UIMin = "1", UIMax = "3"))
	int32 NetworkTraceVerbosity = 1;

	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxNetworkWorlds = 16;

	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxNetworkDrivers = 16;

	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "1", ClampMax = "2048"))
	int32 MaxNetworkConnections = 256;

	UPROPERTY(EditAnywhere, Config, Category = "Networking", meta = (ClampMin = "1", ClampMax = "16384"))
	int32 MaxNetworkChannelsPerConnection = 4096;

	/** Optional network budgets. Zero disables a limit until the project establishes a measured baseline. */
	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0"))
	float MaxOutgoingKiBPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0"))
	float MaxIncomingKiBPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0"))
	float MaxNetworkRttMs = 0.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0", ClampMax = "100"))
	float MaxNetworkLossPercent = 0.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0"))
	int32 MaxReliableBacklog = 0;

	UPROPERTY(EditAnywhere, Config, Category = "Networking | Budgets", meta = (ClampMin = "0"))
	float MaxNetworkRegressionPercent = 0.0f;

	/** Full actor/widget inventory is intrusive; keep it opt-in for targeted investigations. */
	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bWriteFullObjectInventory = false;

	/** Capture top-N actor/component/UObject class breakdowns into runtime counters (slower cadence). */
	UPROPERTY(EditAnywhere, Config, Category = "Raw Data")
	bool bCollectPerClassBreakdown = true;

	/** Number of classes to include in per-class breakdowns, ranked by instance count. */
	UPROPERTY(EditAnywhere, Config, Category = "Raw Data", meta = (ClampMin = "1", UIMin = "1"))
	int32 PerClassTopN = 25;

	/** Interval between heavy leak-detection snapshots (per-class breakdowns + streaming contents). */
	UPROPERTY(EditAnywhere, Config, Category = "Raw Data", meta = (ClampMin = "0.5", UIMin = "0.5"))
	float LeakSnapshotIntervalSeconds = 5.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpikeWindowPreSeconds = 5.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Raw Data", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpikeWindowPostSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots")
	bool bCaptureScreenshotOnSpike = true;

	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ScreenshotSpikeThresholdMs = 66.66f;

	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ScreenshotCooldownSeconds = 5.0f;

	/** Minimum separation between persisted spike events. Sustained bad frames are summarized as one cluster. */
	UPROPERTY(EditAnywhere, Config, Category = "Spike Detection", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpikeEventCooldownSeconds = 1.0f;

	/** Hard artifact limit for pathological captures. Additional frames are counted as suppressed evidence. */
	UPROPERTY(EditAnywhere, Config, Category = "Spike Detection", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSpikeEvents = 100;

	/** Compress and write spike screenshots on a background thread instead of the game thread (reduces hitching). */
	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots")
	bool bAsyncScreenshotCompression = true;

	/** Downscale spike screenshots so the longest edge is at most this many pixels (0 disables downscaling). */
	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots", meta = (ClampMin = "0", UIMin = "0"))
	int32 ScreenshotMaxDimension = 1280;

	/** Maximum number of spike screenshots that may be queued for async compression at once. */
	UPROPERTY(EditAnywhere, Config, Category = "Spike Screenshots", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxInFlightScreenshots = 4;

	/** Harvest per-class CPU cost from the engine stats system on spike frames (requires a stats-enabled build). */
	UPROPERTY(EditAnywhere, Config, Category = "Cost Attribution")
	bool bHarvestRuntimeStats = true;

	/** Number of expensive named scopes to harvest from the stats system per spike frame. */
	UPROPERTY(EditAnywhere, Config, Category = "Cost Attribution", meta = (ClampMin = "1", UIMin = "1"))
	int32 RuntimeStatTopN = 40;

	UPROPERTY(EditAnywhere, Config, Category = "Analysis")
	FFilePath PythonExecutable;

	UPROPERTY(EditAnywhere, Config, Category = "Analysis")
	FFilePath AnalyzerScript;

	UPROPERTY(EditAnywhere, Config, Category = "Analysis")
	FString ReportOutputDirectory;

	UPROPERTY(EditAnywhere, Config, Category = "Analysis", meta = (ClampMin = "30", UIMin = "30"))
	int32 AnalysisTimeoutSeconds = 600;

	UPROPERTY(EditAnywhere, Config, Category = "Analysis")
	bool bEnableNativeTraceExtraction = true;

	UPROPERTY(EditAnywhere, Config, Category = "Regression")
	FString BaselineDirectory;

	UPROPERTY(EditAnywhere, Config, Category = "Regression")
	bool bCompareAgainstBaseline = true;

	UPROPERTY(EditAnywhere, Config, Category = "Regression")
	bool bUpdateBaselineAfterSuccessfulAnalysis = false;

	UPROPERTY(EditAnywhere, Config, Category = "Regression | CI")
	bool bCiMode = false;

	UPROPERTY(EditAnywhere, Config, Category = "Regression | CI", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxP99RegressionPercent = 15.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Regression | CI", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxHitchesPerMinute = 3.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Regression | CI", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMemoryGrowthMB = 200.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Budgets")
	EPerfSentinelBudgetProfile BudgetProfile = EPerfSentinelBudgetProfile::Packaged30;

	UPROPERTY(EditAnywhere, Config, Category = "Budgets", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FrameBudgetMs = 33.33f;

	UPROPERTY(EditAnywhere, Config, Category = "Budgets", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HitchThresholdMs = 50.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Budgets", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SevereFrameBudgetMs = 66.66f;

	void ApplyBudgetProfile();
	void ApplyCaptureProfile();
	TArray<FString> GetChannelsForCaptureProfile() const;
	TArray<FString> GetRequiredLaunchArguments() const;
	bool CaptureProfileRequiresRelaunch() const;
	bool AreRequiredLaunchArgumentsPresent() const;

	FString GetResolvedTraceOutputDirectory() const;
	FString GetResolvedReportOutputDirectory() const;
	FString GetResolvedBaselineDirectory() const;
	FString GetResolvedAnalyzerScriptPath() const;
	FString GetResolvedPythonExecutable() const;
};

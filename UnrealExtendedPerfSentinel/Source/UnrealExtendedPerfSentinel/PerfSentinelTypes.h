// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerfSentinelTypes.generated.h"

/** Current state of the PerfSentinel capture and analysis pipeline. */
UENUM(BlueprintType)
enum class EPerfSentinelCaptureState : uint8
{
	Idle,
	Capturing,
	Stopping,
	Analyzing,
	Cancelling
};

UENUM(BlueprintType)
enum class EPerfSentinelAnalysisState : uint8
{
	Idle,
	Running,
	Cancelling,
	Cancelled,
	Succeeded,
	Failed
};

/** Channel-oriented capture profiles. Expensive or launch-only channels are never silently enabled. */
UENUM(BlueprintType)
enum class EPerfSentinelCaptureProfile : uint8
{
	Custom,
	Standard UMETA(DisplayName = "Standard Performance"),
	HitchDiagnosis UMETA(DisplayName = "Hitch Diagnosis"),
	LoadingStreaming UMETA(DisplayName = "Loading / Streaming"),
	MemoryLeak UMETA(DisplayName = "Memory / Leak (Relaunch Required)"),
	Multiplayer UMETA(DisplayName = "Multiplayer (Relaunch Required)"),
	UIAnimation UMETA(DisplayName = "UI / Animation")
};

/** Predefined performance budget profiles for common capture scenarios. */
UENUM(BlueprintType)
enum class EPerfSentinelBudgetProfile : uint8
{
	Custom,
	Editor60 UMETA(DisplayName = "Editor / PIE 60fps"),
	Packaged30 UMETA(DisplayName = "Packaged 30fps"),
	Packaged60 UMETA(DisplayName = "Packaged 60fps"),
	LowEnd UMETA(DisplayName = "Low-End Hardware")
};

/** Describes a single trace capture session. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelTraceSession
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString ScenarioName;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FDateTime StartedAt = FDateTime::MinValue();

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FDateTime StoppedAt = FDateTime::MinValue();

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString TracePath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString MetadataPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString ReportDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	TArray<FString> Channels;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	EPerfSentinelCaptureProfile CaptureProfile = EPerfSentinelCaptureProfile::Standard;

	/** Launch flags required for channels that cannot safely be enabled late. */
	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	TArray<FString> RequiredLaunchArguments;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	bool bLaunchRequirementsSatisfied = true;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString FallbackStatsPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString SpikeEventsPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString FrameSamplesPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString RuntimeContextPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString RuntimeCountersPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString GameStatsPath;

	bool IsCompleted() const
	{
		return StoppedAt != FDateTime::MinValue();
	}
};

/** Input contract for running the external analyzer. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelAnalysisRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString InputTracePath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString InputMetadataPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString OutputReportDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString AnalyzerScriptPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString PythonExecutablePath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString FallbackStatsPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString SpikeEventsPath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString NativeEvidencePath;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString BaselineDirectory;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString BaselineKey;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	bool bUpdateBaseline = false;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	bool bCiMode = false;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float MaxP99RegressionPercent = 15.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float MaxHitchesPerMinute = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float MaxMemoryGrowthMB = 200.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	int32 AnalysisTimeoutSeconds = 600;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float FrameBudgetMs = 33.33f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float HitchThresholdMs = 50.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float SevereFrameBudgetMs = 66.66f;
};

/** Result from an external process, usually the Python analyzer. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelProcessResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	int32 ExitCode = -1;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString StdOut;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString StdErr;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	TArray<FString> GeneratedFiles;

	bool Succeeded() const
	{
		return ExitCode == 0;
	}
};

/** A frame spike observation captured while trace recording is active. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelSpikeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FDateTime Timestamp = FDateTime::MinValue();

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	int64 FrameNumber = 0;

	/** FPlatformTime::Seconds() at the spike; paired with capture_start_platform_seconds to anchor trace time. */
	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	double PlatformSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float FrameTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float GameThreadTimeMs = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float RenderThreadTimeMs = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float RhiThreadTimeMs = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	float GpuTimeMs = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PerfSentinel")
	FString ScreenshotPath;
};

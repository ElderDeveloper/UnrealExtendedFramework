// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PerfSentinelTelemetry.generated.h"

class FJsonObject;

UENUM(BlueprintType)
enum class EPerfSentinelGameplayStage : uint8
{
	Request,
	ServerAccepted,
	Replicated,
	ClientApplied,
	Failed,
	Cancelled
};

/** Explicit instrumentation; preserve the same occurrence ID in the game's request and replication payloads. */
UCLASS()
class UNREALEXTENDEDPERFSENTINEL_API UPerfSentinelTelemetryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "PerfSentinel | Telemetry")
	static FGuid NewOccurrenceId();

	/** Labels must describe operation types, never account IDs, URLs, credentials, or request bodies. */
	UFUNCTION(BlueprintCallable, Category = "PerfSentinel | Telemetry")
	static bool ReportGameplayStage(FGuid OccurrenceId, FName Operation, EPerfSentinelGameplayStage Stage, FName Result);

	/** Payload bytes are application bytes. Wire bytes remain unknown unless the integration measures them explicitly. */
	UFUNCTION(BlueprintCallable, Category = "PerfSentinel | Telemetry")
	static bool ReportTransportOperation(FGuid OccurrenceId, FName Transport, FName Operation, int64 SentPayloadBytes, int64 ReceivedPayloadBytes,
		float DurationMilliseconds, int32 RetryCount, FName Result, int64 MeasuredWireBytes = -1);

	/** Offset maps this process UTC clock to the shared run clock. Unknown uncertainty is -1; this does not synchronize clocks. */
	UFUNCTION(BlueprintCallable, Category = "PerfSentinel | Telemetry")
	static bool ReportClockAlignment(FGuid OccurrenceId, float OffsetMilliseconds, float UncertaintyMilliseconds, FName Source);

	/** Emits an explicitly measured metric. Aggregation is gauge, counter, or duration; counts never imply CPU/GPU costs. */
	UFUNCTION(BlueprintCallable, Category = "PerfSentinel | Telemetry")
	static bool ReportMetric(FName MetricId, FName Domain, double Value, FName Unit, FName Aggregation, UObject* Owner = nullptr);
};

/** Bounded process-local history independent of Trace writer ownership. Native reporting is thread safe. */
class UNREALEXTENDEDPERFSENTINEL_API FPerfSentinelTelemetry
{
public:
	static FPerfSentinelTelemetry& Get();
	void Initialize();
	void Shutdown();
	/** Changes only future idle observations. Existing rows keep the context observed when they were recorded. */
	bool ConfigureDefaultContext(const FString& RunId, const FString& ProcessRole,
		double ClockOffsetMilliseconds = 0.0, double ClockUncertaintyMilliseconds = -1.0);
	void SetCaptureContext(const FString& SessionId, const FString& RunId, const FString& ProcessRole,
		double ClockOffsetMilliseconds = 0.0, double ClockUncertaintyMilliseconds = -1.0);
	void ClearCaptureContext();
	bool Record(const TSharedRef<FJsonObject>& Event);
	/** Copies retained rows newer than Cursor. A cursor of zero retrieves bounded pre-capture history. */
	void ReadSince(uint64& Cursor, int32 MaxRows, TArray<TSharedPtr<FJsonObject>>& OutRows, uint64& OutDroppedRows);
	TSharedRef<FJsonObject> GetCoverage() const;

private:
	struct FHistoryRow
	{
		uint64 Sequence = 0;
		double PlatformSeconds = 0.0;
		int32 Bytes = 0;
		TSharedPtr<FJsonObject> Event;
	};
	void Prune(double NowSeconds);
	mutable FCriticalSection Mutex;
	TArray<FHistoryRow> History;
	uint64 NextSequence = 0;
	uint64 DroppedRows = 0;
	int64 RetainedBytes = 0;
	int32 Capacity = 2048;
	double RetentionSeconds = 30.0;
	bool bEnabled = true;
	FString Session;
	FString Run;
	FString Role;
	FString DefaultRun;
	FString DefaultRole;
	double ClockOffsetMs = 0.0;
	double ClockUncertaintyMs = -1.0;
	double DefaultClockOffsetMs = 0.0;
	double DefaultClockUncertaintyMs = -1.0;
};

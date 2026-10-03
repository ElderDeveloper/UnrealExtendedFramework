// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "PerfSentinelTelemetry.h"

#include "PerfSentinelSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"

namespace
{
const TCHAR* StageName(EPerfSentinelGameplayStage Stage)
{
	switch (Stage)
	{
	case EPerfSentinelGameplayStage::Request: return TEXT("request");
	case EPerfSentinelGameplayStage::ServerAccepted: return TEXT("server_accepted");
	case EPerfSentinelGameplayStage::Replicated: return TEXT("replicated");
	case EPerfSentinelGameplayStage::ClientApplied: return TEXT("client_applied");
	case EPerfSentinelGameplayStage::Failed: return TEXT("failed");
	case EPerfSentinelGameplayStage::Cancelled: return TEXT("cancelled");
	default: return TEXT("unknown");
	}
}

TSharedRef<FJsonObject> MakeOperation(FGuid OccurrenceId, FName Operation, const TCHAR* Type)
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), Type);
	Event->SetStringField(TEXT("occurrence_id"), OccurrenceId.ToString(EGuidFormats::DigitsWithHyphens));
	Event->SetStringField(TEXT("operation"), Operation.ToString().Left(128));
	return Event;
}
}

FGuid UPerfSentinelTelemetryLibrary::NewOccurrenceId() { return FGuid::NewGuid(); }

bool UPerfSentinelTelemetryLibrary::ReportGameplayStage(FGuid OccurrenceId, FName Operation, EPerfSentinelGameplayStage Stage, FName Result)
{
	if (!OccurrenceId.IsValid() || Operation.IsNone()) { return false; }
	TSharedRef<FJsonObject> Event = MakeOperation(OccurrenceId, Operation, TEXT("gameplay_operation"));
	Event->SetStringField(TEXT("stage"), StageName(Stage));
	Event->SetStringField(TEXT("result"), Result.ToString().Left(128));
	return FPerfSentinelTelemetry::Get().Record(Event);
}

bool UPerfSentinelTelemetryLibrary::ReportTransportOperation(FGuid OccurrenceId, FName Transport, FName Operation,
	int64 SentPayloadBytes, int64 ReceivedPayloadBytes, float DurationMilliseconds, int32 RetryCount, FName Result, int64 MeasuredWireBytes)
{
	if (!OccurrenceId.IsValid() || Transport.IsNone() || Operation.IsNone() || !FMath::IsFinite(DurationMilliseconds) || DurationMilliseconds < 0.0f || RetryCount < 0) { return false; }
	TSharedRef<FJsonObject> Event = MakeOperation(OccurrenceId, Operation, TEXT("transport_operation"));
	Event->SetStringField(TEXT("transport"), Transport.ToString().Left(128));
	Event->SetStringField(TEXT("bytes_semantics"), TEXT("application_payload_excludes_protocol_headers_encryption_retransmits"));
	if (SentPayloadBytes >= 0) { Event->SetNumberField(TEXT("sent_payload_bytes"), static_cast<double>(SentPayloadBytes)); }
	if (ReceivedPayloadBytes >= 0) { Event->SetNumberField(TEXT("received_payload_bytes"), static_cast<double>(ReceivedPayloadBytes)); }
	if (MeasuredWireBytes >= 0) { Event->SetNumberField(TEXT("measured_wire_bytes"), static_cast<double>(MeasuredWireBytes)); }
	Event->SetBoolField(TEXT("wire_bytes_measured"), MeasuredWireBytes >= 0);
	Event->SetNumberField(TEXT("duration_ms"), DurationMilliseconds);
	Event->SetNumberField(TEXT("retry_count"), RetryCount);
	Event->SetStringField(TEXT("result"), Result.ToString().Left(128));
	return FPerfSentinelTelemetry::Get().Record(Event);
}

bool UPerfSentinelTelemetryLibrary::ReportClockAlignment(FGuid OccurrenceId, float OffsetMilliseconds, float UncertaintyMilliseconds, FName Source)
{
	if (!OccurrenceId.IsValid() || !FMath::IsFinite(OffsetMilliseconds) || !FMath::IsFinite(UncertaintyMilliseconds)) { return false; }
	TSharedRef<FJsonObject> Event = MakeOperation(OccurrenceId, Source, TEXT("clock_alignment"));
	Event->SetNumberField(TEXT("clock_offset_ms"), OffsetMilliseconds);
	if (UncertaintyMilliseconds >= 0.0f) { Event->SetNumberField(TEXT("clock_uncertainty_ms"), UncertaintyMilliseconds); }
	else { Event->SetField(TEXT("clock_uncertainty_ms"), MakeShared<FJsonValueNull>()); }
	Event->SetStringField(TEXT("alignment_source"), Source.ToString().Left(128));
	return FPerfSentinelTelemetry::Get().Record(Event);
}

bool UPerfSentinelTelemetryLibrary::ReportMetric(FName MetricId, FName Domain, double Value, FName Unit, FName Aggregation, UObject* Owner)
{
	if (MetricId.IsNone() || Domain.IsNone() || Unit.IsNone() || !FMath::IsFinite(Value)) { return false; }
	if (Aggregation != FName(TEXT("gauge")) && Aggregation != FName(TEXT("counter")) && Aggregation != FName(TEXT("duration"))) { return false; }
	if (Aggregation == FName(TEXT("duration")) && Value < 0.0) { return false; }
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), TEXT("metric_measurement"));
	TSharedRef<FJsonObject> Metric = MakeShared<FJsonObject>();
	Metric->SetStringField(TEXT("metric_id"), MetricId.ToString().Left(128));
	Metric->SetStringField(TEXT("domain"), Domain.ToString().Left(64));
	Metric->SetNumberField(TEXT("value"), Value);
	Metric->SetStringField(TEXT("unit"), Unit.ToString().Left(32));
	Metric->SetStringField(TEXT("aggregation"), Aggregation.ToString().ToLower());
	Metric->SetStringField(TEXT("status"), TEXT("observed"));
	Metric->SetStringField(TEXT("scope"), TEXT("process"));
	Metric->SetStringField(TEXT("source"), TEXT("explicit_instrumented_runtime_metric"));
	Metric->SetStringField(TEXT("owner_attribution_status"), TEXT("unspecified"));
	if (Owner)
	{
		// Native callers may report from workers. Even IsValid and class/path lookups must wait for the game thread.
		if (!IsInGameThread()) { Metric->SetStringField(TEXT("owner_attribution_status"), TEXT("omitted_off_game_thread")); }
		else if (IsValid(Owner))
		{
			Metric->SetStringField(TEXT("scope"), Owner->IsAsset() ? TEXT("asset") : TEXT("instance"));
			Metric->SetStringField(TEXT("object_path"), Owner->GetPathName().Left(1024));
			Metric->SetStringField(TEXT("class_path"), Owner->GetClass()->GetPathName().Left(1024));
			Metric->SetStringField(TEXT("owner_attribution_status"), TEXT("observed_live_object"));
			if (Owner->IsAsset()) { Metric->SetStringField(TEXT("asset_path"), Owner->GetPathName().Left(1024)); }
#if WITH_EDITORONLY_DATA
			else if (IsValid(Owner->GetClass()->ClassGeneratedBy))
			{
				Metric->SetStringField(TEXT("class_asset_path"), Owner->GetClass()->ClassGeneratedBy->GetPathName().Left(1024));
			}
#endif
		}
		else { Metric->SetStringField(TEXT("owner_attribution_status"), TEXT("invalid_live_object")); }
	}
	TArray<TSharedPtr<FJsonValue>> Metrics;
	Metrics.Add(MakeShared<FJsonValueObject>(Metric));
	Event->SetArrayField(TEXT("metrics"), Metrics);
	return FPerfSentinelTelemetry::Get().Record(Event);
}

FPerfSentinelTelemetry& FPerfSentinelTelemetry::Get()
{
	static FPerfSentinelTelemetry Instance;
	return Instance;
}

void FPerfSentinelTelemetry::Initialize()
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings) { return; }
	FScopeLock Lock(&Mutex);
	bEnabled = Settings->bEnableTelemetryHistory;
	Capacity = FMath::Clamp(Settings->TelemetryHistoryCapacity, 1, 16384);
	RetentionSeconds = FMath::Clamp(static_cast<double>(Settings->TelemetryHistorySeconds), 0.0, 300.0);
	DefaultRun = Settings->SharedRunId.Left(128);
	DefaultRole = Settings->ProcessRole.Left(64);
	ClockOffsetMs = FMath::IsFinite(Settings->ClockOffsetMilliseconds) ? Settings->ClockOffsetMilliseconds : 0.0;
	ClockUncertaintyMs = FMath::IsFinite(Settings->ClockUncertaintyMilliseconds) ? Settings->ClockUncertaintyMilliseconds : -1.0;
	DefaultClockOffsetMs = ClockOffsetMs;
	DefaultClockUncertaintyMs = ClockUncertaintyMs;
	Prune(FPlatformTime::Seconds());
}

void FPerfSentinelTelemetry::Shutdown()
{
	FScopeLock Lock(&Mutex);
	bEnabled = false;
	History.Reset();
	RetainedBytes = 0;
	Session.Reset(); Run.Reset(); Role.Reset();
}

void FPerfSentinelTelemetry::SetCaptureContext(const FString& SessionId, const FString& RunId, const FString& ProcessRole, double OffsetMs, double UncertaintyMs)
{
	FScopeLock Lock(&Mutex);
	Session = SessionId.Left(128);
	Run = RunId.Left(128);
	Role = ProcessRole.Left(64);
	ClockOffsetMs = FMath::IsFinite(OffsetMs) ? OffsetMs : 0.0;
	ClockUncertaintyMs = FMath::IsFinite(UncertaintyMs) ? UncertaintyMs : -1.0;
}

bool FPerfSentinelTelemetry::ConfigureDefaultContext(const FString& RunId, const FString& ProcessRole, double OffsetMs, double UncertaintyMs)
{
	FScopeLock Lock(&Mutex);
	if (!Session.IsEmpty() || !FMath::IsFinite(OffsetMs) || !FMath::IsFinite(UncertaintyMs)) { return false; }
	DefaultRun = RunId.Left(128);
	DefaultRole = ProcessRole.Left(64);
	DefaultClockOffsetMs = OffsetMs;
	DefaultClockUncertaintyMs = UncertaintyMs;
	ClockOffsetMs = DefaultClockOffsetMs;
	ClockUncertaintyMs = DefaultClockUncertaintyMs;
	return true;
}

void FPerfSentinelTelemetry::ClearCaptureContext()
{
	FScopeLock Lock(&Mutex);
	Session.Reset(); Run.Reset(); Role.Reset();
	ClockOffsetMs = DefaultClockOffsetMs;
	ClockUncertaintyMs = DefaultClockUncertaintyMs;
}

void FPerfSentinelTelemetry::Prune(double NowSeconds)
{
	// Disabling pre-capture retention must not discard live capture rows before the monitor can drain them.
	const double EffectiveRetention = Session.IsEmpty() ? RetentionSeconds : FMath::Max(1.0, RetentionSeconds);
	int32 RemoveCount = 0;
	while (RemoveCount < History.Num() && (History.Num() - RemoveCount > Capacity
		|| NowSeconds - History[RemoveCount].PlatformSeconds > EffectiveRetention || RetainedBytes > 8 * 1024 * 1024))
	{
		RetainedBytes -= History[RemoveCount].Bytes;
		++RemoveCount;
	}
	if (RemoveCount > 0)
	{
		DroppedRows += RemoveCount;
		History.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

bool FPerfSentinelTelemetry::Record(const TSharedRef<FJsonObject>& Event)
{
	const double Now = FPlatformTime::Seconds();
	FString Serialized;
	{
		FScopeLock Lock(&Mutex);
		if (!bEnabled && Session.IsEmpty()) { return false; }
		Event->SetNumberField(TEXT("schema_version"), 1);
		Event->SetStringField(TEXT("event_id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
		Event->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
		Event->SetStringField(TEXT("timebase"), TEXT("process_platform_seconds"));
		Event->SetStringField(TEXT("source"), TEXT("explicit_runtime_telemetry"));
		Event->SetNumberField(TEXT("platform_seconds"), Now);
		Event->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
		Event->SetStringField(TEXT("session_id"), Session);
		Event->SetStringField(TEXT("run_id"), Run.IsEmpty() ? DefaultRun : Run);
		Event->SetStringField(TEXT("process_role"), Role.IsEmpty() ? DefaultRole : Role);
		if (!Event->HasField(TEXT("clock_offset_ms"))) { Event->SetNumberField(TEXT("clock_offset_ms"), ClockOffsetMs); }
		if (!Event->HasField(TEXT("clock_uncertainty_ms")) && ClockUncertaintyMs >= 0.0) { Event->SetNumberField(TEXT("clock_uncertainty_ms"), ClockUncertaintyMs); }
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialized);
		FJsonSerializer::Serialize(Event, Writer);
		if (Serialized.Len() > 65536) { ++DroppedRows; return false; }
		// Copy through serialization, so integrations cannot mutate a row after the history lock is released.
		TSharedPtr<FJsonObject> Copy;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Serialized), Copy) || !Copy.IsValid()) { ++DroppedRows; return false; }
		const int32 Bytes = Serialized.Len() * sizeof(TCHAR);
		History.Add({ ++NextSequence, Now, Bytes, Copy });
		RetainedBytes += Bytes;
		Prune(Now);
	}
	return true;
}

void FPerfSentinelTelemetry::ReadSince(uint64& Cursor, int32 MaxRows, TArray<TSharedPtr<FJsonObject>>& OutRows, uint64& OutDroppedRows)
{
	FScopeLock Lock(&Mutex);
	Prune(FPlatformTime::Seconds());
	OutRows.Reset();
	OutDroppedRows = DroppedRows;
	for (const FHistoryRow& Row : History)
	{
		if (Row.Sequence <= Cursor) { continue; }
		if (OutRows.Num() >= FMath::Clamp(MaxRows, 1, 4096)) { break; }
		OutRows.Add(MakeShared<FJsonObject>(*Row.Event));
		Cursor = Row.Sequence;
	}
}

TSharedRef<FJsonObject> FPerfSentinelTelemetry::GetCoverage() const
{
	FScopeLock Lock(&Mutex);
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("history_enabled"), bEnabled);
	Result->SetBoolField(TEXT("active_capture_enabled"), !Session.IsEmpty());
	Result->SetBoolField(TEXT("enabled"), bEnabled || !Session.IsEmpty());
	Result->SetNumberField(TEXT("history_capacity"), Capacity);
	Result->SetNumberField(TEXT("history_seconds"), RetentionSeconds);
	Result->SetNumberField(TEXT("retained_rows"), History.Num());
	Result->SetNumberField(TEXT("estimated_retained_bytes"), static_cast<double>(RetainedBytes));
	Result->SetNumberField(TEXT("dropped_or_expired_rows"), static_cast<double>(DroppedRows));
	Result->SetStringField(TEXT("http_observation"), TEXT("adapter_required_single_cast_global_delegates_are_not_overwritten"));
	Result->SetStringField(TEXT("transport_scope"), TEXT("explicit_integrations_payload_costs_wire_bytes_only_when_measured"));
	return Result;
}

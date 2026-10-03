// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#include "PerfSentinelCollector.h"

#include "PerfSentinelSettings.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RenderTimer.h"
#include "RHIGlobals.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace
{
TSharedRef<FJsonObject> MetricFields(const TCHAR* Id, const TCHAR* Domain, const TCHAR* Unit, const TCHAR* Source,
	const TCHAR* Aggregation, const TCHAR* Scope)
{
	TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(TEXT("metric_id"), Id);
	Item->SetStringField(TEXT("domain"), Domain);
	Item->SetStringField(TEXT("unit"), Unit);
	Item->SetStringField(TEXT("aggregation"), Aggregation);
	Item->SetStringField(TEXT("scope"), Scope);
	Item->SetStringField(TEXT("source"), Source);
	return Item;
}

void UnavailableMetric(const TSharedRef<FJsonObject>& Metrics, const TCHAR* Id, const TCHAR* Domain, const TCHAR* Unit,
	const TCHAR* Source, const TCHAR* Reason, const TCHAR* Aggregation = TEXT("gauge"), const TCHAR* Scope = TEXT("process"))
{
	TSharedRef<FJsonObject> Item = MetricFields(Id, Domain, Unit, Source, Aggregation, Scope);
	Item->SetStringField(TEXT("status"), TEXT("unavailable"));
	Item->SetStringField(TEXT("unavailable_reason"), Reason);
	Metrics->SetObjectField(Id, Item);
}

void Metric(const TSharedRef<FJsonObject>& Metrics, const TCHAR* Id, const TCHAR* Domain, double Value, const TCHAR* Unit,
	const TCHAR* Source, const TCHAR* Aggregation = TEXT("gauge"), const TCHAR* Scope = TEXT("process"),
	const TCHAR* Status = TEXT("observed"), const TCHAR* TruncationReason = nullptr)
{
	if (!FMath::IsFinite(Value))
	{
		UnavailableMetric(Metrics, Id, Domain, Unit, Source, TEXT("nonfinite_native_counter"), Aggregation, Scope);
		return;
	}
	TSharedRef<FJsonObject> Item = MetricFields(Id, Domain, Unit, Source, Aggregation, Scope);
	Item->SetNumberField(TEXT("value"), Value);
	Item->SetStringField(TEXT("status"), Status);
	if (TruncationReason)
	{
		Item->SetStringField(TEXT("value_semantics"), TEXT("lower_bound"));
		Item->SetStringField(TEXT("truncation_reason"), TruncationReason);
	}
	Metrics->SetObjectField(Id, Item);
}

void MemoryMetric(const TSharedRef<FJsonObject>& Metrics, const TCHAR* Id, const TCHAR* Domain, uint64 Value,
	const TCHAR* Source, double BytesPerUnit = 1.0)
{
	// Counter sentinels must not become huge finite allocation gauges after conversion to JSON numbers.
	if (Value >= static_cast<uint64>(TNumericLimits<int64>::Max())
		|| !FMath::IsFinite(BytesPerUnit) || BytesPerUnit <= 0.0)
	{
		UnavailableMetric(Metrics, Id, Domain, TEXT("bytes"), Source, TEXT("invalid_native_memory_counter"));
		return;
	}
	const double Bytes = static_cast<double>(Value) * BytesPerUnit;
	if (!FMath::IsFinite(Bytes) || Bytes < 0.0)
	{
		UnavailableMetric(Metrics, Id, Domain, TEXT("bytes"), Source, TEXT("invalid_native_memory_counter"));
		return;
	}
	Metric(Metrics, Id, Domain, Bytes, TEXT("bytes"), Source);
}

bool IsGameWorld(const UWorld* World)
{
	return World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
}

bool CollectRender(const TSharedRef<FJsonObject>& Metrics)
{
	Metric(Metrics, TEXT("render.game_thread_ms"), TEXT("cpu"), FPlatformTime::ToMilliseconds(GGameThreadTime), TEXT("ms"), TEXT("GGameThreadTime"), TEXT("sampled_frame"));
	const TCHAR* RendererUnavailable = !FApp::CanEverRender() ? TEXT("renderer_disabled")
		: (!GDynamicRHI ? TEXT("rhi_uninitialized") : (GUsingNullRHI ? TEXT("null_rhi") : nullptr));
	if (RendererUnavailable)
	{
		UnavailableMetric(Metrics, TEXT("render.render_thread_ms"), TEXT("render"), TEXT("ms"), TEXT("GRenderThreadTime"), RendererUnavailable, TEXT("sampled_frame"));
		UnavailableMetric(Metrics, TEXT("render.rhi_thread_ms"), TEXT("render"), TEXT("ms"), TEXT("GRHIThreadTime"), RendererUnavailable, TEXT("sampled_frame"));
		UnavailableMetric(Metrics, TEXT("render.gpu_ms"), TEXT("gpu"), TEXT("ms"), TEXT("RHIGetGPUFrameCycles"), RendererUnavailable, TEXT("sampled_frame"));
		UnavailableMetric(Metrics, TEXT("render.streaming_texture_bytes"), TEXT("render_memory"), TEXT("bytes"), TEXT("GRHIGlobals.StreamingTextureMemorySizeInKB"), RendererUnavailable);
		UnavailableMetric(Metrics, TEXT("render.nonstreaming_texture_bytes"), TEXT("render_memory"), TEXT("bytes"), TEXT("GRHIGlobals.NonStreamingTextureMemorySizeInKB"), RendererUnavailable);
		UnavailableMetric(Metrics, TEXT("render.buffer_bytes"), TEXT("render_memory"), TEXT("bytes"), TEXT("GRHIGlobals.BufferMemorySize"), RendererUnavailable);
		UnavailableMetric(Metrics, TEXT("render.texture_pool_bytes"), TEXT("render_memory"), TEXT("bytes"), TEXT("GTexturePoolSize"), RendererUnavailable);
		return true;
	}
	Metric(Metrics, TEXT("render.render_thread_ms"), TEXT("render"), FPlatformTime::ToMilliseconds(GRenderThreadTime), TEXT("ms"), TEXT("GRenderThreadTime"), TEXT("sampled_frame"));
	Metric(Metrics, TEXT("render.rhi_thread_ms"), TEXT("render"), FPlatformTime::ToMilliseconds(GRHIThreadTime), TEXT("ms"), TEXT("GRHIThreadTime"), TEXT("sampled_frame"));
	const uint32 GpuCycles = RHIGetGPUFrameCycles();
	if (GpuCycles > 0) { Metric(Metrics, TEXT("render.gpu_ms"), TEXT("gpu"), FPlatformTime::ToMilliseconds(GpuCycles), TEXT("ms"), TEXT("RHIGetGPUFrameCycles"), TEXT("sampled_frame")); }
	else { UnavailableMetric(Metrics, TEXT("render.gpu_ms"), TEXT("gpu"), TEXT("ms"), TEXT("RHIGetGPUFrameCycles"), TEXT("gpu_timer_not_populated"), TEXT("sampled_frame")); }
	MemoryMetric(Metrics, TEXT("render.streaming_texture_bytes"), TEXT("render_memory"), GRHIGlobals.StreamingTextureMemorySizeInKB, TEXT("GRHIGlobals.StreamingTextureMemorySizeInKB"), 1024.0);
	MemoryMetric(Metrics, TEXT("render.nonstreaming_texture_bytes"), TEXT("render_memory"), GRHIGlobals.NonStreamingTextureMemorySizeInKB, TEXT("GRHIGlobals.NonStreamingTextureMemorySizeInKB"), 1024.0);
	MemoryMetric(Metrics, TEXT("render.buffer_bytes"), TEXT("render_memory"), GRHIGlobals.BufferMemorySize, TEXT("GRHIGlobals.BufferMemorySize"));
	const int64 TexturePoolBytes = GTexturePoolSize;
	if (TexturePoolBytes < 0) { UnavailableMetric(Metrics, TEXT("render.texture_pool_bytes"), TEXT("render_memory"), TEXT("bytes"), TEXT("GTexturePoolSize"), TEXT("invalid_native_memory_counter")); }
	else { MemoryMetric(Metrics, TEXT("render.texture_pool_bytes"), TEXT("render_memory"), static_cast<uint64>(TexturePoolBytes), TEXT("GTexturePoolSize")); }
	return true;
}

bool CollectMemory(const TSharedRef<FJsonObject>& Metrics)
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	MemoryMetric(Metrics, TEXT("memory.used_physical_bytes"), TEXT("memory"), Stats.UsedPhysical, TEXT("FPlatformMemory::GetStats"));
	MemoryMetric(Metrics, TEXT("memory.available_physical_bytes"), TEXT("memory"), Stats.AvailablePhysical, TEXT("FPlatformMemory::GetStats"));
	MemoryMetric(Metrics, TEXT("memory.used_virtual_bytes"), TEXT("memory"), Stats.UsedVirtual, TEXT("FPlatformMemory::GetStats"));
	return true;
}

bool CollectStreaming(const TSharedRef<FJsonObject>& Metrics)
{
	Metric(Metrics, TEXT("streaming.async_packages"), TEXT("streaming"), GetNumAsyncPackages(), TEXT("count"), TEXT("GetNumAsyncPackages"));
	if (!GEngine)
	{
		UnavailableMetric(Metrics, TEXT("streaming.levels_configured"), TEXT("streaming"), TEXT("count"), TEXT("UWorld::GetStreamingLevels"), TEXT("engine_uninitialized"), TEXT("gauge"), TEXT("all_game_worlds"));
		UnavailableMetric(Metrics, TEXT("streaming.levels_loading"), TEXT("streaming"), TEXT("count"), TEXT("UWorld::GetNumStreamingLevelsBeingLoaded"), TEXT("engine_uninitialized"), TEXT("gauge"), TEXT("all_game_worlds"));
		return true;
	}
	constexpr int32 MaxWorlds = 64;
	int32 WorldCount = 0;
	int64 LevelCount = 0, LoadingLevels = 0;
	bool bPartial = false;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		const UWorld* World = Context.World();
		if (!IsGameWorld(World)) { continue; }
		if (WorldCount >= MaxWorlds) { bPartial = true; break; }
		++WorldCount;
		LevelCount += World->GetStreamingLevels().Num();
		LoadingLevels += World->GetNumStreamingLevelsBeingLoaded();
	}
	const TCHAR* Scope = bPartial ? TEXT("partial_game_worlds") : TEXT("all_game_worlds");
	const TCHAR* Status = bPartial ? TEXT("truncated") : TEXT("observed");
	const TCHAR* TruncationReason = bPartial ? TEXT("world_limit") : nullptr;
	Metric(Metrics, TEXT("streaming.levels_configured"), TEXT("streaming"), static_cast<double>(LevelCount), TEXT("count"), TEXT("UWorld::GetStreamingLevels"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("streaming.levels_loading"), TEXT("streaming"), static_cast<double>(LoadingLevels), TEXT("count"), TEXT("UWorld::GetNumStreamingLevelsBeingLoaded"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metrics->SetBoolField(TEXT("partial"), bPartial);
	Metrics->SetNumberField(TEXT("worlds_sampled"), WorldCount);
	Metrics->SetNumberField(TEXT("world_limit"), MaxWorlds);
	return true;
}

bool CollectWorkload(const TSharedRef<FJsonObject>& Metrics)
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.05, static_cast<double>(Settings->MaxCollectorTimeMilliseconds)) * 0.001;
	const int32 MaxComponents = FMath::Clamp(FMath::Clamp(Settings->MaxCollectorRows, 1, 1024) * 32, 256, 32768);
	int32 Scanned = 0, RuntimeComponents = 0, Physics = 0, Skeletal = 0, Animation = 0, Audio = 0, PlayingAudio = 0, Brains = 0, TickingBrains = 0;
	bool bPartial = false;
	const TCHAR* TruncationReason = nullptr;
	for (TObjectIterator<UActorComponent> It; It; ++It)
	{
		if (Scanned >= MaxComponents) { bPartial = true; TruncationReason = TEXT("component_limit"); break; }
		if ((Scanned & 31) == 0 && FPlatformTime::Seconds() >= Deadline) { bPartial = true; TruncationReason = TEXT("cooperative_time_budget"); break; }
		++Scanned;
		UActorComponent* Component = *It;
		if (!IsValid(Component) || !IsGameWorld(Component->GetWorld()) || Component->IsTemplate()) { continue; }
		++RuntimeComponents;
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component)) { Physics += Primitive->IsSimulatingPhysics() ? 1 : 0; }
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Component)) { ++Skeletal; Animation += Mesh->GetAnimInstance() ? 1 : 0; }
		if (UAudioComponent* Sound = Cast<UAudioComponent>(Component)) { ++Audio; PlayingAudio += Sound->IsPlaying() ? 1 : 0; }
		// Reflection avoids forcing AIModule into every runtime target; count brain components if that module is loaded.
		for (const UClass* Class = Component->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (Class->GetName() == TEXT("BrainComponent")) { ++Brains; TickingBrains += Component->IsComponentTickEnabled() ? 1 : 0; break; }
		}
	}
	const TCHAR* Scope = bPartial ? TEXT("partial_game_world_component_scan") : TEXT("all_game_world_components");
	const TCHAR* Status = bPartial ? TEXT("truncated") : TEXT("observed");
	Metric(Metrics, TEXT("workload.components_observed"), TEXT("workload"), RuntimeComponents, TEXT("count"), TEXT("TObjectIterator<UActorComponent>"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("physics.simulating_components"), TEXT("physics"), Physics, TEXT("count"), TEXT("UPrimitiveComponent::IsSimulatingPhysics"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("animation.skeletal_mesh_components"), TEXT("animation"), Skeletal, TEXT("count"), TEXT("USkeletalMeshComponent"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("animation.anim_instances"), TEXT("animation"), Animation, TEXT("count"), TEXT("USkeletalMeshComponent::GetAnimInstance"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("audio.components"), TEXT("audio"), Audio, TEXT("count"), TEXT("UAudioComponent"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("audio.playing_components"), TEXT("audio"), PlayingAudio, TEXT("count"), TEXT("UAudioComponent::IsPlaying"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("ai.brain_components"), TEXT("ai"), Brains, TEXT("count"), TEXT("BrainComponent class hierarchy"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metric(Metrics, TEXT("ai.ticking_brains"), TEXT("ai"), TickingBrains, TEXT("count"), TEXT("UActorComponent::IsComponentTickEnabled"), TEXT("gauge"), Scope, Status, TruncationReason);
	Metrics->SetBoolField(TEXT("partial"), bPartial);
	Metrics->SetNumberField(TEXT("objects_examined"), Scanned);
	return true;
}
}

FPerfSentinelCollectorRegistry& FPerfSentinelCollectorRegistry::Get()
{
	static FPerfSentinelCollectorRegistry Registry;
	return Registry;
}

void FPerfSentinelCollectorRegistry::InitializeBuiltins()
{
	check(IsInGameThread());
	auto Add = [this](const TCHAR* Id, const TCHAR* Domain, const TCHAR* Source, const TCHAR* Description, bool bDetailed, bool(*Callback)(const TSharedRef<FJsonObject>&))
	{
		if (Collectors.Contains(FName(Id))) { return; }
		FPerfSentinelCollectorDefinition Definition;
		Definition.Id = FName(Id); Definition.Domain = Domain; Definition.Source = Source; Definition.Description = Description;
		Definition.bDetailed = bDetailed; Definition.MinimumIntervalSeconds = bDetailed ? 5.0 : 1.0;
		Definition.Collect = FPerfSentinelCollectorDelegate::CreateLambda([Callback](TSharedRef<FJsonObject> Metrics) { return Callback(Metrics); });
		RegisterCollector(Definition);
	};
	Add(TEXT("engine_render"), TEXT("render"), TEXT("native_frame_timers_and_rhi_counters"), TEXT("Sampled process-wide frame timers and RHI allocation gauges, not per-pass GPU timing."), false, CollectRender);
	Add(TEXT("engine_memory"), TEXT("memory"), TEXT("platform_memory_stats"), TEXT("Process allocation and available-memory gauges."), false, CollectMemory);
	Add(TEXT("engine_streaming"), TEXT("streaming"), TEXT("native_async_and_world_streaming_counters"), TEXT("Configured/loading levels and pending async packages; counts do not measure loading CPU time."), false, CollectStreaming);
	Add(TEXT("engine_workload"), TEXT("physics_animation_ai_audio"), TEXT("bounded_runtime_component_scan"), TEXT("Optional workload gauges. Individual subsystem costs come from native named CPU/GPU scopes, not these counts."), true, CollectWorkload);
}

bool FPerfSentinelCollectorRegistry::RegisterCollector(const FPerfSentinelCollectorDefinition& Definition)
{
	if (!IsInGameThread() || bSampling || Definition.Id.IsNone() || !Definition.Collect.IsBound()
		|| !FMath::IsFinite(Definition.MinimumIntervalSeconds) || Definition.Source.IsEmpty()
		|| Collectors.Num() >= 64 || Collectors.Contains(Definition.Id)) { return false; }
	FCollectorState State;
	State.Definition = Definition;
	State.Definition.MinimumIntervalSeconds = FMath::Max(0.1, Definition.MinimumIntervalSeconds);
	Collectors.Add(Definition.Id, MoveTemp(State));
	return true;
}

void FPerfSentinelCollectorRegistry::UnregisterCollector(FName Id)
{
	if (IsInGameThread() && !bSampling) { Collectors.Remove(Id); }
}

void FPerfSentinelCollectorRegistry::ResetCapture()
{
	check(IsInGameThread());
	for (auto& Pair : Collectors) { Pair.Value.LastSampleSeconds = -TNumericLimits<double>::Max(); Pair.Value.ConsecutiveOverruns = 0; }
}

TSharedRef<FJsonObject> FPerfSentinelCollectorRegistry::GetCatalog() const
{
	TSharedRef<FJsonObject> Catalog = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const auto& Pair : Collectors)
	{
		const FPerfSentinelCollectorDefinition& Definition = Pair.Value.Definition;
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("collector_id"), Definition.Id.ToString());
		Entry->SetStringField(TEXT("domain"), Definition.Domain.Left(128));
		Entry->SetStringField(TEXT("source"), Definition.Source.Left(256));
		Entry->SetStringField(TEXT("description"), Definition.Description.Left(1024));
		Entry->SetBoolField(TEXT("detailed"), Definition.bDetailed);
		Entry->SetNumberField(TEXT("minimum_interval_seconds"), Definition.MinimumIntervalSeconds);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Catalog->SetArrayField(TEXT("collectors"), Entries);
	Catalog->SetStringField(TEXT("budget_semantics"), TEXT("limits_scheduling_and_cooperative_scans_cannot_preempt_integration_callbacks"));
	return Catalog;
}

TSharedRef<FJsonObject> FPerfSentinelCollectorRegistry::Sample(double Seconds, const UPerfSentinelSettings& Settings)
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(PerfSentinel_CollectorCatalog);
	TGuardValue<bool> SamplingGuard(bSampling, true);
	const double Started = FPlatformTime::Seconds();
	const double BudgetMs = FMath::Max(0.05, static_cast<double>(Settings.MaxCollectorTimeMilliseconds));
	TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetStringField(TEXT("event_type"), TEXT("collector_snapshot"));
	TArray<TSharedPtr<FJsonValue>> Values;
	int32 Rows = 0;
	for (auto& Pair : Collectors)
	{
		FCollectorState& State = Pair.Value;
		const bool bLightweight = Settings.CaptureProfile == EPerfSentinelCaptureProfile::LightweightBaseline;
		const double Cadence = FMath::Max(State.Definition.MinimumIntervalSeconds, static_cast<double>(State.Definition.bDetailed ? Settings.DetailedCollectorIntervalSeconds : Settings.CollectorIntervalSeconds));
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("collector_id"), Pair.Key.ToString());
		Result->SetStringField(TEXT("source"), State.Definition.Source.Left(256));
		Result->SetNumberField(TEXT("cadence_seconds"), Cadence);
		Result->SetNumberField(TEXT("elapsed_ms"), 0.0);
		if (State.Definition.bDetailed && (bLightweight || !Settings.bEnableDetailedCollectors)) { Result->SetStringField(TEXT("status"), bLightweight ? TEXT("disabled_by_profile") : TEXT("disabled_by_settings")); }
		else if (State.ConsecutiveOverruns >= 3) { Result->SetStringField(TEXT("status"), TEXT("suspended_after_budget_overruns")); }
		else if (Seconds - State.LastSampleSeconds < Cadence) { Result->SetStringField(TEXT("status"), TEXT("not_due")); }
		else if ((FPlatformTime::Seconds() - Started) * 1000.0 >= BudgetMs || Rows >= FMath::Clamp(Settings.MaxCollectorRows, 1, 4096)) { Result->SetStringField(TEXT("status"), TEXT("deferred_by_budget")); }
		else
		{
			const double CollectorStarted = FPlatformTime::Seconds();
			TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
			const bool bAvailable = State.Definition.Collect.Execute(Metrics);
			const int32 RemainingRows = FMath::Max(0, FMath::Clamp(Settings.MaxCollectorRows, 1, 4096) - Rows);
			int32 RetainedRows = 0;
			bool bPartial = false;
			Metrics->TryGetBoolField(TEXT("partial"), bPartial);
			bool bOutputTruncated = false;
			for (auto It = Metrics->Values.CreateIterator(); It; ++It)
			{
				if (RetainedRows++ >= RemainingRows) { It.RemoveCurrent(); bOutputTruncated = true; }
			}
			Result->SetBoolField(TEXT("truncated"), bPartial || bOutputTruncated);
			Result->SetBoolField(TEXT("scan_truncated"), bPartial);
			Result->SetBoolField(TEXT("output_truncated"), bOutputTruncated);
			const double ElapsedMs = (FPlatformTime::Seconds() - CollectorStarted) * 1000.0;
			State.LastSampleSeconds = Seconds;
			State.ConsecutiveOverruns = ElapsedMs > BudgetMs ? State.ConsecutiveOverruns + 1 : 0;
			Result->SetNumberField(TEXT("elapsed_ms"), ElapsedMs);
			Result->SetStringField(TEXT("status"), !bAvailable ? TEXT("unavailable") : (ElapsedMs > BudgetMs ? TEXT("observed_budget_overrun") : TEXT("observed")));
			Result->SetObjectField(TEXT("metrics"), Metrics);
			Rows += Metrics->Values.Num();
		}
		Values.Add(MakeShared<FJsonValueObject>(Result));
	}
	Snapshot->SetArrayField(TEXT("collectors"), Values);
	Snapshot->SetNumberField(TEXT("collector_overhead_ms"), (FPlatformTime::Seconds() - Started) * 1000.0);
	Snapshot->SetNumberField(TEXT("collector_budget_ms"), BudgetMs);
	return Snapshot;
}

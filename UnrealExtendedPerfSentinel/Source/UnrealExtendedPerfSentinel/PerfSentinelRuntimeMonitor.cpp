// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelRuntimeMonitor.h"

#include "PerfSentinelSettings.h"
#include "PerfSentinelStatHarvester.h"
#include "PerfSentinelTraceController.h"

#include "Async/Async.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/Widget.h"
#include "DynamicRHI.h"
#include "ImageUtils.h"
#include "RenderTimer.h"
#include "RHIGlobals.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
UWorld* GetPerfSentinelWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game || World->WorldType == EWorldType::Editor))
		{
			return World;
		}
	}

	return GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr;
}

FString WorldTypeToString(EWorldType::Type WorldType)
{
	switch (WorldType)
	{
	case EWorldType::None:
		return TEXT("None");
	case EWorldType::Game:
		return TEXT("Game");
	case EWorldType::Editor:
		return TEXT("Editor");
	case EWorldType::PIE:
		return TEXT("PIE");
	case EWorldType::EditorPreview:
		return TEXT("EditorPreview");
	case EWorldType::GamePreview:
		return TEXT("GamePreview");
	case EWorldType::GameRPC:
		return TEXT("GameRPC");
	case EWorldType::Inactive:
		return TEXT("Inactive");
	default:
		return TEXT("Unknown");
	}
}

FString BuildConfigurationToString()
{
#if UE_BUILD_SHIPPING
	return TEXT("Shipping");
#elif UE_BUILD_TEST
	return TEXT("Test");
#elif UE_BUILD_DEBUG
	return TEXT("Debug");
#elif UE_BUILD_DEVELOPMENT
	return TEXT("Development");
#else
	return TEXT("Unknown");
#endif
}

void AddWorldCounterFields(const UWorld* World, const TSharedRef<FJsonObject>& Root)
{
	int32 ActorCount = 0;
	int32 TickingActorCount = 0;
	int32 ComponentCount = 0;
	int32 TickingComponentCount = 0;
	int32 PrimitiveComponentCount = 0;
	int32 LoadedStreamingLevelCount = 0;
	int32 VisibleStreamingLevelCount = 0;

	if (World)
	{
		for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			++ActorCount;
			if (It->IsActorTickEnabled())
			{
				++TickingActorCount;
			}
		}

		for (TObjectIterator<UActorComponent> It; It; ++It)
		{
			const UActorComponent* Component = *It;
			if (!Component || Component->GetWorld() != World)
			{
				continue;
			}

			++ComponentCount;
			if (Component->IsComponentTickEnabled())
			{
				++TickingComponentCount;
			}
			if (Cast<UPrimitiveComponent>(Component))
			{
				++PrimitiveComponentCount;
			}
		}

		for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (!StreamingLevel)
			{
				continue;
			}
			if (StreamingLevel->IsLevelLoaded())
			{
				++LoadedStreamingLevelCount;
			}
			if (StreamingLevel->IsLevelVisible())
			{
				++VisibleStreamingLevelCount;
			}
		}
	}

	int32 UObjectCount = 0;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		++UObjectCount;
	}

	Root->SetNumberField(TEXT("actor_count"), ActorCount);
	Root->SetNumberField(TEXT("ticking_actor_count"), TickingActorCount);
	Root->SetNumberField(TEXT("component_count"), ComponentCount);
	Root->SetNumberField(TEXT("ticking_component_count"), TickingComponentCount);
	Root->SetNumberField(TEXT("primitive_component_count"), PrimitiveComponentCount);
	Root->SetNumberField(TEXT("uobject_count"), UObjectCount);
	Root->SetNumberField(TEXT("loaded_streaming_level_count"), LoadedStreamingLevelCount);
	Root->SetNumberField(TEXT("visible_streaming_level_count"), VisibleStreamingLevelCount);
}

void AddMemoryFields(const TSharedRef<FJsonObject>& Root)
{
	const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	Root->SetNumberField(TEXT("memory_used_physical_bytes"), static_cast<double>(MemoryStats.UsedPhysical));
	Root->SetNumberField(TEXT("memory_peak_used_physical_bytes"), static_cast<double>(MemoryStats.PeakUsedPhysical));
	Root->SetNumberField(TEXT("memory_available_physical_bytes"), static_cast<double>(MemoryStats.AvailablePhysical));
	Root->SetNumberField(TEXT("memory_used_virtual_bytes"), static_cast<double>(MemoryStats.UsedVirtual));
	Root->SetNumberField(TEXT("memory_peak_used_virtual_bytes"), static_cast<double>(MemoryStats.PeakUsedVirtual));
}

void WriteJsonObjectToFile(const FString& Path, const TSharedRef<FJsonObject>& Object)
{
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(Object, Writer);

	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
	FFileHelper::SaveStringToFile(OutputString, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString SlateVisibilityToString(ESlateVisibility Visibility)
{
	if (const UEnum* Enum = StaticEnum<ESlateVisibility>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Visibility));
	}
	return FString::FromInt(static_cast<int32>(Visibility));
}
}

FPerfSentinelRuntimeMonitor::FPerfSentinelRuntimeMonitor() = default;

FPerfSentinelRuntimeMonitor::~FPerfSentinelRuntimeMonitor()
{
	Deactivate();
}

void FPerfSentinelRuntimeMonitor::Activate(const FString& InSessionBaseName, const FString& InSessionOutputDirectory)
{
	Deactivate();

	SessionBaseName = FPaths::MakeValidFileName(InSessionBaseName);
	if (SessionBaseName.IsEmpty())
	{
		SessionBaseName = TEXT("PerfSentinel");
	}

	SpikeOutputDirectory = InSessionOutputDirectory;
	if (SpikeOutputDirectory.IsEmpty())
	{
		SpikeOutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PerfSentinel"), SessionBaseName);
	}
	SpikeOutputDirectory = FPaths::ConvertRelativePathToFull(SpikeOutputDirectory);
	FPaths::NormalizeDirectoryName(SpikeOutputDirectory);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.CreateDirectoryTree(*SpikeOutputDirectory))
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("RuntimeMonitor: Failed to create spike output directory: %s"), *SpikeOutputDirectory);
	}

	SpikesDirectory = FPaths::Combine(SpikeOutputDirectory, TEXT("spikes"));
	FPaths::NormalizeDirectoryName(SpikesDirectory);
	if (!PlatformFile.CreateDirectoryTree(*SpikesDirectory))
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("RuntimeMonitor: Failed to create spikes directory: %s"), *SpikesDirectory);
	}

	SpikeEventsPath = FPaths::Combine(SpikesDirectory, FString::Printf(TEXT("%s_spikes.ndjson"), *SessionBaseName));
	FrameSamplesPath = FPaths::Combine(SpikeOutputDirectory, FString::Printf(TEXT("%s_frames.ndjson"), *SessionBaseName));
	RuntimeContextPath = FPaths::Combine(SpikeOutputDirectory, TEXT("runtime_context.json"));
	RuntimeCountersPath = FPaths::Combine(SpikeOutputDirectory, FString::Printf(TEXT("%s_runtime_counters.ndjson"), *SessionBaseName));
	GameStatsPath = FPaths::Combine(SpikeOutputDirectory, TEXT("GameStats.json"));
	FPaths::NormalizeFilename(SpikeEventsPath);
	FPaths::NormalizeFilename(FrameSamplesPath);
	FPaths::NormalizeFilename(RuntimeContextPath);
	FPaths::NormalizeFilename(RuntimeCountersPath);
	FPaths::NormalizeFilename(GameStatsPath);

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (Settings && Settings->bCollectFrameSamples)
	{
		FrameSamplesWriter.Reset(IFileManager::Get().CreateFileWriter(*FrameSamplesPath));
		if (!FrameSamplesWriter)
		{
			UE_LOG(LogPerfSentinel, Warning, TEXT("RuntimeMonitor: Failed to open frame sample stream: %s"), *FrameSamplesPath);
		}
	}
	if (Settings && Settings->bCollectRuntimeCounters)
	{
		RuntimeCountersWriter.Reset(IFileManager::Get().CreateFileWriter(*RuntimeCountersPath));
		if (!RuntimeCountersWriter)
		{
			UE_LOG(LogPerfSentinel, Warning, TEXT("RuntimeMonitor: Failed to open runtime counter stream: %s"), *RuntimeCountersPath);
		}
	}

	SpikeSnapshots.Reset();
	FrameSampleBuffer.Reset();
	PendingSpikeWindows.Reset();
	AggregatedScopes.Reset();
	PendingScreenshotPaths.Reset();
	LastScreenshotTimeSeconds = -TNumericLimits<double>::Max();
	LastSpikeEventTimeSeconds = -TNumericLimits<double>::Max();
	SuppressedSpikeCount = 0;
	WorstSuppressedFrameMs = 0.0f;
	LastRuntimeCounterTimeSeconds = -TNumericLimits<double>::Max();
	LastLeakSnapshotTimeSeconds = -TNumericLimits<double>::Max();
	CaptureStartPlatformSeconds = FPlatformTime::Seconds();
	GarbageCollectionCount = 0;
	LastGarbageCollectPlatformSeconds = -1.0;
	bActive = true;

	// Force the engine stats system to collect per-frame scope data so spike-frame cost can be attributed.
	bStatHarvestEnabled = Settings && Settings->bHarvestRuntimeStats && FPerfSentinelStatHarvester::IsAvailable();
	if (bStatHarvestEnabled)
	{
		FPerfSentinelStatHarvester::Enable();
	}

	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FPerfSentinelRuntimeMonitor::HandlePostGarbageCollect);

	if (Settings && Settings->bCaptureScreenshotOnSpike && Settings->bAsyncScreenshotCompression && GEngine && GEngine->GameViewport)
	{
		ScreenshotCapturedHandle = UGameViewportClient::OnScreenshotCaptured().AddRaw(this, &FPerfSentinelRuntimeMonitor::HandleScreenshotCaptured);
	}

	WriteRuntimeContext();
	WriteGameStats();

	EndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FPerfSentinelRuntimeMonitor::HandleEndFrame);

	UE_LOG(LogPerfSentinel, Log, TEXT("RuntimeMonitor: Activated for session %s (stat harvest: %s)"),
		*SessionBaseName, bStatHarvestEnabled ? TEXT("on") : TEXT("off/unavailable"));
}

void FPerfSentinelRuntimeMonitor::Deactivate()
{
	if (EndFrameHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
		EndFrameHandle.Reset();
	}

	if (ScreenshotCapturedHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		ScreenshotCapturedHandle.Reset();
	}

	if (PostGarbageCollectHandle.IsValid())
	{
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
		PostGarbageCollectHandle.Reset();
	}

	if (bStatHarvestEnabled)
	{
		FPerfSentinelStatHarvester::Disable();
		bStatHarvestEnabled = false;
	}

	if (bActive)
	{
		FlushSpikeWindows(true);
		WriteGameStats();
		UE_LOG(LogPerfSentinel, Log, TEXT("RuntimeMonitor: Deactivated with %d spike event(s)."), SpikeSnapshots.Num());
	}

	PendingScreenshotPaths.Reset();
	FrameSamplesWriter.Reset();
	RuntimeCountersWriter.Reset();
	bActive = false;
}

bool FPerfSentinelRuntimeMonitor::CaptureManualSpike()
{
	if (!bActive)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("CaptureManualSpike: Runtime monitor is not active."));
		return false;
	}

	return CaptureSpike(FApp::GetDeltaTime() * 1000.0f, true);
}

void FPerfSentinelRuntimeMonitor::HandleEndFrame()
{
	if (!bActive)
	{
		return;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		return;
	}

	const float FrameTimeMs = FApp::GetDeltaTime() * 1000.0f;
	RecordFrameSample(*Settings);
	FlushSpikeWindows(false);

	if (FrameTimeMs >= Settings->ScreenshotSpikeThresholdMs)
	{
		CaptureSpike(FrameTimeMs, false);
	}
}

void FPerfSentinelRuntimeMonitor::RecordFrameSample(const UPerfSentinelSettings& Settings)
{
	FFrameSample Sample;
	Sample.Timestamp = FDateTime::UtcNow();
	Sample.PlatformSeconds = FPlatformTime::Seconds();
	Sample.AppSeconds = FApp::GetCurrentTime();
	Sample.FrameNumber = static_cast<int64>(GFrameCounter);
	Sample.FrameTimeMs = FApp::GetDeltaTime() * 1000.0f;
	FillThreadTimings(Sample);

	if (Settings.bCollectFrameSamples)
	{
		AppendFrameSample(Sample);
	}

	if (Settings.bWriteSpikeWindows)
	{
		FrameSampleBuffer.Add(Sample);
		PruneFrameSampleBuffer(Sample.PlatformSeconds, Settings.SpikeWindowPreSeconds + Settings.SpikeWindowPostSeconds + 1.0f);
	}

	if (Settings.bCollectRuntimeCounters)
	{
		const double IntervalSeconds = FMath::Max(0.1, static_cast<double>(Settings.RuntimeCounterIntervalSeconds));
		if ((Sample.PlatformSeconds - LastRuntimeCounterTimeSeconds) >= IntervalSeconds)
		{
			AppendRuntimeCounters(Sample);
			LastRuntimeCounterTimeSeconds = Sample.PlatformSeconds;
		}
	}
}

void FPerfSentinelRuntimeMonitor::FillThreadTimings(FFrameSample& Sample)
{
	// Same global counters that drive `stat unit`. Values are cycles; convert to ms.
	Sample.GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	Sample.RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	Sample.RhiThreadMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	Sample.GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
}

void FPerfSentinelRuntimeMonitor::AppendFrameSample(const FFrameSample& Sample) const
{
	if (!FrameSamplesWriter)
	{
		return;
	}

	WriteJsonLine(FrameSamplesWriter.Get(), BuildFrameSampleObject(Sample));
}

void FPerfSentinelRuntimeMonitor::AppendRuntimeCounters(const FFrameSample& Sample)
{
	if (!RuntimeCountersWriter)
	{
		return;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("timestamp"), Sample.Timestamp.ToIso8601());
	Root->SetNumberField(TEXT("platform_seconds"), Sample.PlatformSeconds);
	Root->SetNumberField(TEXT("app_seconds"), Sample.AppSeconds);
	Root->SetNumberField(TEXT("frame_number"), static_cast<double>(Sample.FrameNumber));
	Root->SetNumberField(TEXT("frame_time_ms"), Sample.FrameTimeMs);
	Root->SetNumberField(TEXT("delta_seconds"), FApp::GetDeltaTime());
	Root->SetNumberField(TEXT("game_thread_ms"), Sample.GameThreadMs);
	Root->SetNumberField(TEXT("render_thread_ms"), Sample.RenderThreadMs);
	Root->SetNumberField(TEXT("rhi_thread_ms"), Sample.RhiThreadMs);
	Root->SetNumberField(TEXT("gpu_ms"), Sample.GpuMs);

	// Garbage-collection tracking (cheap, every counter line) for leak analysis.
	Root->SetNumberField(TEXT("gc_count"), GarbageCollectionCount);
	Root->SetNumberField(TEXT("time_since_last_gc_seconds"),
		LastGarbageCollectPlatformSeconds >= 0.0 ? (Sample.PlatformSeconds - LastGarbageCollectPlatformSeconds) : -1.0);

	UWorld* World = GetPerfSentinelWorld();
	if (World)
	{
		Root->SetStringField(TEXT("map_name"), World->GetMapName());
		Root->SetStringField(TEXT("world_type"), WorldTypeToString(World->WorldType));
	}
	AddWorldCounterFields(World, Root);
	AddMemoryFields(Root);

	// Heavy per-class breakdown + streaming-level contents on a slower cadence to limit overhead.
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (Settings && Settings->bCollectPerClassBreakdown)
	{
		const double LeakInterval = FMath::Max(0.5, static_cast<double>(Settings->LeakSnapshotIntervalSeconds));
		if ((Sample.PlatformSeconds - LastLeakSnapshotTimeSeconds) >= LeakInterval)
		{
			Root->SetBoolField(TEXT("has_class_breakdown"), true);
			AddPerClassBreakdownFields(World, Root);
			LastLeakSnapshotTimeSeconds = Sample.PlatformSeconds;
		}
	}

	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		Root->SetNumberField(TEXT("viewport_width"), ViewportSize.X);
		Root->SetNumberField(TEXT("viewport_height"), ViewportSize.Y);
	}

	WriteJsonLine(RuntimeCountersWriter.Get(), Root);
}

void FPerfSentinelRuntimeMonitor::AddPerClassBreakdownFields(const UWorld* World, const TSharedRef<FJsonObject>& Root) const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const int32 TopN = Settings ? FMath::Max(1, Settings->PerClassTopN) : 25;

	TMap<FName, int32> ActorClassCounts;
	TMap<FName, int32> ComponentClassCounts;
	TMap<FName, int32> ObjectClassCounts;

	if (World)
	{
		for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (const UClass* Class = It->GetClass())
			{
				ActorClassCounts.FindOrAdd(Class->GetFName())++;
			}
		}

		for (TObjectIterator<UActorComponent> It; It; ++It)
		{
			const UActorComponent* Component = *It;
			if (Component && Component->GetWorld() == World && Component->GetClass())
			{
				ComponentClassCounts.FindOrAdd(Component->GetClass()->GetFName())++;
			}
		}
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (const UClass* Class = It->GetClass())
		{
			ObjectClassCounts.FindOrAdd(Class->GetFName())++;
		}
	}

	auto WriteTopClasses = [&Root, TopN](const TCHAR* FieldName, TMap<FName, int32>& Counts)
	{
		TArray<TPair<FName, int32>> Sorted = Counts.Array();
		Sorted.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B) { return A.Value > B.Value; });

		TArray<TSharedPtr<FJsonValue>> Values;
		const int32 Num = FMath::Min(TopN, Sorted.Num());
		for (int32 Index = 0; Index < Num; ++Index)
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("class"), Sorted[Index].Key.ToString());
			Entry->SetNumberField(TEXT("count"), Sorted[Index].Value);
			Values.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(FieldName, Values);
	};

	WriteTopClasses(TEXT("actor_classes"), ActorClassCounts);
	WriteTopClasses(TEXT("component_classes"), ComponentClassCounts);
	WriteTopClasses(TEXT("uobject_classes"), ObjectClassCounts);

	// Streaming-level contents (names), to detect load/unload churn.
	if (World)
	{
		TArray<TSharedPtr<FJsonValue>> LoadedLevels;
		TArray<TSharedPtr<FJsonValue>> VisibleLevels;
		for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			if (!StreamingLevel)
			{
				continue;
			}
			const FString LevelName = StreamingLevel->GetWorldAssetPackageName();
			if (StreamingLevel->IsLevelLoaded())
			{
				LoadedLevels.Add(MakeShared<FJsonValueString>(LevelName));
			}
			if (StreamingLevel->IsLevelVisible())
			{
				VisibleLevels.Add(MakeShared<FJsonValueString>(LevelName));
			}
		}
		Root->SetArrayField(TEXT("loaded_streaming_levels"), LoadedLevels);
		Root->SetArrayField(TEXT("visible_streaming_levels"), VisibleLevels);
	}
}

bool FPerfSentinelRuntimeMonitor::CaptureSpike(float FrameTimeMs, bool bManual)
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("CaptureSpike: PerfSentinel settings are unavailable."));
		return false;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	if (!bManual)
	{
		const bool bAtArtifactLimit = SpikeSnapshots.Num() >= FMath::Max(1, Settings->MaxSpikeEvents);
		const bool bInsideCluster = (NowSeconds - LastSpikeEventTimeSeconds) < FMath::Max(0.0, static_cast<double>(Settings->SpikeEventCooldownSeconds));
		if (bAtArtifactLimit || bInsideCluster)
		{
			++SuppressedSpikeCount;
			WorstSuppressedFrameMs = FMath::Max(WorstSuppressedFrameMs, FrameTimeMs);
			return false;
		}
	}
	LastSpikeEventTimeSeconds = NowSeconds;
	const bool bCooldownElapsed = (NowSeconds - LastScreenshotTimeSeconds) >= Settings->ScreenshotCooldownSeconds;
	const int32 SpikeIndex = SpikeSnapshots.Num() + 1;
	const int64 SpikeFrameNumber = static_cast<int64>(GFrameCounter);
	const FString SpikeDirectory = BuildSpikeDirectory(SpikeIndex, SpikeFrameNumber);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SpikeDirectory);

	// Drop a trace bookmark so the offline analyzer can align this spike to true trace time.
	TRACE_BOOKMARK(TEXT("PerfSentinelSpike_%d_Frame_%lld"), SpikeIndex, SpikeFrameNumber);

	// Attribute this spike frame's CPU cost to per-class scopes via the engine stats system.
	HarvestSpikeStats();

	FString ScreenshotPath;
	if (Settings->bCaptureScreenshotOnSpike && (bManual || bCooldownElapsed))
	{
		const FString RequestedPath = BuildScreenshotPath(SpikeDirectory);
		if (RequestSpikeScreenshot(RequestedPath, Settings->bAsyncScreenshotCompression))
		{
			ScreenshotPath = RequestedPath;
			LastScreenshotTimeSeconds = NowSeconds;
		}
	}

	FPerfSentinelSpikeSnapshot Snapshot;
	Snapshot.Timestamp = FDateTime::UtcNow();
	Snapshot.FrameNumber = SpikeFrameNumber;
	Snapshot.PlatformSeconds = NowSeconds;
	Snapshot.FrameTimeMs = FrameTimeMs;
	Snapshot.GameThreadTimeMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	Snapshot.RenderThreadTimeMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	Snapshot.RhiThreadTimeMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	Snapshot.GpuTimeMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	Snapshot.ScreenshotPath = MakeProjectSavedRelativePath(ScreenshotPath);

	if (!AppendSpikeEvent(Snapshot, Settings->ScreenshotSpikeThresholdMs, SpikeDirectory))
	{
		return false;
	}

	SpikeSnapshots.Add(Snapshot);

	FFrameSample SpikeSample;
	SpikeSample.Timestamp = Snapshot.Timestamp;
	SpikeSample.PlatformSeconds = NowSeconds;
	SpikeSample.AppSeconds = FApp::GetCurrentTime();
	SpikeSample.FrameNumber = Snapshot.FrameNumber;
	SpikeSample.FrameTimeMs = Snapshot.FrameTimeMs;
	QueueSpikeWindow(SpikeSample, *Settings, SpikeDirectory);

	UE_LOG(
		LogPerfSentinel,
		Log,
		TEXT("RuntimeMonitor: %s spike captured. Frame %.2f ms, screenshot: %s"),
		bManual ? TEXT("Manual") : TEXT("Frame-time"),
		FrameTimeMs,
		Snapshot.ScreenshotPath.IsEmpty() ? TEXT("<none>") : *Snapshot.ScreenshotPath);

	return true;
}

void FPerfSentinelRuntimeMonitor::HarvestSpikeStats()
{
	if (!bStatHarvestEnabled)
	{
		return;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const int32 TopN = Settings ? FMath::Max(1, Settings->RuntimeStatTopN) : 40;

	TArray<FPerfSentinelStatSample> Samples;
	if (!FPerfSentinelStatHarvester::HarvestTopScopes(TopN, Samples))
	{
		return;
	}

	for (const FPerfSentinelStatSample& Sample : Samples)
	{
		FAggregatedScope& Scope = AggregatedScopes.FindOrAdd(Sample.ScopeName);
		Scope.TotalInclusiveMs += Sample.InclusiveMs;
		Scope.MaxInclusiveMs = FMath::Max(Scope.MaxInclusiveMs, Sample.InclusiveMs);
		Scope.TotalExclusiveMs += Sample.ExclusiveMs;
		Scope.CallCount += Sample.CallCount;
		++Scope.SpikeCount;
	}
}

bool FPerfSentinelRuntimeMonitor::RequestSpikeScreenshot(const FString& TargetPath, bool bAsync)
{
	if (TargetPath.IsEmpty())
	{
		return false;
	}

	if (!bAsync || !ScreenshotCapturedHandle.IsValid())
	{
		// Synchronous engine path (writes the PNG on the game thread). Used when async is disabled.
		FScreenshotRequest::RequestScreenshot(TargetPath, false, false);
		return true;
	}

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const int32 MaxInFlight = Settings ? FMath::Max(1, Settings->MaxInFlightScreenshots) : 4;
	if (PendingScreenshotPaths.Num() >= MaxInFlight)
	{
		UE_LOG(LogPerfSentinel, Verbose, TEXT("RequestSpikeScreenshot: Dropping screenshot, %d already in flight."), PendingScreenshotPaths.Num());
		return false;
	}

	// Request a capture with no engine-side filename: the engine only reads back the buffer and
	// broadcasts OnScreenshotCaptured. We then compress + write off the game thread.
	PendingScreenshotPaths.Add(TargetPath);
	FScreenshotRequest::RequestScreenshot(false);
	return true;
}

void FPerfSentinelRuntimeMonitor::HandleScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
	if (PendingScreenshotPaths.Num() == 0 || Width <= 0 || Height <= 0 || Bitmap.Num() == 0)
	{
		return;
	}

	const FString TargetPath = PendingScreenshotPaths[0];
	PendingScreenshotPaths.RemoveAt(0, 1, EAllowShrinking::No);

	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	const int32 MaxDimension = Settings ? FMath::Max(0, Settings->ScreenshotMaxDimension) : 0;

	// Copy the bitmap; the source reference is only valid for the duration of this broadcast.
	TArray<FColor> BitmapCopy = Bitmap;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [BitmapCopy = MoveTemp(BitmapCopy), Width, Height, TargetPath, MaxDimension]() mutable
	{
		int32 OutWidth = Width;
		int32 OutHeight = Height;
		TArray<FColor>* PixelsPtr = &BitmapCopy;

		TArray<FColor> Resized;
		const int32 LongestEdge = FMath::Max(Width, Height);
		if (MaxDimension > 0 && LongestEdge > MaxDimension)
		{
			const float Scale = static_cast<float>(MaxDimension) / static_cast<float>(LongestEdge);
			OutWidth = FMath::Max(1, FMath::RoundToInt(Width * Scale));
			OutHeight = FMath::Max(1, FMath::RoundToInt(Height * Scale));
			Resized.SetNumUninitialized(OutWidth * OutHeight);
			FImageUtils::ImageResize(Width, Height, BitmapCopy, OutWidth, OutHeight, Resized, false, true);
			PixelsPtr = &Resized;
		}

		TArray64<uint8> PngData;
		FImageUtils::PNGCompressImageArray(OutWidth, OutHeight, TArrayView64<const FColor>(PixelsPtr->GetData(), PixelsPtr->Num()), PngData);

		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(TargetPath));
		if (!FFileHelper::SaveArrayToFile(PngData, *TargetPath))
		{
			UE_LOG(LogPerfSentinel, Warning, TEXT("PerfSentinel: Failed to write async spike screenshot: %s"), *TargetPath);
		}
	});
}

void FPerfSentinelRuntimeMonitor::HandlePostGarbageCollect()
{
	++GarbageCollectionCount;
	LastGarbageCollectPlatformSeconds = FPlatformTime::Seconds();
}

bool FPerfSentinelRuntimeMonitor::AppendSpikeEvent(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs, const FString& SpikeDirectory) const
{
	if (SpikeEventsPath.IsEmpty())
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("AppendSpikeEvent: Spike events path is empty."));
		return false;
	}

	const TSharedRef<FJsonObject> Root = BuildSpikeEventObject(Snapshot, ThresholdMs);
	Root->SetStringField(TEXT("spike_directory"), MakeProjectSavedRelativePath(SpikeDirectory));

	FString Line;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(Root, Writer);
	Line.AppendChar(TEXT('\n'));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(SpikeEventsPath));

	if (!FFileHelper::SaveStringToFile(Line, *SpikeEventsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogPerfSentinel, Error, TEXT("AppendSpikeEvent: Failed to append spike event: %s"), *SpikeEventsPath);
		return false;
	}

	WriteSpikeEventFile(Snapshot, ThresholdMs, SpikeDirectory);
	return true;
}

void FPerfSentinelRuntimeMonitor::QueueSpikeWindow(const FFrameSample& SpikeSample, const UPerfSentinelSettings& Settings, const FString& SpikeDirectory)
{
	if (!Settings.bWriteSpikeWindows)
	{
		return;
	}

	FPendingSpikeWindow Window;
	Window.SpikeFrameNumber = SpikeSample.FrameNumber;
	Window.StartSeconds = SpikeSample.PlatformSeconds - FMath::Max(0.0f, Settings.SpikeWindowPreSeconds);
	Window.EndSeconds = SpikeSample.PlatformSeconds + FMath::Max(0.0f, Settings.SpikeWindowPostSeconds);
	Window.Path = BuildSpikeWindowPath(SpikeDirectory);
	PendingSpikeWindows.Add(MoveTemp(Window));
}

void FPerfSentinelRuntimeMonitor::FlushSpikeWindows(bool bForce)
{
	if (PendingSpikeWindows.Num() == 0)
	{
		return;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	for (int32 Index = PendingSpikeWindows.Num() - 1; Index >= 0; --Index)
	{
		const FPendingSpikeWindow& Window = PendingSpikeWindows[Index];
		if (bForce || NowSeconds >= Window.EndSeconds)
		{
			WriteSpikeWindow(Window);
			PendingSpikeWindows.RemoveAtSwap(Index);
		}
	}
}

void FPerfSentinelRuntimeMonitor::WriteSpikeWindow(const FPendingSpikeWindow& Window) const
{
	FString Output;
	for (const FFrameSample& Sample : FrameSampleBuffer)
	{
		if (Sample.PlatformSeconds < Window.StartSeconds || Sample.PlatformSeconds > Window.EndSeconds)
		{
			continue;
		}

		FString Line;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
		const TSharedRef<FJsonObject> Object = BuildFrameSampleObject(Sample);
		Object->SetNumberField(TEXT("spike_frame_number"), static_cast<double>(Window.SpikeFrameNumber));
		FJsonSerializer::Serialize(Object, Writer);
		Output += Line;
		Output.AppendChar(TEXT('\n'));
	}

	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Window.Path));
	if (!FFileHelper::SaveStringToFile(Output, *Window.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("RuntimeMonitor: Failed to write spike window: %s"), *Window.Path);
	}
}

void FPerfSentinelRuntimeMonitor::PruneFrameSampleBuffer(double CurrentPlatformSeconds, float RetentionSeconds)
{
	const double OldestSeconds = CurrentPlatformSeconds - FMath::Max(0.0f, RetentionSeconds);
	int32 RemoveCount = 0;
	while (RemoveCount < FrameSampleBuffer.Num() && FrameSampleBuffer[RemoveCount].PlatformSeconds < OldestSeconds)
	{
		++RemoveCount;
	}

	if (RemoveCount > 0)
	{
		FrameSampleBuffer.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

void FPerfSentinelRuntimeMonitor::WriteRuntimeContext() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("captured_at"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("session"), SessionBaseName);
	Root->SetStringField(TEXT("project"), FApp::GetProjectName());
	Root->SetStringField(TEXT("build_configuration"), BuildConfigurationToString());
	Root->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
	Root->SetStringField(TEXT("cpu_brand"), FPlatformMisc::GetCPUBrand());
	Root->SetStringField(TEXT("gpu_brand"), FPlatformMisc::GetPrimaryGPUBrand());
	Root->SetStringField(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT(""));
	Root->SetNumberField(TEXT("frame_number"), static_cast<double>(GFrameCounter));

	UWorld* World = GetPerfSentinelWorld();
	if (World)
	{
		Root->SetStringField(TEXT("map_name"), World->GetMapName());
		Root->SetStringField(TEXT("world_type"), WorldTypeToString(World->WorldType));
	}

	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		Root->SetNumberField(TEXT("viewport_width"), ViewportSize.X);
		Root->SetNumberField(TEXT("viewport_height"), ViewportSize.Y);
	}
	else
	{
		Root->SetNumberField(TEXT("system_resolution_width"), GSystemResolution.ResX);
		Root->SetNumberField(TEXT("system_resolution_height"), GSystemResolution.ResY);
	}

	AddWorldCounterFields(World, Root);
	AddMemoryFields(Root);
	WriteJsonObjectToFile(RuntimeContextPath, Root);
}

void FPerfSentinelRuntimeMonitor::WriteGameStats() const
{
	const UPerfSentinelSettings* Settings = UPerfSentinelSettings::Get();
	if (!Settings || !Settings->bWriteGameStats)
	{
		return;
	}

	UWorld* World = GetPerfSentinelWorld();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("captured_at"), FDateTime::UtcNow().ToIso8601());
	Root->SetStringField(TEXT("session"), SessionBaseName);
	if (World)
	{
		Root->SetStringField(TEXT("map_name"), World->GetMapName());
		Root->SetStringField(TEXT("world_type"), WorldTypeToString(World->WorldType));
	}

	// Cost provenance for this file. Runtime stats are filled at Deactivate once spikes were harvested.
	const bool bHarvestAvailable = bStatHarvestEnabled && AggregatedScopes.Num() > 0;
	Root->SetStringField(TEXT("cost_source"), bHarvestAvailable
		? TEXT("runtime_stats")
		: (bStatHarvestEnabled ? TEXT("inventory_only") : TEXT("stats_unavailable")));

	// Heuristic: only blueprint-style classes are matched against harvested scope names to avoid
	// false positives from generic native class names ("Actor", "StaticMeshActor", ...).
	auto LooksLikeBlueprint = [](const FString& ClassName)
	{
		return ClassName.EndsWith(TEXT("_C"))
			|| ClassName.StartsWith(TEXT("BP_"))
			|| ClassName.StartsWith(TEXT("WBP_"))
			|| ClassName.StartsWith(TEXT("UI_"))
			|| ClassName.Contains(TEXT("Widget"));
	};

	auto ApplyCost = [this, bHarvestAvailable, &LooksLikeBlueprint](const TSharedRef<FJsonObject>& Object, const FString& ClassShortName)
	{
		double TotalMs = 0.0;
		double MaxMs = 0.0;
		int32 SpikeCount = 0;
		bool bMatched = false;

		if (bHarvestAvailable && LooksLikeBlueprint(ClassShortName))
		{
			FString Base = ClassShortName;
			Base.RemoveFromEnd(TEXT("_C"));
			if (Base.Len() >= 3)
			{
				for (const TPair<FString, FAggregatedScope>& Pair : AggregatedScopes)
				{
					if (Pair.Key.Contains(Base, ESearchCase::IgnoreCase))
					{
						TotalMs += Pair.Value.TotalInclusiveMs;
						MaxMs = FMath::Max(MaxMs, Pair.Value.MaxInclusiveMs);
						SpikeCount = FMath::Max(SpikeCount, Pair.Value.SpikeCount);
						bMatched = true;
					}
				}
			}
		}

		TArray<TSharedPtr<FJsonValue>> Evidence;
		if (bMatched)
		{
			const bool bSuspicion = MaxMs >= 1.0 || TotalMs >= 5.0 || SpikeCount >= 3;
			Object->SetNumberField(TEXT("total_time_ms"), TotalMs);
			Object->SetNumberField(TEXT("max_spike_time_ms"), MaxMs);
			Object->SetNumberField(TEXT("spike_count"), SpikeCount);
			Object->SetBoolField(TEXT("suspicion"), bSuspicion);
			Object->SetStringField(TEXT("cost_status"), TEXT("measured_runtime_stats"));
			if (bSuspicion)
			{
				if (MaxMs >= 1.0) { Evidence.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("max_spike_time_ms=%.3f"), MaxMs))); }
				if (TotalMs >= 5.0) { Evidence.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("total_time_ms=%.3f"), TotalMs))); }
				if (SpikeCount >= 3) { Evidence.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("spike_count=%d"), SpikeCount))); }
			}
		}
		else
		{
			Object->SetNumberField(TEXT("total_time_ms"), -1.0);
			Object->SetNumberField(TEXT("max_spike_time_ms"), -1.0);
			Object->SetNumberField(TEXT("spike_count"), 0);
			Object->SetBoolField(TEXT("suspicion"), false);
			Object->SetStringField(TEXT("cost_status"), bStatHarvestEnabled ? TEXT("inventory_only") : TEXT("stats_unavailable"));
		}
		Object->SetArrayField(TEXT("suspicion_evidence"), Evidence);
	};

	TArray<TSharedPtr<FJsonValue>> ActorValues;
	if (Settings->bWriteFullObjectInventory && World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}

			TSharedRef<FJsonObject> ActorObject = MakeShared<FJsonObject>();
			ActorObject->SetStringField(TEXT("name"), Actor->GetName());
#if WITH_EDITOR
			ActorObject->SetStringField(TEXT("label"), Actor->GetActorLabel());
#endif
			ActorObject->SetStringField(TEXT("path"), Actor->GetPathName());
			ActorObject->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : FString());
			ActorObject->SetStringField(TEXT("level"), Actor->GetLevel() ? Actor->GetLevel()->GetPathName() : FString());
			ActorObject->SetBoolField(TEXT("tick_enabled"), Actor->IsActorTickEnabled());
			ActorObject->SetBoolField(TEXT("hidden"), Actor->IsHidden());
			if (const AActor* Owner = Actor->GetOwner())
			{
				ActorObject->SetStringField(TEXT("owner"), Owner->GetPathName());
			}
			if (const AActor* AttachParent = Actor->GetAttachParentActor())
			{
				ActorObject->SetStringField(TEXT("attach_parent"), AttachParent->GetPathName());
			}

			const FVector Location = Actor->GetActorLocation();
			TSharedRef<FJsonObject> LocationObject = MakeShared<FJsonObject>();
			LocationObject->SetNumberField(TEXT("x"), Location.X);
			LocationObject->SetNumberField(TEXT("y"), Location.Y);
			LocationObject->SetNumberField(TEXT("z"), Location.Z);
			ActorObject->SetObjectField(TEXT("location"), LocationObject);
			ActorObject->SetNumberField(TEXT("component_count"), Actor->GetComponents().Num());
			ApplyCost(ActorObject, Actor->GetClass() ? Actor->GetClass()->GetName() : FString());

			ActorValues.Add(MakeShared<FJsonValueObject>(ActorObject));
		}
	}

	TArray<TSharedPtr<FJsonValue>> WidgetValues;
	if (Settings->bWriteFullObjectInventory)
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			const UUserWidget* Widget = *It;
			if (!IsValid(Widget))
			{
				continue;
			}

			UWorld* WidgetWorld = Widget->GetWorld();
			if (World && WidgetWorld && WidgetWorld != World)
			{
				continue;
			}

			TSharedRef<FJsonObject> WidgetObject = MakeShared<FJsonObject>();
			WidgetObject->SetStringField(TEXT("name"), Widget->GetName());
			WidgetObject->SetStringField(TEXT("path"), Widget->GetPathName());
			WidgetObject->SetStringField(TEXT("class"), Widget->GetClass() ? Widget->GetClass()->GetPathName() : FString());
			WidgetObject->SetStringField(TEXT("visibility"), SlateVisibilityToString(Widget->GetVisibility()));
			WidgetObject->SetBoolField(TEXT("is_visible"), Widget->IsVisible());
			WidgetObject->SetBoolField(TEXT("is_in_viewport"), Widget->IsInViewport());
			if (const APlayerController* OwningPlayer = Widget->GetOwningPlayer())
			{
				WidgetObject->SetStringField(TEXT("owning_player"), OwningPlayer->GetPathName());
			}
			if (WidgetWorld)
			{
				WidgetObject->SetStringField(TEXT("world"), WidgetWorld->GetPathName());
			}
			ApplyCost(WidgetObject, Widget->GetClass() ? Widget->GetClass()->GetName() : FString());

			WidgetValues.Add(MakeShared<FJsonValueObject>(WidgetObject));
		}
	}

	// Raw harvested per-scope cost, sorted by total inclusive time, regardless of actor matching.
	TArray<TPair<FString, FAggregatedScope>> SortedScopes = AggregatedScopes.Array();
	SortedScopes.Sort([](const TPair<FString, FAggregatedScope>& A, const TPair<FString, FAggregatedScope>& B)
	{
		return A.Value.TotalInclusiveMs > B.Value.TotalInclusiveMs;
	});

	TArray<TSharedPtr<FJsonValue>> ScopeValues;
	for (const TPair<FString, FAggregatedScope>& Pair : SortedScopes)
	{
		TSharedRef<FJsonObject> ScopeObject = MakeShared<FJsonObject>();
		ScopeObject->SetStringField(TEXT("scope"), Pair.Key);
		ScopeObject->SetNumberField(TEXT("total_time_ms"), Pair.Value.TotalInclusiveMs);
		ScopeObject->SetNumberField(TEXT("max_spike_time_ms"), Pair.Value.MaxInclusiveMs);
		ScopeObject->SetNumberField(TEXT("exclusive_time_ms"), Pair.Value.TotalExclusiveMs);
		ScopeObject->SetNumberField(TEXT("call_count"), static_cast<double>(Pair.Value.CallCount));
		ScopeObject->SetNumberField(TEXT("spike_count"), Pair.Value.SpikeCount);
		ScopeValues.Add(MakeShared<FJsonValueObject>(ScopeObject));
	}

	Root->SetNumberField(TEXT("actor_count"), ActorValues.Num());
	Root->SetArrayField(TEXT("actors"), ActorValues);
	Root->SetNumberField(TEXT("widget_count"), WidgetValues.Num());
	Root->SetArrayField(TEXT("widgets"), WidgetValues);
	Root->SetNumberField(TEXT("runtime_stat_count"), ScopeValues.Num());
	Root->SetArrayField(TEXT("runtime_stats"), ScopeValues);
	Root->SetNumberField(TEXT("suppressed_spike_count"), SuppressedSpikeCount);
	Root->SetNumberField(TEXT("worst_suppressed_frame_ms"), WorstSuppressedFrameMs);
	Root->SetBoolField(TEXT("full_object_inventory_collected"), Settings->bWriteFullObjectInventory);
	WriteJsonObjectToFile(GameStatsPath, Root);
}

void FPerfSentinelRuntimeMonitor::WriteSpikeEventFile(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs, const FString& SpikeDirectory) const
{
	FString EventPath = FPaths::Combine(SpikeDirectory, TEXT("event.json"));
	EventPath = FPaths::ConvertRelativePathToFull(EventPath);
	FPaths::NormalizeFilename(EventPath);
	WriteJsonObjectToFile(EventPath, BuildSpikeEventObject(Snapshot, ThresholdMs));
}

TSharedRef<FJsonObject> FPerfSentinelRuntimeMonitor::BuildFrameSampleObject(const FFrameSample& Sample) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("timestamp"), Sample.Timestamp.ToIso8601());
	Root->SetStringField(TEXT("source"), TEXT("runtime_monitor"));
	Root->SetNumberField(TEXT("platform_seconds"), Sample.PlatformSeconds);
	Root->SetNumberField(TEXT("app_seconds"), Sample.AppSeconds);
	Root->SetNumberField(TEXT("frame_number"), static_cast<double>(Sample.FrameNumber));
	Root->SetNumberField(TEXT("frame_time_ms"), Sample.FrameTimeMs);
	Root->SetNumberField(TEXT("delta_seconds"), Sample.FrameTimeMs / 1000.0f);
	Root->SetNumberField(TEXT("game_thread_ms"), Sample.GameThreadMs);
	Root->SetNumberField(TEXT("render_thread_ms"), Sample.RenderThreadMs);
	Root->SetNumberField(TEXT("rhi_thread_ms"), Sample.RhiThreadMs);
	Root->SetNumberField(TEXT("gpu_ms"), Sample.GpuMs);
	return Root;
}

TSharedRef<FJsonObject> FPerfSentinelRuntimeMonitor::BuildSpikeEventObject(const FPerfSentinelSpikeSnapshot& Snapshot, float ThresholdMs) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("timestamp"), Snapshot.Timestamp.ToIso8601());
	Root->SetNumberField(TEXT("frame_number"), static_cast<double>(Snapshot.FrameNumber));
	Root->SetNumberField(TEXT("platform_seconds"), Snapshot.PlatformSeconds);
	Root->SetNumberField(TEXT("frame_time_ms"), Snapshot.FrameTimeMs);
	Root->SetNumberField(TEXT("threshold_ms"), ThresholdMs);
	Root->SetNumberField(TEXT("game_thread_ms"), Snapshot.GameThreadTimeMs);
	Root->SetNumberField(TEXT("render_thread_ms"), Snapshot.RenderThreadTimeMs);
	Root->SetNumberField(TEXT("rhi_thread_ms"), Snapshot.RhiThreadTimeMs);
	Root->SetNumberField(TEXT("gpu_ms"), Snapshot.GpuTimeMs);
	if (!Snapshot.ScreenshotPath.IsEmpty())
	{
		Root->SetStringField(TEXT("screenshot"), Snapshot.ScreenshotPath);
		Root->SetStringField(TEXT("screenshot_status"), TEXT("requested_verify_file"));
	}
	return Root;
}

void FPerfSentinelRuntimeMonitor::WriteJsonLine(FArchive* Writer, const TSharedRef<FJsonObject>& Object)
{
	if (!Writer)
	{
		return;
	}

	FString Line;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
	FJsonSerializer::Serialize(Object, JsonWriter);
	Line.AppendChar(TEXT('\n'));

	FTCHARToUTF8 Utf8Line(*Line);
	Writer->Serialize(const_cast<ANSICHAR*>(Utf8Line.Get()), Utf8Line.Length());
}

FString FPerfSentinelRuntimeMonitor::BuildSpikeDirectory(int32 SpikeIndex, int64 SpikeFrameNumber) const
{
	FString SpikeDirectory = FPaths::Combine(
		SpikesDirectory,
		FString::Printf(TEXT("spike_%04d_frame_%lld"), SpikeIndex, SpikeFrameNumber));
	SpikeDirectory = FPaths::ConvertRelativePathToFull(SpikeDirectory);
	FPaths::NormalizeDirectoryName(SpikeDirectory);
	return SpikeDirectory;
}

FString FPerfSentinelRuntimeMonitor::BuildScreenshotPath(const FString& SpikeDirectory) const
{
	FString ScreenshotPath = FPaths::Combine(SpikeDirectory, TEXT("screenshot.png"));
	ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
	FPaths::NormalizeFilename(ScreenshotPath);
	return ScreenshotPath;
}

FString FPerfSentinelRuntimeMonitor::BuildSpikeWindowPath(const FString& SpikeDirectory) const
{
	FString SpikeWindowPath = FPaths::Combine(SpikeDirectory, TEXT("window.ndjson"));
	SpikeWindowPath = FPaths::ConvertRelativePathToFull(SpikeWindowPath);
	FPaths::NormalizeFilename(SpikeWindowPath);
	return SpikeWindowPath;
}

FString FPerfSentinelRuntimeMonitor::MakeProjectSavedRelativePath(const FString& AbsolutePath) const
{
	if (AbsolutePath.IsEmpty())
	{
		return FString();
	}

	FString RelativePath = AbsolutePath;
	FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	FPaths::NormalizeFilename(RelativePath);
	FPaths::NormalizeDirectoryName(SavedDir);
	FPaths::MakePathRelativeTo(RelativePath, *SavedDir);
	FPaths::NormalizeFilename(RelativePath);
	return RelativePath;
}

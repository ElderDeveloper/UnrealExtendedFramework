// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "PerfSentinelEventRecorder.h"

#include "PerfSentinelRuntimeMonitor.h"
#include "PerfSentinelTraceController.h"

#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Serialization/Archive.h"
#include "UObject/Package.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

namespace
{
/** Level streaming in a world-partition map can add many rows; the cap keeps a pathological capture bounded. */
constexpr int32 MaxEventRows = 50000;
constexpr int32 MaxChurnClasses = 12;

FString PackageNameOf(const UObject* Object)
{
	const UPackage* Package = Object ? Object->GetOutermost() : nullptr;
	return Package ? Package->GetName() : FString();
}

TArray<TSharedPtr<FJsonValue>> TopClasses(const TMap<FName, int32>& Counts)
{
	TArray<TPair<FName, int32>> Sorted = Counts.Array();
	Sorted.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B) { return A.Value > B.Value; });
	TArray<TSharedPtr<FJsonValue>> Values;
	for (int32 Index = 0; Index < FMath::Min(MaxChurnClasses, Sorted.Num()); ++Index)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("class"), Sorted[Index].Key.ToString());
		Entry->SetNumberField(TEXT("count"), Sorted[Index].Value);
		Values.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Values;
}
}

FPerfSentinelEventRecorder::~FPerfSentinelEventRecorder()
{
	Stop(FPlatformTime::Seconds());
}

void FPerfSentinelEventRecorder::Start(const FString& InPath, double InCaptureStartPlatformSeconds)
{
	Stop(InCaptureStartPlatformSeconds);
	Path = InPath;
	CaptureStartPlatformSeconds = InCaptureStartPlatformSeconds;
	EventCount = 0;
	DroppedEventCount = 0;
	GcStartSeconds = -1.0;
	SpawnedByClass.Reset();
	DestroyedByClass.Reset();
	SpawnedTotal = 0;
	DestroyedTotal = 0;
	LastChurnFlushSeconds = InCaptureStartPlatformSeconds;

	Writer.Reset(IFileManager::Get().CreateFileWriter(*Path));
	if (!Writer)
	{
		UE_LOG(LogPerfSentinel, Warning, TEXT("EventRecorder: Cannot open event stream: %s"), *Path);
		return;
	}

	PreGarbageCollectHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FPerfSentinelEventRecorder::HandlePreGarbageCollect);
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FPerfSentinelEventRecorder::HandlePostGarbageCollect);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddRaw(this, &FPerfSentinelEventRecorder::HandleLevelAdded);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddRaw(this, &FPerfSentinelEventRecorder::HandleLevelRemoved);
	ViewportResizedHandle = FViewport::ViewportResizedEvent.AddRaw(this, &FPerfSentinelEventRecorder::HandleViewportResized);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FPerfSentinelEventRecorder::HandlePostLoadMap);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddRaw(this, &FPerfSentinelEventRecorder::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddRaw(this, &FPerfSentinelEventRecorder::HandleTravelFailure);
	}
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddRaw(this, &FPerfSentinelEventRecorder::HandlePostLogin);
	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddRaw(this, &FPerfSentinelEventRecorder::HandleLogout);

	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("recording_started"), InCaptureStartPlatformSeconds);
	Event->SetNumberField(TEXT("max_event_rows"), MaxEventRows);
	WriteEvent(Event);
}

void FPerfSentinelEventRecorder::Stop(double NowSeconds)
{
	if (!Writer)
	{
		return;
	}

	FlushActorChurn(NowSeconds);
	UnbindWorld();
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGarbageCollectHandle);
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	FViewport::ViewportResizedEvent.Remove(ViewportResizedHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
	PreGarbageCollectHandle.Reset();
	PostGarbageCollectHandle.Reset();
	LevelAddedHandle.Reset();
	LevelRemovedHandle.Reset();
	ViewportResizedHandle.Reset();
	PostLoadMapHandle.Reset();
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();
	PostLoginHandle.Reset();
	LogoutHandle.Reset();

	// The closing row ignores the row cap so the analysis always learns how many rows were dropped.
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("recording_stopped"), NowSeconds);
	Event->SetNumberField(TEXT("event_rows"), EventCount);
	Event->SetNumberField(TEXT("dropped_event_rows"), DroppedEventCount);
	FPerfSentinelRuntimeMonitor::WriteJsonLine(Writer.Get(), Event);
	Writer.Reset();
}

void FPerfSentinelEventRecorder::Tick(UWorld* GameWorld, double NowSeconds)
{
	if (!Writer)
	{
		return;
	}
	if (GameWorld != BoundWorld.Get())
	{
		FlushActorChurn(NowSeconds);
		UnbindWorld();
		BindWorld(GameWorld);
	}
	if (NowSeconds - LastChurnFlushSeconds >= 1.0)
	{
		FlushActorChurn(NowSeconds);
	}
}

void FPerfSentinelEventRecorder::RecordFocusChange(bool bFocused, double NowSeconds)
{
	WriteEvent(MakeEvent(bFocused ? TEXT("focus_gained") : TEXT("focus_lost"), NowSeconds));
}

TSharedRef<FJsonObject> FPerfSentinelEventRecorder::MakeEvent(const TCHAR* Type, double NowSeconds) const
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event"), Type);
	Event->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
	Event->SetNumberField(TEXT("platform_seconds"), NowSeconds);
	Event->SetNumberField(TEXT("capture_elapsed_seconds"), NowSeconds - CaptureStartPlatformSeconds);
	Event->SetNumberField(TEXT("frame_number"), static_cast<double>(GFrameCounter));
	return Event;
}

void FPerfSentinelEventRecorder::WriteEvent(const TSharedRef<FJsonObject>& Event)
{
	if (!Writer)
	{
		return;
	}
	if (EventCount >= MaxEventRows)
	{
		++DroppedEventCount;
		return;
	}
	FPerfSentinelRuntimeMonitor::WriteJsonLine(Writer.Get(), Event);
	++EventCount;
}

void FPerfSentinelEventRecorder::BindWorld(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateRaw(this, &FPerfSentinelEventRecorder::HandleActorSpawned));
	ActorDestroyedHandle = World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateRaw(this, &FPerfSentinelEventRecorder::HandleActorDestroyed));
	BoundWorld = World;
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("game_world_observed"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("world"), World->GetMapName());
	Event->SetStringField(TEXT("net_mode"), World->GetNetMode() == NM_Client ? TEXT("client")
		: (World->GetNetMode() == NM_ListenServer ? TEXT("listen_server") : (World->GetNetMode() == NM_DedicatedServer ? TEXT("dedicated_server") : TEXT("standalone"))));
	WriteEvent(Event);
}

void FPerfSentinelEventRecorder::UnbindWorld()
{
	if (UWorld* World = BoundWorld.Get())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->RemoveOnActorDestroyedHandler(ActorDestroyedHandle);
	}
	ActorSpawnedHandle.Reset();
	ActorDestroyedHandle.Reset();
	BoundWorld.Reset();
}

void FPerfSentinelEventRecorder::FlushActorChurn(double NowSeconds)
{
	if (SpawnedTotal > 0 || DestroyedTotal > 0)
	{
		TSharedRef<FJsonObject> Event = MakeEvent(TEXT("actor_churn"), NowSeconds);
		Event->SetNumberField(TEXT("interval_start_platform_seconds"), LastChurnFlushSeconds);
		Event->SetNumberField(TEXT("interval_seconds"), FMath::Max(0.0, NowSeconds - LastChurnFlushSeconds));
		Event->SetNumberField(TEXT("spawned"), SpawnedTotal);
		Event->SetNumberField(TEXT("destroyed"), DestroyedTotal);
		Event->SetArrayField(TEXT("top_spawned_classes"), TopClasses(SpawnedByClass));
		Event->SetArrayField(TEXT("top_destroyed_classes"), TopClasses(DestroyedByClass));
		WriteEvent(Event);
	}
	SpawnedByClass.Reset();
	DestroyedByClass.Reset();
	SpawnedTotal = 0;
	DestroyedTotal = 0;
	LastChurnFlushSeconds = NowSeconds;
}

void FPerfSentinelEventRecorder::HandlePreGarbageCollect()
{
	GcStartSeconds = FPlatformTime::Seconds();
	GcObjectsBefore = GUObjectArray.GetObjectArrayNumMinusAvailable();
}

void FPerfSentinelEventRecorder::HandlePostGarbageCollect()
{
	if (GcStartSeconds < 0.0)
	{
		return;
	}
	const double EndSeconds = FPlatformTime::Seconds();
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("gc"), GcStartSeconds);
	Event->SetNumberField(TEXT("end_platform_seconds"), EndSeconds);
	Event->SetNumberField(TEXT("duration_ms"), (EndSeconds - GcStartSeconds) * 1000.0);
	Event->SetStringField(TEXT("duration_semantics"), TEXT("pre_to_post_garbage_collect_on_game_thread_incremental_purge_may_continue_later"));
	Event->SetNumberField(TEXT("object_slots_before"), GcObjectsBefore);
	WriteEvent(Event);
	GcStartSeconds = -1.0;
}

void FPerfSentinelEventRecorder::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("level_added"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("level"), PackageNameOf(Level));
	Event->SetStringField(TEXT("world"), World->GetMapName());
	WriteEvent(Event);
}

void FPerfSentinelEventRecorder::HandleLevelRemoved(ULevel* Level, UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("level_removed"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("level"), Level ? PackageNameOf(Level) : FString(TEXT("<all levels>")));
	Event->SetStringField(TEXT("world"), World->GetMapName());
	WriteEvent(Event);
}

void FPerfSentinelEventRecorder::HandleActorSpawned(AActor* Actor)
{
	if (Actor && Actor->GetClass())
	{
		++SpawnedByClass.FindOrAdd(Actor->GetClass()->GetFName());
		++SpawnedTotal;
	}
}

void FPerfSentinelEventRecorder::HandleActorDestroyed(AActor* Actor)
{
	if (Actor && Actor->GetClass())
	{
		++DestroyedByClass.FindOrAdd(Actor->GetClass()->GetFName());
		++DestroyedTotal;
	}
}

void FPerfSentinelEventRecorder::HandleViewportResized(FViewport* Viewport, uint32 Unused)
{
	(void)Unused;
	if (!Viewport)
	{
		return;
	}
	const FIntPoint Size = Viewport->GetSizeXY();
	const bool bGameViewport = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport == Viewport;
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("viewport_resized"), FPlatformTime::Seconds());
	Event->SetNumberField(TEXT("width"), Size.X);
	Event->SetNumberField(TEXT("height"), Size.Y);
	Event->SetBoolField(TEXT("game_viewport"), bGameViewport);
	WriteEvent(Event);
	if (bGameViewport)
	{
		TRACE_BOOKMARK(TEXT("PerfSentinel.ViewportResized %dx%d"), Size.X, Size.Y);
	}
}

void FPerfSentinelEventRecorder::HandlePostLoadMap(UWorld* World)
{
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("map_loaded"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("world"), World ? World->GetMapName() : FString());
	WriteEvent(Event);
	TRACE_BOOKMARK(TEXT("PerfSentinel.MapLoaded %s"), World ? *World->GetMapName() : TEXT(""));
}

void FPerfSentinelEventRecorder::HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Message)
{
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("network_failure"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("failure"), ENetworkFailure::ToString(FailureType));
	Event->SetStringField(TEXT("message"), Message.Left(512));
	Event->SetStringField(TEXT("net_driver"), Driver ? Driver->NetDriverName.ToString() : FString());
	Event->SetStringField(TEXT("world"), World ? World->GetMapName() : FString());
	WriteEvent(Event);
	TRACE_BOOKMARK(TEXT("PerfSentinel.NetworkFailure %s"), ENetworkFailure::ToString(FailureType));
	UE_LOG(LogPerfSentinel, Warning, TEXT("EventRecorder: Network failure %s: %s"), ENetworkFailure::ToString(FailureType), *Message.Left(512));
}

void FPerfSentinelEventRecorder::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Message)
{
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("travel_failure"), FPlatformTime::Seconds());
	Event->SetStringField(TEXT("failure"), ETravelFailure::ToString(FailureType));
	Event->SetStringField(TEXT("message"), Message.Left(512));
	Event->SetStringField(TEXT("world"), World ? World->GetMapName() : FString());
	WriteEvent(Event);
	TRACE_BOOKMARK(TEXT("PerfSentinel.TravelFailure %s"), ETravelFailure::ToString(FailureType));
}

void FPerfSentinelEventRecorder::HandlePostLogin(AGameModeBase* GameMode, APlayerController* Player)
{
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("player_login"), FPlatformTime::Seconds());
	Event->SetNumberField(TEXT("players"), GameMode ? GameMode->GetNumPlayers() : -1);
	Event->SetNumberField(TEXT("controller_id"), Player ? static_cast<double>(Player->GetUniqueID()) : -1.0);
	Event->SetBoolField(TEXT("local_controller"), Player && Player->IsLocalController());
	Event->SetStringField(TEXT("world"), GameMode && GameMode->GetWorld() ? GameMode->GetWorld()->GetMapName() : FString());
	WriteEvent(Event);
	TRACE_BOOKMARK(TEXT("PerfSentinel.PlayerLogin"));
}

void FPerfSentinelEventRecorder::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	TSharedRef<FJsonObject> Event = MakeEvent(TEXT("player_logout"), FPlatformTime::Seconds());
	Event->SetNumberField(TEXT("players"), GameMode ? GameMode->GetNumPlayers() : -1);
	Event->SetNumberField(TEXT("controller_id"), Exiting ? static_cast<double>(Exiting->GetUniqueID()) : -1.0);
	Event->SetStringField(TEXT("world"), GameMode && GameMode->GetWorld() ? GameMode->GetWorld()->GetMapName() : FString());
	WriteEvent(Event);
	TRACE_BOOKMARK(TEXT("PerfSentinel.PlayerLogout"));
}

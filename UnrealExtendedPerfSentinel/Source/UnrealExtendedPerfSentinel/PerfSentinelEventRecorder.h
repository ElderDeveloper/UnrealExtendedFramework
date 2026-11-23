// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class AController;
class AGameModeBase;
class APlayerController;
class FArchive;
class FJsonObject;
class FViewport;
class ULevel;
class UNetDriver;
class UWorld;

/**
 * Records discrete engine events during a capture into <session>_events.ndjson: garbage collections with their
 * duration, levels added to and removed from game worlds, actor spawn/destroy churn per class, viewport resizes,
 * map loads, focus changes, network and travel failures, and player logins and logouts.
 *
 * Rows carry platform_seconds. The analyzer maps them onto trace time with the PerfSentinel.CaptureStart
 * bookmark, whose platform time the metadata records. Every engine callback used here runs on the game thread.
 */
class FPerfSentinelEventRecorder
{
public:
	~FPerfSentinelEventRecorder();

	void Start(const FString& InPath, double InCaptureStartPlatformSeconds);
	void Stop(double NowSeconds);

	/** Writes the accumulated actor churn and follows the game world when it changes. Called about once per second. */
	void Tick(UWorld* GameWorld, double NowSeconds);
	void RecordFocusChange(bool bFocused, double NowSeconds);

	bool IsRecording() const { return Writer.IsValid(); }
	const FString& GetPath() const { return Path; }
	int32 GetEventCount() const { return EventCount; }
	int32 GetDroppedEventCount() const { return DroppedEventCount; }

private:
	TSharedRef<FJsonObject> MakeEvent(const TCHAR* Type, double NowSeconds) const;
	void WriteEvent(const TSharedRef<FJsonObject>& Event);
	void BindWorld(UWorld* World);
	void UnbindWorld();
	void FlushActorChurn(double NowSeconds);

	void HandlePreGarbageCollect();
	void HandlePostGarbageCollect();
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void HandleLevelRemoved(ULevel* Level, UWorld* World);
	void HandleActorSpawned(AActor* Actor);
	void HandleActorDestroyed(AActor* Actor);
	void HandleViewportResized(FViewport* Viewport, uint32 Unused);
	void HandlePostLoadMap(UWorld* World);
	void HandleNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type FailureType, const FString& Message);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Message);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* Player);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);

	FString Path;
	TUniquePtr<FArchive> Writer;
	double CaptureStartPlatformSeconds = 0.0;
	int32 EventCount = 0;
	int32 DroppedEventCount = 0;

	double GcStartSeconds = -1.0;
	int32 GcObjectsBefore = 0;

	TWeakObjectPtr<UWorld> BoundWorld;
	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle ActorDestroyedHandle;
	TMap<FName, int32> SpawnedByClass;
	TMap<FName, int32> DestroyedByClass;
	int32 SpawnedTotal = 0;
	int32 DestroyedTotal = 0;
	double LastChurnFlushSeconds = 0.0;

	FDelegateHandle PreGarbageCollectHandle;
	FDelegateHandle PostGarbageCollectHandle;
	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
	FDelegateHandle ViewportResizedHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
};

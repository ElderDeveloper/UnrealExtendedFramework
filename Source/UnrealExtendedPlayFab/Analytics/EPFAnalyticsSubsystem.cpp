// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAnalyticsSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Auth/EPFAuthSubsystem.h"
#include "Shared/EPFSettings.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameModeBase.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

FString UEPFAnalyticsSubsystem::GetQueueFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("EPF") / TEXT("AnalyticsQueue.json");
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EventsLoggedCount = 0;
	SessionStartTime  = FDateTime::UtcNow();

	const UEPFSettings* Settings = GetSettings();
	bAutoAnalyticsEnabled = Settings && Settings->bAutoAnalyticsEnabled;

	// Always load any previously persisted offline events from disk.
	LoadQueueFromDisk();

	if (bAutoAnalyticsEnabled)
	{
		RegisterAutoTrackingHooks();

		if (Settings && Settings->AutoAnalyticsConfig.bTrackSessionStart)
		{
			TMap<FString, FString> Body;
			Body.Add(TEXT("Platform"),     FString(FPlatformProperties::PlatformName()));
			Body.Add(TEXT("Timestamp"),    SessionStartTime.ToIso8601());
			DispatchOrQueue(TEXT("session_start"), Body);
		}
	}
}

void UEPFAnalyticsSubsystem::Deinitialize()
{
	if (bAutoAnalyticsEnabled)
	{
		const UEPFSettings* Settings = GetSettings();
		if (Settings && Settings->AutoAnalyticsConfig.bTrackSessionEnd && IsAuthenticated())
		{
			const FDateTime Now = FDateTime::UtcNow();
			const FTimespan Duration = Now - SessionStartTime;

			TMap<FString, FString> Body;
			Body.Add(TEXT("SessionDurationSeconds"), FString::FromInt(FMath::FloorToInt(Duration.GetTotalSeconds())));
			Body.Add(TEXT("EventsLogged"),           FString::FromInt(EventsLoggedCount));
			LogEvent(TEXT("session_end"), Body);
		}

		UnregisterAutoTrackingHooks();
	}

	Super::Deinitialize();
}

// ── Auto Tracking Toggle ──────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::SetAutoAnalyticsEnabled(bool bEnabled)
{
	if (bAutoAnalyticsEnabled == bEnabled)
	{
		return;
	}

	bAutoAnalyticsEnabled = bEnabled;

	if (bEnabled)
	{
		RegisterAutoTrackingHooks();
	}
	else
	{
		UnregisterAutoTrackingHooks();
	}

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAnalyticsSubsystem — Auto analytics %s"),
		bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

bool UEPFAnalyticsSubsystem::IsAutoAnalyticsEnabled() const
{
	return bAutoAnalyticsEnabled;
}

// ── Hook Registration ─────────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::RegisterAutoTrackingHooks()
{
	const UEPFSettings* Settings = GetSettings();
	const FEPFAutoAnalyticsConfig& Cfg = Settings ? Settings->AutoAnalyticsConfig : FEPFAutoAnalyticsConfig();

	// ── PlayFab auth (login / logout) ─────────────────────────────────────────
	// OnLoginComplete / OnLogoutComplete are DYNAMIC multicast delegates (BlueprintAssignable).
	// They require AddDynamic (not AddUObject) and the callback must be UFUNCTION.
	if (UEPFAuthSubsystem* Auth = GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>())
	{
		Auth->OnLoginComplete.AddUniqueDynamic(this,  &UEPFAnalyticsSubsystem::OnPlayFabLoginCompleted);
		Auth->OnLogoutComplete.AddUniqueDynamic(this, &UEPFAnalyticsSubsystem::OnPlayFabLogoutCompleted);
	}

	// ── Level transitions ─────────────────────────────────────────────────────
	PreLoadMapHandle  = FCoreUObjectDelegates::PreLoadMap.AddUObject(this,          &UEPFAnalyticsSubsystem::OnPreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UEPFAnalyticsSubsystem::OnPostLoadMap);

	// ── App focus ─────────────────────────────────────────────────────────────
	AppDeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UEPFAnalyticsSubsystem::OnAppDeactivated);
	AppActivateHandle   = FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UEPFAnalyticsSubsystem::OnAppActivated);

	// ── Network failure ───────────────────────────────────────────────────────
	if (Cfg.bTrackNetworkFailure && GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddWeakLambda(this,
			[this](UWorld* /*World*/, UNetDriver* /*Driver*/, ENetworkFailure::Type FailureType, const FString& ErrorString)
			{
				const UEPFSettings* S = GetSettings();
				if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackNetworkFailure) return;

				TMap<FString, FString> Body;
				Body.Add(TEXT("FailureType"), ENetworkFailure::ToString(FailureType));
				Body.Add(TEXT("Error"),       ErrorString);
				DispatchOrQueue(TEXT("network_failure"), Body);
			});
	}

	// ── Crash detection ───────────────────────────────────────────────────────
	// In a crash context we MUST NOT make a network call.
	// Write directly to the offline queue on disk — it will be sent on the next session.
	if (Cfg.bTrackCrash)
	{
		CrashHandle = FCoreDelegates::OnHandleSystemError.AddLambda([this]()
		{
			const UEPFSettings* S = GetSettings();
			if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackCrash) return;

			FEPFQueuedEvent Queued;
			Queued.EventName  = TEXT("crash_detected");
			Queued.Timestamp  = FDateTime::UtcNow().ToIso8601();
			Queued.Body.Add(TEXT("Platform"), FString(FPlatformProperties::PlatformName()));
			OfflineQueue.Add(MoveTemp(Queued));
			SaveQueueToDisk(); // Best-effort — writes before the process dies
		});
	}

	// ── Low memory ────────────────────────────────────────────────────────────
	if (Cfg.bTrackLowMemory)
	{
		LowMemoryHandle = FCoreDelegates::ApplicationShouldUnloadResourcesDelegate.AddWeakLambda(this, [this]()
		{
			const UEPFSettings* S = GetSettings();
			if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackLowMemory) return;
			DispatchOrQueue(TEXT("low_memory_warning"), {});
		});
	}

	// ── Multiplayer player connections (server / listen-server only) ───────────
	if (Cfg.bTrackPlayerConnections)
	{
		PlayerJoinedHandle = FGameModeEvents::GameModePostLoginEvent.AddWeakLambda(this,
			[this](AGameModeBase* /*GameMode*/, APlayerController* PC)
			{
				const UEPFSettings* S = GetSettings();
				if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackPlayerConnections) return;

				TMap<FString, FString> Body;
				if (PC)
				{
					Body.Add(TEXT("PlayerName"), PC->GetName());
				}
				DispatchOrQueue(TEXT("player_joined"), Body);
			});

		PlayerLeftHandle = FGameModeEvents::GameModeLogoutEvent.AddWeakLambda(this,
			[this](AGameModeBase* /*GameMode*/, AController* Controller)
			{
				const UEPFSettings* S = GetSettings();
				if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackPlayerConnections) return;

				TMap<FString, FString> Body;
				if (Controller)
				{
					Body.Add(TEXT("PlayerName"), Controller->GetName());
				}
				DispatchOrQueue(TEXT("player_left"), Body);
			});
	}

	// ── Input device (gamepad connected / disconnected) ────────────────────────
	// FCoreDelegates::OnUserInputDeviceConnectionChange was removed in UE5.
	// The delegate now lives on IPlatformInputDeviceMapper.
	if (Cfg.bTrackInputDeviceChanged)
	{
		InputDeviceHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,
			[this](EInputDeviceConnectionState ConnectionState, FPlatformUserId /*UserId*/, FInputDeviceId /*DeviceId*/)
			{
				const UEPFSettings* S = GetSettings();
				if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackInputDeviceChanged) return;

				const bool bConnected = (ConnectionState == EInputDeviceConnectionState::Connected);
				TMap<FString, FString> Body;
				Body.Add(TEXT("Connected"), bConnected ? TEXT("true") : TEXT("false"));
				DispatchOrQueue(TEXT("input_device_changed"), Body);
			});
	}

	// ── FPS sampling (recurring ticker) ──────────────────────────────────────
	if (Cfg.bTrackFrameRate)
	{
		const float Interval = FMath::Max(Cfg.FrameRateSampleIntervalSeconds, 30.0f);
		FpsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) -> bool
			{
				const UEPFSettings* S = GetSettings();
				if (!bAutoAnalyticsEnabled || !S || !S->AutoAnalyticsConfig.bTrackFrameRate)
				{
					return false; // Stop ticking if disabled at runtime
				}
				// Use FApp::GetDeltaTime() for current frame time (GAverageFPS is not reliably exported)
				const float FrameDelta = FApp::GetDeltaTime();
				const float FPS = (FrameDelta > SMALL_NUMBER) ? (1.0f / FrameDelta) : 0.0f;
				TMap<FString, FString> Body;
				Body.Add(TEXT("AverageFPS"), FString::Printf(TEXT("%.1f"), FPS));
				DispatchOrQueue(TEXT("fps_sample"), Body);
				return true; // Keep ticking
			}),
			Interval
		);
	}

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAnalyticsSubsystem — Auto-tracking hooks registered"));
}

void UEPFAnalyticsSubsystem::UnregisterAutoTrackingHooks()
{
	// PlayFab auth — RemoveDynamic mirrors AddDynamic (no FDelegateHandle involved)
	if (UEPFAuthSubsystem* Auth = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEPFAuthSubsystem>() : nullptr)
	{
		Auth->OnLoginComplete.RemoveDynamic(this,  &UEPFAnalyticsSubsystem::OnPlayFabLoginCompleted);
		Auth->OnLogoutComplete.RemoveDynamic(this, &UEPFAnalyticsSubsystem::OnPlayFabLogoutCompleted);
	}

	// Level transitions
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	// App focus
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(AppDeactivateHandle);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(AppActivateHandle);

	// Engine-level hooks
	if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkFailureHandle); }
	FCoreDelegates::OnHandleSystemError.Remove(CrashHandle);
	FCoreDelegates::ApplicationShouldUnloadResourcesDelegate.Remove(LowMemoryHandle);
	FGameModeEvents::GameModePostLoginEvent.Remove(PlayerJoinedHandle);
	FGameModeEvents::GameModeLogoutEvent.Remove(PlayerLeftHandle);
	// Input device mapper
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(InputDeviceHandle);

	// FPS ticker
	if (FpsTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FpsTickerHandle);
	}

	// Reset all handles
	PreLoadMapHandle    = FDelegateHandle();
	PostLoadMapHandle   = FDelegateHandle();
	AppDeactivateHandle = FDelegateHandle();
	AppActivateHandle   = FDelegateHandle();
	NetworkFailureHandle= FDelegateHandle();
	CrashHandle         = FDelegateHandle();
	LowMemoryHandle     = FDelegateHandle();
	PlayerJoinedHandle  = FDelegateHandle();
	PlayerLeftHandle    = FDelegateHandle();
	InputDeviceHandle   = FDelegateHandle();
	FpsTickerHandle     = FTSTicker::FDelegateHandle();

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAnalyticsSubsystem — Auto-tracking hooks unregistered"));
}

// ── Auto-Tracking Callbacks ───────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::OnPlayFabLoginCompleted(const FEPFResult& Result, const FString& PlayFabId)
{
	if (!Result.bSuccess) return;

	const UEPFSettings* Settings = GetSettings();
	if (!Settings) return;

	if (Settings->AutoAnalyticsConfig.bTrackLogin)
	{
		TMap<FString, FString> Body;
		Body.Add(TEXT("PlayFabId"), PlayFabId);
		LogEvent(TEXT("player_login"), Body);
	}

	if (Settings->AutoAnalyticsConfig.bTrackDeviceInfo)
	{
		ReportDeviceInfo();
	}

	// Authenticated — flush any events that were queued offline.
	FlushOfflineQueue();
}

void UEPFAnalyticsSubsystem::OnPlayFabLogoutCompleted()
{
	const UEPFSettings* Settings = GetSettings();
	if (Settings && Settings->AutoAnalyticsConfig.bTrackLogout)
	{
		// Player is about to lose the session token; log synchronously before it clears.
		LogSimpleEvent(TEXT("player_logout"));
	}
}

void UEPFAnalyticsSubsystem::OnPreLoadMap(const FString& MapName)
{
	const UEPFSettings* Settings = GetSettings();
	if (Settings && Settings->AutoAnalyticsConfig.bTrackLevelChange)
	{
		TMap<FString, FString> Body;
		Body.Add(TEXT("MapName"), MapName);
		DispatchOrQueue(TEXT("level_change_started"), Body);
	}
}

void UEPFAnalyticsSubsystem::OnPostLoadMap(UWorld* World)
{
	const UEPFSettings* Settings = GetSettings();
	if (Settings && Settings->AutoAnalyticsConfig.bTrackLevelChange && World)
	{
		TMap<FString, FString> Body;
		Body.Add(TEXT("MapName"), World->GetMapName());
		DispatchOrQueue(TEXT("level_loaded"), Body);
	}
}

void UEPFAnalyticsSubsystem::OnAppDeactivated()
{
	const UEPFSettings* Settings = GetSettings();
	if (Settings && Settings->AutoAnalyticsConfig.bTrackAppBackground)
	{
		DispatchOrQueue(TEXT("app_backgrounded"), {});
	}
}

void UEPFAnalyticsSubsystem::OnAppActivated()
{
	const UEPFSettings* Settings = GetSettings();
	if (Settings && Settings->AutoAnalyticsConfig.bTrackAppForeground)
	{
		DispatchOrQueue(TEXT("app_foregrounded"), {});
	}
}

// ── Dispatch or Queue ─────────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::DispatchOrQueue(const FString& EventName, const TMap<FString, FString>& Body)
{
	if (IsAuthenticated())
	{
		LogEvent(EventName, Body);
		return;
	}

	// Not online — push to the persistent offline queue.
	const UEPFSettings* Settings = GetSettings();
	const int32 QueueLimit = Settings ? Settings->OfflineQueueLimit : 100;

	// Drop the oldest entry if we're at capacity.
	if (OfflineQueue.Num() >= QueueLimit)
	{
		OfflineQueue.RemoveAt(0);
		UE_LOG(LogExtendedPlayFab, Warning,
			TEXT("EPFAnalyticsSubsystem — Offline queue full (%d), dropping oldest event"), QueueLimit);
	}

	FEPFQueuedEvent Queued;
	Queued.EventName  = EventName;
	Queued.Body       = Body;
	Queued.Timestamp  = FDateTime::UtcNow().ToIso8601();
	OfflineQueue.Add(MoveTemp(Queued));

	SaveQueueToDisk();

	UE_LOG(LogExtendedPlayFab, Verbose,
		TEXT("EPFAnalyticsSubsystem — Queued offline event '%s' (%d in queue)"), *EventName, OfflineQueue.Num());
}

// ── Offline Queue Actions ─────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::FlushOfflineQueue()
{
	if (OfflineQueue.IsEmpty())
	{
		return;
	}

	if (!IsAuthenticated())
	{
		UE_LOG(LogExtendedPlayFab, Warning,
			TEXT("EPFAnalyticsSubsystem — FlushOfflineQueue called but player is not authenticated; skipping."));
		return;
	}

	UE_LOG(LogExtendedPlayFab, Log,
		TEXT("EPFAnalyticsSubsystem — Flushing %d offline events"), OfflineQueue.Num());

	// Snapshot and clear so any new events during flush go to a fresh queue.
	TArray<FEPFQueuedEvent> Snapshot = MoveTemp(OfflineQueue);
	OfflineQueue.Reset();
	SaveQueueToDisk(); // write empty file immediately

	for (const FEPFQueuedEvent& Queued : Snapshot)
	{
		TMap<FString, FString> BodyWithMeta = Queued.Body;
		BodyWithMeta.Add(TEXT("queued_at"), Queued.Timestamp);
		LogEvent(Queued.EventName, BodyWithMeta);
	}

	// Broadcast a single flush-complete notification using the last dispatched state.
	OnOfflineQueueFlushed.Broadcast(FEPFResult::Success());
}

void UEPFAnalyticsSubsystem::ClearOfflineQueue()
{
	const int32 Dropped = OfflineQueue.Num();
	OfflineQueue.Reset();
	SaveQueueToDisk();

	UE_LOG(LogExtendedPlayFab, Log,
		TEXT("EPFAnalyticsSubsystem — Cleared offline queue (%d events discarded)"), Dropped);
}

int32 UEPFAnalyticsSubsystem::GetOfflineQueueCount() const
{
	return OfflineQueue.Num();
}

// ── Disk Persistence ──────────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::SaveQueueToDisk() const
{
	TArray<TSharedPtr<FJsonValue>> EventArray;

	for (const FEPFQueuedEvent& Queued : OfflineQueue)
	{
		TSharedRef<FJsonObject> EventObj = MakeShared<FJsonObject>();
		EventObj->SetStringField(TEXT("EventName"), Queued.EventName);
		EventObj->SetStringField(TEXT("Timestamp"),  Queued.Timestamp);

		TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Queued.Body)
		{
			BodyObj->SetStringField(Pair.Key, Pair.Value);
		}
		EventObj->SetObjectField(TEXT("Body"), BodyObj);

		EventArray.Add(MakeShared<FJsonValueObject>(EventObj));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("events"), EventArray);

	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (FJsonSerializer::Serialize(Root, Writer))
	{
		const FString FilePath = GetQueueFilePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
		if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
		{
			UE_LOG(LogExtendedPlayFab, Warning,
				TEXT("EPFAnalyticsSubsystem — Failed to save offline queue to: %s"), *FilePath);
		}
	}
}

void UEPFAnalyticsSubsystem::LoadQueueFromDisk()
{
	const FString FilePath = GetQueueFilePath();

	if (!FPaths::FileExists(FilePath))
	{
		return; // Nothing to load yet.
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogExtendedPlayFab, Warning,
			TEXT("EPFAnalyticsSubsystem — Failed to read offline queue from: %s"), *FilePath);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogExtendedPlayFab, Warning,
			TEXT("EPFAnalyticsSubsystem — Offline queue file is corrupt and will be discarded."));
		IFileManager::Get().Delete(*FilePath);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* EventArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), EventArray))
	{
		return;
	}

	OfflineQueue.Reset();

	for (const TSharedPtr<FJsonValue>& Value : *EventArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Value->TryGetObject(ObjPtr) || !ObjPtr) continue;

		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FEPFQueuedEvent Queued;
		Obj->TryGetStringField(TEXT("EventName"), Queued.EventName);
		Obj->TryGetStringField(TEXT("Timestamp"),  Queued.Timestamp);

		const TSharedPtr<FJsonObject>* BodyObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("Body"), BodyObjPtr) && BodyObjPtr)
		{
			for (const auto& Pair : (*BodyObjPtr)->Values)
			{
				FString StrVal;
				if (Pair.Value->TryGetString(StrVal))
				{
					Queued.Body.Add(Pair.Key, StrVal);
				}
			}
		}

		if (!Queued.EventName.IsEmpty())
		{
			OfflineQueue.Add(MoveTemp(Queued));
		}
	}

	if (OfflineQueue.Num() > 0)
	{
		UE_LOG(LogExtendedPlayFab, Log,
			TEXT("EPFAnalyticsSubsystem — Loaded %d offline events from disk"), OfflineQueue.Num());
	}
}

// ── Manual Event Actions ──────────────────────────────────────────────────────

void UEPFAnalyticsSubsystem::LogEvent(const FString& EventName, const TMap<FString, FString>& Body)
{
	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body)
	{
		BodyObj->SetStringField(Pair.Key, Pair.Value);
	}
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(
		TEXT("/Client/WritePlayerEvent"),
		RequestBody,
		true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject> Response)
		{
			if (Result.bSuccess)
			{
				EventsLoggedCount++;
				UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalyticsSubsystem — Event logged: %s"), *EventName);
			}
			else
			{
				UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFAnalyticsSubsystem — Failed to log event: %s"), *EventName);
			}
			OnEventLogged.Broadcast(Result);
		})
	);
}

void UEPFAnalyticsSubsystem::LogSimpleEvent(const FString& EventName)
{
	TMap<FString, FString> EmptyBody;
	LogEvent(EventName, EmptyBody);
}

void UEPFAnalyticsSubsystem::LogEvents(const TArray<FEPFAnalyticsEvent>& Events)
{
	for (const FEPFAnalyticsEvent& Event : Events)
	{
		LogEvent(Event.EventName, Event.Body);
	}
}

void UEPFAnalyticsSubsystem::LogCharacterEvent(const FString& CharacterId, const FString& EventName, const TMap<FString, FString>& Body)
{
	if (CharacterId.IsEmpty() || EventName.IsEmpty()) { OnEventLogged.Broadcast(FEPFResult::Failure(TEXT("CharacterId and EventName are required"))); return; }

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("CharacterId"), CharacterId);
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body) BodyObj->SetStringField(Pair.Key, Pair.Value);
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(TEXT("/Client/WriteCharacterEvent"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) { EventsLoggedCount++; UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalytics — Character event: %s"), *EventName); }
			OnEventLogged.Broadcast(Result);
		}));
}

void UEPFAnalyticsSubsystem::LogTitleEvent(const FString& EventName, const TMap<FString, FString>& Body)
{
	if (EventName.IsEmpty()) { OnEventLogged.Broadcast(FEPFResult::Failure(TEXT("EventName cannot be empty"))); return; }

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("EventName"), EventName);

	TSharedRef<FJsonObject> BodyObj = MakeShared<FJsonObject>();
	for (const auto& Pair : Body) BodyObj->SetStringField(Pair.Key, Pair.Value);
	RequestBody->SetObjectField(TEXT("Body"), BodyObj);

	SendPlayFabRequestDetailed(TEXT("/Client/WriteTitleEvent"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this, EventName](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) { EventsLoggedCount++; UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFAnalytics — Title event: %s"), *EventName); }
			OnEventLogged.Broadcast(Result);
		}));
}

void UEPFAnalyticsSubsystem::ReportDeviceInfo()
{
	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();

	TSharedRef<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("OS"),     FPlatformMisc::GetOSVersion());
	InfoObj->SetStringField(TEXT("Device"), FPlatformProperties::PlatformName());
	RequestBody->SetObjectField(TEXT("Info"), InfoObj);

	SendPlayFabRequestDetailed(TEXT("/Client/ReportDeviceInfo"), RequestBody, true,
		FOnPlayFabResponseDetailed::CreateLambda([this](const FEPFResult& Result, TSharedPtr<FJsonObject>)
		{
			if (Result.bSuccess) UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFAnalytics — Device info reported"));
			OnEventLogged.Broadcast(Result);
		}));
}

// ── Queries ───────────────────────────────────────────────────────────────────

int32 UEPFAnalyticsSubsystem::GetEventsLoggedCount() const
{
	return EventsLoggedCount;
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAnalyticsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFEventLogged, const FEPFResult&, Result);

/**
 * Logs PlayFab analytics as telemetry events.
 * All events share one namespace and one payload envelope so gameplay and
 * lifecycle telemetry can be queried the same way in PlayFab.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAnalyticsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Telemetry Actions ─────────────────────────────────────────────────────

	/** Log a single telemetry event */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Log a telemetry event with no custom body */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogSimpleEvent(const FString& EventName);

	/** Log a batch of telemetry events */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvents(const TArray<FEPFAnalyticsEvent>& Events);

	/** Log a telemetry event tagged with a character id */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogCharacterEvent(const FString& CharacterId, const FString& EventName, const TMap<FString, FString>& Body);

	/** Log a telemetry event tagged as title scope */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogTitleEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Log device information as telemetry */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void ReportDeviceInfo();

	// ── Auto Tracking ─────────────────────────────────────────────────────────

	/**
	 * Enable or disable auto-tracking at runtime.
	 * This overrides the Project Settings value for the current session only.
	 * When enabling, hooks are registered immediately; when disabling, they are unregistered.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void SetAutoAnalyticsEnabled(bool bEnabled);

	/** Returns true if auto-tracking is currently active */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Analytics")
	bool IsAutoAnalyticsEnabled() const;

	// ── Offline Queue ─────────────────────────────────────────────────────────

	/** Returns the number of events currently waiting in the offline queue */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Analytics")
	int32 GetOfflineQueueCount() const;

	/**
	 * Flush all queued offline events to PlayFab now.
	 * Only works when the player is authenticated. Called automatically on login.
	 */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void FlushOfflineQueue();

	/** Discard the offline queue without sending any events */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void ClearOfflineQueue();

	// ── Queries ───────────────────────────────────────────────────────────────

	/** Get the total number of events successfully logged this session */
	UFUNCTION(BlueprintPure, Category = "PlayFab|Analytics")
	int32 GetEventsLoggedCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Fired when any event (auto or manual) succeeds or fails */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Analytics")
	FOnEPFEventLogged OnEventLogged;

	/** Fired once after FlushOfflineQueue() completes (success or partial) */
	UPROPERTY(BlueprintAssignable, Category = "PlayFab|Analytics")
	FOnEPFEventLogged OnOfflineQueueFlushed;

private:

	// ── State ─────────────────────────────────────────────────────────────────

	int32 EventsLoggedCount = 0;
	bool bAutoAnalyticsEnabled = false;

	/** UTC time the subsystem was initialized — used for session_end duration */
	FDateTime SessionStartTime;

	/** Events captured while offline, persisted to disk */
	TArray<FEPFQueuedEvent> OfflineQueue;

	// ── Lifecycle Hook Handles ────────────────────────────────────────────────
	// Note: Login/Logout use DYNAMIC delegates — no FDelegateHandle; unbound via RemoveDynamic

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle AppDeactivateHandle;
	FDelegateHandle AppActivateHandle;

	// Engine-level auto-tracking handles
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle CrashHandle;
	FDelegateHandle LowMemoryHandle;
	FDelegateHandle PlayerJoinedHandle;
	FDelegateHandle PlayerLeftHandle;
	FDelegateHandle InputDeviceHandle;
	FTSTicker::FDelegateHandle FpsTickerHandle; // FTSTicker returns its own handle type

	// ── Internal Helpers ─────────────────────────────────────────────────────

	/** Register all auto-tracking engine/subsystem delegates */
	void RegisterAutoTrackingHooks();

	/** Unregister all auto-tracking delegates */
	void UnregisterAutoTrackingHooks();

	/** Write a single telemetry event using the normalized payload schema. */
	void WriteTelemetryEvent(
		const FString& EventName,
		const TMap<FString, FString>& Body,
		const FString& EventSource,
		const FString& EventScope,
		const FString& TargetId = FString(),
		const FString& OriginalTimestamp = FString());

	/**
	 * If an entity token is available, send the auto-tracked event immediately via telemetry.
	 * Otherwise, push it into the offline queue (respecting OfflineQueueLimit)
	 * and save the queue to disk.
	 */
	void DispatchOrQueue(const FString& EventName, const TMap<FString, FString>& Body);

	/** Persist the current offline queue to Saved/EPF/AnalyticsQueue.json */
	void SaveQueueToDisk() const;

	/** Load the persisted offline queue from disk during Initialize() */
	void LoadQueueFromDisk();

	/** Returns the absolute path to the offline queue file */
	static FString GetQueueFilePath();

	// ── Auto-Tracking Callbacks ───────────────────────────────────────────────
	// Login/Logout MUST be UFUNCTION because OnLoginComplete/OnLogoutComplete
	// are DECLARE_DYNAMIC_MULTICAST_DELEGATE (Blueprint-assignable) and require
	// AddDynamic/RemoveDynamic, which need reflected function names.

	UFUNCTION()
	void OnPlayFabLoginCompleted(const FEPFResult& Result, const FString& PlayFabId);

	UFUNCTION()
	void OnPlayFabLogoutCompleted();

	void OnPreLoadMap(const FString& MapName);
	void OnPostLoadMap(UWorld* World);
	void OnAppDeactivated();
	void OnAppActivated();
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Shared/EPFSubsystem.h"
#include "EPFAnalyticsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFEventLogged, const FEPFResult&, Result);

/**
 * Logs PlayStream analytics events — replaces GameAnalytics.
 * Events appear in the PlayFab dashboard and feed into segmentation, A/B testing, and reports.
 *
 * Auto-tracking (optional, toggled via Project Settings → Extended PlayFab → Auto Analytics):
 *  - Hooks into login/logout, level transitions, and app lifecycle automatically.
 *  - Events generated while offline are persisted to Saved/EPF/AnalyticsQueue.json and
 *    flushed to PlayFab the next time the player successfully logs in.
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAnalyticsSubsystem : public UEPFSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Manual Event Actions ──────────────────────────────────────────────────

	/** Log a single player event */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Log an event with no custom body (just the event name) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogSimpleEvent(const FString& EventName);

	/** Log a batch of events */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogEvents(const TArray<FEPFAnalyticsEvent>& Events);

	/** Log a character-scoped event */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogCharacterEvent(const FString& CharacterId, const FString& EventName, const TMap<FString, FString>& Body);

	/** Log a title-scoped event (global, not player-specific) */
	UFUNCTION(BlueprintCallable, Category = "PlayFab|Analytics")
	void LogTitleEvent(const FString& EventName, const TMap<FString, FString>& Body);

	/** Report device information to PlayFab (OS, device model, etc.) */
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

	/**
	 * If the player is authenticated, send the event immediately via LogEvent().
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

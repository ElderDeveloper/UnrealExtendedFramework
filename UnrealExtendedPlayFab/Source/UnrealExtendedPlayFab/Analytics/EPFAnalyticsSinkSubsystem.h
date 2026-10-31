// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Analytics/EFAnalyticsTypes.h"
#include "Analytics/EPFAnalyticsSubsystem.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EPFAnalyticsSinkSubsystem.generated.h"

class UEFAnalyticsSubsystem;

/**
 * The "PlayFab" analytics sink: delivers events recorded with the Extended analytics subsystem
 * (UEFAnalyticsSubsystem) to PlayFab as telemetry, through UEPFAnalyticsSubsystem::WriteEvents.
 *
 * - Ready while the player is logged in with an entity token. Until then the analytics subsystem
 *   holds the events and hands them over after the login.
 * - Event names become what PlayFab accepts (SanitizeEventName). Attributes become the payload's
 *   "properties"; session_id, sequence, user_id and, when the name had to change, original_name
 *   sit beside them. event_source is "extended", which tells these apart from the plugin's own
 *   Auto Analytics ("auto") and LogEvent ("manual") events.
 * - Up to UEPFAnalyticsSubsystem::MaxEventsPerWrite events per request. A batch that fails with a
 *   retryable error (connection, 429, 5xx, after the request layer's own retries) is sent again
 *   after a growing delay; other failures drop the batch. At most MaxEventsHeld events wait for a
 *   retry; the oldest go first. Nothing is written to disk.
 *
 * Create it with MakeShared: it hands weak references to itself to its callbacks. Holds the
 * PlayFab analytics subsystem weakly. Game thread only.
 */
class UNREALEXTENDEDPLAYFAB_API FEPFAnalyticsSink : public IEFAnalyticsSink, public TSharedFromThis<FEPFAnalyticsSink>
{
public:
	/** PlayFab's limit for an event name. */
	static constexpr int32 MaxEventNameLength = 64;

	/** Events kept for a retry at most. */
	static constexpr int32 MaxEventsHeld = 1000;

	static constexpr float MinRetryDelaySeconds = 5.0f;
	static constexpr float MaxRetryDelaySeconds = 300.0f;

	explicit FEPFAnalyticsSink(UEPFAnalyticsSubsystem* InPlayFabAnalytics);
	virtual ~FEPFAnalyticsSink() override;

	/** "PlayFab". */
	static FName StaticSinkName();

	// IEFAnalyticsSink
	virtual FName GetSinkName() const override { return StaticSinkName(); }
	virtual bool IsReady() const override;
	virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) override;
	virtual void Flush(bool bShuttingDown) override;
	virtual void DiscardPending() override;
	virtual FString DescribeStatus() const override;

	/** Stops sending and retrying and lets go of the PlayFab subsystem. Called when the game instance ends. */
	void Shutdown();

	/** Events waiting for a retry. */
	int32 GetHeldEventCount() const { return Outbox.Num(); }

	int64 GetSentEventCount() const { return SentCount; }
	int64 GetDroppedEventCount() const { return DroppedCount; }

	/**
	 * Name as PlayFab accepts it: letters, digits and underscores, at most MaxEventNameLength.
	 * Each run of other characters becomes one underscore, dropped at the start and end, so
	 * "Game.Combat.EnemyKilled" is "Game_Combat_EnemyKilled". Empty when Name has no letter or digit.
	 */
	static FString SanitizeEventName(const FString& Name);

	/** Event as WriteEvents takes it. A name with no letter or digit is sent as unnamed_event. */
	static FEPFTelemetryEventData ToTelemetryEvent(const FEFAnalyticsEvent& Event);

private:
	/** Sends Outbox in batches while the player is logged in and no retry delay is running. */
	void SendWaiting(bool bIgnoreRetryDelay);

	void HandleBatchResult(const FEFResult& Result, const TArray<FEPFTelemetryEventData>& Batch);

	/** Drops the oldest events beyond MaxEventsHeld. */
	void TrimOutbox();

	void ScheduleRetry();
	void CancelRetry();

	TWeakObjectPtr<UEPFAnalyticsSubsystem> PlayFabAnalytics;

	/** Events not sent yet: waiting for a retry, or handed over while a retry delay runs. */
	TArray<FEPFTelemetryEventData> Outbox;

	FTSTicker::FDelegateHandle RetryTicker;
	double RetryNotBeforeSeconds = 0.0;
	float RetryDelaySeconds = 0.0f;

	int32 BatchesInFlight = 0;
	int64 SentCount = 0;
	int64 DroppedCount = 0;
	FString LastFailure;
	bool bStopped = false;
};

/**
 * Adds the PlayFab analytics sink to this game instance's UEFAnalyticsSubsystem, and removes it when
 * the game instance ends. Created when Send Analytics Events is on and a Title ID is set (Project
 * Settings > Extended Framework > Extended PlayFab).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFAnalyticsSinkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The sink this game instance added, or null. */
	TSharedPtr<FEPFAnalyticsSink> GetSink() const { return Sink; }

private:
	TSharedPtr<FEPFAnalyticsSink> Sink;

	/** The analytics subsystem the sink was added to. */
	TWeakObjectPtr<UEFAnalyticsSubsystem> RegisteredWith;
};

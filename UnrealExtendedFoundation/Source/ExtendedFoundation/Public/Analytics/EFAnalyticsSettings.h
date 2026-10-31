// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Settings/EFDeveloperSettings.h"
#include "EFAnalyticsSettings.generated.h"

/**
 * Project Settings > Extended Framework > Extended Analytics.
 *
 * Saved to DefaultGame.ini, which ships with the game. An HTTP header such as an ingestion key is
 * therefore visible to players: use a write-only key that can do nothing but submit events.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Extended Analytics"))
class EXTENDEDFOUNDATION_API UEFAnalyticsSettings : public UEFDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UEFAnalyticsSettings* Get() { return GetDefault<UEFAnalyticsSettings>(); }

	/** Off: events are still kept for debug UI but never reach a sink. */
	UPROPERTY(config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/**
	 * Nothing reaches a sink until the game calls Set Analytics Consent (Granted). Turn this on where
	 * the law asks for opt-in (GDPR and similar). Events recorded before the decision wait in memory.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Consent")
	bool bRequireConsent = false;

	/** Events kept while waiting for consent; the oldest are dropped beyond this. */
	UPROPERTY(config, EditAnywhere, Category = "Consent", meta = (ClampMin = "0", EditCondition = "bRequireConsent"))
	int32 MaxEventsAwaitingConsent = 1000;

	/** Remembers the player's decision in Saved/ExtendedAnalytics so they are asked once. */
	UPROPERTY(config, EditAnywhere, Category = "Consent")
	bool bRememberConsent = true;

	/** Seconds between deliveries to sinks. */
	UPROPERTY(config, EditAnywhere, Category = "Batching", meta = (ClampMin = "0.5", Units = "Seconds"))
	float FlushIntervalSeconds = 15.f;

	/** Events per batch; a full batch is delivered at once. */
	UPROPERTY(config, EditAnywhere, Category = "Batching", meta = (ClampMin = "1"))
	int32 MaxBatchSize = 50;

	/** Events kept for a sink that is not ready (e.g. not signed in); the oldest are dropped beyond this. */
	UPROPERTY(config, EditAnywhere, Category = "Batching", meta = (ClampMin = "0"))
	int32 MaxEventsWaitingPerSink = 2000;

	/** Records session_start and session_end (with durationSeconds). */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	bool bRecordSessionEvents = true;

	/** Adds build, config, platform, engine and project attributes to every event. */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	bool bIncludeBuildAttributes = true;

	/**
	 * Message bus channels recorded as events automatically (with every channel below them). The
	 * event name is the channel, e.g. Game.Combat.EnemyKilled; the payload's fields become attributes.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	TArray<FGameplayTag> ForwardedMessageChannels;

	/** Sinks to ignore by name (Log, File, Http, PlayFab, ...). */
	UPROPERTY(config, EditAnywhere, Category = "Sinks")
	TArray<FName> DisabledSinks;

	/** Log sink: writes every event to the log. Extended.Analytics.Log 1 does the same at run time. */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|Log")
	bool bLogSink = false;

	/** File sink: appends events as JSON lines to Saved/<FileSinkFolder>/, one file per session. Handy for playtests. */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|File")
	bool bFileSink = false;

	UPROPERTY(config, EditAnywhere, Category = "Sinks|File", meta = (EditCondition = "bFileSink"))
	FString FileSinkFolder = TEXT("Analytics");

	/**
	 * HTTP sink: POSTs each batch as {"events": [...]} to this URL. Empty turns the sink off.
	 * Batches can arrive twice after a lost response; deduplicate on sessionId + sequence.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http")
	FString HttpEndpoint;

	/** Headers sent with every batch, e.g. an ingestion key. They ship with the game. */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http")
	TMap<FString, FString> HttpHeaders;

	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http", meta = (ClampMin = "1", Units = "Seconds"))
	float HttpTimeoutSeconds = 15.f;

	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http", meta = (ClampMin = "0", ClampMax = "10"))
	int32 HttpMaxRetries = 2;

	/** Saves batches that could not be sent (offline, shutdown) under Saved/ExtendedAnalytics/Pending and sends them on the next run. */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http")
	bool bPersistUnsentHttpBatches = true;

	/** Saved batches kept at most; the oldest are deleted beyond this. */
	UPROPERTY(config, EditAnywhere, Category = "Sinks|Http", meta = (ClampMin = "0", EditCondition = "bPersistUnsentHttpBatches"))
	int32 MaxPersistedHttpBatches = 100;
};

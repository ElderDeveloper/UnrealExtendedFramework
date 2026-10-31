// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFAnalyticsTypes.generated.h"

class FJsonObject;

/** Whether the player agreed to analytics. */
UENUM(BlueprintType)
enum class EEFAnalyticsConsent : uint8
{
	/** Not asked yet. With consent required, events wait in memory until the player decides. */
	Unknown,
	Granted,
	/** Events are dropped, and anything waiting or saved for later sending is deleted. */
	Denied
};

/** One analytics event, as every sink receives it. */
USTRUCT(BlueprintType)
struct EXTENDEDFOUNDATION_API FEFAnalyticsEvent
{
	GENERATED_BODY()

	/** e.g. level_started, or a message channel such as Game.Combat.EnemyKilled. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString Name;

	/** The event's attributes merged over the global attributes. Values are text; numbers are written in invariant form. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	TMap<FString, FString> Attributes;

	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FDateTime TimestampUtc;

	/** New for every game instance run. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString SessionId;

	/** Set with SetUserId; empty until then. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	FString UserId;

	/** 1 for the session's first event, counting up. Lets a back end spot gaps and duplicates. */
	UPROPERTY(BlueprintReadOnly, Category = "Analytics")
	int64 Sequence = 0;

	/** {"name", "timestamp" (ISO 8601 UTC), "sessionId", "userId", "sequence", "attributes": {...}} */
	TSharedRef<FJsonObject> ToJson() const;

	static bool FromJson(const TSharedPtr<FJsonObject>& Json, FEFAnalyticsEvent& OutEvent);
};

/**
 * Receives analytics events from UEFAnalyticsSubsystem and delivers them to a back end.
 *
 * The foundation has Log, File and Http sinks; other plugins add theirs (PlayFab) with
 * UEFAnalyticsSubsystem::AddSink. All calls come on the game thread.
 */
class EXTENDEDFOUNDATION_API IEFAnalyticsSink
{
public:
	virtual ~IEFAnalyticsSink() = default;

	/** Unique name, e.g. Log, File, Http, PlayFab. Listed in DisabledSinks to turn the sink off. */
	virtual FName GetSinkName() const = 0;

	/**
	 * Whether the sink can take events now, e.g. its service is signed in. While it cannot, the
	 * subsystem keeps its events (up to MaxEventsWaitingPerSink) and sends them once it can.
	 */
	virtual bool IsReady() const { return true; }

	/** Delivers a batch. From here the sink owns delivery: retries, persistence, rate limits. */
	virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) = 0;

	/** Send now anything buffered; when bShuttingDown, persist what cannot be sent in time. */
	virtual void Flush(bool bShuttingDown) {}

	/** Consent was denied: drop everything buffered or persisted. */
	virtual void DiscardPending() {}

	/** One line for Extended.Status, e.g. "3 batches waiting". No secrets. */
	virtual FString DescribeStatus() const { return FString(); }
};

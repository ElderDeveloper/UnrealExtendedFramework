// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Analytics/EFAnalyticsTypes.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "Messages/EFMessageSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFAnalyticsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFOnAnalyticsConsentChanged, EEFAnalyticsConsent, Consent);

/**
 * Records analytics events once and delivers them to every enabled sink: the built-in log, file and
 * HTTP sinks, and sinks other plugins add (PlayFab).
 *
 *     UEFAnalyticsSubsystem::Get(this)->RecordEvent(TEXT("level_completed"), { { TEXT("level"), LevelName }, { TEXT("seconds"), LexToString(Seconds) } });
 *
 * - Events are batched and sent every FlushIntervalSeconds, or sooner when a batch fills.
 * - With bRequireConsent, nothing reaches a sink until SetConsent(Granted). Denied deletes what
 *   was waiting. The decision is remembered between runs.
 * - Every event carries the session id, a sequence number, the user id (SetUserId) and global
 *   attributes (build, platform, and SetGlobalAttribute).
 * - Message bus channels listed in ForwardedMessageChannels become events automatically, with the
 *   payload's fields as attributes.
 * - session_start and session_end are recorded when the game instance starts and ends.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFAnalyticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEventRecorded, const FEFAnalyticsEvent& /*Event*/);

	static UEFAnalyticsSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Records an event. Names are trimmed; empty names are rejected. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics", meta = (AutoCreateRefTerm = "Attributes"))
	void RecordEvent(const FString& Name, const TMap<FString, FString>& Attributes);

	/** Records an event whose attributes are the fields of a struct. Nested structs and arrays become JSON text. */
	void RecordStructEvent(const FString& Name, const UScriptStruct* Struct, const void* StructData);

	template <typename TStruct>
	void RecordStructEvent(const FString& Name, const TStruct& Struct)
	{
		RecordStructEvent(Name, TBaseStructure<TStruct>::Get(), &Struct);
	}

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics")
	void SetConsent(EEFAnalyticsConsent NewConsent);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Analytics")
	EEFAnalyticsConsent GetConsent() const { return Consent; }

	/** Whether events currently reach sinks: enabled, and consent granted or not required. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Analytics")
	bool IsCollecting() const;

	/** Id sent with every following event, e.g. the platform user id. Empty clears it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics")
	void SetUserId(const FString& InUserId);

	/** Adds an attribute to every following event. An empty Value removes it. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics")
	void SetGlobalAttribute(const FString& Key, const FString& Value);

	/** Sends the events recorded so far without waiting for the next interval. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Analytics")
	void Flush();

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Analytics")
	FString GetSessionId() const { return SessionId; }

	/** Adds a sink. A sink with the same name replaces the old one. Sinks named in DisabledSinks are ignored. */
	void AddSink(TSharedRef<IEFAnalyticsSink> Sink);

	void RemoveSink(FName SinkName);

	TArray<TSharedRef<IEFAnalyticsSink>> GetSinks() const;

	/** Events recorded but not yet handed to every sink (batching, waiting for consent or for a sink to be ready). */
	int32 GetPendingEventCount() const;

	/** The most recent events, oldest first, for debug UI. Kept whether or not consent was given; never sent. */
	const TArray<FEFAnalyticsEvent>& GetRecentEvents() const { return RecentEvents; }

	UPROPERTY(BlueprintAssignable, Category = "Extended Framework|Analytics")
	FEFOnAnalyticsConsentChanged OnConsentChanged;

	FOnEventRecorded OnEventRecorded;

	/** Where consent is remembered between runs. */
	static FString GetConsentFilePath();

private:
	struct FSinkEntry
	{
		TSharedPtr<IEFAnalyticsSink> Sink;
		/** Events for this sink while it is not ready. */
		TArray<FEFAnalyticsEvent> Waiting;
	};

	void StartSession();
	void EndSession();
	void CreateBuiltInSinks();
	void BindForwardedMessages();
	void HandleForwardedMessage(FGameplayTag Channel, const FInstancedStruct& Payload);
	void Enqueue(FEFAnalyticsEvent&& Event);
	void DeliverBatch(bool bShuttingDown);
	void DeliverToSink(FSinkEntry& Entry, const TArray<FEFAnalyticsEvent>& Batch, bool bShuttingDown);
	bool TickFlush(float DeltaTime);
	void LoadConsent();
	void SaveConsent() const;
	bool IsSinkDisabled(FName SinkName) const;

	TArray<FSinkEntry> Sinks;
	/** Recorded, waiting for the next flush. */
	TArray<FEFAnalyticsEvent> Batch;
	/** Recorded while consent is required and unknown. */
	TArray<FEFAnalyticsEvent> AwaitingConsent;
	TArray<FEFAnalyticsEvent> RecentEvents;
	TMap<FString, FString> GlobalAttributes;
	TArray<FEFMessageListenerHandle> ForwardedMessageHandles;
	FString SessionId;
	FString UserId;
	int64 NextSequence = 1;
	double SessionStartSeconds = 0.0;
	EEFAnalyticsConsent Consent = EEFAnalyticsConsent::Unknown;
	FTSTicker::FDelegateHandle FlushTickerHandle;
	int32 StatusHandle = 0;
	bool bSessionActive = false;
};

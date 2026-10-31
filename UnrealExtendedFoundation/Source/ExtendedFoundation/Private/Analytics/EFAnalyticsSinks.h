// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Analytics/EFAnalyticsTypes.h"

class FEFHttpOperation;
struct FEFHttpResponse;

/** Writes every event to the log (sink name Log). */
class FEFAnalyticsLogSink final : public IEFAnalyticsSink
{
public:
	virtual FName GetSinkName() const override { return TEXT("Log"); }
	virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) override;
};

/** Appends events as JSON lines to one file per session (sink name File). */
class FEFAnalyticsFileSink final : public IEFAnalyticsSink
{
public:
	explicit FEFAnalyticsFileSink(const FString& InFilePath) : FilePath(InFilePath) {}

	virtual FName GetSinkName() const override { return TEXT("File"); }
	virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) override;
	virtual void DiscardPending() override;
	virtual FString DescribeStatus() const override;

	const FString& GetFilePath() const { return FilePath; }

private:
	FString FilePath;
	int32 Written = 0;
};

/**
 * POSTs batches as {"events": [...]} to an endpoint (sink name Http), retrying through
 * FEFHttpOperation. Batches that cannot be sent are saved and sent again on the next run.
 */
class FEFAnalyticsHttpSink final : public IEFAnalyticsSink, public TSharedFromThis<FEFAnalyticsHttpSink>
{
public:
	struct FConfig
	{
		FString Endpoint;
		TMap<FString, FString> Headers;
		float TimeoutSeconds = 15.f;
		int32 MaxRetries = 2;
		bool bPersistUnsent = true;
		int32 MaxPersistedBatches = 100;
		/** Where unsent batches are saved. */
		FString PendingFolder;
	};

	explicit FEFAnalyticsHttpSink(const FConfig& InConfig) : Config(InConfig) {}

	/** Sends the batches earlier runs saved. Call once, after the sink is owned by a shared pointer. */
	void ResendPersisted();

	virtual FName GetSinkName() const override { return TEXT("Http"); }
	virtual void SendEvents(const TArray<FEFAnalyticsEvent>& Events) override;
	virtual void Flush(bool bShuttingDown) override;
	virtual void DiscardPending() override;
	virtual FString DescribeStatus() const override;

	static FString GetDefaultPendingFolder();

	int32 GetInFlightCount() const { return InFlight.Num(); }

private:
	struct FInFlight
	{
		int64 Id = 0;
		FString Body;
		int32 EventCount = 0;
		/** The saved file this batch came from, deleted once it is delivered. */
		FString PersistedFile;
		TSharedPtr<FEFHttpOperation> Operation;
	};

	void Post(const FString& Body, int32 EventCount, const FString& PersistedFile);
	void HandleResponse(int64 Id, const FEFHttpResponse& Response);
	void Persist(const FString& Body);
	void TrimPersisted();
	TArray<FString> FindPersistedFiles() const;

	FConfig Config;
	TArray<FInFlight> InFlight;
	int64 LastId = 0;
	int32 SentEvents = 0;
	int32 FailedBatches = 0;
};

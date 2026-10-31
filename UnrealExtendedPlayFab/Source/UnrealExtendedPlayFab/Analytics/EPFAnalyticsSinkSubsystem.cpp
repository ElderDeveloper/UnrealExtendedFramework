// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAnalyticsSinkSubsystem.h"
#include "UnrealExtendedPlayFab.h"
#include "Shared/EPFSettings.h"

#include "Analytics/EFAnalyticsSubsystem.h"
#include "HAL/PlatformTime.h"

namespace
{
	/** payload.event_source of every event this sink sends. */
	constexpr const TCHAR* SinkEventSource = TEXT("extended");

	bool IsAllowedInEventName(TCHAR Char)
	{
		return (Char >= TEXT('a') && Char <= TEXT('z'))
			|| (Char >= TEXT('A') && Char <= TEXT('Z'))
			|| (Char >= TEXT('0') && Char <= TEXT('9'))
			|| Char == TEXT('_');
	}
}

// ── FEPFAnalyticsSink ────────────────────────────────────────────────────────

FEPFAnalyticsSink::FEPFAnalyticsSink(UEPFAnalyticsSubsystem* InPlayFabAnalytics)
	: PlayFabAnalytics(InPlayFabAnalytics)
{
}

FEPFAnalyticsSink::~FEPFAnalyticsSink()
{
	CancelRetry();
}

FName FEPFAnalyticsSink::StaticSinkName()
{
	static const FName SinkName(TEXT("PlayFab"));
	return SinkName;
}

bool FEPFAnalyticsSink::IsReady() const
{
	// WriteTelemetryEvents is an entity API: it needs the entity token a login leaves behind.
	const UEPFAnalyticsSubsystem* Analytics = PlayFabAnalytics.Get();
	return !bStopped && Analytics && !Analytics->GetEntityToken().IsEmpty();
}

void FEPFAnalyticsSink::SendEvents(const TArray<FEFAnalyticsEvent>& Events)
{
	if (bStopped)
	{
		return;
	}

	Outbox.Reserve(Outbox.Num() + Events.Num());
	for (const FEFAnalyticsEvent& Event : Events)
	{
		Outbox.Add(ToTelemetryEvent(Event));
	}
	TrimOutbox();
	SendWaiting(/*bIgnoreRetryDelay*/ false);
}

void FEPFAnalyticsSink::Flush(bool bShuttingDown)
{
	if (bStopped)
	{
		return;
	}

	SendWaiting(/*bIgnoreRetryDelay*/ true);

	if (bShuttingDown && Outbox.Num() > 0)
	{
		// Only left when the player is not logged in: there is nowhere to send them, and they are
		// not kept on disk.
		EF_LOG(ExtendedPlayFab, Log, TEXT("PlayFab analytics sink — %d events not sent before shutdown were dropped"), Outbox.Num());
		DroppedCount += Outbox.Num();
		Outbox.Reset();
		CancelRetry();
	}
}

void FEPFAnalyticsSink::DiscardPending()
{
	Outbox.Reset();
	CancelRetry();
	RetryDelaySeconds = 0.0f;
	RetryNotBeforeSeconds = 0.0;
}

FString FEPFAnalyticsSink::DescribeStatus() const
{
	if (bStopped || !PlayFabAnalytics.IsValid())
	{
		return TEXT("Stopped.");
	}

	FString Status = FString::Printf(TEXT("%s; %lld sent, %d waiting, %d batches in flight"),
		IsReady() ? TEXT("Logged in") : TEXT("Waiting for a PlayFab login"),
		SentCount, Outbox.Num(), BatchesInFlight);
	if (DroppedCount > 0)
	{
		Status += FString::Printf(TEXT(", %lld dropped"), DroppedCount);
	}
	if (!LastFailure.IsEmpty())
	{
		Status += FString::Printf(TEXT("; last failure: %s"), *LastFailure);
	}
	return Status;
}

void FEPFAnalyticsSink::Shutdown()
{
	bStopped = true;
	CancelRetry();
	if (Outbox.Num() > 0)
	{
		EF_LOG(ExtendedPlayFab, Log, TEXT("PlayFab analytics sink — %d unsent events dropped at shutdown"), Outbox.Num());
		DroppedCount += Outbox.Num();
		Outbox.Reset();
	}
	PlayFabAnalytics.Reset();
}

void FEPFAnalyticsSink::SendWaiting(bool bIgnoreRetryDelay)
{
	UEPFAnalyticsSubsystem* Analytics = PlayFabAnalytics.Get();
	if (Outbox.Num() == 0 || !Analytics || !IsReady())
	{
		return;
	}
	if (!bIgnoreRetryDelay && FPlatformTime::Seconds() < RetryNotBeforeSeconds)
	{
		return;
	}

	// Taken out first: a request that fails before it is sent answers inside WriteEvents, and a
	// batch it puts back must not come round again in this loop.
	TArray<FEPFTelemetryEventData> ToSend = MoveTemp(Outbox);
	Outbox.Reset();

	const int32 MaxPerWrite = UEPFAnalyticsSubsystem::MaxEventsPerWrite;
	const TWeakPtr<FEPFAnalyticsSink> WeakSink(AsShared());
	for (int32 First = 0; First < ToSend.Num(); First += MaxPerWrite)
	{
		const TArray<FEPFTelemetryEventData> Batch(ToSend.GetData() + First, FMath::Min(MaxPerWrite, ToSend.Num() - First));
		++BatchesInFlight;
		Analytics->WriteEvents(Batch, SinkEventSource, [WeakSink, Batch](const FEFResult& Result)
		{
			if (const TSharedPtr<FEPFAnalyticsSink> PinnedSink = WeakSink.Pin())
			{
				PinnedSink->HandleBatchResult(Result, Batch);
			}
		});
	}
}

void FEPFAnalyticsSink::HandleBatchResult(const FEFResult& Result, const TArray<FEPFTelemetryEventData>& Batch)
{
	BatchesInFlight = FMath::Max(BatchesInFlight - 1, 0);

	if (Result.IsSuccess())
	{
		SentCount += Batch.Num();
		RetryDelaySeconds = 0.0f;
		return;
	}

	LastFailure = Result.ToString();

	if (Result.bRetryable && !bStopped)
	{
		// Back in front of anything newer, and tried again after a delay that doubles while
		// PlayFab keeps failing.
		Outbox.Insert(Batch, 0);
		TrimOutbox();
		RetryDelaySeconds = FMath::Clamp(RetryDelaySeconds * 2.0f, MinRetryDelaySeconds, MaxRetryDelaySeconds);
		RetryNotBeforeSeconds = FPlatformTime::Seconds() + RetryDelaySeconds;
		ScheduleRetry();
		EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab analytics sink — %d events not delivered (%s); retrying in %.0fs"),
			Batch.Num(), *LastFailure, RetryDelaySeconds);
		return;
	}

	DroppedCount += Batch.Num();
	EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab analytics sink — %d events dropped: %s"), Batch.Num(), *LastFailure);
}

void FEPFAnalyticsSink::TrimOutbox()
{
	const int32 Excess = Outbox.Num() - MaxEventsHeld;
	if (Excess > 0)
	{
		Outbox.RemoveAt(0, Excess);
		DroppedCount += Excess;
		EF_LOG(ExtendedPlayFab, Warning, TEXT("PlayFab analytics sink — more than %d events waiting; dropped the %d oldest"), MaxEventsHeld, Excess);
	}
}

void FEPFAnalyticsSink::ScheduleRetry()
{
	CancelRetry();

	const TWeakPtr<FEPFAnalyticsSink> WeakSink(AsShared());
	RetryTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSink](float) -> bool
	{
		if (const TSharedPtr<FEPFAnalyticsSink> PinnedSink = WeakSink.Pin())
		{
			PinnedSink->RetryTicker.Reset();
			PinnedSink->SendWaiting(/*bIgnoreRetryDelay*/ true);
		}
		return false;
	}), RetryDelaySeconds);
}

void FEPFAnalyticsSink::CancelRetry()
{
	if (RetryTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetryTicker);
		RetryTicker.Reset();
	}
}

FString FEPFAnalyticsSink::SanitizeEventName(const FString& Name)
{
	FString Sanitized;
	Sanitized.Reserve(FMath::Min(Name.Len(), MaxEventNameLength));

	// A run of characters PlayFab does not take becomes one underscore, and only between kept ones.
	bool bSeparatorPending = false;
	for (const TCHAR Char : Name)
	{
		if (!IsAllowedInEventName(Char))
		{
			bSeparatorPending = !Sanitized.IsEmpty();
			continue;
		}
		if (bSeparatorPending && Char != TEXT('_'))
		{
			Sanitized.AppendChar(TEXT('_'));
		}
		bSeparatorPending = false;
		Sanitized.AppendChar(Char);
	}

	// Only underscores is no name at all.
	bool bHasLetterOrDigit = false;
	for (const TCHAR Char : Sanitized)
	{
		if (Char != TEXT('_'))
		{
			bHasLetterOrDigit = true;
			break;
		}
	}
	if (!bHasLetterOrDigit)
	{
		return FString();
	}

	return Sanitized.Left(MaxEventNameLength);
}

FEPFTelemetryEventData FEPFAnalyticsSink::ToTelemetryEvent(const FEFAnalyticsEvent& Event)
{
	FEPFTelemetryEventData Data;
	Data.EventName = SanitizeEventName(Event.Name);
	if (Data.EventName.IsEmpty())
	{
		Data.EventName = TEXT("unnamed_event");
	}
	Data.Properties = Event.Attributes;
	Data.TimestampUtc = Event.TimestampUtc;

	if (!Event.SessionId.IsEmpty())
	{
		Data.PayloadFields.Add(TEXT("session_id"), Event.SessionId);
	}
	Data.PayloadFields.Add(TEXT("sequence"), LexToString(Event.Sequence));
	if (!Event.UserId.IsEmpty())
	{
		Data.PayloadFields.Add(TEXT("user_id"), Event.UserId);
	}
	if (!Data.EventName.Equals(Event.Name, ESearchCase::CaseSensitive))
	{
		Data.PayloadFields.Add(TEXT("original_name"), Event.Name);
	}
	return Data;
}

// ── UEPFAnalyticsSinkSubsystem ───────────────────────────────────────────────

bool UEPFAnalyticsSinkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	return Super::ShouldCreateSubsystem(Outer) && Settings && Settings->bSendAnalyticsEvents && !Settings->TitleId.IsEmpty();
}

void UEPFAnalyticsSinkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UEPFAnalyticsSubsystem* PlayFabAnalytics = Collection.InitializeDependency<UEPFAnalyticsSubsystem>();
	if (UEFAnalyticsSubsystem* Analytics = Collection.InitializeDependency<UEFAnalyticsSubsystem>())
	{
		Sink = MakeShared<FEPFAnalyticsSink>(PlayFabAnalytics);
		Analytics->AddSink(Sink.ToSharedRef());
		RegisteredWith = Analytics;
	}
}

void UEPFAnalyticsSinkSubsystem::Deinitialize()
{
	if (UEFAnalyticsSubsystem* Analytics = RegisteredWith.Get())
	{
		Analytics->RemoveSink(FEPFAnalyticsSink::StaticSinkName());
	}
	if (Sink.IsValid())
	{
		Sink->Shutdown();
	}
	Sink.Reset();
	RegisteredWith.Reset();

	Super::Deinitialize();
}

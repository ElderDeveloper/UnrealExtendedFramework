// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAnalytics.h"
#include "Analytics/EPFAnalyticsSubsystem.h"

// ── Log Single Event ─────────────────────────────────────────────────────────

UEPFAsyncLogEvent* UEPFAsyncLogEvent::LogEvent(UObject* WorldContext, const FString& EventName, const TMap<FString, FString>& Body)
{
	auto* Action = NewObject<UEPFAsyncLogEvent>();
	Action->LoggedEventName = EventName;
	Action->EventBody = Body;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncLogEvent::ExecuteAction()
{
	if (LoggedEventName.IsEmpty()) { FailInvalidArgument(TEXT("Event name cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFAnalyticsSubsystem>();
	if (!Sub) return;
	Sub->OnEventLogged.AddDynamic(this, &UEPFAsyncLogEvent::HandleComplete);
	CallWithContext([&] { Sub->LogEvent(LoggedEventName, EventBody); });
}

void UEPFAsyncLogEvent::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFAnalyticsSubsystem>())
		Sub->OnEventLogged.RemoveDynamic(this, &UEPFAsyncLogEvent::HandleComplete);
}

void UEPFAsyncLogEvent::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to log event")));
}

// ── Log Batch Events ─────────────────────────────────────────────────────────

UEPFAsyncLogEvents* UEPFAsyncLogEvents::LogEvents(UObject* WorldContext, const TArray<FEPFAnalyticsEvent>& Events)
{
	auto* Action = NewObject<UEPFAsyncLogEvents>();
	Action->EventsToLog = Events;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncLogEvents::ExecuteAction()
{
	if (EventsToLog.Num() == 0) { FailInvalidArgument(TEXT("Events array cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFAnalyticsSubsystem>();
	if (!Sub) return;
	Sub->OnEventLogged.AddDynamic(this, &UEPFAsyncLogEvents::HandleComplete);
	CallWithContext([&] { Sub->LogEvents(EventsToLog); });
}

void UEPFAsyncLogEvents::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFAnalyticsSubsystem>())
		Sub->OnEventLogged.RemoveDynamic(this, &UEPFAsyncLogEvents::HandleComplete);
}

void UEPFAsyncLogEvents::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to log batch events")));
}

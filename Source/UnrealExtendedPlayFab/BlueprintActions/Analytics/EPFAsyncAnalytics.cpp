// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAnalytics.h"
#include "Analytics/EPFAnalyticsSubsystem.h"
#include "Engine/GameInstance.h"

// ── Log Single Event ─────────────────────────────────────────────────────────

UEPFAsyncLogEvent* UEPFAsyncLogEvent::LogEvent(UObject* WorldContext, const FString& EventName, const TMap<FString, FString>& Body)
{
	auto* Action = NewObject<UEPFAsyncLogEvent>();
	Action->EventName = EventName;
	Action->Body = Body;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncLogEvent::Activate()
{
	if (EventName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Event name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFAnalyticsSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Analytics subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnEventLogged.AddDynamic(this, &UEPFAsyncLogEvent::HandleComplete);
	Sub->LogEvent(EventName, Body);
}

void UEPFAsyncLogEvent::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFAnalyticsSubsystem>(WorldContext.Get()))
		Sub->OnEventLogged.RemoveDynamic(this, &UEPFAsyncLogEvent::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to log event")));
	SetReadyToDestroy();
}

// ── Log Batch Events ─────────────────────────────────────────────────────────

UEPFAsyncLogEvents* UEPFAsyncLogEvents::LogEvents(UObject* WorldContext, const TArray<FEPFAnalyticsEvent>& Events)
{
	auto* Action = NewObject<UEPFAsyncLogEvents>();
	Action->Events = Events;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncLogEvents::Activate()
{
	if (Events.Num() == 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Events array cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFAnalyticsSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Analytics subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnEventLogged.AddDynamic(this, &UEPFAsyncLogEvents::HandleComplete);
	Sub->LogEvents(Events);
}

void UEPFAsyncLogEvents::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFAnalyticsSubsystem>(WorldContext.Get()))
		Sub->OnEventLogged.RemoveDynamic(this, &UEPFAsyncLogEvents::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to log batch events")));
	SetReadyToDestroy();
}

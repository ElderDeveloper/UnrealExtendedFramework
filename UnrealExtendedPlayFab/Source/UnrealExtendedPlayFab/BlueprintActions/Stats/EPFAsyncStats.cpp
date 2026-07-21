// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncStats.h"
#include "Stats/EPFStatsSubsystem.h"
#include "Engine/GameInstance.h"

// ── Get ──────────────────────────────────────────────────────────────────────

UEPFAsyncGetStats* UEPFAsyncGetStats::GetStats(UObject* WorldContext, const TArray<FString>& StatNames)
{
	auto* Action = NewObject<UEPFAsyncGetStats>();
	Action->StatNames = StatNames;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetStats::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFStatsSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Stats subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnStatsReceived.AddDynamic(this, &UEPFAsyncGetStats::HandleComplete);
	StatNames.Num() > 0 ? Sub->GetStats(StatNames) : Sub->GetAllStats();
}

void UEPFAsyncGetStats::HandleComplete(const FEPFResult& Result, const TArray<FEPFStatistic>& Stats)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFStatsSubsystem>(WorldContext.Get()))
		Sub->OnStatsReceived.RemoveDynamic(this, &UEPFAsyncGetStats::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Stats) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get stats")));
	SetReadyToDestroy();
}

// ── Update ───────────────────────────────────────────────────────────────────

UEPFAsyncUpdateStats* UEPFAsyncUpdateStats::UpdateStats(UObject* WorldContext, const TMap<FString, int32>& Stats)
{
	auto* Action = NewObject<UEPFAsyncUpdateStats>();
	Action->Stats = Stats;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncUpdateStats::Activate()
{
	if (Stats.Num() == 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Stats map cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFStatsSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Stats subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnStatsUpdated.AddDynamic(this, &UEPFAsyncUpdateStats::HandleComplete);
	Sub->UpdateStats(Stats);
}

void UEPFAsyncUpdateStats::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFStatsSubsystem>(WorldContext.Get()))
		Sub->OnStatsUpdated.RemoveDynamic(this, &UEPFAsyncUpdateStats::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update stats")));
	SetReadyToDestroy();
}

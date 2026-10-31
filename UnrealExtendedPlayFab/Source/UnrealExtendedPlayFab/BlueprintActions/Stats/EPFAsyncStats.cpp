// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncStats.h"
#include "Stats/EPFStatsSubsystem.h"

// ── Get ──────────────────────────────────────────────────────────────────────

UEPFAsyncGetStats* UEPFAsyncGetStats::GetStats(UObject* WorldContext, const TArray<FString>& StatNames)
{
	auto* Action = NewObject<UEPFAsyncGetStats>();
	Action->RequestedStatNames = StatNames;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetStats::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFStatsSubsystem>();
	if (!Sub) return;
	Sub->OnStatsReceived.AddDynamic(this, &UEPFAsyncGetStats::HandleComplete);
	CallWithContext([&]
	{
		if (RequestedStatNames.Num() > 0) { Sub->GetStats(RequestedStatNames); }
		else { Sub->GetAllStats(); }
	});
}

void UEPFAsyncGetStats::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFStatsSubsystem>())
		Sub->OnStatsReceived.RemoveDynamic(this, &UEPFAsyncGetStats::HandleComplete);
}

void UEPFAsyncGetStats::HandleComplete(const FEPFResult& Result, const TArray<FEPFStatistic>& Stats)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Stats) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get stats")));
}

// ── Update ───────────────────────────────────────────────────────────────────

UEPFAsyncUpdateStats* UEPFAsyncUpdateStats::UpdateStats(UObject* WorldContext, const TMap<FString, int32>& Stats)
{
	auto* Action = NewObject<UEPFAsyncUpdateStats>();
	Action->StatsToUpdate = Stats;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncUpdateStats::ExecuteAction()
{
	if (StatsToUpdate.Num() == 0) { FailInvalidArgument(TEXT("Stats map cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFStatsSubsystem>();
	if (!Sub) return;
	Sub->OnStatsUpdated.AddDynamic(this, &UEPFAsyncUpdateStats::HandleComplete);
	CallWithContext([&] { Sub->UpdateStats(StatsToUpdate); });
}

void UEPFAsyncUpdateStats::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFStatsSubsystem>())
		Sub->OnStatsUpdated.RemoveDynamic(this, &UEPFAsyncUpdateStats::HandleComplete);
}

void UEPFAsyncUpdateStats::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update stats")));
}

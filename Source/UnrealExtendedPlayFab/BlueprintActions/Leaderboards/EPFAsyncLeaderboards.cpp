// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncLeaderboards.h"
#include "Leaderboards/EPFLeaderboardSubsystem.h"
#include "Engine/GameInstance.h"

// ── Global ───────────────────────────────────────────────────────────────────

UEPFAsyncGetLeaderboard* UEPFAsyncGetLeaderboard::GetLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 StartPosition, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetLeaderboard>();
	Action->StatisticName = StatisticName;
	Action->StartPosition = FMath::Max(0, StartPosition);
	Action->MaxResultsCount = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetLeaderboard::Activate()
{
	if (StatisticName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Statistic name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Leaderboard subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetLeaderboard::HandleComplete);
	Sub->GetLeaderboard(StatisticName, StartPosition, MaxResultsCount);
}

void UEPFAsyncGetLeaderboard::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get()))
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetLeaderboard::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get leaderboard")));
	SetReadyToDestroy();
}

// ── Around Player ────────────────────────────────────────────────────────────

UEPFAsyncGetLeaderboardAroundPlayer* UEPFAsyncGetLeaderboardAroundPlayer::GetLeaderboardAroundPlayer(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetLeaderboardAroundPlayer>();
	Action->StatisticName = StatisticName;
	Action->MaxResultsCount = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetLeaderboardAroundPlayer::Activate()
{
	if (StatisticName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Statistic name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Leaderboard subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete);
	Sub->GetLeaderboardAroundPlayer(StatisticName, MaxResultsCount);
}

void UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get()))
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get leaderboard around player")));
	SetReadyToDestroy();
}

// ── Friends ──────────────────────────────────────────────────────────────────

UEPFAsyncGetFriendsLeaderboard* UEPFAsyncGetFriendsLeaderboard::GetFriendsLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetFriendsLeaderboard>();
	Action->StatisticName = StatisticName;
	Action->MaxResultsCount = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetFriendsLeaderboard::Activate()
{
	if (StatisticName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Statistic name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Leaderboard subsystem not available"))); SetReadyToDestroy(); return; }
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetFriendsLeaderboard::HandleComplete);
	Sub->GetFriendsLeaderboard(StatisticName, MaxResultsCount);
}

void UEPFAsyncGetFriendsLeaderboard::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFLeaderboardSubsystem>(WorldContext.Get()))
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetFriendsLeaderboard::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get friends leaderboard")));
	SetReadyToDestroy();
}

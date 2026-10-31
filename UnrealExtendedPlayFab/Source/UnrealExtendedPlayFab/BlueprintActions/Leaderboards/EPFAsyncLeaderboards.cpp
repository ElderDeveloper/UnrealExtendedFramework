// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncLeaderboards.h"
#include "Leaderboards/EPFLeaderboardSubsystem.h"

// ── Global ───────────────────────────────────────────────────────────────────

UEPFAsyncGetLeaderboard* UEPFAsyncGetLeaderboard::GetLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 StartPosition, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetLeaderboard>();
	Action->RequestedStatisticName = StatisticName;
	Action->RequestedStartPosition = FMath::Max(0, StartPosition);
	Action->RequestedMaxResults = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetLeaderboard::ExecuteAction()
{
	if (RequestedStatisticName.IsEmpty()) { FailInvalidArgument(TEXT("Statistic name cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFLeaderboardSubsystem>();
	if (!Sub) return;
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetLeaderboard::HandleComplete);
	CallWithContext([&] { Sub->GetLeaderboard(RequestedStatisticName, RequestedStartPosition, RequestedMaxResults); });
}

void UEPFAsyncGetLeaderboard::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFLeaderboardSubsystem>())
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetLeaderboard::HandleComplete);
}

void UEPFAsyncGetLeaderboard::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get leaderboard")));
}

// ── Around Player ────────────────────────────────────────────────────────────

UEPFAsyncGetLeaderboardAroundPlayer* UEPFAsyncGetLeaderboardAroundPlayer::GetLeaderboardAroundPlayer(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetLeaderboardAroundPlayer>();
	Action->RequestedStatisticName = StatisticName;
	Action->RequestedMaxResults = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetLeaderboardAroundPlayer::ExecuteAction()
{
	if (RequestedStatisticName.IsEmpty()) { FailInvalidArgument(TEXT("Statistic name cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFLeaderboardSubsystem>();
	if (!Sub) return;
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete);
	CallWithContext([&] { Sub->GetLeaderboardAroundPlayer(RequestedStatisticName, RequestedMaxResults); });
}

void UEPFAsyncGetLeaderboardAroundPlayer::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFLeaderboardSubsystem>())
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete);
}

void UEPFAsyncGetLeaderboardAroundPlayer::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get leaderboard around player")));
}

// ── Friends ──────────────────────────────────────────────────────────────────

UEPFAsyncGetFriendsLeaderboard* UEPFAsyncGetFriendsLeaderboard::GetFriendsLeaderboard(UObject* WorldContext, const FString& StatisticName, int32 MaxResultsCount)
{
	auto* Action = NewObject<UEPFAsyncGetFriendsLeaderboard>();
	Action->RequestedStatisticName = StatisticName;
	Action->RequestedMaxResults = FMath::Clamp(MaxResultsCount, 1, 100);
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetFriendsLeaderboard::ExecuteAction()
{
	if (RequestedStatisticName.IsEmpty()) { FailInvalidArgument(TEXT("Statistic name cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFLeaderboardSubsystem>();
	if (!Sub) return;
	Sub->OnLeaderboardReceived.AddDynamic(this, &UEPFAsyncGetFriendsLeaderboard::HandleComplete);
	CallWithContext([&] { Sub->GetFriendsLeaderboard(RequestedStatisticName, RequestedMaxResults); });
}

void UEPFAsyncGetFriendsLeaderboard::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFLeaderboardSubsystem>())
		Sub->OnLeaderboardReceived.RemoveDynamic(this, &UEPFAsyncGetFriendsLeaderboard::HandleComplete);
}

void UEPFAsyncGetFriendsLeaderboard::HandleComplete(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& Entries)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Entries) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get friends leaderboard")));
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/UserStats/ESteamUserStatsAsyncActions.h"

// Every node below keeps its subsystem in a member named Subsystem, so locals use StatsSubsystem.

// ---- USteamAsyncFindLeaderboard ----

USteamAsyncFindLeaderboard* USteamAsyncFindLeaderboard::FindLeaderboard(UObject* WorldContext, const FString& LeaderboardName, bool bCreateIfMissing, EESteamLeaderboardSortMethod SortMethod, EESteamLeaderboardDisplayType DisplayType, float Timeout)
{
	USteamAsyncFindLeaderboard* Action = NewObject<USteamAsyncFindLeaderboard>();
	Action->RequestedName = LeaderboardName;
	Action->bCreateIfMissing = bCreateIfMissing;
	Action->SortMethod = SortMethod;
	Action->DisplayType = DisplayType;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncFindLeaderboard::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnLeaderboardFound.AddDynamic(this, &USteamAsyncFindLeaderboard::HandleLeaderboardFound);

	bool bStarted = false;
	CallWithContext([&]
	{
		bStarted = bCreateIfMissing
			? StatsSubsystem->FindOrCreateLeaderboard(RequestedName, SortMethod, DisplayType)
			: StatsSubsystem->FindLeaderboard(RequestedName);
	});
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncFindLeaderboard::HandleLeaderboardFound(bool bSuccess, int64 LeaderboardHandle, FString LeaderboardName)
{
	if (!IsResultForThisCall() || LeaderboardName != RequestedName)
	{
		return;
	}
	Complete(bSuccess, LeaderboardHandle);
}

void USteamAsyncFindLeaderboard::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0);
}

void USteamAsyncFindLeaderboard::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardFound.RemoveDynamic(this, &USteamAsyncFindLeaderboard::HandleLeaderboardFound);
	}
}

void USteamAsyncFindLeaderboard::Complete(bool bSuccess, int64 LeaderboardHandle)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(LeaderboardHandle);
	}
	else
	{
		OnFailure.Broadcast(LeaderboardHandle);
	}
}

// ---- USteamAsyncUploadLeaderboardScore ----

USteamAsyncUploadLeaderboardScore* USteamAsyncUploadLeaderboardScore::UploadLeaderboardScore(UObject* WorldContext, int64 LeaderboardHandle, int32 Score, bool bForceUpdate, float Timeout)
{
	USteamAsyncUploadLeaderboardScore* Action = NewObject<USteamAsyncUploadLeaderboardScore>();
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->Score = Score;
	Action->bForceUpdate = bForceUpdate;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncUploadLeaderboardScore::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnLeaderboardScoreUploaded.AddDynamic(this, &USteamAsyncUploadLeaderboardScore::HandleScoreUploaded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->UploadLeaderboardScore(LeaderboardHandle, Score, bForceUpdate); });
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncUploadLeaderboardScore::HandleScoreUploaded(bool bSuccess, int32 UploadedScore, bool bScoreChanged, int32 NewGlobalRank)
{
	// FOnSteamLeaderboardScoreUploaded carries no leaderboard handle; the subsystem tags each result
	// with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, NewGlobalRank);
}

void USteamAsyncUploadLeaderboardScore::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0);
}

void USteamAsyncUploadLeaderboardScore::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardScoreUploaded.RemoveDynamic(this, &USteamAsyncUploadLeaderboardScore::HandleScoreUploaded);
	}
}

void USteamAsyncUploadLeaderboardScore::Complete(bool bSuccess, int32 NewGlobalRank)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(NewGlobalRank);
	}
	else
	{
		OnFailure.Broadcast(NewGlobalRank);
	}
}

// ---- USteamAsyncDownloadLeaderboardEntries ----

USteamAsyncDownloadLeaderboardEntries* USteamAsyncDownloadLeaderboardEntries::DownloadLeaderboardEntries(UObject* WorldContext, int64 LeaderboardHandle, EESteamLeaderboardDataRequest RequestType, int32 RangeStart, int32 RangeEnd, float Timeout)
{
	USteamAsyncDownloadLeaderboardEntries* Action = NewObject<USteamAsyncDownloadLeaderboardEntries>();
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->RequestType = RequestType;
	Action->RangeStart = RangeStart;
	Action->RangeEnd = RangeEnd;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncDownloadLeaderboardEntries::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnLeaderboardEntriesDownloaded.AddDynamic(this, &USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->DownloadLeaderboardEntries(LeaderboardHandle, RequestType, RangeStart, RangeEnd); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
	}
}

void USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	// FOnSteamLeaderboardEntriesDownloaded carries no leaderboard handle; the subsystem tags each
	// result with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Entries);
}

void USteamAsyncDownloadLeaderboardEntries::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamLeaderboardEntry>());
}

void USteamAsyncDownloadLeaderboardEntries::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardEntriesDownloaded.RemoveDynamic(this, &USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded);
	}
}

void USteamAsyncDownloadLeaderboardEntries::Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Entries);
	}
	else
	{
		OnFailure.Broadcast(Entries);
	}
}

// ---- USteamAsyncRequestUserStats ----

USteamAsyncRequestUserStats* USteamAsyncRequestUserStats::RequestUserStats(UObject* WorldContext, FESteamId TargetUser, float Timeout)
{
	USteamAsyncRequestUserStats* Action = NewObject<USteamAsyncRequestUserStats>();
	Action->TargetUser = TargetUser;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestUserStats::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnUserStatsReceived.AddDynamic(this, &USteamAsyncRequestUserStats::HandleUserStatsReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->RequestUserStats(TargetUser); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncRequestUserStats::HandleUserStatsReceived(FESteamId SteamId, bool bSuccess)
{
	if (!IsResultForThisCall() || SteamId != TargetUser)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncRequestUserStats::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncRequestUserStats::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnUserStatsReceived.RemoveDynamic(this, &USteamAsyncRequestUserStats::HandleUserStatsReceived);
	}
}

void USteamAsyncRequestUserStats::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncGetNumberOfCurrentPlayers ----

USteamAsyncGetNumberOfCurrentPlayers* USteamAsyncGetNumberOfCurrentPlayers::GetNumberOfCurrentPlayers(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetNumberOfCurrentPlayers* Action = NewObject<USteamAsyncGetNumberOfCurrentPlayers>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetNumberOfCurrentPlayers::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnNumberOfCurrentPlayers.AddDynamic(this, &USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->GetNumberOfCurrentPlayers(); });
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers(bool bSuccess, int32 PlayerCount)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, PlayerCount);
}

void USteamAsyncGetNumberOfCurrentPlayers::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0);
}

void USteamAsyncGetNumberOfCurrentPlayers::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnNumberOfCurrentPlayers.RemoveDynamic(this, &USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers);
	}
}

void USteamAsyncGetNumberOfCurrentPlayers::Complete(bool bSuccess, int32 PlayerCount)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PlayerCount);
	}
	else
	{
		OnFailure.Broadcast(PlayerCount);
	}
}

// ---- USteamAsyncRequestGlobalStats ----

USteamAsyncRequestGlobalStats* USteamAsyncRequestGlobalStats::RequestGlobalStats(UObject* WorldContext, int32 HistoryDays, float Timeout)
{
	USteamAsyncRequestGlobalStats* Action = NewObject<USteamAsyncRequestGlobalStats>();
	Action->HistoryDays = HistoryDays;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestGlobalStats::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnGlobalStatsReceived.AddDynamic(this, &USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->RequestGlobalStats(HistoryDays); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived(bool bSuccess)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncRequestGlobalStats::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncRequestGlobalStats::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnGlobalStatsReceived.RemoveDynamic(this, &USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived);
	}
}

void USteamAsyncRequestGlobalStats::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncRequestGlobalAchievementPercentages ----

USteamAsyncRequestGlobalAchievementPercentages* USteamAsyncRequestGlobalAchievementPercentages::RequestGlobalAchievementPercentages(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestGlobalAchievementPercentages* Action = NewObject<USteamAsyncRequestGlobalAchievementPercentages>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestGlobalAchievementPercentages::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnGlobalAchievementPercentagesReady.AddDynamic(this, &USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->RequestGlobalAchievementPercentages(); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady(bool bSuccess)
{
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncRequestGlobalAchievementPercentages::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncRequestGlobalAchievementPercentages::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnGlobalAchievementPercentagesReady.RemoveDynamic(this, &USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady);
	}
}

void USteamAsyncRequestGlobalAchievementPercentages::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncDownloadLeaderboardEntriesForUsers ----

USteamAsyncDownloadLeaderboardEntriesForUsers* USteamAsyncDownloadLeaderboardEntriesForUsers::DownloadLeaderboardEntriesForUsers(UObject* WorldContext, int64 LeaderboardHandle, const TArray<FESteamId>& Users, float Timeout)
{
	USteamAsyncDownloadLeaderboardEntriesForUsers* Action = NewObject<USteamAsyncDownloadLeaderboardEntriesForUsers>();
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->Users = Users;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnLeaderboardEntriesDownloaded.AddDynamic(this, &USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->DownloadLeaderboardEntriesForUsers(LeaderboardHandle, Users); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
	}
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	// FOnSteamLeaderboardEntriesDownloaded carries no leaderboard handle; both download nodes share
	// it. The subsystem tags each result with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Entries);
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamLeaderboardEntry>());
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardEntriesDownloaded.RemoveDynamic(this, &USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded);
	}
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Entries);
	}
	else
	{
		OnFailure.Broadcast(Entries);
	}
}

// ---- USteamAsyncAttachLeaderboardUGC ----

USteamAsyncAttachLeaderboardUGC* USteamAsyncAttachLeaderboardUGC::AttachLeaderboardUGC(UObject* WorldContext, int64 LeaderboardHandle, int64 UGCHandle, float Timeout)
{
	USteamAsyncAttachLeaderboardUGC* Action = NewObject<USteamAsyncAttachLeaderboardUGC>();
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->UGCHandle = UGCHandle;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncAttachLeaderboardUGC::ExecuteAction()
{
	UESteamUserStatsSubsystem* StatsSubsystem = RequireGameInstanceSubsystem<UESteamUserStatsSubsystem>();
	if (!StatsSubsystem)
	{
		return;
	}

	Subsystem = StatsSubsystem;
	StatsSubsystem->OnLeaderboardUGCSet.AddDynamic(this, &USteamAsyncAttachLeaderboardUGC::HandleUGCSet);

	bool bStarted = false;
	CallWithContext([&] { bStarted = StatsSubsystem->AttachLeaderboardUGC(LeaderboardHandle, UGCHandle); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncAttachLeaderboardUGC::HandleUGCSet(bool bSuccess)
{
	// FOnSteamLeaderboardUGCSet carries no leaderboard handle; the subsystem tags each result with
	// the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncAttachLeaderboardUGC::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncAttachLeaderboardUGC::CancelWork()
{
	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardUGCSet.RemoveDynamic(this, &USteamAsyncAttachLeaderboardUGC::HandleUGCSet);
	}
}

void USteamAsyncAttachLeaderboardUGC::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

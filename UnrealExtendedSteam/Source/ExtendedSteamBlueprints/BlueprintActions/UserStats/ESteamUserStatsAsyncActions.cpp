// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/UserStats/ESteamUserStatsAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamUserStatsSubsystem* GetUserStatsSubsystem(const UObject* WorldContext)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamUserStatsSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncFindLeaderboard ----

USteamAsyncFindLeaderboard* USteamAsyncFindLeaderboard::FindLeaderboard(UObject* WorldContext, const FString& LeaderboardName, bool bCreateIfMissing, EESteamLeaderboardSortMethod SortMethod, EESteamLeaderboardDisplayType DisplayType, float Timeout)
{
	USteamAsyncFindLeaderboard* Action = NewObject<USteamAsyncFindLeaderboard>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->RequestedName = LeaderboardName;
	Action->bCreateIfMissing = bCreateIfMissing;
	Action->SortMethod = SortMethod;
	Action->DisplayType = DisplayType;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncFindLeaderboard::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false, 0);
		return;
	}

	StatsSubsystem->OnLeaderboardFound.AddDynamic(this, &USteamAsyncFindLeaderboard::HandleLeaderboardFound);

	const bool bStarted = bCreateIfMissing
		? StatsSubsystem->FindOrCreateLeaderboard(RequestedName, SortMethod, DisplayType)
		: StatsSubsystem->FindLeaderboard(RequestedName);
	if (!bStarted)
	{
		Complete(false, 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncFindLeaderboard::HandleLeaderboardFound(bool bSuccess, int64 LeaderboardHandle, FString LeaderboardName)
{
	if (LeaderboardName != RequestedName)
	{
		return;
	}
	Complete(bSuccess, LeaderboardHandle);
}

void USteamAsyncFindLeaderboard::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncFindLeaderboard::Complete(bool bSuccess, int64 LeaderboardHandle)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardFound.RemoveDynamic(this, &USteamAsyncFindLeaderboard::HandleLeaderboardFound);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(LeaderboardHandle);
	}
	else
	{
		OnFailure.Broadcast(LeaderboardHandle);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncUploadLeaderboardScore ----

USteamAsyncUploadLeaderboardScore* USteamAsyncUploadLeaderboardScore::UploadLeaderboardScore(UObject* WorldContext, int64 LeaderboardHandle, int32 Score, bool bForceUpdate, float Timeout)
{
	USteamAsyncUploadLeaderboardScore* Action = NewObject<USteamAsyncUploadLeaderboardScore>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->Score = Score;
	Action->bForceUpdate = bForceUpdate;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncUploadLeaderboardScore::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false, 0);
		return;
	}

	StatsSubsystem->OnLeaderboardScoreUploaded.AddDynamic(this, &USteamAsyncUploadLeaderboardScore::HandleScoreUploaded);

	if (!StatsSubsystem->UploadLeaderboardScore(LeaderboardHandle, Score, bForceUpdate))
	{
		Complete(false, 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncUploadLeaderboardScore::HandleScoreUploaded(bool bSuccess, int32 UploadedScore, bool bScoreChanged, int32 NewGlobalRank)
{
	// NOTE: FOnSteamLeaderboardScoreUploaded carries no leaderboard handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, NewGlobalRank);
}

void USteamAsyncUploadLeaderboardScore::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncUploadLeaderboardScore::Complete(bool bSuccess, int32 NewGlobalRank)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardScoreUploaded.RemoveDynamic(this, &USteamAsyncUploadLeaderboardScore::HandleScoreUploaded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(NewGlobalRank);
	}
	else
	{
		OnFailure.Broadcast(NewGlobalRank);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncDownloadLeaderboardEntries ----

USteamAsyncDownloadLeaderboardEntries* USteamAsyncDownloadLeaderboardEntries::DownloadLeaderboardEntries(UObject* WorldContext, int64 LeaderboardHandle, EESteamLeaderboardDataRequest RequestType, int32 RangeStart, int32 RangeEnd, float Timeout)
{
	USteamAsyncDownloadLeaderboardEntries* Action = NewObject<USteamAsyncDownloadLeaderboardEntries>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->RequestType = RequestType;
	Action->RangeStart = RangeStart;
	Action->RangeEnd = RangeEnd;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncDownloadLeaderboardEntries::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
		return;
	}

	StatsSubsystem->OnLeaderboardEntriesDownloaded.AddDynamic(this, &USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded);

	if (!StatsSubsystem->DownloadLeaderboardEntries(LeaderboardHandle, RequestType, RangeStart, RangeEnd))
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	// NOTE: FOnSteamLeaderboardEntriesDownloaded carries no leaderboard handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Entries);
}

void USteamAsyncDownloadLeaderboardEntries::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamLeaderboardEntry>());
}

void USteamAsyncDownloadLeaderboardEntries::Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardEntriesDownloaded.RemoveDynamic(this, &USteamAsyncDownloadLeaderboardEntries::HandleEntriesDownloaded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Entries);
	}
	else
	{
		OnFailure.Broadcast(Entries);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncRequestUserStats ----

USteamAsyncRequestUserStats* USteamAsyncRequestUserStats::RequestUserStats(UObject* WorldContext, FESteamId TargetUser, float Timeout)
{
	USteamAsyncRequestUserStats* Action = NewObject<USteamAsyncRequestUserStats>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->TargetUser = TargetUser;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestUserStats::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false);
		return;
	}

	StatsSubsystem->OnUserStatsReceived.AddDynamic(this, &USteamAsyncRequestUserStats::HandleUserStatsReceived);

	if (!StatsSubsystem->RequestUserStats(TargetUser))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestUserStats::HandleUserStatsReceived(FESteamId SteamId, bool bSuccess)
{
	if (SteamId != TargetUser)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncRequestUserStats::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncRequestUserStats::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnUserStatsReceived.RemoveDynamic(this, &USteamAsyncRequestUserStats::HandleUserStatsReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncGetNumberOfCurrentPlayers ----

USteamAsyncGetNumberOfCurrentPlayers* USteamAsyncGetNumberOfCurrentPlayers::GetNumberOfCurrentPlayers(UObject* WorldContext, float Timeout)
{
	USteamAsyncGetNumberOfCurrentPlayers* Action = NewObject<USteamAsyncGetNumberOfCurrentPlayers>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetNumberOfCurrentPlayers::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false, 0);
		return;
	}

	StatsSubsystem->OnNumberOfCurrentPlayers.AddDynamic(this, &USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers);

	if (!StatsSubsystem->GetNumberOfCurrentPlayers())
	{
		Complete(false, 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers(bool bSuccess, int32 PlayerCount)
{
	Complete(bSuccess, PlayerCount);
}

void USteamAsyncGetNumberOfCurrentPlayers::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncGetNumberOfCurrentPlayers::Complete(bool bSuccess, int32 PlayerCount)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnNumberOfCurrentPlayers.RemoveDynamic(this, &USteamAsyncGetNumberOfCurrentPlayers::HandleNumberOfCurrentPlayers);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PlayerCount);
	}
	else
	{
		OnFailure.Broadcast(PlayerCount);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncRequestGlobalStats ----

USteamAsyncRequestGlobalStats* USteamAsyncRequestGlobalStats::RequestGlobalStats(UObject* WorldContext, int32 HistoryDays, float Timeout)
{
	USteamAsyncRequestGlobalStats* Action = NewObject<USteamAsyncRequestGlobalStats>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->HistoryDays = HistoryDays;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestGlobalStats::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false);
		return;
	}

	StatsSubsystem->OnGlobalStatsReceived.AddDynamic(this, &USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived);

	if (!StatsSubsystem->RequestGlobalStats(HistoryDays))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived(bool bSuccess)
{
	Complete(bSuccess);
}

void USteamAsyncRequestGlobalStats::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncRequestGlobalStats::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnGlobalStatsReceived.RemoveDynamic(this, &USteamAsyncRequestGlobalStats::HandleGlobalStatsReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncRequestGlobalAchievementPercentages ----

USteamAsyncRequestGlobalAchievementPercentages* USteamAsyncRequestGlobalAchievementPercentages::RequestGlobalAchievementPercentages(UObject* WorldContext, float Timeout)
{
	USteamAsyncRequestGlobalAchievementPercentages* Action = NewObject<USteamAsyncRequestGlobalAchievementPercentages>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestGlobalAchievementPercentages::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false);
		return;
	}

	StatsSubsystem->OnGlobalAchievementPercentagesReady.AddDynamic(this, &USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady);

	if (!StatsSubsystem->RequestGlobalAchievementPercentages())
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady(bool bSuccess)
{
	Complete(bSuccess);
}

void USteamAsyncRequestGlobalAchievementPercentages::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncRequestGlobalAchievementPercentages::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnGlobalAchievementPercentagesReady.RemoveDynamic(this, &USteamAsyncRequestGlobalAchievementPercentages::HandlePercentagesReady);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncDownloadLeaderboardEntriesForUsers ----

USteamAsyncDownloadLeaderboardEntriesForUsers* USteamAsyncDownloadLeaderboardEntriesForUsers::DownloadLeaderboardEntriesForUsers(UObject* WorldContext, int64 LeaderboardHandle, const TArray<FESteamId>& Users, float Timeout)
{
	USteamAsyncDownloadLeaderboardEntriesForUsers* Action = NewObject<USteamAsyncDownloadLeaderboardEntriesForUsers>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->Users = Users;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
		return;
	}

	StatsSubsystem->OnLeaderboardEntriesDownloaded.AddDynamic(this, &USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded);

	if (!StatsSubsystem->DownloadLeaderboardEntriesForUsers(LeaderboardHandle, Users))
	{
		Complete(false, TArray<FESteamLeaderboardEntry>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	// NOTE: FOnSteamLeaderboardEntriesDownloaded carries no leaderboard handle; cannot discriminate.
	// Relies on Timeout + the subsystem's single-in-flight download queue.
	Complete(bSuccess, Entries);
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamLeaderboardEntry>());
}

void USteamAsyncDownloadLeaderboardEntriesForUsers::Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardEntriesDownloaded.RemoveDynamic(this, &USteamAsyncDownloadLeaderboardEntriesForUsers::HandleEntriesDownloaded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Entries);
	}
	else
	{
		OnFailure.Broadcast(Entries);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncAttachLeaderboardUGC ----

USteamAsyncAttachLeaderboardUGC* USteamAsyncAttachLeaderboardUGC::AttachLeaderboardUGC(UObject* WorldContext, int64 LeaderboardHandle, int64 UGCHandle, float Timeout)
{
	USteamAsyncAttachLeaderboardUGC* Action = NewObject<USteamAsyncAttachLeaderboardUGC>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUserStatsSubsystem(WorldContext);
	Action->LeaderboardHandle = LeaderboardHandle;
	Action->UGCHandle = UGCHandle;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncAttachLeaderboardUGC::Activate()
{
	UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get();
	if (!StatsSubsystem)
	{
		Complete(false);
		return;
	}

	StatsSubsystem->OnLeaderboardUGCSet.AddDynamic(this, &USteamAsyncAttachLeaderboardUGC::HandleUGCSet);

	if (!StatsSubsystem->AttachLeaderboardUGC(LeaderboardHandle, UGCHandle))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncAttachLeaderboardUGC::HandleUGCSet(bool bSuccess)
{
	// NOTE: FOnSteamLeaderboardUGCSet carries no leaderboard handle; cannot discriminate.
	// Relies on Timeout + the subsystem's single-in-flight attach-UGC queue.
	Complete(bSuccess);
}

void USteamAsyncAttachLeaderboardUGC::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncAttachLeaderboardUGC::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUserStatsSubsystem* StatsSubsystem = Subsystem.Get())
	{
		StatsSubsystem->OnLeaderboardUGCSet.RemoveDynamic(this, &USteamAsyncAttachLeaderboardUGC::HandleUGCSet);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

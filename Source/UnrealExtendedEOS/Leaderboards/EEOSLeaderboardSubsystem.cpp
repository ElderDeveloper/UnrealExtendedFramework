// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLeaderboardSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSLeaderboardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSLeaderboardSubsystem::Deinitialize()
{
	CachedEntries.Empty();
	Super::Deinitialize();
}

void UEEOSLeaderboardSubsystem::QueryLeaderboard(const FString& LeaderboardId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboard"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboard — Leaderboards interface not available"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	FOnlineLeaderboardReadRef ReadRef = MakeShareable(new FOnlineLeaderboardRead());
	ReadRef->LeaderboardName = LeaderboardId;
	CachedLeaderboardRead = ReadRef;

	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.RemoveAll(this);
	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.AddUObject(this, &UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete);
	LeaderboardInterface->ReadLeaderboards(TArray<FUniqueNetIdRef>(), ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboard — Querying '%s'..."), *LeaderboardId);
}

void UEEOSLeaderboardSubsystem::QueryFriendsLeaderboard(const FString& LeaderboardId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryFriendsLeaderboard"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	FOnlineLeaderboardReadRef ReadRef = MakeShareable(new FOnlineLeaderboardRead());
	ReadRef->LeaderboardName = LeaderboardId;
	CachedLeaderboardRead = ReadRef;

	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.RemoveAll(this);
	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.AddUObject(this, &UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete);
	LeaderboardInterface->ReadLeaderboardsForFriends(0, ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryFriendsLeaderboard — Querying friends for '%s'..."), *LeaderboardId);
}

void UEEOSLeaderboardSubsystem::QueryLeaderboardAroundPlayer(const FString& LeaderboardId, int32 Range)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboardAroundPlayer"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	FOnlineLeaderboardReadRef ReadRef = MakeShareable(new FOnlineLeaderboardRead());
	ReadRef->LeaderboardName = LeaderboardId;
	CachedLeaderboardRead = ReadRef;

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.RemoveAll(this);
	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.AddUObject(this, &UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete);
	LeaderboardInterface->ReadLeaderboardsAroundUser(UserId->AsShared(), static_cast<uint32>(Range), ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardAroundPlayer — '%s' (range=%d)"), *LeaderboardId, Range);
}

void UEEOSLeaderboardSubsystem::QueryLeaderboardByRange(const FString& LeaderboardId, int32 StartRank, int32 EndRank)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboardByRange"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return;
	}

	FOnlineLeaderboardReadRef ReadRef = MakeShareable(new FOnlineLeaderboardRead());
	ReadRef->LeaderboardName = LeaderboardId;
	CachedLeaderboardRead = ReadRef;

	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.RemoveAll(this);
	LeaderboardInterface->OnLeaderboardReadCompleteDelegates.AddUObject(this, &UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete);
	LeaderboardInterface->ReadLeaderboardsAroundRank(StartRank, static_cast<uint32>(EndRank - StartRank + 1), ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardByRange — '%s' [%d-%d]"), *LeaderboardId, StartRank, EndRank);
}

void UEEOSLeaderboardSubsystem::UploadScore(const FString& LeaderboardId, int32 Score)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UploadScore"));
		OnScoreUploaded.Broadcast(false, LeaderboardId);
		return;
	}

	// EOS leaderboards are stat-driven — scores are ingested via Stats, leaderboards auto-update
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::UploadScore — '%s' = %d (use Stats::IngestStat for EOS leaderboards)"), *LeaderboardId, Score);
	OnScoreUploaded.Broadcast(true, LeaderboardId);
}

// ── Queries ──────────────────────────────────────────────────────────────────

TArray<FEEOSLeaderboardEntry> UEEOSLeaderboardSubsystem::GetLeaderboardEntries() const
{
	return CachedEntries;
}

int32 UEEOSLeaderboardSubsystem::GetLocalPlayerRank() const
{
	if (!IsEOSAvailable()) return -1;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return -1;

	FString LocalId = UserId->ToString();
	for (const auto& Entry : CachedEntries)
	{
		if (Entry.UserId == LocalId)
		{
			return Entry.Rank;
		}
	}
	return -1;
}

int32 UEEOSLeaderboardSubsystem::GetLocalPlayerScore() const
{
	if (!IsEOSAvailable()) return 0;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return 0;

	FString LocalId = UserId->ToString();
	for (const auto& Entry : CachedEntries)
	{
		if (Entry.UserId == LocalId)
		{
			return Entry.Score;
		}
	}
	return 0;
}

int32 UEEOSLeaderboardSubsystem::GetEntryCount() const
{
	return CachedEntries.Num();
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete(bool bWasSuccessful)
{
	CachedEntries.Empty();

	if (bWasSuccessful && CachedLeaderboardRead.IsValid())
	{
		for (int32 i = 0; i < CachedLeaderboardRead->Rows.Num(); ++i)
		{
			const FOnlineStatsRow& Row = CachedLeaderboardRead->Rows[i];

			FEEOSLeaderboardEntry Entry;
			Entry.Rank = Row.Rank;
			Entry.UserId = Row.PlayerId.IsValid() ? Row.PlayerId->ToString() : TEXT("");
			Entry.DisplayName = Row.NickName;

			// Extract the score from the first column (primary stat)
			if (Row.Columns.Num() > 0)
			{
				for (const auto& Column : Row.Columns)
				{
					FVariantData Value = Column.Value;
					if (Value.GetType() == EOnlineKeyValuePairDataType::Int32)
					{
						int32 IntScore = 0;
						Value.GetValue(IntScore);
						Entry.Score = IntScore;
					}
					else if (Value.GetType() == EOnlineKeyValuePairDataType::Float)
					{
						float FloatScore = 0.f;
						Value.GetValue(FloatScore);
						Entry.Score = static_cast<int32>(FloatScore);
					}
					else if (Value.GetType() == EOnlineKeyValuePairDataType::Int64)
					{
						int64 Int64Score = 0;
						Value.GetValue(Int64Score);
						Entry.Score = static_cast<int32>(Int64Score);
					}
					break; // Use the first column as the score
				}
			}

			CachedEntries.Add(Entry);
		}
	}

	CachedLeaderboardRead.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem: Leaderboard read %s (%d entries)"),
		bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), CachedEntries.Num());
	OnLeaderboardQueried.Broadcast(bWasSuccessful, CachedEntries);

	// Clean up delegate
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineLeaderboardsPtr LB = EOSSub->GetLeaderboardsInterface();
		if (LB.IsValid())
		{
			LB->OnLeaderboardReadCompleteDelegates.RemoveAll(this);
		}
	}
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLeaderboardSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#include "eos_sdk.h"
#include "eos_stats.h"

/** Mirrors the engine-private EOS_MAX_NUM_RANKINGS (OnlineLeaderboardsEOS.h:13) — the EOS OSS
 *  rejects ReadLeaderboardsAroundRank with a center rank above this and clamps its copy window
 *  to it, i.e. only the top 1000 rankings are ever retrievable. */
static constexpr int32 GMaxEOSRankings = 1000;

void UEEOSLeaderboardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSLeaderboardSubsystem::Deinitialize()
{
	// Clear our binding on the interface-wide read-complete list if a query is still in flight
	if (LeaderboardReadCompleteHandle.IsValid())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
			if (LeaderboardInterface.IsValid())
			{
				LeaderboardInterface->ClearOnLeaderboardReadCompleteDelegate_Handle(LeaderboardReadCompleteHandle);
			}
		}
		LeaderboardReadCompleteHandle.Reset();
	}

	CachedLeaderboardRead.Reset();
	CachedEntries.Empty();
	PendingRankMin = -1;
	PendingRankMax = -1;
	Super::Deinitialize();
}

FOnlineLeaderboardReadRef UEEOSLeaderboardSubsystem::MakeLeaderboardReadObject(const FString& LeaderboardId) const
{
	FOnlineLeaderboardReadRef ReadRef = MakeShareable(new FOnlineLeaderboardRead());
	ReadRef->LeaderboardName = LeaderboardId;

	// The EOS OSS reads scores by STAT name through SortedColumn + ColumnMetadata, NOT through
	// LeaderboardName: ReadLeaderboards() builds its EOS_Leaderboards_QueryLeaderboardUserScores
	// StatInfo array from ColumnMetadata (OnlineLeaderboardsEOS.cpp:219–232) and sorts rows by
	// SortedColumn (:292). A read object without them requests zero stats and never returns a
	// score. Rank-window reads (ReadLeaderboardsAroundRank) use them too: with exactly one
	// metadata entry matching SortedColumn the engine takes the single-query path and keys each
	// row's score under SortedColumn (:434–435, :476). We use the leaderboard id as the stat
	// name — for the stat-name query paths the caller must pass the backing stat's name.
	ReadRef->SortedColumn = LeaderboardId;
	ReadRef->ColumnMetadata.Add(FColumnMetaData(LeaderboardId, EOnlineKeyValuePairDataType::Int32));
	return ReadRef;
}

bool UEEOSLeaderboardSubsystem::RejectIfQueryInFlight(const TCHAR* FunctionName, const FString& LeaderboardId)
{
	// In-flight = our read object exists, regardless of ReadState: the engine's deferred-
	// failure paths (zero-friends, rank > 1000) never move the state past NotStarted, and a
	// second query stomping the object/binding in that window would leak the first binding.
	// Log-only rejection — broadcasting a failure here would be indistinguishable from the
	// in-flight query's real completion for its waiters on the shared delegate (R1).
	if (CachedLeaderboardRead.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::%s — A leaderboard query ('%s') is already in flight, rejecting '%s' (no broadcast)"),
			FunctionName, *CachedLeaderboardRead->LeaderboardName, *LeaderboardId);
		return true;
	}
	return false;
}

void UEEOSLeaderboardSubsystem::BindLeaderboardReadDelegate(IOnlineLeaderboardsPtr LeaderboardInterface)
{
	// Bind with a handle on the interface-wide list — never RemoveAll(this): other systems
	// (or a future second binding) must not be wiped when this one op is cleaned up
	LeaderboardReadCompleteHandle = LeaderboardInterface->AddOnLeaderboardReadCompleteDelegate_Handle(
		FOnLeaderboardReadCompleteDelegate::CreateUObject(this, &UEEOSLeaderboardSubsystem::HandleLeaderboardReadComplete));
}

bool UEEOSLeaderboardSubsystem::QueryLeaderboard(const FString& LeaderboardId, int32 MaxEntries)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboard"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboard — Leaderboards interface not available"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	if (RejectIfQueryInFlight(TEXT("QueryLeaderboard"), LeaderboardId))
	{
		return false;
	}

	// ReadLeaderboards() with an empty player array fails immediately in the EOS OSS
	// (OnlineLeaderboardsEOS.cpp:197–204) — a global read must go through the rank-window
	// path. ReadLeaderboardsAroundRank(Rank, Range) copies the 0-based record indices
	// [clamp(Rank-Range, 0, 1000), clamp(Rank+Range, 0, 999)] (:363–364), so for the top N
	// entries (indices 0..N-1) center the window on (N-1)/2. Odd N over-fetches one record
	// at the end; the completion handler trims to the requested rank window.
	const int32 ClampedN = FMath::Clamp(MaxEntries, 1, GMaxEOSRankings);
	if (ClampedN != MaxEntries)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboard — MaxEntries %d clamped to %d (EOS serves at most the top %d rankings)"),
			MaxEntries, ClampedN, GMaxEOSRankings);
	}
	const int32 CenterIndex = (ClampedN - 1) / 2;
	const uint32 Range = static_cast<uint32>((ClampedN - 1) - CenterIndex);

	FOnlineLeaderboardReadRef ReadRef = MakeLeaderboardReadObject(LeaderboardId);
	CachedLeaderboardRead = ReadRef;
	PendingRankMin = 1;
	PendingRankMax = ClampedN;

	BindLeaderboardReadDelegate(LeaderboardInterface);
	LeaderboardInterface->ReadLeaderboardsAroundRank(CenterIndex, Range, ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboard — Querying top %d of '%s'..."), ClampedN, *LeaderboardId);
	return true;
}

bool UEEOSLeaderboardSubsystem::QueryFriendsLeaderboard(const FString& LeaderboardId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryFriendsLeaderboard"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	if (RejectIfQueryInFlight(TEXT("QueryFriendsLeaderboard"), LeaderboardId))
	{
		return false;
	}

	// SortedColumn + ColumnMetadata are mandatory here: this routes into ReadLeaderboards(),
	// whose per-user EOS_Leaderboards_QueryLeaderboardUserScores requests exactly the stats
	// listed in ColumnMetadata — with none, it can never return a score.
	FOnlineLeaderboardReadRef ReadRef = MakeLeaderboardReadObject(LeaderboardId);
	CachedLeaderboardRead = ReadRef;

	BindLeaderboardReadDelegate(LeaderboardInterface);
	LeaderboardInterface->ReadLeaderboardsForFriends(0, ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryFriendsLeaderboard — Querying friends for '%s'..."), *LeaderboardId);
	return true;
}

bool UEEOSLeaderboardSubsystem::QueryLeaderboardAroundPlayer(const FString& LeaderboardId, int32 Range)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboardAroundPlayer"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	if (RejectIfQueryInFlight(TEXT("QueryLeaderboardAroundPlayer"), LeaderboardId))
	{
		return false;
	}

	FUniqueNetIdPtr UserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	// SortedColumn + ColumnMetadata are mandatory here: ReadLeaderboardsAroundUser() is a thin
	// wrapper over ReadLeaderboards() (user-scores path), which requests exactly the stats
	// listed in ColumnMetadata — with none, it can never return a score.
	FOnlineLeaderboardReadRef ReadRef = MakeLeaderboardReadObject(LeaderboardId);
	CachedLeaderboardRead = ReadRef;

	BindLeaderboardReadDelegate(LeaderboardInterface);
	LeaderboardInterface->ReadLeaderboardsAroundUser(UserId->AsShared(), static_cast<uint32>(Range), ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardAroundPlayer — '%s' (range=%d)"), *LeaderboardId, Range);
	return true;
}

bool UEEOSLeaderboardSubsystem::QueryLeaderboardByRange(const FString& LeaderboardId, int32 StartRank, int32 EndRank)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryLeaderboardByRange"));
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineLeaderboardsPtr LeaderboardInterface = EOSSub->GetLeaderboardsInterface();
	if (!LeaderboardInterface.IsValid())
	{
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}

	if (RejectIfQueryInFlight(TEXT("QueryLeaderboardByRange"), LeaderboardId))
	{
		return false;
	}

	// Ranks are 1-based and inclusive. EOS only serves the top 1000 rankings, and the OSS
	// rejects a center rank above that (OnlineLeaderboardsEOS.cpp:352–360).
	if (StartRank < 1)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardByRange — StartRank %d clamped to 1 (ranks are 1-based)"), StartRank);
		StartRank = 1;
	}
	if (EndRank < StartRank || StartRank > GMaxEOSRankings)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardByRange — Invalid range [%d-%d] for '%s'"), StartRank, EndRank, *LeaderboardId);
		OnLeaderboardQueried.Broadcast(false, TArray<FEEOSLeaderboardEntry>());
		return false;
	}
	if (EndRank > GMaxEOSRankings)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardByRange — EndRank %d clamped to %d (EOS serves at most the top %d rankings)"),
			EndRank, GMaxEOSRankings, GMaxEOSRankings);
		EndRank = GMaxEOSRankings;
	}

	// ReadLeaderboardsAroundRank(Rank, Range) is a CENTER + range window over 0-based record
	// indices: it copies [clamp(Rank-Range, 0, 1000), clamp(Rank+Range, 0, 999)]
	// (OnlineLeaderboardsEOS.cpp:363–364). Convert the 1-based rank window [StartRank, EndRank]
	// to the index window [StartRank-1, EndRank-1]: center on its midpoint and round the range
	// up. Even-sized windows over-fetch one record at the front; the completion handler trims
	// rows back to [StartRank, EndRank] by their EOS rank.
	const int32 StartIndex = StartRank - 1;
	const int32 EndIndex = EndRank - 1;
	const int32 CenterIndex = (StartIndex + EndIndex) / 2;
	const uint32 Range = static_cast<uint32>(EndIndex - CenterIndex);

	FOnlineLeaderboardReadRef ReadRef = MakeLeaderboardReadObject(LeaderboardId);
	CachedLeaderboardRead = ReadRef;
	PendingRankMin = StartRank;
	PendingRankMax = EndRank;

	BindLeaderboardReadDelegate(LeaderboardInterface);
	LeaderboardInterface->ReadLeaderboardsAroundRank(CenterIndex, Range, ReadRef);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::QueryLeaderboardByRange — '%s' [%d-%d]"), *LeaderboardId, StartRank, EndRank);
	return true;
}

bool UEEOSLeaderboardSubsystem::UploadScore(const FString& LeaderboardId, int32 Score)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UploadScore"));
		OnScoreUploaded.Broadcast(false, LeaderboardId);
		return false;
	}

	// EOS leaderboards are stat-backed — the score upload IS a stat ingest; the backend
	// re-ranks the leaderboard from ingested stats. The OSS stats path is not used because
	// FOnlineStatsEOS::UpdateStats() executes its completion delegate synchronously with
	// Success without waiting for EOS_Stats_IngestStat (OnlineStatsEOS.cpp:368) — ingesting
	// through the raw SDK is the only way to broadcast the real async result.
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	EOS_HStats StatsHandle = PlatformHandle ? EOS_Platform_GetStatsInterface(PlatformHandle) : nullptr;
	if (!StatsHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLeaderboardSubsystem::UploadScore — Stats interface not available"));
		OnScoreUploaded.Broadcast(false, LeaderboardId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	FUniqueNetIdPtr LocalUserId = EOSSub->GetIdentityInterface()->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::UploadScore — User not logged in"));
		OnScoreUploaded.Broadcast(false, LeaderboardId);
		return false;
	}

	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — the ingest needs the
	// bare PUID, and EOS_ProductUserId_FromString performs NO validation, so guard the string.
	const FString LocalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(LocalUserId->ToString());
	if (LocalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSLeaderboardSubsystem::UploadScore — Logged-in user has no Product User ID (no Connect session)"));
		OnScoreUploaded.Broadcast(false, LeaderboardId);
		return false;
	}
	const EOS_ProductUserId LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*LocalPUIDStr));

	// EOS stat names are upper case in the Dev Portal; mirror the engine's stats path, which
	// uppercases the outgoing name and warns (OnlineStatsEOS.cpp:294–300).
	const FString StatName = LeaderboardId.ToUpper();
	if (StatName != LeaderboardId)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::UploadScore — Stat name '%s' is not upper case; ingesting as '%s' (EOS stat names are upper case in the Dev Portal)"),
			*LeaderboardId, *StatName);
	}
	const FTCHARToUTF8 StatNameUtf8(*StatName);

	EOS_Stats_IngestData IngestData = {};
	IngestData.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
	IngestData.StatName = StatNameUtf8.Get();
	IngestData.IngestAmount = Score;

	EOS_Stats_IngestStatOptions Options = {};
	Options.ApiVersion = EOS_STATS_INGESTSTAT_API_LATEST;
	Options.LocalUserId = LocalPUID;
	Options.TargetUserId = LocalPUID;
	Options.Stats = &IngestData;
	Options.StatsCount = 1;

	struct FUploadScoreContext
	{
		TWeakObjectPtr<UEEOSLeaderboardSubsystem> Self;
		FString LeaderboardId;
	};

	FUploadScoreContext* Context = new FUploadScoreContext();
	Context->Self = this;
	Context->LeaderboardId = LeaderboardId;

	EOS_Stats_IngestStat(StatsHandle, &Options, Context,
		[](const EOS_Stats_IngestStatCompleteCallbackInfo* Data)
		{
			TUniquePtr<FUploadScoreContext> Ctx(static_cast<FUploadScoreContext*>(Data->ClientData));
			if (!Ctx) return;

			UEEOSLeaderboardSubsystem* Self = Ctx->Self.Get();
			if (!Self) return;

			const bool bSuccess = Data->ResultCode == EOS_EResult::EOS_Success;
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem: Score ingested for '%s'"), *Ctx->LeaderboardId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLeaderboardSubsystem::UploadScore — EOS_Stats_IngestStat failed for '%s': %s"),
					*Ctx->LeaderboardId, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnScoreUploaded.Broadcast(bSuccess, Ctx->LeaderboardId);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem::UploadScore — Ingesting '%s' = %d..."), *StatName, Score);
	return true;
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
	// The read-complete list is interface-wide: any other system's leaderboard read also lands
	// here. Only consume a completion that belongs to us — i.e. our pending read object exists
	// and its state is not InProgress. NotStarted-while-bound is deliberately CONSUMABLE
	// (terminal): the engine's deferred-failure paths — zero-friends ReadLeaderboardsForFriends
	// (OnlineLeaderboardsEOS.cpp:328-335) and rank > 1000 (:352-359) — fire the completion
	// next tick without ever setting the read state, so requiring Done/Failed here would
	// ignore our own failure and wedge the in-flight guard forever (same class as the
	// session-family zero-result fix). A genuinely InProgress read keeps the binding and
	// waits for its own completion.
	if (!CachedLeaderboardRead.IsValid() || CachedLeaderboardRead->ReadState == EOnlineAsyncTaskState::InProgress)
	{
		return;
	}

	CachedEntries.Empty();

	// Rank-window queries (top-N / by-range) over-fetch by up to one record because the
	// engine's AroundRank is a symmetric (center, range) window — trim back to what was asked.
	const int32 RankMin = PendingRankMin;
	const int32 RankMax = PendingRankMax;
	PendingRankMin = -1;
	PendingRankMax = -1;

	if (bWasSuccessful)
	{
		for (int32 i = 0; i < CachedLeaderboardRead->Rows.Num(); ++i)
		{
			const FOnlineStatsRow& Row = CachedLeaderboardRead->Rows[i];

			if (RankMin >= 0 && (Row.Rank < RankMin || Row.Rank > RankMax))
			{
				continue;
			}

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

	// Clear only our own handle — never RemoveAll(this) — and do it BEFORE broadcasting so a
	// listener can start a fresh query from the callback without us wiping its new binding
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		IOnlineLeaderboardsPtr LB = EOSSub->GetLeaderboardsInterface();
		if (LB.IsValid())
		{
			LB->ClearOnLeaderboardReadCompleteDelegate_Handle(LeaderboardReadCompleteHandle);
		}
	}
	LeaderboardReadCompleteHandle.Reset();

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSLeaderboardSubsystem: Leaderboard read %s (%d entries)"),
		bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), CachedEntries.Num());
	OnLeaderboardQueried.Broadcast(bWasSuccessful, CachedEntries);
}

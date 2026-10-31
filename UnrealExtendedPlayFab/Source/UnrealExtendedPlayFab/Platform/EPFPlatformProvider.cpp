// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFPlatformProvider.h"
#include "UnrealExtendedPlayFab.h"
#include "Auth/EPFAuthSubsystem.h"
#include "Leaderboards/EPFLeaderboardSubsystem.h"
#include "PlayerProfile/EPFPlayerProfileSubsystem.h"
#include "Shared/EPFSettings.h"
#include "Stats/EPFStatsSubsystem.h"

#include "Async/EFCallContext.h"
#include "Engine/GameInstance.h"
#include "Platform/EFPlatformSubsystem.h"

namespace
{
	/** Answers OnDone unless it is empty: direct callers may pass none. */
	template <typename FunctionType, typename... ArgTypes>
	void AnswerIfBound(const FunctionType& OnDone, ArgTypes&&... Args)
	{
		if (OnDone)
		{
			OnDone(Forward<ArgTypes>(Args)...);
		}
	}

	int64 ParseFirstScoreOr(const TArray<FString>& Scores, int32 Fallback)
	{
		return Scores.Num() > 0 ? FCString::Atoi64(*Scores[0]) : static_cast<int64>(Fallback);
	}
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool UEPFPlatformProvider::ShouldCreateSubsystem(UObject* Outer) const
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	return Super::ShouldCreateSubsystem(Outer) && Settings && Settings->bEnablePlatformProvider;
}

void UEPFPlatformProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	// Registers with the platform subsystem first. Nothing is available yet, as nobody is logged in.
	Super::Initialize(Collection);
	bShuttingDown = false;

	AuthSubsystem = Collection.InitializeDependency<UEPFAuthSubsystem>();
	StatsSubsystem = Collection.InitializeDependency<UEPFStatsSubsystem>();
	LeaderboardSubsystem = Collection.InitializeDependency<UEPFLeaderboardSubsystem>();
	ProfileSubsystem = Collection.InitializeDependency<UEPFPlayerProfileSubsystem>();

	if (UEPFStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		Stats->OnStatsUpdated.AddUniqueDynamic(this, &UEPFPlatformProvider::HandleStatsUpdated);
		Stats->OnStatsReceived.AddUniqueDynamic(this, &UEPFPlatformProvider::HandleStatsReceived);
	}
	if (UEPFLeaderboardSubsystem* Boards = LeaderboardSubsystem.Get())
	{
		Boards->OnLeaderboardReceived.AddUniqueDynamic(this, &UEPFPlatformProvider::HandleLeaderboardReceived);
	}
	if (UEPFAuthSubsystem* Auth = AuthSubsystem.Get())
	{
		Auth->OnLoginComplete.AddUniqueDynamic(this, &UEPFPlatformProvider::HandleLoginComplete);
	}
}

void UEPFPlatformProvider::Deinitialize()
{
	bShuttingDown = true;
	CancelAllCalls();

	if (UEPFStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		Stats->OnStatsUpdated.RemoveDynamic(this, &UEPFPlatformProvider::HandleStatsUpdated);
		Stats->OnStatsReceived.RemoveDynamic(this, &UEPFPlatformProvider::HandleStatsReceived);
	}
	if (UEPFLeaderboardSubsystem* Boards = LeaderboardSubsystem.Get())
	{
		Boards->OnLeaderboardReceived.RemoveDynamic(this, &UEPFPlatformProvider::HandleLeaderboardReceived);
	}
	if (UEPFAuthSubsystem* Auth = AuthSubsystem.Get())
	{
		Auth->OnLoginComplete.RemoveDynamic(this, &UEPFPlatformProvider::HandleLoginComplete);
	}

	AuthSubsystem.Reset();
	StatsSubsystem.Reset();
	LeaderboardSubsystem.Reset();
	ProfileSubsystem.Reset();

	// Unregisters from the platform subsystem; must come last.
	Super::Deinitialize();
}

// ── Provider ─────────────────────────────────────────────────────────────────

FName UEPFPlatformProvider::GetProviderName() const
{
	static const FName ProviderName(TEXT("PlayFab"));
	return ProviderName;
}

bool UEPFPlatformProvider::SupportsFeature(EEFPlatformFeature Feature) const
{
	// Everything else keeps the base class's NotAvailable answers, and the Local provider can take
	// those features over.
	const bool bServed = Feature == EEFPlatformFeature::Stats || Feature == EEFPlatformFeature::Leaderboards;
	return bServed && IsEnabledInSettings();
}

bool UEPFPlatformProvider::IsFeatureAvailable(EEFPlatformFeature Feature) const
{
	if (bShuttingDown || !SupportsFeature(Feature) || !HasEntitySession())
	{
		return false;
	}
	return Feature == EEFPlatformFeature::Stats ? StatsSubsystem.IsValid() : LeaderboardSubsystem.IsValid();
}

FEFPlatformUser UEPFPlatformProvider::GetLocalUser() const
{
	FEFPlatformUser User = Super::GetLocalUser();

	const UEPFAuthSubsystem* Auth = AuthSubsystem.Get();
	if (!Auth || !Auth->IsLoggedIn())
	{
		return User;
	}

	User.bSignedIn = true;
	User.UserId = Auth->GetPlayFabId();
	User.DisplayName = Auth->GetDisplayName();

	// The login response only has a display name when the title asks for it; a profile fetched
	// since may have one.
	if (User.DisplayName.IsEmpty())
	{
		if (const UEPFPlayerProfileSubsystem* Profiles = ProfileSubsystem.Get())
		{
			const FEPFPlayerProfile Profile = Profiles->GetCachedProfile();
			if (!User.UserId.IsEmpty() && Profile.PlayFabId == User.UserId)
			{
				User.DisplayName = Profile.DisplayName;
			}
		}
	}
	return User;
}

// ── Stats ────────────────────────────────────────────────────────────────────

void UEPFPlatformProvider::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	if (bShuttingDown)
	{
		AnswerIfBound(OnDone, FEFResult::Cancelled(GetProviderName()));
		return;
	}

	TMap<FString, int32> Values;
	FString Problem;
	if (!MakeStatisticUpdates(Updates, Values, Problem))
	{
		AnswerIfBound(OnDone, FEFResult::InvalidArgument(GetProviderName(), Problem));
		return;
	}
	if (Values.Num() == 0)
	{
		AnswerIfBound(OnDone, FEFResult::Success(GetProviderName()));
		return;
	}

	UEPFStatsSubsystem* Stats = StatsSubsystem.Get();
	if (!Stats)
	{
		AnswerIfBound(OnDone, FEFResult::NotAvailable(GetProviderName(), TEXT("UEPFStatsSubsystem")));
		return;
	}

	const int64 PendingId = StatWrites.Add(MoveTemp(OnDone));
	StartCall(EEPFPlatformCallKind::StatWrite, PendingId, [Stats, &Values]()
	{
		Stats->UpdateStats(Values);
	});
}

void UEPFPlatformProvider::QueryStats(const TArray<FString>& StatIds, FOnStats OnDone)
{
	if (bShuttingDown)
	{
		AnswerIfBound(OnDone, FEFResult::Cancelled(GetProviderName()), TArray<FEFStatValue>());
		return;
	}

	UEPFStatsSubsystem* Stats = StatsSubsystem.Get();
	if (!Stats)
	{
		AnswerIfBound(OnDone, FEFResult::NotAvailable(GetProviderName(), TEXT("UEPFStatsSubsystem")), TArray<FEFStatValue>());
		return;
	}

	const int64 PendingId = StatQueries.Add([StatIds, OnDone = MoveTemp(OnDone)](const FEFResult& Result, const TArray<FEPFStatistic>& PlayFabStats)
	{
		AnswerIfBound(OnDone, Result, Result.IsSuccess() ? UEPFPlatformProvider::ToPlatformStats(PlayFabStats, StatIds) : TArray<FEFStatValue>());
	});
	StartCall(EEPFPlatformCallKind::StatQuery, PendingId, [Stats, &StatIds]()
	{
		if (StatIds.Num() > 0)
		{
			Stats->GetStats(StatIds);
		}
		else
		{
			Stats->GetAllStats();
		}
	});
}

// ── Leaderboards ─────────────────────────────────────────────────────────────

void UEPFPlatformProvider::SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone)
{
	if (bShuttingDown)
	{
		AnswerIfBound(OnDone, FEFResult::Cancelled(GetProviderName()));
		return;
	}
	if (LeaderboardId.IsEmpty())
	{
		AnswerIfBound(OnDone, FEFResult::InvalidArgument(GetProviderName(), TEXT("SubmitScore needs a leaderboard id")));
		return;
	}

	UEPFStatsSubsystem* Stats = StatsSubsystem.Get();
	if (!Stats)
	{
		AnswerIfBound(OnDone, FEFResult::NotAvailable(GetProviderName(), TEXT("UEPFStatsSubsystem")));
		return;
	}

	// A PlayFab leaderboard ranks a statistic: reporting the statistic is the submission, and the
	// statistic's aggregation decides whether a lower score replaces a higher one.
	TMap<FString, int32> Values;
	Values.Add(LeaderboardId, ClampToInt32(Score));

	const int64 PendingId = StatWrites.Add(MoveTemp(OnDone));
	StartCall(EEPFPlatformCallKind::StatWrite, PendingId, [Stats, &Values]()
	{
		Stats->UpdateStats(Values);
	});
}

void UEPFPlatformProvider::QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	if (bShuttingDown)
	{
		AnswerIfBound(OnDone, FEFResult::Cancelled(GetProviderName()), TArray<FEFLeaderboardEntry>());
		return;
	}
	if (LeaderboardId.IsEmpty() || Count <= 0)
	{
		AnswerIfBound(OnDone, FEFResult::InvalidArgument(GetProviderName(), TEXT("QueryLeaderboard needs a leaderboard id and a Count above 0")), TArray<FEFLeaderboardEntry>());
		return;
	}

	UEPFLeaderboardSubsystem* Boards = LeaderboardSubsystem.Get();
	if (!Boards)
	{
		AnswerIfBound(OnDone, FEFResult::NotAvailable(GetProviderName(), TEXT("UEPFLeaderboardSubsystem")), TArray<FEFLeaderboardEntry>());
		return;
	}

	const UEPFAuthSubsystem* Auth = AuthSubsystem.Get();
	const FString LocalEntityId = Auth ? Auth->GetEntityId() : FString();
	const bool bAroundLocalUser = Range == EEFLeaderboardRange::AroundUser;

	const int64 PendingId = LeaderboardQueries.Add([LocalEntityId, Count, bAroundLocalUser, OnDone = MoveTemp(OnDone)](const FEFResult& Result, const TArray<FEPFLeaderboardEntry>& PlayFabEntries)
	{
		AnswerIfBound(OnDone, Result, Result.IsSuccess()
			? UEPFPlatformProvider::ToPlatformEntries(PlayFabEntries, LocalEntityId, Count, bAroundLocalUser)
			: TArray<FEFLeaderboardEntry>());
	});
	StartCall(EEPFPlatformCallKind::LeaderboardQuery, PendingId, [Boards, &LeaderboardId, Range, Count]()
	{
		switch (Range)
		{
		case EEFLeaderboardRange::AroundUser:
			// PlayFab returns this many entries besides the player's own.
			Boards->GetLeaderboardAroundPlayer(LeaderboardId, FMath::Max(Count - 1, 1));
			break;
		case EEFLeaderboardRange::Friends:
			Boards->GetFriendsLeaderboard(LeaderboardId, Count);
			break;
		case EEFLeaderboardRange::Top:
		default:
			Boards->GetLeaderboard(LeaderboardId, 0, Count);
			break;
		}
	});
}

// ── Subsystem broadcasts ─────────────────────────────────────────────────────

void UEPFPlatformProvider::HandleStatsUpdated(const FEPFResult& Result)
{
	const int64 PendingId = TakeActiveCall(EEPFPlatformCallKind::StatWrite);
	if (PendingId != 0)
	{
		// Work the caller starts from its callback must not carry this finished call's context.
		EFCallContext::FScope NoContext(0);
		StatWrites.Complete(PendingId, ToPlatformResult(Result, GetProviderName()));
	}
}

void UEPFPlatformProvider::HandleStatsReceived(const FEPFResult& Result, const TArray<FEPFStatistic>& PlayFabStats)
{
	// UpdateStats also broadcasts OnStatsReceived, under the write's context; TakeActiveCall checks
	// the kind, so that broadcast leaves the write waiting for its OnStatsUpdated.
	const int64 PendingId = TakeActiveCall(EEPFPlatformCallKind::StatQuery);
	if (PendingId != 0)
	{
		EFCallContext::FScope NoContext(0);
		StatQueries.Complete(PendingId, ToPlatformResult(Result, GetProviderName()), PlayFabStats);
	}
}

void UEPFPlatformProvider::HandleLeaderboardReceived(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& PlayFabEntries)
{
	const int64 PendingId = TakeActiveCall(EEPFPlatformCallKind::LeaderboardQuery);
	if (PendingId != 0)
	{
		EFCallContext::FScope NoContext(0);
		LeaderboardQueries.Complete(PendingId, ToPlatformResult(Result, GetProviderName()), PlayFabEntries);
	}
}

void UEPFPlatformProvider::HandleLoginComplete(const FEPFResult& Result, const FString& LoggedInPlayFabId)
{
	if (!Result.bSuccess || bShuttingDown)
	{
		return;
	}

	// Writes made before the login wait in the platform subsystem's queue; send them now rather
	// than at its next retry.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UEFPlatformSubsystem* Platform = GameInstance->GetSubsystem<UEFPlatformSubsystem>())
		{
			Platform->FlushQueuedWrites();
		}
	}
}

// ── Call tracking ────────────────────────────────────────────────────────────

void UEPFPlatformProvider::StartCall(EEPFPlatformCallKind Kind, int64 PendingId, TFunctionRef<void()> Call)
{
	const int64 CallContext = EFCallContext::New();

	FCall& Tracked = Calls.Add(CallContext);
	Tracked.Kind = Kind;
	Tracked.PendingId = PendingId;

	TWeakObjectPtr<UEPFPlatformProvider> WeakThis(this);
	Tracked.Timeout = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, CallContext](float) -> bool
	{
		if (UEPFPlatformProvider* This = WeakThis.Get())
		{
			This->TimeOutCall(CallContext);
		}
		return false;
	}), GetCallTimeoutSeconds());

	// UEPFSubsystem captures the active context when it sends the request and makes it active again
	// while the subsystem broadcasts the answer; that is how the handlers find this call. A request
	// that fails before it is sent is broadcast inside this scope, before Call returns.
	EFCallContext::FScope Scope(CallContext);
	Call();
}

int64 UEPFPlatformProvider::TakeActiveCall(EEPFPlatformCallKind Kind)
{
	const int64 CallContext = EFCallContext::GetActive();
	const FCall* Tracked = CallContext != 0 ? Calls.Find(CallContext) : nullptr;
	if (!Tracked || Tracked->Kind != Kind)
	{
		return 0;
	}

	const int64 PendingId = Tracked->PendingId;
	if (Tracked->Timeout.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Tracked->Timeout);
	}
	Calls.Remove(CallContext);
	return PendingId;
}

void UEPFPlatformProvider::TimeOutCall(int64 CallContext)
{
	FCall Tracked;
	if (!Calls.RemoveAndCopyValue(CallContext, Tracked))
	{
		return;
	}

	const float TimeoutSeconds = GetCallTimeoutSeconds();
	EF_LOG(ExtendedPlayFab, Warning, TEXT("EPFPlatformProvider — PlayFab did not answer a platform call within %.0fs; answering TimedOut."), TimeoutSeconds);

	const FEFResult TimedOutResult = FEFResult::TimedOut(GetProviderName(), TimeoutSeconds);
	switch (Tracked.Kind)
	{
	case EEPFPlatformCallKind::StatWrite:
		StatWrites.Complete(Tracked.PendingId, TimedOutResult);
		break;
	case EEPFPlatformCallKind::StatQuery:
		StatQueries.Complete(Tracked.PendingId, TimedOutResult, TArray<FEPFStatistic>());
		break;
	case EEPFPlatformCallKind::LeaderboardQuery:
		LeaderboardQueries.Complete(Tracked.PendingId, TimedOutResult, TArray<FEPFLeaderboardEntry>());
		break;
	}
}

void UEPFPlatformProvider::CancelAllCalls()
{
	for (const TPair<int64, FCall>& Pair : Calls)
	{
		if (Pair.Value.Timeout.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Pair.Value.Timeout);
		}
	}
	Calls.Reset();

	const FEFResult CancelledResult = FEFResult::Cancelled(GetProviderName());
	StatWrites.CompleteAll(CancelledResult);
	StatQueries.CompleteAll(CancelledResult, TArray<FEPFStatistic>());
	LeaderboardQueries.CompleteAll(CancelledResult, TArray<FEPFLeaderboardEntry>());
}

bool UEPFPlatformProvider::IsEnabledInSettings() const
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	return Settings && Settings->bEnablePlatformProvider && !Settings->TitleId.IsEmpty();
}

bool UEPFPlatformProvider::HasEntitySession() const
{
	// Progression statistics and leaderboards are entity APIs: they need the entity token.
	const UEPFAuthSubsystem* Auth = AuthSubsystem.Get();
	return Auth && Auth->IsLoggedIn() && !Auth->GetEntityToken().IsEmpty();
}

float UEPFPlatformProvider::GetCallTimeoutSeconds() const
{
	const UEPFSettings* Settings = UEPFSettings::Get();
	return FMath::Max(Settings ? Settings->PlatformCallTimeoutSeconds : 30.0f, 1.0f);
}

// ── Conversions ──────────────────────────────────────────────────────────────

int32 UEPFPlatformProvider::ClampToInt32(int64 Value)
{
	return static_cast<int32>(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32));
}

bool UEPFPlatformProvider::MakeStatisticUpdates(const TArray<FEFStatUpdate>& Updates, TMap<FString, int32>& OutValues, FString& OutProblem)
{
	OutValues.Reset();
	OutProblem.Reset();

	// Each value is clamped before it is added, so the int64 total cannot overflow.
	TMap<FString, int64> Totals;
	for (const FEFStatUpdate& Update : Updates)
	{
		if (Update.ProviderId.IsEmpty())
		{
			OutProblem = FString::Printf(TEXT("The update for stat %s has no PlayFab statistic name"), *Update.Id.ToString());
			return false;
		}

		const int64 Value = ClampToInt32(Update.Value);
		int64* Existing = Totals.Find(Update.ProviderId);
		if (Existing && Update.Mode == EEFStatUpdateMode::Increment)
		{
			*Existing += Value;
		}
		else
		{
			Totals.Add(Update.ProviderId, Value);
		}
	}

	for (const TPair<FString, int64>& Pair : Totals)
	{
		OutValues.Add(Pair.Key, ClampToInt32(Pair.Value));
	}
	return true;
}

TArray<FEFStatValue> UEPFPlatformProvider::ToPlatformStats(const TArray<FEPFStatistic>& PlayFabStats, const TArray<FString>& StatIds)
{
	TArray<FEFStatValue> Values;
	for (const FEPFStatistic& Stat : PlayFabStats)
	{
		if (Stat.StatName.IsEmpty() || (StatIds.Num() > 0 && !StatIds.Contains(Stat.StatName)))
		{
			continue;
		}
		if (Values.ContainsByPredicate([&Stat](const FEFStatValue& Added) { return Added.ProviderId == Stat.StatName; }))
		{
			continue;
		}

		FEFStatValue& Value = Values.AddDefaulted_GetRef();
		Value.ProviderId = Stat.StatName;
		Value.Value = ParseFirstScoreOr(Stat.Scores, Stat.Value);
	}
	return Values;
}

TArray<FEFLeaderboardEntry> UEPFPlatformProvider::ToPlatformEntries(const TArray<FEPFLeaderboardEntry>& PlayFabEntries, const FString& LocalEntityId, int32 Count, bool bAroundLocalUser)
{
	TArray<FEFLeaderboardEntry> Entries;
	Entries.Reserve(PlayFabEntries.Num());
	for (const FEPFLeaderboardEntry& PlayFabEntry : PlayFabEntries)
	{
		FEFLeaderboardEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Rank = PlayFabEntry.Position;
		Entry.UserId = PlayFabEntry.EntityId.IsEmpty() ? PlayFabEntry.PlayFabId : PlayFabEntry.EntityId;
		Entry.DisplayName = PlayFabEntry.DisplayName;
		Entry.Score = ParseFirstScoreOr(PlayFabEntry.Scores, PlayFabEntry.StatValue);
		Entry.bIsLocalUser = !LocalEntityId.IsEmpty() && Entry.UserId == LocalEntityId;
	}

	// PlayFab already ranks them; this keeps "best first" true whatever order they arrive in, with
	// unranked entries last.
	Entries.StableSort([](const FEFLeaderboardEntry& A, const FEFLeaderboardEntry& B)
	{
		const int32 RankA = A.Rank > 0 ? A.Rank : MAX_int32;
		const int32 RankB = B.Rank > 0 ? B.Rank : MAX_int32;
		return RankA < RankB;
	});

	if (Count > 0 && Entries.Num() > Count)
	{
		int32 First = 0;
		if (bAroundLocalUser)
		{
			const int32 LocalIndex = Entries.IndexOfByPredicate([](const FEFLeaderboardEntry& Entry) { return Entry.bIsLocalUser; });
			if (LocalIndex != INDEX_NONE)
			{
				First = FMath::Clamp(LocalIndex - (Count - 1) / 2, 0, Entries.Num() - Count);
			}
		}
		Entries = TArray<FEFLeaderboardEntry>(Entries.GetData() + First, Count);
	}
	return Entries;
}

FEFResult UEPFPlatformProvider::ToPlatformResult(const FEPFResult& Result, FName ProviderName)
{
	FEFResult Converted = Result.ToEFResult();
	Converted.Source = ProviderName;
	if (Converted.bSuccess)
	{
		return Converted;
	}

	if (Converted.Code.IsEmpty())
	{
		Converted.Code = EFResultCodes::Failed;
	}
	else if (Converted.Code.Equals(TEXT("MissingAuthentication"), ESearchCase::IgnoreCase))
	{
		Converted.Code = EFPlatformResultCodes::NotSignedIn;
	}
	else if (Converted.Code.Equals(TEXT("NotConfigured"), ESearchCase::IgnoreCase))
	{
		Converted.Code = EFResultCodes::NotAvailable;
	}
	else if (Converted.Code.EndsWith(TEXT("NotFound"), ESearchCase::IgnoreCase))
	{
		Converted.Message = FString::Printf(TEXT("%s: %s"), *Converted.Code, *Converted.Message);
		Converted.Code = EFPlatformResultCodes::NotFound;
	}
	return Converted;
}

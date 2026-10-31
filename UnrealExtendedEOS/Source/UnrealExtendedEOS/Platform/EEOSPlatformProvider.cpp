// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Platform/EEOSPlatformProvider.h"

#include "Achievements/EEOSAchievementSubsystem.h"
#include "Async/EFCallContext.h"
#include "Auth/EEOSAuthSubsystem.h"
#include "Auth/EEOSConnectSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlineStatsInterface.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "Leaderboards/EEOSLeaderboardSubsystem.h"
#include "OnlineSubsystem.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "Shared/EEOSLog.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSSettings.h"
#include "Social/EEOSPresenceSubsystem.h"
#include "Stats/EEOSStatsSubsystem.h"
#include "Storage/EEOSPlayerStorageSubsystem.h"
#include "UserInfo/EEOSUserInfoSubsystem.h"
#include "UObject/Class.h"

const FName UEEOSPlatformProvider::ProviderName(TEXT("EOS"));

namespace EEOSPlatformPrivate
{
	/** How often timeouts are checked and busy calls are started again. */
	constexpr float TickIntervalSeconds = 0.25f;

	/** EOS_STATS_MAX_INGEST_STATS (eos_stats_types.h): stats one ingest may carry. */
	constexpr int32 MaxIngestStats = 3000;

	/** EOS_PRESENCE_DATA_MAX_KEYS (eos_presence_types.h). The presence subsystem keeps one key ("RichPresence") for the rich text. */
	constexpr int32 MaxPresenceProperties = 32 - 1;

	/** EOS serves at most the top 1000 rankings (UEEOSLeaderboardSubsystem::GMaxEOSRankings). */
	constexpr int32 MaxRankings = 1000;

	/** The operation name UEEOSPresenceSubsystem reports its writes under. */
	const FName SetPresenceOperation(TEXT("SetPresence"));

	const TCHAR* KindToString(EEOSPlatform::ECallKind Kind)
	{
		switch (Kind)
		{
		case EEOSPlatform::ECallKind::UnlockAchievement: return TEXT("UnlockAchievement");
		case EEOSPlatform::ECallKind::QueryAchievements: return TEXT("QueryAchievements");
		case EEOSPlatform::ECallKind::UpdateStats:       return TEXT("UpdateStats");
		case EEOSPlatform::ECallKind::QueryStats:        return TEXT("QueryStats");
		case EEOSPlatform::ECallKind::SubmitScore:       return TEXT("SubmitScore");
		case EEOSPlatform::ECallKind::QueryLeaderboard:  return TEXT("QueryLeaderboard");
		case EEOSPlatform::ECallKind::WriteFile:         return TEXT("WriteCloudFile");
		case EEOSPlatform::ECallKind::ReadFile:          return TEXT("ReadCloudFile");
		case EEOSPlatform::ECallKind::DeleteFile:        return TEXT("DeleteCloudFile");
		case EEOSPlatform::ECallKind::ListFiles:         return TEXT("ListCloudFiles");
		case EEOSPlatform::ECallKind::SetPresence:       return TEXT("SetPresence");
		default:                                         return TEXT("Unknown");
		}
	}
}

// ── Helpers ─────────────────────────────────────────────────────────────────

int32 EEOSPlatform::ClampToInt32(int64 Value)
{
	return static_cast<int32>(FMath::Clamp<int64>(Value, TNumericLimits<int32>::Min(), TNumericLimits<int32>::Max()));
}

TMap<FString, int32> EEOSPlatform::ToIngestAmounts(const TArray<FEFStatUpdate>& Updates)
{
	// Running values stay inside int32, so adding two of them cannot overflow int64.
	TMap<FString, int64> Running;
	for (const FEFStatUpdate& Update : Updates)
	{
		const FString Name = (Update.ProviderId.IsEmpty() ? Update.Id.ToString() : Update.ProviderId).ToUpper();
		int64& Value = Running.FindOrAdd(Name);
		Value = Update.Mode == EEFStatUpdateMode::Increment
			? ClampToInt32(Value + ClampToInt32(Update.Value))
			: ClampToInt32(Update.Value);
	}

	TMap<FString, int32> Amounts;
	for (const TPair<FString, int64>& Pair : Running)
	{
		Amounts.Add(Pair.Key, ClampToInt32(Pair.Value));
	}
	return Amounts;
}

TArray<FEFLeaderboardEntry> EEOSPlatform::SliceAroundUser(const TArray<FEFLeaderboardEntry>& Ranked, int32 Count)
{
	TArray<FEFLeaderboardEntry> Window;
	const int32 UserIndex = Ranked.IndexOfByPredicate([](const FEFLeaderboardEntry& Entry) { return Entry.bIsLocalUser; });
	if (UserIndex == INDEX_NONE || Count <= 0)
	{
		return Window;
	}

	const int32 Size = FMath::Min(Count, Ranked.Num());
	const int32 First = FMath::Clamp(UserIndex - (Size - 1) / 2, 0, Ranked.Num() - Size);
	Window.Append(Ranked.GetData() + First, Size);
	return Window;
}

void EEOSPlatform::RankByScore(TArray<FEFLeaderboardEntry>& Entries, int32 Count)
{
	Entries.StableSort([](const FEFLeaderboardEntry& A, const FEFLeaderboardEntry& B) { return A.Score > B.Score; });
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index].Rank = Index + 1;
	}

	if (Count > 0 && Entries.Num() > Count)
	{
		const int32 UserIndex = Entries.IndexOfByPredicate([](const FEFLeaderboardEntry& Entry) { return Entry.bIsLocalUser; });
		if (UserIndex >= Count)
		{
			Entries[Count - 1] = Entries[UserIndex];
		}
		Entries.SetNum(Count);
	}
}

// ── Lifetime ────────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bShuttingDown = false;

	// Initialize the EOS subsystems first so their delegates exist to bind to.
	if (UEEOSAchievementSubsystem* Achievements = Collection.InitializeDependency<UEEOSAchievementSubsystem>())
	{
		AchievementSubsystem = Achievements;
		Achievements->OnAchievementUnlocked.AddDynamic(this, &UEEOSPlatformProvider::HandleAchievementUnlocked);
		Achievements->OnAchievementsQueried.AddDynamic(this, &UEEOSPlatformProvider::HandleAchievementsQueried);
	}
	if (UEEOSStatsSubsystem* Stats = Collection.InitializeDependency<UEEOSStatsSubsystem>())
	{
		StatsSubsystem = Stats;
		Stats->OnStatIngested.AddDynamic(this, &UEEOSPlatformProvider::HandleStatIngested);
		Stats->OnStatsQueried.AddDynamic(this, &UEEOSPlatformProvider::HandleStatsQueried);
	}
	if (UEEOSLeaderboardSubsystem* Leaderboards = Collection.InitializeDependency<UEEOSLeaderboardSubsystem>())
	{
		LeaderboardSubsystem = Leaderboards;
		Leaderboards->OnScoreUploaded.AddDynamic(this, &UEEOSPlatformProvider::HandleScoreUploaded);
		Leaderboards->OnLeaderboardQueried.AddDynamic(this, &UEEOSPlatformProvider::HandleLeaderboardQueried);
	}
	if (UEEOSPlayerStorageSubsystem* Storage = Collection.InitializeDependency<UEEOSPlayerStorageSubsystem>())
	{
		StorageSubsystem = Storage;
		Storage->OnPlayerDataWritten.AddDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataWritten);
		Storage->OnPlayerDataRead.AddDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataRead);
		Storage->OnPlayerDataDeleted.AddDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataDeleted);
		Storage->OnPlayerFilesQueried.AddDynamic(this, &UEEOSPlatformProvider::HandlePlayerFilesQueried);
	}
	if (UEEOSPresenceSubsystem* Presence = Collection.InitializeDependency<UEEOSPresenceSubsystem>())
	{
		PresenceSubsystem = Presence;
		Presence->OnOperationCompleted.AddDynamic(this, &UEEOSPlatformProvider::HandlePresenceOutcome);
	}
	AuthSubsystem = Collection.InitializeDependency<UEEOSAuthSubsystem>();
	ConnectSubsystem = Collection.InitializeDependency<UEEOSConnectSubsystem>();
	UserInfoSubsystem = Collection.InitializeDependency<UEEOSUserInfoSubsystem>();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSPlatformProvider::TickCalls), EEOSPlatformPrivate::TickIntervalSeconds);
}

void UEEOSPlatformProvider::Deinitialize()
{
	bShuttingDown = true;
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	// Unbind first, so nothing the EOS subsystems report while shutting down reaches a call twice.
	if (UEEOSAchievementSubsystem* Achievements = AchievementSubsystem.Get())
	{
		Achievements->OnAchievementUnlocked.RemoveDynamic(this, &UEEOSPlatformProvider::HandleAchievementUnlocked);
		Achievements->OnAchievementsQueried.RemoveDynamic(this, &UEEOSPlatformProvider::HandleAchievementsQueried);
	}
	if (UEEOSStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		Stats->OnStatIngested.RemoveDynamic(this, &UEEOSPlatformProvider::HandleStatIngested);
		Stats->OnStatsQueried.RemoveDynamic(this, &UEEOSPlatformProvider::HandleStatsQueried);
	}
	if (UEEOSLeaderboardSubsystem* Leaderboards = LeaderboardSubsystem.Get())
	{
		Leaderboards->OnScoreUploaded.RemoveDynamic(this, &UEEOSPlatformProvider::HandleScoreUploaded);
		Leaderboards->OnLeaderboardQueried.RemoveDynamic(this, &UEEOSPlatformProvider::HandleLeaderboardQueried);
	}
	if (UEEOSPlayerStorageSubsystem* Storage = StorageSubsystem.Get())
	{
		Storage->OnPlayerDataWritten.RemoveDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataWritten);
		Storage->OnPlayerDataRead.RemoveDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataRead);
		Storage->OnPlayerDataDeleted.RemoveDynamic(this, &UEEOSPlatformProvider::HandlePlayerDataDeleted);
		Storage->OnPlayerFilesQueried.RemoveDynamic(this, &UEEOSPlatformProvider::HandlePlayerFilesQueried);
	}
	if (UEEOSPresenceSubsystem* Presence = PresenceSubsystem.Get())
	{
		Presence->OnOperationCompleted.RemoveDynamic(this, &UEEOSPlatformProvider::HandlePresenceOutcome);
	}

	// Callbacks may start new calls; those answer Cancelled at once, so this ends.
	while (Calls.Num() > 0)
	{
		TArray<int64> Ids;
		Calls.GetKeys(Ids);
		for (const int64 CallId : Ids)
		{
			FailCall(CallId, FEFResult::Cancelled(ProviderName));
		}
	}

	AchievementSubsystem.Reset();
	StatsSubsystem.Reset();
	LeaderboardSubsystem.Reset();
	StorageSubsystem.Reset();
	PresenceSubsystem.Reset();
	AuthSubsystem.Reset();
	ConnectSubsystem.Reset();
	UserInfoSubsystem.Reset();

	Super::Deinitialize();
}

// ── Availability and user ───────────────────────────────────────────────────

bool UEEOSPlatformProvider::SupportsFeature(EEFPlatformFeature Feature) const
{
	const UEEOSSettings* Settings = UEEOSSettings::Get();
	switch (Feature)
	{
	case EEFPlatformFeature::Achievements: return !Settings || Settings->bEnableAchievements;
	case EEFPlatformFeature::Leaderboards: return !Settings || Settings->bEnableLeaderboards;
	default:                               return true;
	}
}

const UEEOSSubsystem* UEEOSPlatformProvider::GetFeatureSubsystem(EEFPlatformFeature Feature) const
{
	switch (Feature)
	{
	case EEFPlatformFeature::Achievements: return AchievementSubsystem.Get();
	case EEFPlatformFeature::Stats:        return StatsSubsystem.Get();
	case EEFPlatformFeature::Leaderboards: return LeaderboardSubsystem.Get();
	case EEFPlatformFeature::CloudStorage: return StorageSubsystem.Get();
	case EEFPlatformFeature::Presence:     return PresenceSubsystem.Get();
	default:                               return nullptr;
	}
}

EEOSPlatform::EFeatureState UEEOSPlatformProvider::GetFeatureState(EEFPlatformFeature Feature) const
{
	if (bShuttingDown)
	{
		return EEOSPlatform::EFeatureState::ShuttingDown;
	}
	if (!SupportsFeature(Feature))
	{
		return EEOSPlatform::EFeatureState::Disabled;
	}

	const UEEOSSubsystem* Subsystem = GetFeatureSubsystem(Feature);
	if (!Subsystem)
	{
		return EEOSPlatform::EFeatureState::NoSubsystem;
	}

	// Observes the existing EOS instance only; never creates one or starts a login.
	const FEEOSReadinessSnapshot Readiness = Subsystem->GetEOSReadiness();
	if (Readiness.State != EEOSReadinessState::Ready)
	{
		return EEOSPlatform::EFeatureState::NotRunning;
	}

	// Presence belongs to the Epic account; everything else is a game service keyed by the
	// Product User ID, so it needs the Connect login.
	const bool bSignedIn = Feature == EEFPlatformFeature::Presence ? Readiness.bHasEpicAccount : Readiness.bConnectLoggedIn;
	if (!bSignedIn)
	{
		return EEOSPlatform::EFeatureState::NotSignedIn;
	}

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::DoesInstanceExist(Readiness.Instance) ? IOnlineSubsystem::Get(Readiness.Instance) : nullptr;
	if (!OnlineSubsystem)
	{
		return EEOSPlatform::EFeatureState::NotRunning;
	}

	bool bHasInterface = true;
	switch (Feature)
	{
	case EEFPlatformFeature::Achievements: bHasInterface = OnlineSubsystem->GetAchievementsInterface().IsValid(); break;
	case EEFPlatformFeature::Stats:        bHasInterface = OnlineSubsystem->GetStatsInterface().IsValid(); break;
	case EEFPlatformFeature::Leaderboards: bHasInterface = OnlineSubsystem->GetLeaderboardsInterface().IsValid(); break;
	case EEFPlatformFeature::CloudStorage: bHasInterface = OnlineSubsystem->GetUserCloudInterface().IsValid(); break;
	case EEFPlatformFeature::Presence:     bHasInterface = OnlineSubsystem->GetPresenceInterface().IsValid(); break;
	default: break;
	}
	return bHasInterface ? EEOSPlatform::EFeatureState::Ready : EEOSPlatform::EFeatureState::NoInterface;
}

bool UEEOSPlatformProvider::IsFeatureAvailable(EEFPlatformFeature Feature) const
{
	return GetFeatureState(Feature) == EEOSPlatform::EFeatureState::Ready;
}

FEFResult UEEOSPlatformProvider::CheckFeature(EEFPlatformFeature Feature) const
{
	const TCHAR* FeatureName = EFPlatform::FeatureToString(Feature);
	switch (GetFeatureState(Feature))
	{
	case EEOSPlatform::EFeatureState::Ready:
		return FEFResult::Success(ProviderName);
	case EEOSPlatform::EFeatureState::ShuttingDown:
		return FEFResult::Cancelled(ProviderName);
	case EEOSPlatform::EFeatureState::Disabled:
		return FEFResult::NotAvailable(ProviderName, FString::Printf(TEXT("%s are turned off in Project Settings > Extended EOS > Features"), FeatureName));
	case EEOSPlatform::EFeatureState::NoSubsystem:
		return FEFResult::NotAvailable(ProviderName, FString::Printf(TEXT("the EOS subsystem for %s is not running"), FeatureName));
	case EEOSPlatform::EFeatureState::NotRunning:
		return FEFResult::NotAvailable(ProviderName, TEXT("the EOS platform is not running"));
	case EEOSPlatform::EFeatureState::NotSignedIn:
		return FEFResult::Failure(ProviderName, EFPlatformResultCodes::NotSignedIn, Feature == EEFPlatformFeature::Presence
			? FString(TEXT("Presence needs a logged-in Epic account"))
			: FString::Printf(TEXT("%s need an EOS Connect login (a Product User ID)"), FeatureName));
	case EEOSPlatform::EFeatureState::NoInterface:
	default:
		return FEFResult::NotAvailable(ProviderName, FString::Printf(TEXT("the EOS online subsystem has no %s interface"), FeatureName));
	}
}

FString UEEOSPlatformProvider::GetLocalProductUserId() const
{
	const UEEOSConnectSubsystem* Connect = ConnectSubsystem.Get();
	return Connect ? Connect->GetProductUserId() : FString();
}

FEFPlatformUser UEEOSPlatformProvider::GetLocalUser() const
{
	FEFPlatformUser User = Super::GetLocalUser();
	const UEEOSAuthSubsystem* Auth = AuthSubsystem.Get();

	// Connect reads the live Connect login; Auth also knows a Product User ID from its own Connect login.
	User.UserId = GetLocalProductUserId();
	if (User.UserId.IsEmpty() && Auth)
	{
		User.UserId = Auth->GetProductUserId();
	}
	User.bSignedIn = !User.UserId.IsEmpty();
	if (!User.bSignedIn)
	{
		return User;
	}

	// The Epic (or platform) nickname, then the user info cache when it holds this account, then
	// the name given to a Device ID login.
	User.DisplayName = Auth ? Auth->GetDisplayName() : FString();
	if (User.DisplayName.IsEmpty() && Auth)
	{
		if (const UEEOSUserInfoSubsystem* UserInfo = UserInfoSubsystem.Get())
		{
			const FString EpicAccountId = UEEOSBlueprintLibrary::ExtractEpicAccountId(Auth->GetLoggedInUserId());
			const FEEOSUserInfo Info = UserInfo->GetCachedUserInfo();
			if (!EpicAccountId.IsEmpty() && Info.EpicAccountId == EpicAccountId)
			{
				User.DisplayName = Info.DisplayName;
			}
		}
	}
	if (User.DisplayName.IsEmpty())
	{
		if (const UEEOSConnectSubsystem* Connect = ConnectSubsystem.Get())
		{
			User.DisplayName = Connect->GetDeviceIdDisplayName();
		}
	}
	return User;
}

// ── Achievements ────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::UnlockAchievement(const FString& AchievementId, FOnResult OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::Achievements);
	if (!Ready.IsSuccess() || AchievementId.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("AchievementId is empty")) : Ready);
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::UnlockAchievement;
	Call.Key = AchievementId;
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone)
{
	if (Progress >= 1.f)
	{
		UnlockAchievement(AchievementId, MoveTemp(OnDone));
		return;
	}

	// Never write partial progress: the EOS online subsystem turns any achievement write into a
	// permanent unlock. EOS shows progress only from the stat thresholds set in the Developer Portal.
	if (OnDone)
	{
		OnDone(FEFResult::NotAvailable(ProviderName, TEXT("EOS has no client API for partial achievement progress; update the stat the achievement is defined over in the Developer Portal (progress 1 unlocks)")));
	}
}

void UEEOSPlatformProvider::QueryAchievements(FOnAchievements OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::Achievements);
	if (!Ready.IsSuccess())
	{
		if (OnDone)
		{
			OnDone(Ready, TArray<FEFAchievementState>());
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::QueryAchievements;
	Call.OnAchievements = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

// ── Stats ───────────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	FEFResult Problem = CheckFeature(EEFPlatformFeature::Stats);
	TMap<FString, int32> Amounts;
	if (Problem.IsSuccess())
	{
		Amounts = EEOSPlatform::ToIngestAmounts(Updates);
		if (Amounts.Num() == 0)
		{
			Problem = FEFResult::InvalidArgument(ProviderName, TEXT("no stat updates"));
		}
		else if (Updates.ContainsByPredicate([](const FEFStatUpdate& Update) { return Update.ProviderId.IsEmpty() && Update.Id.IsNone(); }))
		{
			Problem = FEFResult::InvalidArgument(ProviderName, TEXT("a stat update has no id"));
		}
		else if (Amounts.Num() > EEOSPlatformPrivate::MaxIngestStats)
		{
			Problem = FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("%d stats in one update; EOS ingests at most %d at once"), Amounts.Num(), EEOSPlatformPrivate::MaxIngestStats));
		}
	}
	if (!Problem.IsSuccess())
	{
		if (OnDone)
		{
			OnDone(Problem);
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::UpdateStats;
	Call.IngestAmounts = MoveTemp(Amounts);
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::QueryStats(const TArray<FString>& StatIds, FOnStats OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::Stats);
	if (!Ready.IsSuccess())
	{
		if (OnDone)
		{
			OnDone(Ready, TArray<FEFStatValue>());
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::QueryStats;
	Call.StatIds = StatIds;
	Call.OnStats = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

// ── Leaderboards ────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::Leaderboards);
	if (!Ready.IsSuccess() || LeaderboardId.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("LeaderboardId is empty")) : Ready);
		}
		return;
	}

	// EOS leaderboards rank a stat: the score is ingested into the stat named like the board
	// (upper case, as EOS stat names are), and the board applies the stat's aggregation.
	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::SubmitScore;
	Call.Key = LeaderboardId.ToUpper();
	Call.Score = EEOSPlatform::ClampToInt32(Score);
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::Leaderboards);
	if (!Ready.IsSuccess() || LeaderboardId.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("LeaderboardId is empty")) : Ready, TArray<FEFLeaderboardEntry>());
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::QueryLeaderboard;
	Call.Key = LeaderboardId;
	Call.Range = Range;
	Call.Count = FMath::Clamp(Count, 1, EEOSPlatformPrivate::MaxRankings);
	Call.OnLeaderboard = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

// ── Cloud storage ───────────────────────────────────────────────────────────

void UEEOSPlatformProvider::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::CloudStorage);
	if (!Ready.IsSuccess() || FileName.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("FileName is empty")) : Ready);
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::WriteFile;
	Call.Key = FileName;
	Call.Data = MakeShared<TArray<uint8>>(Data);
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::ReadCloudFile(const FString& FileName, FOnCloudFile OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::CloudStorage);
	if (!Ready.IsSuccess() || FileName.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("FileName is empty")) : Ready, TArray<uint8>());
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::ReadFile;
	Call.Key = FileName;
	Call.OnCloudFile = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::DeleteCloudFile(const FString& FileName, FOnResult OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::CloudStorage);
	if (!Ready.IsSuccess() || FileName.IsEmpty())
	{
		if (OnDone)
		{
			OnDone(Ready.IsSuccess() ? FEFResult::InvalidArgument(ProviderName, TEXT("FileName is empty")) : Ready);
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::DeleteFile;
	Call.Key = FileName;
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

void UEEOSPlatformProvider::ListCloudFiles(FOnCloudFiles OnDone)
{
	const FEFResult Ready = CheckFeature(EEFPlatformFeature::CloudStorage);
	if (!Ready.IsSuccess())
	{
		if (OnDone)
		{
			OnDone(Ready, TArray<FEFCloudFileInfo>());
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::ListFiles;
	Call.OnCloudFiles = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

// ── Presence ────────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone)
{
	FEFResult Problem = CheckFeature(EEFPlatformFeature::Presence);
	if (Problem.IsSuccess() && Properties.Num() > EEOSPlatformPrivate::MaxPresenceProperties)
	{
		Problem = FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("%d presence properties; EOS presence holds at most %d besides the rich text"), Properties.Num(), EEOSPlatformPrivate::MaxPresenceProperties));
	}
	if (!Problem.IsSuccess())
	{
		if (OnDone)
		{
			OnDone(Problem);
		}
		return;
	}

	EEOSPlatform::FCall Call;
	Call.Kind = EEOSPlatform::ECallKind::SetPresence;
	Call.Status = Status;
	for (const TPair<FString, FString>& Pair : Properties)
	{
		if (!Pair.Key.IsEmpty())
		{
			Call.Properties.Emplace(Pair.Key, Pair.Value);
		}
	}
	Call.Properties.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B) { return A.Key < B.Key; });
	Call.OnResult = MoveTemp(OnDone);
	StartCall(AddCall(MoveTemp(Call)));
}

// ── Call bookkeeping ────────────────────────────────────────────────────────

int64 UEEOSPlatformProvider::AddCall(EEOSPlatform::FCall&& Call)
{
	// The call id doubles as the EFCallContext the EOS subsystems report its result under.
	const int64 CallId = EFCallContext::New();
	Call.Deadline = FPlatformTime::Seconds() + CallTimeoutSeconds;
	Calls.Add(CallId, MoveTemp(Call));
	return CallId;
}

void UEEOSPlatformProvider::StartCall(int64 CallId)
{
	for (;;)
	{
		EEOSPlatform::FCall* Call = Calls.Find(CallId);
		if (!Call || Call->bStarting)
		{
			// Answered, or a start is already running further up the stack (it picks up the next step).
			return;
		}

		const int32 Step = Call->Step;
		Call->bStarting = true;
		Call->bStarted = false;
		Call->bWaitingToStart = false;

		EEOSPlatform::EStart Started = EEOSPlatform::EStart::Failed;
		{
			EFCallContext::FScope Scope(CallId);
			Started = RunStep(CallId);
		}

		// The map may have changed while the subsystem ran: callbacks can add and answer calls.
		Call = Calls.Find(CallId);
		if (!Call)
		{
			return;
		}
		Call->bStarting = false;
		if (Call->Step != Step)
		{
			// The step's result arrived inside its own start and the call moved on.
			continue;
		}

		switch (Started)
		{
		case EEOSPlatform::EStart::Started:
			Call->bStarted = true;
			if (Call->Kind == EEOSPlatform::ECallKind::SetPresence)
			{
				Call->RequestIdCeiling = FEEOSNativeOperationLease::NextRequestId();
			}
			return;
		case EEOSPlatform::EStart::Busy:
			Call->bWaitingToStart = true;
			return;
		case EEOSPlatform::EStart::Failed:
		default:
			FailCall(CallId, Failed(FString::Printf(TEXT("EOS could not start %s"), EEOSPlatformPrivate::KindToString(Call->Kind))));
			return;
		}
	}
}

EEOSPlatform::EStart UEEOSPlatformProvider::RunStep(int64 CallId)
{
	EEOSPlatform::FCall* Call = Calls.Find(CallId);
	if (!Call)
	{
		return EEOSPlatform::EStart::Failed;
	}

	// Copy what the subsystem needs first: anything it broadcasts may change Calls.
	const EEOSPlatform::ECallKind Kind = Call->Kind;
	const FString Key = Call->Key;
	const int32 Step = Call->Step;
	const FEFResult Gone = FEFResult::NotAvailable(ProviderName, FString::Printf(TEXT("the EOS subsystem for %s is not running"), EEOSPlatformPrivate::KindToString(Kind)));

	switch (Kind)
	{
	case EEOSPlatform::ECallKind::UnlockAchievement:
	{
		UEEOSAchievementSubsystem* Achievements = AchievementSubsystem.Get();
		if (!Achievements)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		return Achievements->UnlockAchievement(Key) ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Failed;
	}

	case EEOSPlatform::ECallKind::QueryAchievements:
	{
		UEEOSAchievementSubsystem* Achievements = AchievementSubsystem.Get();
		if (!Achievements)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		return Achievements->QueryAchievements() ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Failed;
	}

	case EEOSPlatform::ECallKind::UpdateStats:
	{
		UEEOSStatsSubsystem* Stats = StatsSubsystem.Get();
		if (!Stats)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		const TMap<FString, int32> Amounts = Call->IngestAmounts;
		return Stats->IngestStatsBatch(Amounts) ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Failed;
	}

	case EEOSPlatform::ECallKind::QueryStats:
	{
		UEEOSStatsSubsystem* Stats = StatsSubsystem.Get();
		if (!Stats)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		// Ingests upper-case stat names, so ask for them the same way.
		TArray<FString> Names;
		for (const FString& StatId : Call->StatIds)
		{
			Names.Add(StatId.ToUpper());
		}
		return Stats->QueryLocalStats(Names) ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Failed;
	}

	case EEOSPlatform::ECallKind::SubmitScore:
	{
		UEEOSLeaderboardSubsystem* Leaderboards = LeaderboardSubsystem.Get();
		if (!Leaderboards)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		return Leaderboards->UploadScore(Key, Call->Score) ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Failed;
	}

	case EEOSPlatform::ECallKind::QueryLeaderboard:
	{
		UEEOSLeaderboardSubsystem* Leaderboards = LeaderboardSubsystem.Get();
		if (!Leaderboards)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		// The subsystem runs one read at a time and refuses others without answering: wait for it.
		bool bStarted = false;
		switch (Call->Range)
		{
		case EEFLeaderboardRange::Top:
			bStarted = Leaderboards->QueryLeaderboard(Key, Call->Count);
			break;
		case EEFLeaderboardRange::AroundUser:
			// EOS ranks only the top 1000 and reads them in one query; the user is found among them.
			bStarted = Leaderboards->QueryLeaderboardByRange(Key, 1, EEOSPlatformPrivate::MaxRankings);
			break;
		case EEFLeaderboardRange::Friends:
		default:
			bStarted = Step == 0 ? Leaderboards->QueryFriendsLeaderboard(Key) : Leaderboards->QueryLeaderboardAroundPlayer(Key);
			break;
		}
		return bStarted ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Busy;
	}

	case EEOSPlatform::ECallKind::WriteFile:
	{
		UEEOSPlayerStorageSubsystem* Storage = StorageSubsystem.Get();
		if (!Storage)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		// Refused without an answer while another transfer of the same file runs: wait for it.
		static const TArray<uint8> NoData;
		const TSharedPtr<const TArray<uint8>> Data = Call->Data;
		return Storage->WritePlayerData(Key, Data.IsValid() ? *Data : NoData) ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Busy;
	}

	case EEOSPlatform::ECallKind::ReadFile:
	case EEOSPlatform::ECallKind::DeleteFile:
	case EEOSPlatform::ECallKind::ListFiles:
	{
		UEEOSPlayerStorageSubsystem* Storage = StorageSubsystem.Get();
		if (!Storage)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}
		bool bStarted = false;
		if (IsFileListStep(*Call))
		{
			bStarted = Storage->QueryPlayerFiles();
		}
		else if (Kind == EEOSPlatform::ECallKind::ReadFile)
		{
			bStarted = Storage->ReadPlayerData(Key);
		}
		else
		{
			bStarted = Storage->DeletePlayerData(Key);
		}
		return bStarted ? EEOSPlatform::EStart::Started : EEOSPlatform::EStart::Busy;
	}

	case EEOSPlatform::ECallKind::SetPresence:
	{
		UEEOSPresenceSubsystem* Presence = PresenceSubsystem.Get();
		if (!Presence)
		{
			FailCall(CallId, Gone);
			return EEOSPlatform::EStart::Failed;
		}

		const FString Status = Call->Status;
		const TPair<FString, FString> Property = Step > 0 && Call->Properties.IsValidIndex(Step - 1) ? Call->Properties[Step - 1] : TPair<FString, FString>();
		Call->RequestIdFloor = FEEOSNativeOperationLease::NextRequestId();
		Call->RequestIdCeiling = 0;

		const bool bAccepted = Step == 0 ? Presence->SetPresence(Status, Status) : Presence->SetPresenceKey(Property.Key, Property.Value);
		if (bAccepted)
		{
			return EEOSPlatform::EStart::Started;
		}
		if (Presence->GetLastOperationRejection().Code == EEOSOperationCode::Busy)
		{
			return EEOSPlatform::EStart::Busy;
		}
		FailCall(CallId, FEFResult::Failure(ProviderName, EFPlatformResultCodes::NotSignedIn, TEXT("Presence needs a logged-in Epic account")));
		return EEOSPlatform::EStart::Failed;
	}

	default:
		return EEOSPlatform::EStart::Failed;
	}
}

void UEEOSPlatformProvider::AdvanceCall(int64 CallId)
{
	if (EEOSPlatform::FCall* Call = Calls.Find(CallId))
	{
		++Call->Step;
		Call->bStarted = false;
		StartCall(CallId);
	}
}

bool UEEOSPlatformProvider::TakeCall(int64 CallId, EEOSPlatform::FCall& OutCall)
{
	EEOSPlatform::FCall* Call = Calls.Find(CallId);
	if (!Call)
	{
		return false;
	}
	OutCall = MoveTemp(*Call);
	Calls.Remove(CallId);
	return true;
}

void UEEOSPlatformProvider::SucceedCall(int64 CallId)
{
	EEOSPlatform::FCall Call;
	if (TakeCall(CallId, Call) && Call.OnResult)
	{
		// Work the caller starts from its callback must not carry this finished call's context.
		EFCallContext::FScope NoContext(0);
		Call.OnResult(FEFResult::Success(ProviderName));
	}
}

void UEEOSPlatformProvider::FailCall(int64 CallId, const FEFResult& Result)
{
	EEOSPlatform::FCall Call;
	if (TakeCall(CallId, Call))
	{
		AnswerFailure(Call, Result);
	}
}

void UEEOSPlatformProvider::AnswerFailure(EEOSPlatform::FCall& Call, const FEFResult& Result)
{
	EFCallContext::FScope NoContext(0);
	if (Call.OnResult)
	{
		Call.OnResult(Result);
	}
	if (Call.OnAchievements)
	{
		Call.OnAchievements(Result, TArray<FEFAchievementState>());
	}
	if (Call.OnStats)
	{
		Call.OnStats(Result, TArray<FEFStatValue>());
	}
	if (Call.OnLeaderboard)
	{
		Call.OnLeaderboard(Result, TArray<FEFLeaderboardEntry>());
	}
	if (Call.OnCloudFile)
	{
		Call.OnCloudFile(Result, TArray<uint8>());
	}
	if (Call.OnCloudFiles)
	{
		Call.OnCloudFiles(Result, TArray<FEFCloudFileInfo>());
	}
}

EEOSPlatform::FCall* UEEOSPlatformProvider::FindStartedCall(int64 CallId, EEOSPlatform::ECallKind Kind, int32 Step, const FString* Key)
{
	EEOSPlatform::FCall* Call = CallId != 0 ? Calls.Find(CallId) : nullptr;
	if (!Call || Call->Kind != Kind || (!Call->bStarted && !Call->bStarting))
	{
		return nullptr;
	}
	if (Step != INDEX_NONE && Call->Step != Step)
	{
		return nullptr;
	}
	if (Key && Call->Key != *Key)
	{
		return nullptr;
	}
	return Call;
}

bool UEEOSPlatformProvider::IsFileListStep(const EEOSPlatform::FCall& Call)
{
	// Reads and deletes that failed list the files to tell a missing file from a failure.
	return Call.Kind == EEOSPlatform::ECallKind::ListFiles || ((Call.Kind == EEOSPlatform::ECallKind::ReadFile || Call.Kind == EEOSPlatform::ECallKind::DeleteFile) && Call.Step == 1);
}

FEFResult UEEOSPlatformProvider::Failed(const FString& Message) const
{
	return FEEOSResult::Failure(Message, EFResultCodes::Failed).ToEFResult();
}

FEFResult UEEOSPlatformProvider::FromOutcome(const FEEOSOperationOutcome& Outcome) const
{
	if (Outcome.bSuccess)
	{
		return FEFResult::Success(ProviderName);
	}
	if (Outcome.Code == EEOSOperationCode::Canceled || Outcome.Code == EEOSOperationCode::Superseded)
	{
		return FEFResult::Failure(ProviderName, EFResultCodes::Cancelled, Outcome.Message);
	}
	const FString Code = StaticEnum<EEOSOperationCode>()->GetNameStringByValue(static_cast<int64>(Outcome.Code));
	return FEEOSResult::Failure(Outcome.Message.IsEmpty() ? FString(TEXT("EOS presence write failed")) : Outcome.Message, Code).ToEFResult();
}

FEFLeaderboardEntry UEEOSPlatformProvider::ToEntry(const FEEOSLeaderboardEntry& Row, const FString& LocalProductUserId) const
{
	FEFLeaderboardEntry Entry;
	Entry.Rank = Row.Rank;
	const FString ProductUserId = UEEOSBlueprintLibrary::ExtractProductUserId(Row.UserId);
	Entry.UserId = ProductUserId.IsEmpty() ? Row.UserId : ProductUserId;
	Entry.DisplayName = Row.DisplayName;
	Entry.Score = Row.Score;
	Entry.bIsLocalUser = !LocalProductUserId.IsEmpty() && Entry.UserId == LocalProductUserId;
	return Entry;
}

bool UEEOSPlatformProvider::TickCalls(float)
{
	if (bShuttingDown)
	{
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	TArray<int64> Expired;
	TArray<int64> Waiting;
	for (const TPair<int64, EEOSPlatform::FCall>& Pair : Calls)
	{
		if (Pair.Value.bStarting)
		{
			continue;
		}
		if (Now >= Pair.Value.Deadline)
		{
			Expired.Add(Pair.Key);
		}
		else if (Pair.Value.bWaitingToStart)
		{
			Waiting.Add(Pair.Key);
		}
	}

	for (const int64 CallId : Expired)
	{
		if (const EEOSPlatform::FCall* Call = Calls.Find(CallId))
		{
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPlatformProvider: %s '%s' had no EOS result after %.0f s; answering TimedOut"),
				EEOSPlatformPrivate::KindToString(Call->Kind), *FEEOSNativeOperationLease::SafeField(Call->Key), CallTimeoutSeconds);
		}
		FailCall(CallId, FEFResult::TimedOut(ProviderName, CallTimeoutSeconds));
	}

	// Oldest first, so busy calls of one kind start in the order they were made.
	Waiting.Sort();
	for (const int64 CallId : Waiting)
	{
		const EEOSPlatform::FCall* Call = Calls.Find(CallId);
		if (Call && Call->bWaitingToStart)
		{
			StartCall(CallId);
		}
	}
	return true;
}

// ── Achievement results ─────────────────────────────────────────────────────

void UEEOSPlatformProvider::HandleAchievementUnlocked(bool bSuccess, const FString& AchievementId)
{
	const int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::UnlockAchievement, INDEX_NONE, &AchievementId))
	{
		return;
	}
	if (bSuccess)
	{
		SucceedCall(CallId);
	}
	else
	{
		FailCall(CallId, Failed(FString::Printf(TEXT("EOS did not unlock achievement %s"), *AchievementId)));
	}
}

void UEEOSPlatformProvider::HandleAchievementsQueried(bool bSuccess, const TArray<FEEOSAchievement>& Achievements)
{
	const int64 CallId = EFCallContext::GetActive();
	EEOSPlatform::FCall Call;
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::QueryAchievements) || !TakeCall(CallId, Call))
	{
		return;
	}
	if (!bSuccess)
	{
		AnswerFailure(Call, Failed(TEXT("EOS could not read the achievements")));
		return;
	}

	// The subsystem's list includes its local-only partial progress for locked achievements.
	TArray<FEFAchievementState> States;
	States.Reserve(Achievements.Num());
	for (const FEEOSAchievement& Achievement : Achievements)
	{
		FEFAchievementState& State = States.AddDefaulted_GetRef();
		State.ProviderId = Achievement.AchievementId;
		State.bUnlocked = Achievement.bUnlocked;
		State.Progress = Achievement.bUnlocked ? 1.f : FMath::Clamp(Achievement.Progress, 0.f, 1.f);
		State.UnlockTimeUtc = Achievement.UnlockTime;
		State.DisplayName = Achievement.DisplayName;
	}
	if (Call.OnAchievements)
	{
		EFCallContext::FScope NoContext(0);
		Call.OnAchievements(FEFResult::Success(ProviderName), States);
	}
}

// ── Stat results ────────────────────────────────────────────────────────────

void UEEOSPlatformProvider::HandleStatIngested(bool bSuccess)
{
	const int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::UpdateStats))
	{
		return;
	}
	if (bSuccess)
	{
		SucceedCall(CallId);
	}
	else
	{
		FailCall(CallId, Failed(TEXT("EOS did not ingest the stats")));
	}
}

void UEEOSPlatformProvider::HandleStatsQueried(bool bSuccess, const TArray<FEEOSStat>& Stats)
{
	const int64 CallId = EFCallContext::GetActive();
	EEOSPlatform::FCall Call;
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::QueryStats) || !TakeCall(CallId, Call))
	{
		return;
	}
	if (!bSuccess)
	{
		AnswerFailure(Call, Failed(TEXT("EOS could not read the stats")));
		return;
	}

	TArray<FEFStatValue> Values;
	if (Call.StatIds.Num() == 0)
	{
		for (const FEEOSStat& Stat : Stats)
		{
			FEFStatValue& Value = Values.AddDefaulted_GetRef();
			Value.ProviderId = Stat.StatName;
			Value.Value = Stat.Value;
		}
	}
	else
	{
		// Answer in the caller's spelling; a stat never ingested reads as 0, as on the other stores.
		for (const FString& StatId : Call.StatIds)
		{
			FEFStatValue& Value = Values.AddDefaulted_GetRef();
			Value.ProviderId = StatId;
			const FEEOSStat* Found = Stats.FindByPredicate([&StatId](const FEEOSStat& Stat) { return Stat.StatName.Equals(StatId, ESearchCase::IgnoreCase); });
			Value.Value = Found ? Found->Value : 0;
		}
	}
	if (Call.OnStats)
	{
		EFCallContext::FScope NoContext(0);
		Call.OnStats(FEFResult::Success(ProviderName), Values);
	}
}

// ── Leaderboard results ─────────────────────────────────────────────────────

void UEEOSPlatformProvider::HandleScoreUploaded(bool bSuccess, const FString& LeaderboardId)
{
	const int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::SubmitScore, INDEX_NONE, &LeaderboardId))
	{
		return;
	}
	if (bSuccess)
	{
		SucceedCall(CallId);
	}
	else
	{
		FailCall(CallId, Failed(FString::Printf(TEXT("EOS did not ingest the score for %s"), *LeaderboardId)));
	}
}

void UEEOSPlatformProvider::HandleLeaderboardQueried(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Rows)
{
	const int64 CallId = EFCallContext::GetActive();
	EEOSPlatform::FCall* Call = FindStartedCall(CallId, EEOSPlatform::ECallKind::QueryLeaderboard);
	if (!Call)
	{
		return;
	}

	const FString LocalProductUserId = GetLocalProductUserId();
	TArray<FEFLeaderboardEntry> Entries;
	Entries.Reserve(Rows.Num());
	for (const FEEOSLeaderboardEntry& Row : Rows)
	{
		Entries.Add(ToEntry(Row, LocalProductUserId));
	}

	if (Call->Range == EEFLeaderboardRange::Friends && Call->Step == 0)
	{
		// Friends' scores; the user's own row comes next. EOS fails this read when the user has
		// no friends, so a failure here still lets the user's row answer.
		Call->bFirstReadSucceeded = bSuccess;
		if (bSuccess)
		{
			Call->Entries = MoveTemp(Entries);
		}
		AdvanceCall(CallId);
		return;
	}

	EEOSPlatform::FCall Finished;
	TakeCall(CallId, Finished);
	const bool bAnyRead = bSuccess || (Finished.Range == EEFLeaderboardRange::Friends && Finished.bFirstReadSucceeded);
	if (!bAnyRead)
	{
		AnswerFailure(Finished, Failed(FString::Printf(TEXT("EOS could not read leaderboard %s"), *Finished.Key)));
		return;
	}

	FEFResult Result = FEFResult::Success(ProviderName);
	TArray<FEFLeaderboardEntry> Answer;
	switch (Finished.Range)
	{
	case EEFLeaderboardRange::Top:
		Answer = MoveTemp(Entries);
		if (Answer.Num() > Finished.Count)
		{
			Answer.SetNum(Finished.Count);
		}
		break;

	case EEFLeaderboardRange::AroundUser:
		Answer = EEOSPlatform::SliceAroundUser(Entries, Finished.Count);
		if (Answer.Num() == 0)
		{
			Result = FEFResult::NotAvailable(ProviderName, FString::Printf(TEXT("the local user is not among the top %d entries of %s that EOS ranks (or has no score)"), EEOSPlatformPrivate::MaxRankings, *Finished.Key));
		}
		break;

	case EEFLeaderboardRange::Friends:
	default:
		Answer = MoveTemp(Finished.Entries);
		if (bSuccess)
		{
			for (const FEFLeaderboardEntry& Own : Entries)
			{
				if (!Answer.ContainsByPredicate([&Own](const FEFLeaderboardEntry& Entry) { return Entry.UserId == Own.UserId; }))
				{
					Answer.Add(Own);
				}
			}
		}
		// Ranks among the friends returned, highest score first.
		EEOSPlatform::RankByScore(Answer, Finished.Count);
		break;
	}

	EFCallContext::FScope NoContext(0);
	if (!Result.IsSuccess())
	{
		AnswerFailure(Finished, Result);
	}
	else if (Finished.OnLeaderboard)
	{
		Finished.OnLeaderboard(Result, Answer);
	}
}

// ── Cloud storage results ───────────────────────────────────────────────────

void UEEOSPlatformProvider::HandlePlayerDataWritten(bool bSuccess, const FString& FileName)
{
	const int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::WriteFile, INDEX_NONE, &FileName))
	{
		return;
	}
	if (bSuccess)
	{
		SucceedCall(CallId);
	}
	else
	{
		FailCall(CallId, Failed(FString::Printf(TEXT("EOS could not write %s"), *FileName)));
	}
}

void UEEOSPlatformProvider::HandlePlayerDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	const int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::ReadFile, 0, &FileName))
	{
		return;
	}
	if (!bSuccess)
	{
		// EOS reports a missing file as a plain failure; the file list tells the two apart.
		AdvanceCall(CallId);
		return;
	}

	EEOSPlatform::FCall Call;
	if (TakeCall(CallId, Call) && Call.OnCloudFile)
	{
		EFCallContext::FScope NoContext(0);
		Call.OnCloudFile(FEFResult::Success(ProviderName), Data);
	}
}

void UEEOSPlatformProvider::HandlePlayerDataDeleted(bool bSuccess, const FString& FileName)
{
	// A failure found before the delete started reports inside our call context; the finished
	// delete does not carry one, so it is matched by file name (the subsystem runs one per file).
	int64 CallId = EFCallContext::GetActive();
	if (!FindStartedCall(CallId, EEOSPlatform::ECallKind::DeleteFile, 0, &FileName))
	{
		CallId = 0;
		for (const TPair<int64, EEOSPlatform::FCall>& Pair : Calls)
		{
			const EEOSPlatform::FCall& Call = Pair.Value;
			if (Call.Kind == EEOSPlatform::ECallKind::DeleteFile && Call.Step == 0 && Call.bStarted && Call.Key == FileName && (CallId == 0 || Pair.Key < CallId))
			{
				CallId = Pair.Key;
			}
		}
		if (CallId == 0)
		{
			return;
		}
	}

	if (bSuccess)
	{
		SucceedCall(CallId);
	}
	else
	{
		// Deleting a file that does not exist succeeds; the file list tells whether it is still there.
		AdvanceCall(CallId);
	}
}

void UEEOSPlatformProvider::HandlePlayerFilesQueried(const TArray<FString>& FileNames)
{
	// Inside one of our own starts this is a failure found before listing began, reported as an
	// empty list. Otherwise it is a finished listing, which the subsystem shares with everyone
	// waiting for one.
	const int64 ActiveCallId = EFCallContext::GetActive();
	if (const EEOSPlatform::FCall* Starting = Calls.Find(ActiveCallId))
	{
		if (Starting->bStarting && IsFileListStep(*Starting))
		{
			FinishFileListStep(ActiveCallId, FileNames, false);
			return;
		}
	}

	TArray<int64> Waiting;
	for (const TPair<int64, EEOSPlatform::FCall>& Pair : Calls)
	{
		if (Pair.Value.bStarted && IsFileListStep(Pair.Value))
		{
			Waiting.Add(Pair.Key);
		}
	}
	Waiting.Sort();
	for (const int64 CallId : Waiting)
	{
		const EEOSPlatform::FCall* Call = Calls.Find(CallId);
		if (Call && Call->bStarted && IsFileListStep(*Call))
		{
			FinishFileListStep(CallId, FileNames, true);
		}
	}
}

void UEEOSPlatformProvider::FinishFileListStep(int64 CallId, const TArray<FString>& FileNames, bool bListed)
{
	EEOSPlatform::FCall Call;
	if (!TakeCall(CallId, Call))
	{
		return;
	}

	EFCallContext::FScope NoContext(0);
	if (Call.Kind == EEOSPlatform::ECallKind::ListFiles)
	{
		if (!bListed)
		{
			AnswerFailure(Call, Failed(TEXT("EOS could not list the cloud files")));
			return;
		}
		// The storage subsystem reports names only.
		TArray<FEFCloudFileInfo> Files;
		for (const FString& FileName : FileNames)
		{
			Files.AddDefaulted_GetRef().Name = FileName;
		}
		if (Call.OnCloudFiles)
		{
			Call.OnCloudFiles(FEFResult::Success(ProviderName), Files);
		}
		return;
	}

	const bool bFileExists = !bListed || FileNames.Contains(Call.Key);
	if (Call.Kind == EEOSPlatform::ECallKind::ReadFile)
	{
		AnswerFailure(Call, bFileExists
			? Failed(FString::Printf(TEXT("EOS could not read %s"), *Call.Key))
			: FEFResult::Failure(ProviderName, EFPlatformResultCodes::NotFound, FString::Printf(TEXT("%s does not exist"), *Call.Key)));
		return;
	}

	// DeleteFile
	if (bFileExists)
	{
		AnswerFailure(Call, Failed(FString::Printf(TEXT("EOS could not delete %s"), *Call.Key)));
	}
	else if (Call.OnResult)
	{
		Call.OnResult(FEFResult::Success(ProviderName));
	}
}

// ── Presence results ────────────────────────────────────────────────────────

void UEEOSPlatformProvider::HandlePresenceOutcome(const FEEOSOperationOutcome& Outcome)
{
	if (Outcome.Operation != EEOSPlatformPrivate::SetPresenceOperation)
	{
		return;
	}

	// The step whose request id bracket holds this outcome's id (see EEOSPlatform::FCall::RequestIdFloor).
	int64 CallId = 0;
	for (const TPair<int64, EEOSPlatform::FCall>& Pair : Calls)
	{
		const EEOSPlatform::FCall& Call = Pair.Value;
		if (Call.Kind == EEOSPlatform::ECallKind::SetPresence && (Call.bStarted || Call.bStarting)
			&& Outcome.RequestId > Call.RequestIdFloor && (Call.RequestIdCeiling == 0 || Outcome.RequestId < Call.RequestIdCeiling))
		{
			CallId = Pair.Key;
			break;
		}
	}
	if (CallId == 0)
	{
		return;
	}

	if (!Outcome.bSuccess)
	{
		FailCall(CallId, FromOutcome(Outcome));
		return;
	}

	const EEOSPlatform::FCall* Call = Calls.Find(CallId);
	if (Call && Call->Step >= Call->Properties.Num())
	{
		SucceedCall(CallId);
	}
	else
	{
		AdvanceCall(CallId);
	}
}

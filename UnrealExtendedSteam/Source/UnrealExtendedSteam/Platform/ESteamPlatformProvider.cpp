// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Platform/ESteamPlatformProvider.h"

#include "Async/EFCallContext.h"
#include "CoreGlobals.h"
#include "Engine/GameInstance.h"
#include "ExtendedSteamSharedModule.h"
#include "HAL/PlatformTime.h"
#include "Platform/EFPlatformCatalog.h"
#include "Platform/EFPlatformSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

const FName UESteamPlatformProvider::ProviderName(TEXT("Steam"));

namespace ESteamPlatformPrivate
{
	/** How often pending calls are checked against their deadline. */
	constexpr float TimeoutCheckIntervalSeconds = 1.f;

	/** Most rows one leaderboard query returns, as in the platform subsystem. */
	constexpr int32 MaxLeaderboardEntries = 100;

	/** Steam shows progress toasts as Cur / Max; used when the achievement has no progress stat. */
	constexpr int32 DefaultProgressMax = 100;

	bool IsSteamClientUp()
	{
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
	}

	/** Steam stores INT stats and leaderboard scores as int32. */
	int32 ClampToInt32(int64 Value)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32));
	}

	const TCHAR* DescribeWait(uint8 Wait)
	{
		static const TCHAR* const Names[] =
		{
			TEXT("the user's stats to load"),
			TEXT("StoreStats to finish"),
			TEXT("the achievement progress toast"),
			TEXT("FindLeaderboard"),
			TEXT("UploadLeaderboardScore"),
			TEXT("DownloadLeaderboardEntries"),
			TEXT("the cloud file write"),
			TEXT("the cloud file read")
		};
		return Wait < UE_ARRAY_COUNT(Names) ? Names[Wait] : TEXT("Steam");
	}

	/** Steam's request type and row range for a platform range. */
	void GetDownloadRequest(EEFLeaderboardRange Range, int32 Count, EESteamLeaderboardDataRequest& OutRequest, int32& OutStart, int32& OutEnd)
	{
		switch (Range)
		{
		case EEFLeaderboardRange::AroundUser:
		{
			// The user's row plus up to Count - 1 neighbours, slightly more below than above.
			const int32 Above = (Count - 1) / 2;
			OutRequest = EESteamLeaderboardDataRequest::GlobalAroundUser;
			OutStart = -Above;
			OutEnd = Count - 1 - Above;
			break;
		}
		case EEFLeaderboardRange::Friends:
			// Steam returns every friend's row and ignores the range; the result is trimmed to Count.
			OutRequest = EESteamLeaderboardDataRequest::Friends;
			OutStart = 0;
			OutEnd = 0;
			break;
		case EEFLeaderboardRange::Top:
		default:
			OutRequest = EESteamLeaderboardDataRequest::Global;
			OutStart = 1;
			OutEnd = Count;
			break;
		}
	}
}

// Lifecycle

void UESteamPlatformProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bShuttingDown = false;
	StatsSubsystem = Collection.InitializeDependency<UESteamUserStatsSubsystem>();
	StorageSubsystem = Collection.InitializeDependency<UESteamRemoteStorageSubsystem>();
	FriendsSubsystem = Collection.InitializeDependency<UESteamFriendsSubsystem>();
	UserSubsystem = Collection.InitializeDependency<UESteamUserSubsystem>();
	BindSubsystemEvents();

	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		SteamInitializedHandle = Shared.OnSteamClientInitialized.AddUObject(this, &UESteamPlatformProvider::HandleSteamClientInitialized);
		SteamShutdownHandle = Shared.OnSteamClientShutdown.AddUObject(this, &UESteamPlatformProvider::HandleSteamClientShutdown);
	}

	TimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UESteamPlatformProvider::TickTimeouts), ESteamPlatformPrivate::TimeoutCheckIntervalSeconds);

	// SDKs before 1.61 load the user's stats on request only; start that now so the first call is fast.
	RequestLocalStats();
}

void UESteamPlatformProvider::Deinitialize()
{
	bShuttingDown = true;

	if (TimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTickerHandle);
		TimeoutTickerHandle.Reset();
	}

	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		Shared.OnSteamClientInitialized.Remove(SteamInitializedHandle);
		Shared.OnSteamClientShutdown.Remove(SteamShutdownHandle);
	}
	SteamInitializedHandle.Reset();
	SteamShutdownHandle.Reset();

	UnbindSubsystemEvents();
	AnswerAll(FEFResult::Cancelled(ProviderName));

	LeaderboardHandles.Reset();
	bLocalStatsLoaded = false;
	bLocalStatsRequested = false;
	StatsSubsystem.Reset();
	StorageSubsystem.Reset();
	FriendsSubsystem.Reset();
	UserSubsystem.Reset();

	Super::Deinitialize();
}

void UESteamPlatformProvider::BindSubsystemEvents()
{
	if (UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		Stats->OnUserStatsReceived.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleUserStatsReceived);
		Stats->OnUserStatsStored.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleUserStatsStored);
		Stats->OnAchievementStored.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleAchievementStored);
		Stats->OnUserStatsUnloaded.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleUserStatsUnloaded);
		Stats->OnLeaderboardFound.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleLeaderboardFound);
		Stats->OnLeaderboardScoreUploaded.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleLeaderboardScoreUploaded);
		Stats->OnLeaderboardEntriesDownloaded.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleLeaderboardEntriesDownloaded);
	}
	if (UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get())
	{
		Storage->OnFileWriteComplete.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleFileWriteComplete);
		Storage->OnFileReadComplete.AddUniqueDynamic(this, &UESteamPlatformProvider::HandleFileReadComplete);
	}
}

void UESteamPlatformProvider::UnbindSubsystemEvents()
{
	if (UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		Stats->OnUserStatsReceived.RemoveDynamic(this, &UESteamPlatformProvider::HandleUserStatsReceived);
		Stats->OnUserStatsStored.RemoveDynamic(this, &UESteamPlatformProvider::HandleUserStatsStored);
		Stats->OnAchievementStored.RemoveDynamic(this, &UESteamPlatformProvider::HandleAchievementStored);
		Stats->OnUserStatsUnloaded.RemoveDynamic(this, &UESteamPlatformProvider::HandleUserStatsUnloaded);
		Stats->OnLeaderboardFound.RemoveDynamic(this, &UESteamPlatformProvider::HandleLeaderboardFound);
		Stats->OnLeaderboardScoreUploaded.RemoveDynamic(this, &UESteamPlatformProvider::HandleLeaderboardScoreUploaded);
		Stats->OnLeaderboardEntriesDownloaded.RemoveDynamic(this, &UESteamPlatformProvider::HandleLeaderboardEntriesDownloaded);
	}
	if (UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get())
	{
		Storage->OnFileWriteComplete.RemoveDynamic(this, &UESteamPlatformProvider::HandleFileWriteComplete);
		Storage->OnFileReadComplete.RemoveDynamic(this, &UESteamPlatformProvider::HandleFileReadComplete);
	}
}

void UESteamPlatformProvider::HandleSteamClientInitialized()
{
	RequestLocalStats();
}

void UESteamPlatformProvider::HandleSteamClientShutdown()
{
	// The Steam subsystems drop their in-flight requests with the client, so nothing pending can
	// be answered by Steam any more.
	LeaderboardHandles.Reset();
	bLocalStatsLoaded = false;
	bLocalStatsRequested = false;
	AnswerAll(Fail(EFResultCodes::NotAvailable, TEXT("The Steam client shut down before answering."), /*bRetryable*/ true));
}

// Availability

UESteamPlatformProvider::EBlocker UESteamPlatformProvider::GetBlocker(EEFPlatformFeature Feature) const
{
	if (bShuttingDown)
	{
		return EBlocker::ShuttingDown;
	}
	if (!ESteamPlatformPrivate::IsSteamClientUp())
	{
		return EBlocker::SteamDown;
	}

	switch (Feature)
	{
	case EEFPlatformFeature::Achievements:
	case EEFPlatformFeature::Stats:
	case EEFPlatformFeature::Leaderboards:
		if (!StatsSubsystem.IsValid())
		{
			return EBlocker::NoSubsystem;
		}
		break;
	case EEFPlatformFeature::CloudStorage:
		// Steam Cloud writes locally while offline and syncs once the user is back online.
		return StorageSubsystem.IsValid() ? EBlocker::None : EBlocker::NoSubsystem;
	case EEFPlatformFeature::Presence:
		if (!FriendsSubsystem.IsValid())
		{
			return EBlocker::NoSubsystem;
		}
		break;
	default:
		return EBlocker::NoSubsystem;
	}

	const UESteamUserSubsystem* User = UserSubsystem.Get();
	if (!User)
	{
		return EBlocker::NoSubsystem;
	}
	return User->IsLoggedOn() ? EBlocker::None : EBlocker::LoggedOff;
}

FEFResult UESteamPlatformProvider::MakeBlockedResult(EBlocker Blocker, EEFPlatformFeature Feature) const
{
	switch (Blocker)
	{
	case EBlocker::ShuttingDown:
		return FEFResult::Cancelled(ProviderName);
	case EBlocker::SteamDown:
		return Fail(EFResultCodes::NotAvailable,
			TEXT("The Steam client API is not initialized (Steam is not running, or this build has no Steamworks SDK)."));
	case EBlocker::LoggedOff:
		return Fail(EFPlatformResultCodes::NotSignedIn, TEXT("The Steam user is not logged on to the Steam servers."), /*bRetryable*/ true);
	case EBlocker::NoSubsystem:
	default:
		return Fail(EFResultCodes::NotAvailable,
			FString::Printf(TEXT("The Extended Steam subsystem for %s does not exist in this game instance."), EFPlatform::FeatureToString(Feature)));
	}
}

bool UESteamPlatformProvider::CanServe(EEFPlatformFeature Feature, FEFResult& OutProblem) const
{
	const EBlocker Blocker = GetBlocker(Feature);
	if (Blocker == EBlocker::None)
	{
		return true;
	}
	OutProblem = MakeBlockedResult(Blocker, Feature);
	return false;
}

bool UESteamPlatformProvider::IsFeatureAvailable(EEFPlatformFeature Feature) const
{
	return GetBlocker(Feature) == EBlocker::None;
}

bool UESteamPlatformProvider::SupportsFeature(EEFPlatformFeature Feature) const
{
#if WITH_EXTENDEDSTEAM_SDK
	// The Steam client API never runs on a dedicated server; let another provider take over there.
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	switch (Feature)
	{
	case EEFPlatformFeature::Achievements:
	case EEFPlatformFeature::Stats:
	case EEFPlatformFeature::Leaderboards:
		return StatsSubsystem.IsValid();
	case EEFPlatformFeature::CloudStorage:
		return StorageSubsystem.IsValid();
	case EEFPlatformFeature::Presence:
		return FriendsSubsystem.IsValid();
	default:
		return false;
	}
#else
	// Built without the Steamworks SDK: every Steam call is a no-op, so nothing can ever be served.
	return false;
#endif
}

bool UESteamPlatformProvider::AreLocalStatsReady() const
{
#if ESTEAM_SDK_AT_LEAST(161)
	// Since SDK 1.61 the Steam client syncs the user's stats before the game starts.
	return true;
#else
	return bLocalStatsLoaded;
#endif
}

FEFPlatformUser UESteamPlatformProvider::GetLocalUser() const
{
	FEFPlatformUser User;
	User.Provider = ProviderName;
	if (bShuttingDown || !ESteamPlatformPrivate::IsSteamClientUp())
	{
		return User;
	}

	if (const UESteamUserSubsystem* Steam = UserSubsystem.Get())
	{
		const FESteamId SteamId = Steam->GetLocalSteamId();
		if (SteamId.IsValid())
		{
			User.UserId = SteamId.ToString();
			User.bSignedIn = Steam->IsLoggedOn();
		}
	}
	if (const UESteamFriendsSubsystem* Friends = FriendsSubsystem.Get())
	{
		User.DisplayName = Friends->GetPersonaName();
	}
	return User;
}

// Results

FEFResult UESteamPlatformProvider::Ok() const
{
	return FEFResult::Success(ProviderName);
}

FEFResult UESteamPlatformProvider::Fail(const FString& Code, const FString& Message, bool bRetryable) const
{
	return FEFResult::Failure(ProviderName, Code, Message, bRetryable);
}

// Pending calls

UESteamPlatformProvider::FPendingCall UESteamPlatformProvider::MakeCall(EWait Wait, const FString& Key) const
{
	FPendingCall Call;
	Call.Id = EFCallContext::New();
	Call.Wait = Wait;
	Call.Key = Key;
	Call.DeadlineSeconds = FPlatformTime::Seconds() + CallTimeoutSeconds;
	return Call;
}

int32 UESteamPlatformProvider::FindCall(int64 Id) const
{
	return PendingCalls.IndexOfByPredicate([Id](const FPendingCall& Call) { return Call.Id == Id; });
}

int32 UESteamPlatformProvider::FindCallForActiveContext(EWait Wait) const
{
	const int64 Context = EFCallContext::GetActive();
	if (Context == 0)
	{
		// A broadcast without a context came from a request this provider did not make.
		return INDEX_NONE;
	}
	return PendingCalls.IndexOfByPredicate([Context, Wait](const FPendingCall& Call) { return Call.Id == Context && Call.Wait == Wait; });
}

UESteamPlatformProvider::FPendingCall UESteamPlatformProvider::TakeCall(int32 Index)
{
	FPendingCall Call = MoveTemp(PendingCalls[Index]);
	PendingCalls.RemoveAt(Index);
	return Call;
}

void UESteamPlatformProvider::Answer(FPendingCall& Call, const FEFResult& Result)
{
	if (Call.Resume)
	{
		Call.Resume(Result);
	}
	else if (Call.OnResult)
	{
		Call.OnResult(Result);
	}
	else if (Call.OnLeaderboard)
	{
		Call.OnLeaderboard(Result, TArray<FEFLeaderboardEntry>());
	}
	else if (Call.OnCloudFile)
	{
		Call.OnCloudFile(Result, TArray<uint8>());
	}
}

void UESteamPlatformProvider::FailCall(int64 Id, const FEFResult& Result)
{
	// The call may already have been answered, e.g. by a broadcast made before its request returned.
	const int32 Index = FindCall(Id);
	if (Index != INDEX_NONE)
	{
		FPendingCall Call = TakeCall(Index);
		Answer(Call, Result);
	}
}

void UESteamPlatformProvider::AnswerAll(const FEFResult& Result)
{
	// Answers may start new calls. While shutting down or with Steam gone those answer at once, so a
	// few passes empty the list; anything still left after them is answered by its timeout.
	for (int32 Pass = 0; Pass < 4 && PendingCalls.Num() > 0; ++Pass)
	{
		TArray<FPendingCall> Calls = MoveTemp(PendingCalls);
		PendingCalls.Reset();
		for (FPendingCall& Call : Calls)
		{
			Answer(Call, Result);
		}
	}
}

bool UESteamPlatformProvider::TickTimeouts(float /*DeltaTime*/)
{
	const double Now = FPlatformTime::Seconds();
	TArray<FPendingCall> Expired;
	for (int32 Index = 0; Index < PendingCalls.Num();)
	{
		if (PendingCalls[Index].DeadlineSeconds <= Now)
		{
			Expired.Add(TakeCall(Index));
		}
		else
		{
			++Index;
		}
	}

	for (FPendingCall& Call : Expired)
	{
		if (Call.Wait == EWait::LocalStats)
		{
			// Ask again on the next call instead of waiting for a request Steam never answered.
			bLocalStatsRequested = false;
		}

		FEFResult Result = FEFResult::TimedOut(ProviderName, CallTimeoutSeconds);
		Result.Message += FString::Printf(TEXT(" Steam did not answer while waiting for %s."), ESteamPlatformPrivate::DescribeWait(static_cast<uint8>(Call.Wait)));
		if (Call.Wait == EWait::StoreStats)
		{
			Result.Message += TEXT(" The change stays in the Steam client and is uploaded with the next StoreStats.");
		}
		UE_LOG(LogExtendedSteam, Warning, TEXT("Platform provider: %s (%s)"), *Result.Message, *Call.Key);
		Answer(Call, Result);
	}
	return true;
}

// Stats loading (SDKs before 1.61)

void UESteamPlatformProvider::WaitForLocalStats(TFunction<void(const FEFResult& LoadResult)> Resume)
{
	FPendingCall Call = MakeCall(EWait::LocalStats, TEXT("RequestCurrentStats"));
	Call.Resume = MoveTemp(Resume);
	PendingCalls.Add(MoveTemp(Call));

	RequestLocalStats();
	if (!bLocalStatsRequested)
	{
		CompleteLocalStatsWaits(false);
	}
}

void UESteamPlatformProvider::RequestLocalStats()
{
	if (AreLocalStatsReady() || bLocalStatsRequested || bShuttingDown || !ESteamPlatformPrivate::IsSteamClientUp())
	{
		return;
	}
	if (UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get())
	{
		// Completion arrives on OnUserStatsReceived for the local user.
		bLocalStatsRequested = Stats->RequestCurrentStats();
	}
}

void UESteamPlatformProvider::CompleteLocalStatsWaits(bool bSuccess)
{
	TArray<FPendingCall> Waiting;
	for (int32 Index = 0; Index < PendingCalls.Num();)
	{
		if (PendingCalls[Index].Wait == EWait::LocalStats)
		{
			Waiting.Add(TakeCall(Index));
		}
		else
		{
			++Index;
		}
	}

	const FEFResult Result = bSuccess
		? Ok()
		: Fail(EFResultCodes::Failed, TEXT("Steam could not load the user's stats and achievements (RequestCurrentStats failed)."), /*bRetryable*/ true);
	for (FPendingCall& Call : Waiting)
	{
		Answer(Call, Result);
	}
}

void UESteamPlatformProvider::HandleUserStatsReceived(FESteamId SteamId, bool bSuccess)
{
	const UESteamUserSubsystem* User = UserSubsystem.Get();
	if (!User || !SteamId.IsValid() || SteamId != User->GetLocalSteamId())
	{
		return;  // Another user's stats (RequestUserStats).
	}

	bLocalStatsRequested = false;
	if (bSuccess)
	{
		bLocalStatsLoaded = true;
	}
	CompleteLocalStatsWaits(bSuccess);
}

void UESteamPlatformProvider::HandleUserStatsUnloaded(FESteamId SteamId)
{
	const UESteamUserSubsystem* User = UserSubsystem.Get();
	if (User && SteamId.IsValid() && SteamId == User->GetLocalSteamId())
	{
		bLocalStatsLoaded = false;
	}
}

// Achievements

void UESteamPlatformProvider::StoreStatsAndWait(const FString& What, FOnResult OnDone)
{
	UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	if (!Stats || !Stats->StoreStats())
	{
		OnDone(Fail(EFResultCodes::Failed, FString::Printf(
			TEXT("Steam refused StoreStats after %s. The change stays in the Steam client and is uploaded with the next StoreStats."), *What),
			/*bRetryable*/ true));
		return;
	}

	// UserStatsStored carries no call context; the next one answers every call waiting for it.
	FPendingCall Call = MakeCall(EWait::StoreStats, What);
	Call.OnResult = MoveTemp(OnDone);
	PendingCalls.Add(MoveTemp(Call));
}

void UESteamPlatformProvider::HandleUserStatsStored(bool bSuccess)
{
	TArray<FPendingCall> Waiting;
	for (int32 Index = 0; Index < PendingCalls.Num();)
	{
		if (PendingCalls[Index].Wait == EWait::StoreStats)
		{
			Waiting.Add(TakeCall(Index));
		}
		else
		{
			++Index;
		}
	}

	const FEFResult Result = bSuccess
		? Ok()
		: Fail(EFResultCodes::Failed,
			TEXT("Steam did not store the stats: the upload failed, or a stat broke its Steamworks rules (max change, increment only) and was reverted."),
			/*bRetryable*/ true);
	for (FPendingCall& Call : Waiting)
	{
		Answer(Call, Result);
	}
}

void UESteamPlatformProvider::UnlockAchievement(const FString& AchievementId, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Achievements, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (!AreLocalStatsReady())
	{
		WaitForLocalStats([WeakThis = TWeakObjectPtr<UESteamPlatformProvider>(this), AchievementId, OnDone = MoveTemp(OnDone)](const FEFResult& Loaded) mutable
		{
			UESteamPlatformProvider* This = WeakThis.Get();
			if (!Loaded.IsSuccess() || !This)
			{
				OnDone(Loaded.IsSuccess() ? FEFResult::Cancelled(ProviderName) : Loaded);
				return;
			}
			This->UnlockAchievement(AchievementId, MoveTemp(OnDone));
		});
		return;
	}

	UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	bool bUnlocked = false;
	if (!Stats->GetAchievement(AchievementId, bUnlocked))
	{
		OnDone(Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("Steam has no achievement '%s'; check its API name in Steamworks."), *AchievementId)));
		return;
	}
	if (bUnlocked)
	{
		OnDone(Ok());
		return;
	}
	if (!Stats->SetAchievement(AchievementId))
	{
		OnDone(Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused SetAchievement for '%s'."), *AchievementId), /*bRetryable*/ true));
		return;
	}
	StoreStatsAndWait(FString::Printf(TEXT("unlocking %s"), *AchievementId), MoveTemp(OnDone));
}

void UESteamPlatformProvider::SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone)
{
	if (Progress >= 1.f)
	{
		UnlockAchievement(AchievementId, MoveTemp(OnDone));
		return;
	}

	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Achievements, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (!AreLocalStatsReady())
	{
		WaitForLocalStats([WeakThis = TWeakObjectPtr<UESteamPlatformProvider>(this), AchievementId, Progress, OnDone = MoveTemp(OnDone)](const FEFResult& Loaded) mutable
		{
			UESteamPlatformProvider* This = WeakThis.Get();
			if (!Loaded.IsSuccess() || !This)
			{
				OnDone(Loaded.IsSuccess() ? FEFResult::Cancelled(ProviderName) : Loaded);
				return;
			}
			This->SetAchievementProgress(AchievementId, Progress, MoveTemp(OnDone));
		});
		return;
	}

	UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	bool bUnlocked = false;
	if (!Stats->GetAchievement(AchievementId, bUnlocked))
	{
		OnDone(Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("Steam has no achievement '%s'; check its API name in Steamworks."), *AchievementId)));
		return;
	}
	if (bUnlocked || Progress <= 0.f)
	{
		// Nothing to show: Steam keeps no partial progress of its own for an achievement, and an
		// unlocked one stays unlocked.
		OnDone(Ok());
		return;
	}

	// The toast reads Cur / Max: use the bounds of the achievement's progress stat when it has one.
	int32 MinProgress = 0;
	int32 MaxProgress = 0;
	float MinProgressFloat = 0.f;
	float MaxProgressFloat = 0.f;
	if (!Stats->GetAchievementProgressLimitsInt(AchievementId, MinProgress, MaxProgress) || MaxProgress <= MinProgress)
	{
		if (Stats->GetAchievementProgressLimitsFloat(AchievementId, MinProgressFloat, MaxProgressFloat) && MaxProgressFloat > MinProgressFloat)
		{
			MinProgress = FMath::RoundToInt(MinProgressFloat);
			MaxProgress = FMath::RoundToInt(MaxProgressFloat);
		}
		if (MaxProgress <= MinProgress)
		{
			MinProgress = 0;
			MaxProgress = ESteamPlatformPrivate::DefaultProgressMax;
		}
	}
	if (MaxProgress < 2)
	{
		OnDone(Ok());  // No step between 0 and Max to show.
		return;
	}

	// Steam rejects 0 and anything at or above Max.
	const int32 Current = FMath::Clamp(MinProgress + FMath::RoundToInt(Progress * static_cast<float>(MaxProgress - MinProgress)), 1, MaxProgress - 1);
	if (!Stats->IndicateAchievementProgress(AchievementId, Current, MaxProgress))
	{
		OnDone(Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused IndicateAchievementProgress for '%s' (%d / %d)."), *AchievementId, Current, MaxProgress)));
		return;
	}

	// UserAchievementStored carries no call context; match it by achievement name, oldest first.
	FPendingCall Call = MakeCall(EWait::AchievementProgress, AchievementId);
	Call.OnResult = MoveTemp(OnDone);
	PendingCalls.Add(MoveTemp(Call));
}

void UESteamPlatformProvider::HandleAchievementStored(FString AchievementName, int32 CurProgress, int32 MaxProgress)
{
	const int32 Index = PendingCalls.IndexOfByPredicate([&AchievementName](const FPendingCall& Call)
	{
		return Call.Wait == EWait::AchievementProgress && Call.Key.Equals(AchievementName, ESearchCase::IgnoreCase);
	});
	if (Index != INDEX_NONE)
	{
		FPendingCall Call = TakeCall(Index);
		Answer(Call, Ok());
	}
}

void UESteamPlatformProvider::QueryAchievements(FOnAchievements OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Achievements, Problem))
	{
		OnDone(Problem, TArray<FEFAchievementState>());
		return;
	}
	if (!AreLocalStatsReady())
	{
		WaitForLocalStats([WeakThis = TWeakObjectPtr<UESteamPlatformProvider>(this), OnDone = MoveTemp(OnDone)](const FEFResult& Loaded) mutable
		{
			UESteamPlatformProvider* This = WeakThis.Get();
			if (!Loaded.IsSuccess() || !This)
			{
				OnDone(Loaded.IsSuccess() ? FEFResult::Cancelled(ProviderName) : Loaded, TArray<FEFAchievementState>());
				return;
			}
			This->QueryAchievements(MoveTemp(OnDone));
		});
		return;
	}

	const UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	const int32 NumAchievements = Stats->GetNumAchievements();
	TArray<FEFAchievementState> States;
	States.Reserve(NumAchievements);
	for (int32 Index = 0; Index < NumAchievements; ++Index)
	{
		const FString Name = Stats->GetAchievementName(Index);
		bool bUnlocked = false;
		int64 UnlockTime = 0;
		if (Name.IsEmpty() || !Stats->GetAchievementAndUnlockTime(Name, bUnlocked, UnlockTime))
		{
			continue;
		}

		FEFAchievementState& State = States.AddDefaulted_GetRef();
		State.ProviderId = Name;
		State.bUnlocked = bUnlocked;
		// Steam reports no partial progress for an achievement; its progress stat is the game's own.
		State.Progress = bUnlocked ? 1.f : 0.f;
		if (bUnlocked && UnlockTime > 0)
		{
			State.UnlockTimeUtc = FDateTime::FromUnixTimestamp(UnlockTime);
		}
		State.DisplayName = FText::FromString(Stats->GetAchievementDisplayAttribute(Name, TEXT("name")));
	}

	if (NumAchievements > 0 && States.Num() == 0)
	{
		OnDone(Fail(EFResultCodes::Failed, TEXT("Steam has not loaded the user's achievements yet."), /*bRetryable*/ true), States);
		return;
	}
	OnDone(Ok(), States);
}

// Stats

void UESteamPlatformProvider::UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Stats, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (Updates.Num() == 0)
	{
		OnDone(Ok());
		return;
	}
	if (!AreLocalStatsReady())
	{
		WaitForLocalStats([WeakThis = TWeakObjectPtr<UESteamPlatformProvider>(this), Updates, OnDone = MoveTemp(OnDone)](const FEFResult& Loaded) mutable
		{
			UESteamPlatformProvider* This = WeakThis.Get();
			if (!Loaded.IsSuccess() || !This)
			{
				OnDone(Loaded.IsSuccess() ? FEFResult::Cancelled(ProviderName) : Loaded);
				return;
			}
			This->UpdateStats(Updates, MoveTemp(OnDone));
		});
		return;
	}

	UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	TArray<FString> Missing;
	TArray<FString> Rejected;
	int32 Applied = 0;
	int32 Changed = 0;
	for (const FEFStatUpdate& Update : Updates)
	{
		const FString Name = Update.ProviderId.IsEmpty() ? Update.Id.ToString() : Update.ProviderId;

		// Also tells a missing (or non-INT) stat apart from one Steam refuses to change.
		int32 Current = 0;
		if (Name.IsEmpty() || !Stats->GetStatInt(Name, Current))
		{
			Missing.AddUnique(Name);
			continue;
		}

		int64 Wanted = Update.Value;
		if (Update.Mode == EEFStatUpdateMode::Increment)
		{
			// Clamping the delta first keeps the sum inside int64; anything that large saturates anyway.
			Wanted = static_cast<int64>(Current) + FMath::Clamp<int64>(Update.Value, -static_cast<int64>(MAX_uint32), static_cast<int64>(MAX_uint32));
		}
		const int32 NewValue = ESteamPlatformPrivate::ClampToInt32(Wanted);
		if (NewValue != Wanted)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("Platform provider: stat %s value %lld does not fit Steam's 32-bit INT stat; storing %d."), *Name, Wanted, NewValue);
		}

		if (!Stats->SetStatInt(Name, NewValue))
		{
			Rejected.AddUnique(Name);
			continue;
		}
		++Applied;
		Changed += NewValue != Current ? 1 : 0;
	}

	if (Missing.Num() > 0 || Rejected.Num() > 0)
	{
		if (Changed > 0)
		{
			// Upload the updates that did apply; this call reports the ones that did not.
			Stats->StoreStats();
		}

		TArray<FString> Problems;
		if (Missing.Num() > 0)
		{
			Problems.Add(FString::Printf(TEXT("Steam has no INT stat %s"), *FString::Join(Missing, TEXT(", "))));
		}
		if (Rejected.Num() > 0)
		{
			Problems.Add(FString::Printf(TEXT("Steam refused the new value of %s (check the stat's min, max, max change and increment-only settings)"), *FString::Join(Rejected, TEXT(", "))));
		}
		OnDone(Fail(Missing.Num() > 0 ? EFPlatformResultCodes::NotFound : EFResultCodes::Failed,
			FString::Printf(TEXT("%s; %d of %d updates applied."), *FString::Join(Problems, TEXT("; ")), Applied, Updates.Num())));
		return;
	}

	if (Changed == 0)
	{
		OnDone(Ok());  // Every stat already had its value; there is nothing to upload.
		return;
	}
	StoreStatsAndWait(FString::Printf(TEXT("updating %d stat(s)"), Changed), MoveTemp(OnDone));
}

void UESteamPlatformProvider::QueryStats(const TArray<FString>& StatIds, FOnStats OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Stats, Problem))
	{
		OnDone(Problem, TArray<FEFStatValue>());
		return;
	}
	if (!AreLocalStatsReady())
	{
		WaitForLocalStats([WeakThis = TWeakObjectPtr<UESteamPlatformProvider>(this), StatIds, OnDone = MoveTemp(OnDone)](const FEFResult& Loaded) mutable
		{
			UESteamPlatformProvider* This = WeakThis.Get();
			if (!Loaded.IsSuccess() || !This)
			{
				OnDone(Loaded.IsSuccess() ? FEFResult::Cancelled(ProviderName) : Loaded, TArray<FEFStatValue>());
				return;
			}
			This->QueryStats(StatIds, MoveTemp(OnDone));
		});
		return;
	}

	// Steam cannot list a game's stats, so "all stats" means the stats in the platform catalog.
	TArray<FString> Names = StatIds;
	const bool bAllStats = Names.Num() == 0;
	if (bAllStats)
	{
		const UEFPlatformSubsystem* Platform = GetGameInstance() ? GetGameInstance()->GetSubsystem<UEFPlatformSubsystem>() : nullptr;
		if (const UEFPlatformCatalog* Catalog = Platform ? Platform->GetCatalog() : nullptr)
		{
			for (const FEFPlatformCatalogEntry& Entry : Catalog->Stats)
			{
				Names.AddUnique(Catalog->ToProviderId(EEFPlatformCatalogKind::Stat, Entry.Id, ProviderName));
			}
		}
	}

	const UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	TArray<FEFStatValue> Values;
	TArray<FString> Missing;
	for (const FString& Name : Names)
	{
		int32 Value = 0;
		if (Name.IsEmpty() || !Stats->GetStatInt(Name, Value))
		{
			Missing.Add(Name);
			continue;
		}
		FEFStatValue& StatValue = Values.AddDefaulted_GetRef();
		StatValue.ProviderId = Name;
		StatValue.Value = Value;
	}

	// Catalog stats may belong to other stores only; an explicit id Steam does not know is an error.
	if (Missing.Num() > 0 && !bAllStats)
	{
		OnDone(Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("Steam has no INT stat %s."), *FString::Join(Missing, TEXT(", ")))), Values);
		return;
	}
	OnDone(Ok(), Values);
}

// Leaderboards

void UESteamPlatformProvider::SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Leaderboards, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (LeaderboardId.IsEmpty())
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, TEXT("The leaderboard id is empty.")));
		return;
	}

	const int32 SteamScore = ESteamPlatformPrivate::ClampToInt32(Score);
	if (SteamScore != Score)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("Platform provider: score %lld for %s does not fit Steam's 32-bit scores; submitting %d."), Score, *LeaderboardId, SteamScore);
	}

	FPendingCall Call = MakeCall(EWait::FindLeaderboard, LeaderboardId);
	Call.bSubmitScore = true;
	Call.Score = SteamScore;
	Call.OnResult = MoveTemp(OnDone);
	ContinueLeaderboardCall(MoveTemp(Call));
}

void UESteamPlatformProvider::QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Leaderboards, Problem))
	{
		OnDone(Problem, TArray<FEFLeaderboardEntry>());
		return;
	}
	if (LeaderboardId.IsEmpty())
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, TEXT("The leaderboard id is empty.")), TArray<FEFLeaderboardEntry>());
		return;
	}

	FPendingCall Call = MakeCall(EWait::FindLeaderboard, LeaderboardId);
	Call.Range = Range;
	Call.Count = FMath::Clamp(Count, 1, ESteamPlatformPrivate::MaxLeaderboardEntries);
	Call.OnLeaderboard = MoveTemp(OnDone);
	ContinueLeaderboardCall(MoveTemp(Call));
}

void UESteamPlatformProvider::ContinueLeaderboardCall(FPendingCall&& Call)
{
	UESteamUserStatsSubsystem* Stats = StatsSubsystem.Get();
	if (!Stats)
	{
		Answer(Call, MakeBlockedResult(EBlocker::NoSubsystem, EEFPlatformFeature::Leaderboards));
		return;
	}

	const int64 Id = Call.Id;
	const FString Name = Call.Key;
	const int64* CachedHandle = LeaderboardHandles.Find(Name);
	const int64 Handle = CachedHandle ? *CachedHandle : 0;

	// The call is pending before its request starts, so a result reported at any time finds it, and
	// FailCall answers only if nothing did yet.
	if (Handle == 0)
	{
		Call.Wait = EWait::FindLeaderboard;
		PendingCalls.Add(MoveTemp(Call));

		bool bStarted = false;
		{
			EFCallContext::FScope Scope(Id);
			bStarted = Stats->FindLeaderboard(Name);
		}
		if (!bStarted)
		{
			FailCall(Id, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused FindLeaderboard for '%s'."), *Name), /*bRetryable*/ true));
		}
		return;
	}

	if (Call.bSubmitScore)
	{
		const int32 Score = Call.Score;
		Call.Wait = EWait::UploadScore;
		PendingCalls.Add(MoveTemp(Call));

		bool bStarted = false;
		{
			EFCallContext::FScope Scope(Id);
			// Keep-best: whether a lower score replaces a higher one follows the board's sort order.
			bStarted = Stats->UploadLeaderboardScore(Handle, Score, /*bForceUpdate*/ false);
		}
		if (!bStarted)
		{
			FailCall(Id, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused UploadLeaderboardScore for '%s'."), *Name), /*bRetryable*/ true));
		}
		return;
	}

	EESteamLeaderboardDataRequest Request = EESteamLeaderboardDataRequest::Global;
	int32 RangeStart = 1;
	int32 RangeEnd = 1;
	ESteamPlatformPrivate::GetDownloadRequest(Call.Range, Call.Count, Request, RangeStart, RangeEnd);
	Call.Wait = EWait::DownloadEntries;
	PendingCalls.Add(MoveTemp(Call));

	bool bStarted = false;
	{
		EFCallContext::FScope Scope(Id);
		bStarted = Stats->DownloadLeaderboardEntries(Handle, Request, RangeStart, RangeEnd);
	}
	if (!bStarted)
	{
		FailCall(Id, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused DownloadLeaderboardEntries for '%s'."), *Name), /*bRetryable*/ true));
	}
}

void UESteamPlatformProvider::HandleLeaderboardFound(bool bSuccess, int64 LeaderboardHandle, FString LeaderboardName)
{
	// Any successful find gives a handle every later call can reuse, whoever asked for it.
	if (bSuccess && LeaderboardHandle != 0 && !LeaderboardName.IsEmpty())
	{
		LeaderboardHandles.Add(LeaderboardName, LeaderboardHandle);
	}

	const int32 Index = FindCallForActiveContext(EWait::FindLeaderboard);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FPendingCall Call = TakeCall(Index);
	if (!bSuccess || LeaderboardHandle == 0)
	{
		Answer(Call, Fail(EFPlatformResultCodes::NotFound, FString::Printf(
			TEXT("Steam did not find leaderboard '%s'; check its name in Steamworks (or the request failed)."), *Call.Key)));
		return;
	}

	LeaderboardHandles.Add(Call.Key, LeaderboardHandle);
	ContinueLeaderboardCall(MoveTemp(Call));
}

void UESteamPlatformProvider::HandleLeaderboardScoreUploaded(bool bSuccess, int32 Score, bool bScoreChanged, int32 NewGlobalRank)
{
	const int32 Index = FindCallForActiveContext(EWait::UploadScore);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FPendingCall Call = TakeCall(Index);
	if (!bSuccess)
	{
		// A stale handle is found again on the next call.
		LeaderboardHandles.Remove(Call.Key);
		Answer(Call, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam did not accept the score for leaderboard '%s'."), *Call.Key), /*bRetryable*/ true));
		return;
	}
	Answer(Call, Ok());
}

void UESteamPlatformProvider::HandleLeaderboardEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries)
{
	const int32 Index = FindCallForActiveContext(EWait::DownloadEntries);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FPendingCall Call = TakeCall(Index);
	if (!bSuccess)
	{
		LeaderboardHandles.Remove(Call.Key);
		Answer(Call, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam could not download entries of leaderboard '%s'."), *Call.Key), /*bRetryable*/ true));
		return;
	}

	const UESteamUserSubsystem* User = UserSubsystem.Get();
	UESteamFriendsSubsystem* Friends = FriendsSubsystem.Get();
	const FESteamId LocalId = User ? User->GetLocalSteamId() : FESteamId();

	TArray<FEFLeaderboardEntry> Rows;
	Rows.Reserve(Entries.Num());
	for (const FESteamLeaderboardEntry& Entry : Entries)
	{
		FEFLeaderboardEntry& Row = Rows.AddDefaulted_GetRef();
		Row.Rank = Entry.GlobalRank;
		Row.UserId = Entry.SteamId.ToString();
		Row.Score = Entry.Score;
		Row.bIsLocalUser = LocalId.IsValid() && Entry.SteamId == LocalId;
		if (Friends)
		{
			if (Row.bIsLocalUser)
			{
				Row.DisplayName = Friends->GetPersonaName();
			}
			else
			{
				Row.DisplayName = Friends->GetFriendPersonaName(Entry.SteamId);
				if (Row.DisplayName.IsEmpty() || Row.DisplayName == TEXT("[unknown]"))
				{
					// Not cached for non-friends yet; ask Steam so the name is there next time.
					Row.DisplayName.Reset();
					Friends->RequestUserInformation(Entry.SteamId, /*bRequireNameOnly*/ true);
				}
			}
		}
	}

	// Best first; the friends request in particular covers every friend, so trim to the count asked for.
	Rows.StableSort([](const FEFLeaderboardEntry& A, const FEFLeaderboardEntry& B) { return A.Rank < B.Rank; });
	if (Rows.Num() > Call.Count)
	{
		Rows.SetNum(Call.Count);
	}

	if (Call.OnLeaderboard)
	{
		Call.OnLeaderboard(Ok(), Rows);
	}
}

// Cloud storage

void UESteamPlatformProvider::WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::CloudStorage, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (FileName.IsEmpty())
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, TEXT("The cloud file name is empty.")));
		return;
	}
	if (Data.Num() == 0)
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, FString::Printf(TEXT("Steam Cloud cannot store an empty file ('%s')."), *FileName)));
		return;
	}

	FPendingCall Call = MakeCall(EWait::WriteFile, FileName);
	const int64 Id = Call.Id;
	Call.OnResult = MoveTemp(OnDone);
	PendingCalls.Add(MoveTemp(Call));

	UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get();
	bool bStarted = false;
	{
		EFCallContext::FScope Scope(Id);
		bStarted = Storage->FileWriteAsync(FileName, Data);
	}
	if (!bStarted)
	{
		FailCall(Id, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused to write cloud file '%s'."), *FileName), /*bRetryable*/ true));
	}
}

void UESteamPlatformProvider::HandleFileWriteComplete(bool bSuccess, const FString& FileName)
{
	const int32 Index = FindCallForActiveContext(EWait::WriteFile);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FPendingCall Call = TakeCall(Index);
	Answer(Call, bSuccess
		? Ok()
		: Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam Cloud could not write '%s' (quota full, cloud not set up for the app, or a disk error)."), *Call.Key), /*bRetryable*/ true));
}

void UESteamPlatformProvider::ReadCloudFile(const FString& FileName, FOnCloudFile OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::CloudStorage, Problem))
	{
		OnDone(Problem, TArray<uint8>());
		return;
	}
	if (FileName.IsEmpty())
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, TEXT("The cloud file name is empty.")), TArray<uint8>());
		return;
	}

	UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get();
	if (!Storage->FileExists(FileName))
	{
		OnDone(Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("Steam Cloud has no file '%s'."), *FileName)), TArray<uint8>());
		return;
	}
	if (Storage->GetFileSize(FileName) <= 0)
	{
		OnDone(Ok(), TArray<uint8>());  // Steam cannot read an empty file asynchronously; there is nothing to read.
		return;
	}

	FPendingCall Call = MakeCall(EWait::ReadFile, FileName);
	const int64 Id = Call.Id;
	Call.OnCloudFile = MoveTemp(OnDone);
	PendingCalls.Add(MoveTemp(Call));

	bool bStarted = false;
	{
		EFCallContext::FScope Scope(Id);
		bStarted = Storage->FileReadAsync(FileName);
	}
	if (!bStarted)
	{
		FailCall(Id, Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam refused to read cloud file '%s'."), *FileName), /*bRetryable*/ true));
	}
}

void UESteamPlatformProvider::HandleFileReadComplete(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	const int32 Index = FindCallForActiveContext(EWait::ReadFile);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FPendingCall Call = TakeCall(Index);
	if (!bSuccess)
	{
		const UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get();
		const bool bGone = Storage && !Storage->FileExists(Call.Key);
		Answer(Call, bGone
			? Fail(EFPlatformResultCodes::NotFound, FString::Printf(TEXT("Steam Cloud has no file '%s'."), *Call.Key))
			: Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam Cloud could not read '%s'."), *Call.Key), /*bRetryable*/ true));
		return;
	}

	if (Call.OnCloudFile)
	{
		Call.OnCloudFile(Ok(), Data);
	}
}

void UESteamPlatformProvider::DeleteCloudFile(const FString& FileName, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::CloudStorage, Problem))
	{
		OnDone(Problem);
		return;
	}
	if (FileName.IsEmpty())
	{
		OnDone(FEFResult::InvalidArgument(ProviderName, TEXT("The cloud file name is empty.")));
		return;
	}

	UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get();
	if (!Storage->FileExists(FileName) || Storage->FileDelete(FileName))
	{
		OnDone(Ok());
		return;
	}
	OnDone(Fail(EFResultCodes::Failed, FString::Printf(TEXT("Steam Cloud could not delete '%s'."), *FileName), /*bRetryable*/ true));
}

void UESteamPlatformProvider::ListCloudFiles(FOnCloudFiles OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::CloudStorage, Problem))
	{
		OnDone(Problem, TArray<FEFCloudFileInfo>());
		return;
	}

	const UESteamRemoteStorageSubsystem* Storage = StorageSubsystem.Get();
	const int32 NumFiles = Storage->GetFileCount();
	TArray<FEFCloudFileInfo> Files;
	Files.Reserve(NumFiles);
	for (int32 Index = 0; Index < NumFiles; ++Index)
	{
		FString Name;
		int32 Size = 0;
		if (!Storage->GetFileNameAndSize(Index, Name, Size))
		{
			continue;
		}

		FEFCloudFileInfo& File = Files.AddDefaulted_GetRef();
		File.Name = Name;
		File.SizeBytes = Size;
		const int64 Timestamp = Storage->GetFileTimestamp(Name);
		if (Timestamp > 0)
		{
			File.ModifiedUtc = FDateTime::FromUnixTimestamp(Timestamp);
		}
	}
	OnDone(Ok(), Files);
}

// Presence

void UESteamPlatformProvider::SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone)
{
	FEFResult Problem;
	if (!CanServe(EEFPlatformFeature::Presence, Problem))
	{
		OnDone(Problem);
		return;
	}

	// Properties first, then "status", so the Status argument wins over a property of the same name.
	// An empty value removes the key.
	UESteamFriendsSubsystem* Friends = FriendsSubsystem.Get();
	TArray<FString> Rejected;
	for (const TPair<FString, FString>& Property : Properties)
	{
		if (Property.Key.IsEmpty() || Property.Key.Equals(TEXT("status"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Friends->SetRichPresence(Property.Key, Property.Value))
		{
			Rejected.Add(Property.Key);
		}
	}
	if (!Friends->SetRichPresence(TEXT("status"), Status))
	{
		Rejected.Add(TEXT("status"));
	}

	if (Rejected.Num() > 0)
	{
		OnDone(Fail(EFResultCodes::Failed, FString::Printf(
			TEXT("Steam rejected rich presence key(s) %s (at most 30 keys, keys up to 64 and values up to 256 characters)."),
			*FString::Join(Rejected, TEXT(", ")))));
		return;
	}
	OnDone(Ok());
}

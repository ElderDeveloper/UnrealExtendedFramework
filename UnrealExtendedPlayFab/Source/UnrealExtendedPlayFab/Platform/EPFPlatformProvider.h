// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFPendingCalls.h"
#include "Containers/Ticker.h"
#include "Platform/EFPlatformProvider.h"
#include "Shared/EPFTypes.h"
#include "EPFPlatformProvider.generated.h"

class UEPFAuthSubsystem;
class UEPFLeaderboardSubsystem;
class UEPFPlayerProfileSubsystem;
class UEPFStatsSubsystem;

/** What a UEPFPlatformProvider call in flight is waiting for. Internal. */
enum class EEPFPlatformCallKind : uint8
{
	StatWrite,
	StatQuery,
	LeaderboardQuery
};

/**
 * PlayFab as a provider of the Extended platform layer (UEFPlatformSubsystem), named "PlayFab".
 *
 * Serves stats and leaderboards through the plugin's own subsystems, for the logged-in player:
 *
 * - Stats: UEPFStatsSubsystem (Progression statistics, entity token). Values are 32-bit and are
 *   clamped. Set and Increment both report the value; PlayFab applies the aggregation configured on
 *   the statistic (Last, Min, Max or Sum), so a stat updated with Increment must use Sum. Within one
 *   batch, repeated Increments of a stat are added together first and a repeated Set replaces.
 * - Leaderboards: PlayFab leaderboards rank a statistic, so SubmitScore updates the statistic named
 *   by the leaderboard's provider id. Queries go to UEPFLeaderboardSubsystem: Top to
 *   GetLeaderboard, AroundUser to GetLeaderboardAroundEntity, Friends to
 *   GetFriendLeaderboardForEntity. Entries' UserId is the title_player_account entity id the
 *   leaderboard reports.
 * - Achievements, cloud storage and presence: not served (SupportsFeature is false, calls answer
 *   NotAvailable). PlayFab has no achievements or presence, and UEPFEntityFilesSubsystem implements
 *   entity objects (small JSON values), not the entity file upload/download/delete API.
 *
 * Those subsystems report on shared delegates, so each call runs in its own EFCallContext and is
 * answered by the broadcast carrying that context. A call PlayFab has not answered within
 * PlatformCallTimeoutSeconds answers TimedOut; a late answer is then ignored. GetLocalUser reports
 * the PlayFabId and display name from the last login.
 *
 * Turned off with bEnablePlatformProvider (Project Settings > Extended Framework > Extended PlayFab).
 */
UCLASS()
class UNREALEXTENDEDPLAYFAB_API UEPFPlatformProvider : public UEFPlatformProviderSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual FName GetProviderName() const override;

	/** Stats and Leaderboards, while the provider is enabled and a Title ID is set. */
	virtual bool SupportsFeature(EEFPlatformFeature Feature) const override;

	/** A supported feature, with a logged-in player that has an entity token. */
	virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const override;

	virtual FEFPlatformUser GetLocalUser() const override;

	virtual void UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone) override;
	virtual void QueryStats(const TArray<FString>& StatIds, FOnStats OnDone) override;
	virtual void SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone) override;
	virtual void QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone) override;

	/** Calls waiting for PlayFab's answer. */
	int32 GetPendingCallCount() const { return Calls.Num(); }

	// Conversions between PlayFab and platform types. Public for the plugin's tests.

	/** PlayFab statistics are 32-bit. */
	static int32 ClampToInt32(int64 Value);

	/**
	 * The statistic values to send for Updates, one per ProviderId, clamped to int32. A repeated
	 * Increment adds to the value before it; a repeated Set replaces it. False, with OutProblem
	 * filled in, when an update has no ProviderId.
	 */
	static bool MakeStatisticUpdates(const TArray<FEFStatUpdate>& Updates, TMap<FString, int32>& OutValues, FString& OutProblem);

	/** PlayFab's statistics named in StatIds (every one when StatIds is empty), once each. */
	static TArray<FEFStatValue> ToPlatformStats(const TArray<FEPFStatistic>& PlayFabStats, const TArray<FString>& StatIds);

	/**
	 * PlayFab's leaderboard entries, best first, at most Count of them. bIsLocalUser marks the entry
	 * whose entity id is LocalEntityId. With bAroundLocalUser, the entries kept are a window around
	 * that entry.
	 */
	static TArray<FEFLeaderboardEntry> ToPlatformEntries(const TArray<FEPFLeaderboardEntry>& PlayFabEntries, const FString& LocalEntityId, int32 Count, bool bAroundLocalUser);

	/**
	 * A PlayFab result as a platform result from ProviderName. MissingAuthentication becomes
	 * NotSignedIn, NotConfigured becomes NotAvailable, and PlayFab's *NotFound errors become NotFound
	 * (the PlayFab code stays at the start of the message).
	 */
	static FEFResult ToPlatformResult(const FEPFResult& Result, FName ProviderName);

private:
	/** A call in flight, keyed in Calls by the call context it was started in. */
	struct FCall
	{
		EEPFPlatformCallKind Kind = EEPFPlatformCallKind::StatWrite;
		/** Its id in StatWrites, StatQueries or LeaderboardQueries, by Kind. */
		int64 PendingId = 0;
		FTSTicker::FDelegateHandle Timeout;
	};

	UFUNCTION()
	void HandleStatsUpdated(const FEPFResult& Result);

	UFUNCTION()
	void HandleStatsReceived(const FEPFResult& Result, const TArray<FEPFStatistic>& PlayFabStats);

	UFUNCTION()
	void HandleLeaderboardReceived(const FEPFResult& Result, const TArray<FEPFLeaderboardEntry>& PlayFabEntries);

	UFUNCTION()
	void HandleLoginComplete(const FEPFResult& Result, const FString& LoggedInPlayFabId);

	/** Runs Call inside a new call context, tracked as a Kind call with a timeout. */
	void StartCall(EEPFPlatformCallKind Kind, int64 PendingId, TFunctionRef<void()> Call);

	/**
	 * The pending id of the Kind call whose context is active, no longer tracked; 0 when the
	 * broadcast being handled belongs to no call of this provider.
	 */
	int64 TakeActiveCall(EEPFPlatformCallKind Kind);

	void TimeOutCall(int64 CallContext);

	/** Answers every call in flight with Cancelled. */
	void CancelAllCalls();

	/** Enabled in the settings and a Title ID is set. */
	bool IsEnabledInSettings() const;

	bool HasEntitySession() const;

	float GetCallTimeoutSeconds() const;

	TEFPendingCalls<const FEFResult&> StatWrites;
	TEFPendingCalls<const FEFResult&, const TArray<FEPFStatistic>&> StatQueries;
	TEFPendingCalls<const FEFResult&, const TArray<FEPFLeaderboardEntry>&> LeaderboardQueries;

	TMap<int64, FCall> Calls;

	TWeakObjectPtr<UEPFAuthSubsystem> AuthSubsystem;
	TWeakObjectPtr<UEPFStatsSubsystem> StatsSubsystem;
	TWeakObjectPtr<UEPFLeaderboardSubsystem> LeaderboardSubsystem;
	TWeakObjectPtr<UEPFPlayerProfileSubsystem> ProfileSubsystem;

	/** Set in Deinitialize: calls made from then on answer Cancelled at once. */
	bool bShuttingDown = false;
};

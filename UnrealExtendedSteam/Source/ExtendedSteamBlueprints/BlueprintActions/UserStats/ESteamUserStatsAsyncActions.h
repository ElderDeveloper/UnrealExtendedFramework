// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Shared/ESteamTypes.h"
#include "Stats/ESteamUserStatsSubsystem.h"
#include "ESteamUserStatsAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncFindLeaderboardPin, int64, LeaderboardHandle);

/** Finds (optionally creating) a Steam leaderboard by name. */
UCLASS()
class USteamAsyncFindLeaderboard : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Finds a leaderboard by name. When bCreateIfMissing is set, a missing leaderboard is
	 * created with the given sort method and display type (otherwise those are ignored).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Find Leaderboard"))
	static USteamAsyncFindLeaderboard* FindLeaderboard(UObject* WorldContext, const FString& LeaderboardName, bool bCreateIfMissing, EESteamLeaderboardSortMethod SortMethod, EESteamLeaderboardDisplayType DisplayType, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFindLeaderboardPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFindLeaderboardPin OnFailure;

private:
	UFUNCTION()
	void HandleLeaderboardFound(bool bSuccess, int64 LeaderboardHandle, FString LeaderboardName);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 LeaderboardHandle);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	FString RequestedName;
	bool bCreateIfMissing = false;
	EESteamLeaderboardSortMethod SortMethod = EESteamLeaderboardSortMethod::None;
	EESteamLeaderboardDisplayType DisplayType = EESteamLeaderboardDisplayType::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncUploadLeaderboardScorePin, int32, NewGlobalRank);

/** Uploads a score to a leaderboard previously found via "Steam: Find Leaderboard". */
UCLASS()
class USteamAsyncUploadLeaderboardScore : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Upload Leaderboard Score"))
	static USteamAsyncUploadLeaderboardScore* UploadLeaderboardScore(UObject* WorldContext, int64 LeaderboardHandle, int32 Score, bool bForceUpdate, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUploadLeaderboardScorePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUploadLeaderboardScorePin OnFailure;

private:
	UFUNCTION()
	void HandleScoreUploaded(bool bSuccess, int32 Score, bool bScoreChanged, int32 NewGlobalRank);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int32 NewGlobalRank);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	int64 LeaderboardHandle = 0;
	int32 Score = 0;
	bool bForceUpdate = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncDownloadLeaderboardEntriesPin, const TArray<FESteamLeaderboardEntry>&, Entries);

/** Downloads rows from a leaderboard previously found via "Steam: Find Leaderboard". */
UCLASS()
class USteamAsyncDownloadLeaderboardEntries : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Download Leaderboard Entries"))
	static USteamAsyncDownloadLeaderboardEntries* DownloadLeaderboardEntries(UObject* WorldContext, int64 LeaderboardHandle, EESteamLeaderboardDataRequest RequestType, int32 RangeStart, int32 RangeEnd, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDownloadLeaderboardEntriesPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDownloadLeaderboardEntriesPin OnFailure;

private:
	UFUNCTION()
	void HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	int64 LeaderboardHandle = 0;
	EESteamLeaderboardDataRequest RequestType = EESteamLeaderboardDataRequest::Global;
	int32 RangeStart = 0;
	int32 RangeEnd = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncRequestUserStatsPin);

/** Downloads another user's stats/achievements so the GetUserStat/GetUserAchievement getters work. */
UCLASS()
class USteamAsyncRequestUserStats : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request User Stats"))
	static USteamAsyncRequestUserStats* RequestUserStats(UObject* WorldContext, FESteamId TargetUser, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestUserStatsPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestUserStatsPin OnFailure;

private:
	UFUNCTION()
	void HandleUserStatsReceived(FESteamId SteamId, bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	FESteamId TargetUser;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncNumberOfCurrentPlayersPin, int32, PlayerCount);

/** Asks Steam how many players are currently in this game (online + offline). */
UCLASS()
class USteamAsyncGetNumberOfCurrentPlayers : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Number Of Current Players"))
	static USteamAsyncGetNumberOfCurrentPlayers* GetNumberOfCurrentPlayers(UObject* WorldContext, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncNumberOfCurrentPlayersPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncNumberOfCurrentPlayersPin OnFailure;

private:
	UFUNCTION()
	void HandleNumberOfCurrentPlayers(bool bSuccess, int32 PlayerCount);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int32 PlayerCount);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncRequestGlobalStatsPin);

/** Requests aggregated ("global") stats so the subsystem's GetGlobalStat* getters become usable. */
UCLASS()
class USteamAsyncRequestGlobalStats : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * @param HistoryDays Days of day-by-day history to fetch beyond lifetime totals (0-60).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Global Stats"))
	static USteamAsyncRequestGlobalStats* RequestGlobalStats(UObject* WorldContext, int32 HistoryDays, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestGlobalStatsPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestGlobalStatsPin OnFailure;

private:
	UFUNCTION()
	void HandleGlobalStatsReceived(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	int32 HistoryDays = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncRequestGlobalAchievementPercentagesPin);

/** Requests global achievement unlock percentages so the subsystem's percentage getters become usable. */
UCLASS()
class USteamAsyncRequestGlobalAchievementPercentages : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Global Achievement Percentages"))
	static USteamAsyncRequestGlobalAchievementPercentages* RequestGlobalAchievementPercentages(UObject* WorldContext, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestGlobalAchievementPercentagesPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncRequestGlobalAchievementPercentagesPin OnFailure;

private:
	UFUNCTION()
	void HandlePercentagesReady(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
};

/** Downloads the leaderboard rows belonging to an explicit set of users. */
UCLASS()
class USteamAsyncDownloadLeaderboardEntriesForUsers : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Download Leaderboard Entries For Users"))
	static USteamAsyncDownloadLeaderboardEntriesForUsers* DownloadLeaderboardEntriesForUsers(UObject* WorldContext, int64 LeaderboardHandle, const TArray<FESteamId>& Users, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDownloadLeaderboardEntriesPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDownloadLeaderboardEntriesPin OnFailure;

private:
	UFUNCTION()
	void HandleEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	int64 LeaderboardHandle = 0;
	TArray<FESteamId> Users;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncAttachLeaderboardUGCPin);

/** Attaches previously shared UGC to the current user's entry on a leaderboard. */
UCLASS()
class USteamAsyncAttachLeaderboardUGC : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Stats",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Attach Leaderboard UGC"))
	static USteamAsyncAttachLeaderboardUGC* AttachLeaderboardUGC(UObject* WorldContext, int64 LeaderboardHandle, int64 UGCHandle, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAttachLeaderboardUGCPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAttachLeaderboardUGCPin OnFailure;

private:
	UFUNCTION()
	void HandleUGCSet(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUserStatsSubsystem> Subsystem;
	int64 LeaderboardHandle = 0;
	int64 UGCHandle = 0;
};

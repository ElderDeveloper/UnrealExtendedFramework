// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamUserStatsSubsystem.generated.h"

/** Sort order of a leaderboard (mirrors Steamworks ELeaderboardSortMethod). */
UENUM(BlueprintType)
enum class EESteamLeaderboardSortMethod : uint8
{
	None,
	/** Top score is the lowest number. */
	Ascending,
	/** Top score is the highest number. */
	Descending
};

/** How the Steam community website renders a leaderboard score (mirrors ELeaderboardDisplayType). */
UENUM(BlueprintType)
enum class EESteamLeaderboardDisplayType : uint8
{
	None,
	/** Simple numerical score. */
	Numeric,
	/** Score is a time, in seconds. */
	TimeSeconds,
	/** Score is a time, in milliseconds. */
	TimeMilliSeconds
};

/** Which rows to download from a leaderboard (mirrors ELeaderboardDataRequest). */
UENUM(BlueprintType)
enum class EESteamLeaderboardDataRequest : uint8
{
	/** Rows from the full table; RangeStart/RangeEnd in [1, TotalEntries]. */
	Global,
	/** Rows around the current user; RangeStart negative, e.g. -3..3 returns 7 rows. */
	GlobalAroundUser,
	/** All rows belonging to friends of the current user (range ignored). */
	Friends,
	/**
	 * Rows for an explicit set of users (range ignored); issued via
	 * DownloadLeaderboardEntriesForUsers. Users without an entry are omitted from the result.
	 */
	Users
};

/** A single downloaded leaderboard row. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamLeaderboardEntry
{
	GENERATED_BODY()

	/** User owning the entry. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Leaderboards")
	FESteamId SteamId;

	/** Global rank, [1..N] where N is the number of entries in the leaderboard. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Leaderboards")
	int32 GlobalRank = 0;

	/** Score as stored in the leaderboard. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Leaderboards")
	int32 Score = 0;

	/**
	 * Game-defined score details attached to the entry when it was uploaded (empty when the
	 * entry has none). Populated from the SDK's per-entry details buffer, up to 64 int32's.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Leaderboards")
	TArray<int32> Details;

	/**
	 * Handle to UGC attached to the entry via AttachLeaderboardUGC (0 when none). Opaque;
	 * feed to the UGC/RemoteStorage APIs to download the attached content.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Leaderboards")
	int64 UGCHandle = 0;
};

/**
 * Fired when stats/achievements for a user arrive (the local user on startup sync,
 * or another user after RequestUserStats). SteamId is the user the stats belong to.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamUserStatsReceived, FESteamId, SteamId, bool, bSuccess);

/** Fired when a StoreStats upload finished. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamUserStatsStored, bool, bSuccess);

/**
 * Fired when an achievement was stored, or an IndicateAchievementProgress call was
 * acknowledged. CurProgress and MaxProgress are both 0 when the achievement fully unlocked.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamAchievementStored, FString, AchievementName, int32, CurProgress, int32, MaxProgress);

/** Fired when a FindLeaderboard / FindOrCreateLeaderboard request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLeaderboardFound, bool, bSuccess, int64, LeaderboardHandle, FString, LeaderboardName);

/** Fired when an UploadLeaderboardScore request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamLeaderboardScoreUploaded, bool, bSuccess, int32, Score, bool, bScoreChanged, int32, NewGlobalRank);

/** Fired when a DownloadLeaderboardEntries / DownloadLeaderboardEntriesForUsers request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLeaderboardEntriesDownloaded, bool, bSuccess, const TArray<FESteamLeaderboardEntry>&, Entries);

/** Fired when an AttachLeaderboardUGC request completed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamLeaderboardUGCSet, bool, bSuccess);

/** Fired when a GetNumberOfCurrentPlayers request completed. PlayerCount is 0 on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamNumberOfCurrentPlayers, bool, bSuccess, int32, PlayerCount);

/**
 * Fired when a RequestGlobalStats request completed; on success the GetGlobalStat* getters
 * become usable for aggregated stats.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGlobalStatsReceived, bool, bSuccess);

/**
 * Fired when a RequestGlobalAchievementPercentages request completed; on success
 * GetAchievementAchievedPercent and the most-achieved iterators become usable.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGlobalAchievementPercentagesReady, bool, bSuccess);

/**
 * Fired when an achievement icon requested via GetAchievementIcon finished downloading.
 * Query GetAchievementIcon again, or convert ImageHandle with UESteamUtilsSubsystem::GetImageRGBA.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamAchievementIconFetched, FString, AchievementName, int32, ImageHandle);

/** Fired when a user's stats were unloaded; call RequestUserStats again before reading them. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamUserStatsUnloaded, FESteamId, User);

/**
 * Wraps ISteamUserStats: stats, achievements and leaderboards.
 *
 * Leaderboard handles are SteamLeaderboard_t (uint64) values passed through Blueprint
 * as int64; treat them as opaque and only feed back values received from OnLeaderboardFound.
 *
 * Concurrency: same-type async requests are serialized via an internal per-operation FIFO
 * queue (find, upload, download, RequestUserStats, number-of-current-players, global-stats,
 * global-achievement-percentages and attach-UGC each have their own). They complete in order
 * and none are dropped — issuing several before earlier ones finish is safe.
 * DownloadLeaderboardEntriesForUsers reuses the same download queue as DownloadLeaderboardEntries.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamUserStatsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---- Stats lifecycle ----

	/**
	 * Requests the local user's stats/achievements from Steam.
	 * Since Steamworks SDK 1.61 this call was removed from the SDK: the Steam client
	 * synchronizes stats before the game process starts, so on modern SDKs this simply
	 * returns true when Steam is available. On older SDKs the real request is issued
	 * and completion arrives on OnUserStatsReceived.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool RequestCurrentStats();

	/**
	 * Uploads changed stats/achievements to Steam. Result arrives on OnUserStatsStored
	 * (and OnAchievementStored per newly unlocked achievement).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool StoreStats();

	/** Resets all of the local user's stats, and optionally achievements too. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool ResetAllStats(bool bAchievementsToo);

	// ---- Local user stats ----

	/** Reads an integer stat for the local user. Returns false (OutValue 0) when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetStatInt(const FString& Name, int32& OutValue) const;

	/** Reads a float stat for the local user. Returns false (OutValue 0) when unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetStatFloat(const FString& Name, float& OutValue) const;

	/** Sets an integer stat for the local user (call StoreStats to persist). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool SetStatInt(const FString& Name, int32 Value);

	/** Sets a float stat for the local user (call StoreStats to persist). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool SetStatFloat(const FString& Name, float Value);

	/** Updates an AVGRATE stat with new session data (call StoreStats to persist). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool UpdateAvgRateStat(const FString& Name, float CountThisSession, float SessionLengthSeconds);

	// ---- Achievements ----

	/** Reads the unlocked state of an achievement. Returns false when the query itself failed. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetAchievement(const FString& Name, bool& bUnlocked) const;

	/**
	 * Reads the unlocked state and unlock time (unix seconds) of an achievement.
	 * UnlockTime is 0 for achievements unlocked before Steam tracked unlock times (Dec 2009).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetAchievementAndUnlockTime(const FString& Name, bool& bUnlocked, int64& UnlockTime) const;

	/** Unlocks an achievement (call StoreStats to persist and show the overlay toast). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool SetAchievement(const FString& Name);

	/** Relocks an achievement (call StoreStats to persist). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool ClearAchievement(const FString& Name);

	/** Number of achievements defined for this app (0 when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	int32 GetNumAchievements() const;

	/** API name of the achievement at Index in [0, GetNumAchievements) (empty when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	FString GetAchievementName(int32 Index) const;

	/**
	 * Localized display attribute of an achievement. Valid keys: "name", "desc",
	 * "hidden" ("0"/"1"). Empty when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	FString GetAchievementDisplayAttribute(const FString& Name, const FString& Key) const;

	/**
	 * Shows an overlay progress toast for an achievement (does NOT unlock it —
	 * call SetAchievement for that). Triggers OnAchievementStored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool IndicateAchievementProgress(const FString& Name, int32 CurProgress, int32 MaxProgress);

	/**
	 * Reads the min/max bounds of an achievement's associated integer progress stat.
	 * Returns false when the achievement has no progress stat, or the SDK predates the call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetAchievementProgressLimitsInt(const FString& Name, int32& OutMinProgress, int32& OutMaxProgress) const;

	/**
	 * Reads the min/max bounds of an achievement's associated float progress stat.
	 * Returns false when the achievement has no progress stat, or the SDK predates the call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetAchievementProgressLimitsFloat(const FString& Name, float& OutMinProgress, float& OutMaxProgress) const;

	/**
	 * Handle to an achievement's icon image for UESteamUtilsSubsystem::GetImageRGBA, or 0 when
	 * unavailable. 0 may also mean the icon is still downloading: wait for OnAchievementIconFetched
	 * and query again. Steam returns the icon variant matching the user's current unlock state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	int32 GetAchievementIcon(const FString& Name) const;

	// ---- Global achievement percentages ----

	/**
	 * Asks Steam for the global unlock percentage of every achievement. Result arrives on
	 * OnGlobalAchievementPercentagesReady; afterwards GetAchievementAchievedPercent and the
	 * most-achieved iterators are usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool RequestGlobalAchievementPercentages();

	/**
	 * Reads the global unlock percentage [0..100] of an achievement (requires a completed
	 * RequestGlobalAchievementPercentages). Returns false when unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetAchievementAchievedPercent(const FString& Name, float& OutPercent) const;

	/**
	 * Starts iterating achievements by global unlock percentage, most-achieved first. Returns an
	 * iterator index to pass to GetNextMostAchievedAchievementInfo, or -1 when no percentage data
	 * is available (call RequestGlobalAchievementPercentages first).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	int32 GetMostAchievedAchievementInfo(FString& OutName, float& OutPercent, bool& bAchieved) const;

	/**
	 * Continues the most-achieved iteration from PreviousIterator (returned by
	 * GetMostAchievedAchievementInfo or a prior call). Returns the next iterator index, or -1
	 * after the last achievement.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	int32 GetNextMostAchievedAchievementInfo(int32 PreviousIterator, FString& OutName, float& OutPercent, bool& bAchieved) const;

	// ---- Other users ----

	/**
	 * Downloads stats/achievements of another user. Completion arrives on
	 * OnUserStatsReceived; afterwards GetUserStatInt/Float and GetUserAchievement are usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool RequestUserStats(FESteamId SteamId);

	/** Reads another user's achievement state (requires a completed RequestUserStats). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetUserAchievement(FESteamId SteamId, const FString& Name, bool& bUnlocked) const;

	/**
	 * Reads another user's achievement state and unlock time in unix seconds (requires a
	 * completed RequestUserStats). UnlockTime is 0 for achievements unlocked before Steam tracked
	 * unlock times (Dec 2009). Returns false when the query itself failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements")
	bool GetUserAchievementAndUnlockTime(FESteamId SteamId, const FString& Name, bool& bUnlocked, int64& UnlockTime) const;

	/** Reads another user's integer stat (requires a completed RequestUserStats). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetUserStatInt(FESteamId SteamId, const FString& Name, int32& OutValue) const;

	/** Reads another user's float stat (requires a completed RequestUserStats). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetUserStatFloat(FESteamId SteamId, const FString& Name, float& OutValue) const;

	// ---- Global stats ----

	/**
	 * Requests aggregated ("global") stats data. HistoryDays is how many days of day-by-day
	 * history to fetch in addition to lifetime totals (clamped to the SDK maximum of 60; 0 for
	 * totals only). Result arrives on OnGlobalStatsReceived; afterwards the GetGlobalStat* getters
	 * are usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool RequestGlobalStats(int32 HistoryDays);

	/** Reads the lifetime total of an aggregated int stat (requires a completed RequestGlobalStats). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetGlobalStatInt(const FString& Name, int64& OutValue) const;

	/** Reads the lifetime total of an aggregated float stat (requires a completed RequestGlobalStats). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetGlobalStatFloat(const FString& Name, double& OutValue) const;

	/**
	 * Reads day-by-day history of an aggregated int stat, most recent first (index 0 = today).
	 * Fills up to Days elements (requires a completed RequestGlobalStats that requested history).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	void GetGlobalStatHistoryInt(const FString& Name, TArray<int64>& OutValues, int32 Days) const;

	/**
	 * Reads day-by-day history of an aggregated float stat, most recent first (index 0 = today).
	 * Fills up to Days elements (requires a completed RequestGlobalStats that requested history).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	void GetGlobalStatHistoryFloat(const FString& Name, TArray<double>& OutValues, int32 Days) const;

	// ---- Server-wide info ----

	/**
	 * Asks Steam how many players are currently in this game (online + offline). Result arrives
	 * on OnNumberOfCurrentPlayers. Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats")
	bool GetNumberOfCurrentPlayers();

	// ---- Leaderboards ----

	/**
	 * Finds a leaderboard by name, creating it with the given sort/display settings
	 * when it does not exist yet. Result arrives on OnLeaderboardFound.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool FindOrCreateLeaderboard(const FString& LeaderboardName, EESteamLeaderboardSortMethod SortMethod, EESteamLeaderboardDisplayType DisplayType);

	/** Finds an existing leaderboard by name. Result arrives on OnLeaderboardFound. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool FindLeaderboard(const FString& LeaderboardName);

	/**
	 * Uploads a score to a leaderboard found via OnLeaderboardFound. bForceUpdate
	 * replaces the stored score even when worse; otherwise Steam keeps the user's best.
	 * Result arrives on OnLeaderboardScoreUploaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool UploadLeaderboardScore(int64 LeaderboardHandle, int32 Score, bool bForceUpdate);

	/**
	 * As UploadLeaderboardScore, but also attaches game-defined score details — an array of up to
	 * 64 int32's describing how the score was achieved. Result arrives on OnLeaderboardScoreUploaded.
	 * (Separate method because Blueprint UFUNCTIONs cannot overload.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool UploadLeaderboardScoreWithDetails(int64 LeaderboardHandle, int32 Score, bool bForceUpdate, const TArray<int32>& Details);

	/**
	 * Downloads rows from a leaderboard found via OnLeaderboardFound. See
	 * EESteamLeaderboardDataRequest for how RangeStart/RangeEnd are interpreted.
	 * Result arrives on OnLeaderboardEntriesDownloaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool DownloadLeaderboardEntries(int64 LeaderboardHandle, EESteamLeaderboardDataRequest RequestType, int32 RangeStart, int32 RangeEnd);

	/**
	 * Downloads the leaderboard rows belonging to an explicit set of users (max 100 per call;
	 * users without an entry are omitted). Result arrives on OnLeaderboardEntriesDownloaded.
	 * Shares the download queue with DownloadLeaderboardEntries.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool DownloadLeaderboardEntriesForUsers(int64 LeaderboardHandle, const TArray<FESteamId>& Users);

	/**
	 * Attaches previously shared UGC (an ISteamRemoteStorage::FileShare handle) to the current
	 * user's entry on a leaderboard. Result arrives on OnLeaderboardUGCSet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	bool AttachLeaderboardUGC(int64 LeaderboardHandle, int64 UGCHandle);

	/** Total number of entries in a leaderboard, as of the last download (0 when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	int32 GetLeaderboardEntryCount(int64 LeaderboardHandle) const;

	/** API name of a leaderboard found via OnLeaderboardFound (empty when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	FString GetLeaderboardName(int64 LeaderboardHandle) const;

	/** Sort method of a leaderboard found via OnLeaderboardFound (None when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	EESteamLeaderboardSortMethod GetLeaderboardSortMethod(int64 LeaderboardHandle) const;

	/** Display type of a leaderboard found via OnLeaderboardFound (None when unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards")
	EESteamLeaderboardDisplayType GetLeaderboardDisplayType(int64 LeaderboardHandle) const;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats")
	FOnSteamUserStatsReceived OnUserStatsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats")
	FOnSteamUserStatsStored OnUserStatsStored;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements")
	FOnSteamAchievementStored OnAchievementStored;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards")
	FOnSteamLeaderboardFound OnLeaderboardFound;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards")
	FOnSteamLeaderboardScoreUploaded OnLeaderboardScoreUploaded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards")
	FOnSteamLeaderboardEntriesDownloaded OnLeaderboardEntriesDownloaded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards")
	FOnSteamLeaderboardUGCSet OnLeaderboardUGCSet;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats")
	FOnSteamNumberOfCurrentPlayers OnNumberOfCurrentPlayers;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats")
	FOnSteamGlobalStatsReceived OnGlobalStatsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements")
	FOnSteamGlobalAchievementPercentagesReady OnGlobalAchievementPercentagesReady;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements")
	FOnSteamAchievementIconFetched OnAchievementIconFetched;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats")
	FOnSteamUserStatsUnloaded OnUserStatsUnloaded;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamUserStatsCallbacks;
	TSharedPtr<class FESteamUserStatsCallbacks> Callbacks;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Platform/EFPlatformProvider.h"
#include "Social/ESteamFriendsSubsystem.h"
#include "Stats/ESteamUserStatsSubsystem.h"
#include "Storage/ESteamRemoteStorageSubsystem.h"
#include "User/ESteamUserSubsystem.h"
#include "ESteamPlatformProvider.generated.h"

/**
 * The "Steam" provider of the Extended platform layer (UEFPlatformSubsystem).
 *
 * Maps the store-neutral calls onto the Extended Steam subsystems of the same game instance:
 * achievements and stats onto UESteamUserStatsSubsystem (SetAchievement / SetStatInt, then
 * StoreStats), leaderboards onto its FindLeaderboard / UploadLeaderboardScore /
 * DownloadLeaderboardEntries, cloud storage onto UESteamRemoteStorageSubsystem and presence onto
 * UESteamFriendsSubsystem rich presence ("status" plus one key per property).
 *
 * Results the subsystems report on shared delegates are matched to the call that caused them:
 * through EFCallContext for leaderboards and cloud files, by achievement name for progress
 * toasts, and to every call waiting for an upload when Steam reports StoreStats finished. A call
 * that gets no answer within CallTimeoutSeconds fails with TimedOut, so no callback can hang.
 *
 * Available while the Steam client API is initialized and the user is logged on to Steam (cloud
 * storage only needs the client, since Steam Cloud keeps working offline and syncs later).
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamPlatformProvider : public UEFPlatformProviderSubsystem
{
	GENERATED_BODY()

public:
	static const FName ProviderName;

	/** Seconds a call waits for Steam before it fails with TimedOut. */
	static constexpr float CallTimeoutSeconds = 30.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual FName GetProviderName() const override { return ProviderName; }
	virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const override;
	virtual bool SupportsFeature(EEFPlatformFeature Feature) const override;
	virtual FEFPlatformUser GetLocalUser() const override;

	virtual void UnlockAchievement(const FString& AchievementId, FOnResult OnDone) override;
	virtual void SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone) override;
	virtual void QueryAchievements(FOnAchievements OnDone) override;
	virtual void UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone) override;
	virtual void QueryStats(const TArray<FString>& StatIds, FOnStats OnDone) override;
	virtual void SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone) override;
	virtual void QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone) override;
	virtual void WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone) override;
	virtual void ReadCloudFile(const FString& FileName, FOnCloudFile OnDone) override;
	virtual void DeleteCloudFile(const FString& FileName, FOnResult OnDone) override;
	virtual void ListCloudFiles(FOnCloudFiles OnDone) override;
	virtual void SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone) override;

	/** Calls waiting for an answer from Steam. */
	int32 GetPendingCallCount() const { return PendingCalls.Num(); }

private:
	/** Why a feature cannot be served now. */
	enum class EBlocker : uint8
	{
		None,
		ShuttingDown,
		SteamDown,
		NoSubsystem,
		LoggedOff
	};

	/** What a pending call waits for. */
	enum class EWait : uint8
	{
		/** RequestCurrentStats (SDKs before 1.61): OnUserStatsReceived for the local user. */
		LocalStats,
		/** StoreStats: the next OnUserStatsStored. */
		StoreStats,
		/** IndicateAchievementProgress: OnAchievementStored for Key. */
		AchievementProgress,
		/** FindLeaderboard: OnLeaderboardFound under the call's context. */
		FindLeaderboard,
		/** UploadLeaderboardScore: OnLeaderboardScoreUploaded under the call's context. */
		UploadScore,
		/** DownloadLeaderboardEntries: OnLeaderboardEntriesDownloaded under the call's context. */
		DownloadEntries,
		/** FileWriteAsync: OnFileWriteComplete under the call's context. */
		WriteFile,
		/** FileReadAsync: OnFileReadComplete under the call's context. */
		ReadFile
	};

	struct FPendingCall
	{
		/** EFCallContext the call's Steam requests run under; also identifies the call. */
		int64 Id = 0;
		EWait Wait = EWait::StoreStats;
		/** Achievement, leaderboard or file name. */
		FString Key;
		double DeadlineSeconds = 0.0;

		// Leaderboard calls: what to do once the leaderboard handle is known.
		bool bSubmitScore = false;
		int32 Score = 0;
		EEFLeaderboardRange Range = EEFLeaderboardRange::Top;
		int32 Count = 0;

		// Exactly one is set: the caller's callback, or Resume for a call waiting for LocalStats
		// (it reruns the call on success and answers the caller with the failure otherwise).
		FOnResult OnResult;
		FOnLeaderboard OnLeaderboard;
		FOnCloudFile OnCloudFile;
		TFunction<void(const FEFResult& LoadResult)> Resume;
	};

	// Availability

	EBlocker GetBlocker(EEFPlatformFeature Feature) const;
	FEFResult MakeBlockedResult(EBlocker Blocker, EEFPlatformFeature Feature) const;

	/** True when Feature can be served; otherwise sets OutProblem to the result to answer with. */
	bool CanServe(EEFPlatformFeature Feature, FEFResult& OutProblem) const;

	/** Whether the local user's stats and achievements can be read and written now. */
	bool AreLocalStatsReady() const;

	// Results

	FEFResult Ok() const;
	FEFResult Fail(const FString& Code, const FString& Message, bool bRetryable = false) const;

	// Pending calls

	FPendingCall MakeCall(EWait Wait, const FString& Key) const;
	int32 FindCall(int64 Id) const;
	/** The call waiting for Wait under the EFCallContext active now, or INDEX_NONE. */
	int32 FindCallForActiveContext(EWait Wait) const;
	FPendingCall TakeCall(int32 Index);
	/** Answers the call with Result and no data. */
	static void Answer(FPendingCall& Call, const FEFResult& Result);
	/** Answers the call with Id, if it is still pending. */
	void FailCall(int64 Id, const FEFResult& Result);
	/** Answers every pending call with Result. */
	void AnswerAll(const FEFResult& Result);
	bool TickTimeouts(float DeltaTime);

	// Steps

	/** Queues Resume until the local user's stats are loaded (SDKs before 1.61). */
	void WaitForLocalStats(TFunction<void(const FEFResult& LoadResult)> Resume);
	/** Asks Steam for the local user's stats if they are not loaded or requested yet. */
	void RequestLocalStats();
	void CompleteLocalStatsWaits(bool bSuccess);
	/** Calls StoreStats and answers OnDone when Steam reports the upload finished. */
	void StoreStatsAndWait(const FString& What, FOnResult OnDone);
	/** Finds the leaderboard (or uses the cached handle), then uploads or downloads. */
	void ContinueLeaderboardCall(FPendingCall&& Call);

	// Events

	void BindSubsystemEvents();
	void UnbindSubsystemEvents();
	void HandleSteamClientInitialized();
	void HandleSteamClientShutdown();

	UFUNCTION()
	void HandleUserStatsReceived(FESteamId SteamId, bool bSuccess);

	UFUNCTION()
	void HandleUserStatsStored(bool bSuccess);

	UFUNCTION()
	void HandleAchievementStored(FString AchievementName, int32 CurProgress, int32 MaxProgress);

	UFUNCTION()
	void HandleUserStatsUnloaded(FESteamId SteamId);

	UFUNCTION()
	void HandleLeaderboardFound(bool bSuccess, int64 LeaderboardHandle, FString LeaderboardName);

	UFUNCTION()
	void HandleLeaderboardScoreUploaded(bool bSuccess, int32 Score, bool bScoreChanged, int32 NewGlobalRank);

	UFUNCTION()
	void HandleLeaderboardEntriesDownloaded(bool bSuccess, const TArray<FESteamLeaderboardEntry>& Entries);

	UFUNCTION()
	void HandleFileWriteComplete(bool bSuccess, const FString& FileName);

	UFUNCTION()
	void HandleFileReadComplete(bool bSuccess, const FString& FileName, const TArray<uint8>& Data);

	TWeakObjectPtr<UESteamUserStatsSubsystem> StatsSubsystem;
	TWeakObjectPtr<UESteamRemoteStorageSubsystem> StorageSubsystem;
	TWeakObjectPtr<UESteamFriendsSubsystem> FriendsSubsystem;
	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;

	/** In the order the calls were made. */
	TArray<FPendingCall> PendingCalls;

	/** Leaderboard handles by name, from FindLeaderboard. Cleared when Steam shuts down. */
	TMap<FString, int64> LeaderboardHandles;

	FTSTicker::FDelegateHandle TimeoutTickerHandle;
	FDelegateHandle SteamInitializedHandle;
	FDelegateHandle SteamShutdownHandle;

	/** SDKs before 1.61: the local user's stats arrived (RequestCurrentStats completed). */
	bool bLocalStatsLoaded = false;
	/** SDKs before 1.61: a RequestCurrentStats is in flight. */
	bool bLocalStatsRequested = false;
	bool bShuttingDown = false;
};

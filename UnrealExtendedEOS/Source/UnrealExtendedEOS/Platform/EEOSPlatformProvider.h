// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Platform/EFPlatformProvider.h"
#include "Platform/EFPlatformTypes.h"
#include "Result/EFResult.h"
#include "Shared/EEOSOperationTypes.h"
#include "Shared/EEOSTypes.h"
#include "EEOSPlatformProvider.generated.h"

class UEEOSAchievementSubsystem;
class UEEOSAuthSubsystem;
class UEEOSConnectSubsystem;
class UEEOSLeaderboardSubsystem;
class UEEOSPlayerStorageSubsystem;
class UEEOSPresenceSubsystem;
class UEEOSStatsSubsystem;
class UEEOSSubsystem;
class UEEOSUserInfoSubsystem;

namespace EEOSPlatform
{
	/** What a pending platform call is waiting for. */
	enum class ECallKind : uint8
	{
		UnlockAchievement,
		QueryAchievements,
		UpdateStats,
		QueryStats,
		SubmitScore,
		QueryLeaderboard,
		WriteFile,
		ReadFile,
		DeleteFile,
		ListFiles,
		SetPresence
	};

	/** How starting one step of a call went. */
	enum class EStart : uint8
	{
		/** The subsystem took the request; its result arrives on a delegate. */
		Started,
		/** The subsystem refused without answering because it is busy with the same kind of request. Tried again on the next tick. */
		Busy,
		/** The subsystem refused. If it did not report a failure, the call fails with a generic one. */
		Failed
	};

	/** Why a feature can or cannot be served now. */
	enum class EFeatureState : uint8
	{
		Ready,
		ShuttingDown,
		Disabled,
		NoSubsystem,
		NotRunning,
		NotSignedIn,
		NoInterface
	};

	/** A platform call waiting for its EOS subsystem. Exactly one callback is set. */
	struct FCall
	{
		ECallKind Kind = ECallKind::UnlockAchievement;

		/** Achievement, leaderboard or file name the subsystem echoes back in its delegate. */
		FString Key;

		/** Multi-step calls (file checks, friends boards, presence keys): the step running now. */
		int32 Step = 0;

		/** FPlatformTime::Seconds() after which the call answers TimedOut. */
		double Deadline = 0.0;

		/** The current step's start function is running (results may arrive inside it). */
		bool bStarting = false;

		/** The subsystem took the current step and its result is on the way. */
		bool bStarted = false;

		/** The subsystem was busy; the step starts again on the next tick. */
		bool bWaitingToStart = false;

		UEFPlatformProviderSubsystem::FOnResult OnResult;
		UEFPlatformProviderSubsystem::FOnAchievements OnAchievements;
		UEFPlatformProviderSubsystem::FOnStats OnStats;
		UEFPlatformProviderSubsystem::FOnLeaderboard OnLeaderboard;
		UEFPlatformProviderSubsystem::FOnCloudFile OnCloudFile;
		UEFPlatformProviderSubsystem::FOnCloudFiles OnCloudFiles;

		/** UpdateStats: amount to ingest per stat name. */
		TMap<FString, int32> IngestAmounts;

		/** SubmitScore: the score, clamped to int32. */
		int32 Score = 0;

		/** QueryStats: the ids asked for, in the caller's spelling. */
		TArray<FString> StatIds;

		/** QueryLeaderboard. */
		EEFLeaderboardRange Range = EEFLeaderboardRange::Top;
		int32 Count = 0;
		TArray<FEFLeaderboardEntry> Entries;
		bool bFirstReadSucceeded = false;

		/** WriteFile: shared so restarting a busy write does not copy the bytes. */
		TSharedPtr<const TArray<uint8>> Data;

		/** SetPresence: step 0 sets Status, step N sets Properties[N - 1]. */
		FString Status;
		TArray<TPair<FString, FString>> Properties;

		/**
		 * SetPresence: the presence subsystem gives each write a request id from the global
		 * FEEOSNativeOperationLease sequence. Ids drawn just before and just after starting a step
		 * bracket that step's id, so its OnOperationCompleted outcome can be told apart from other
		 * callers' writes. Ceiling is 0 while the step is starting.
		 */
		int64 RequestIdFloor = 0;
		int64 RequestIdCeiling = 0;
	};

	/** EOS ingests 32-bit amounts. */
	UNREALEXTENDEDEOS_API int32 ClampToInt32(int64 Value);

	/**
	 * The amount to ingest per stat for a batch of updates. Names are the ProviderId (the Id when
	 * empty) upper-cased, as EOS stat names are. Updates to one stat apply in order: Set replaces
	 * the amount, Increment adds to it. Amounts are clamped to int32.
	 */
	UNREALEXTENDEDEOS_API TMap<FString, int32> ToIngestAmounts(const TArray<FEFStatUpdate>& Updates);

	/** Up to Count consecutive entries of a ranked list, centred on the entry with bIsLocalUser. Empty when there is none. */
	UNREALEXTENDEDEOS_API TArray<FEFLeaderboardEntry> SliceAroundUser(const TArray<FEFLeaderboardEntry>& Ranked, int32 Count);

	/**
	 * Sorts by score, highest first, numbers the ranks from 1 and keeps at most Count entries. When
	 * the local user would be cut, their entry (with its rank) replaces the last one kept.
	 */
	UNREALEXTENDEDEOS_API void RankByScore(TArray<FEFLeaderboardEntry>& Entries, int32 Count);
}

/**
 * The "EOS" provider of the Extended platform layer (UEFPlatformSubsystem): achievements, stats,
 * leaderboards, player cloud storage and presence through this plugin's EOS subsystems.
 *
 * - Achievements: UEEOSAchievementSubsystem. EOS has no client API for partial progress, so
 *   SetAchievementProgress below 1 answers NotAvailable; drive progress with the stat thresholds
 *   configured in the Developer Portal.
 * - Stats: UEEOSStatsSubsystem ingests. EOS aggregates ingests on its server (sum, max, min,
 *   latest) as configured in the Developer Portal, so Set and Increment both ingest the value.
 * - Leaderboards are defined over a stat: SubmitScore ingests the stat named by the leaderboard's
 *   provider id (UEEOSLeaderboardSubsystem::UploadScore). Top reads ranks 1..Count; AroundUser
 *   reads the top 1000 (all EOS ranks) and slices around the user; Friends reads the friends'
 *   scores plus the user's own and ranks them among themselves.
 * - Cloud storage: UEEOSPlayerStorageSubsystem (EOS Player Data Storage).
 * - Presence: UEEOSPresenceSubsystem. Status is the rich text; each property is a presence data key.
 *
 * Each call answers exactly once. Results are matched to calls through EFCallContext where the
 * subsystem propagates it, otherwise by file name or presence request id. A call that has no
 * answer after CallTimeoutSeconds answers TimedOut; calls pending at shutdown answer Cancelled.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSPlatformProvider : public UEFPlatformProviderSubsystem
{
	GENERATED_BODY()

public:
	static const FName ProviderName;

	/** Seconds a call may wait for its EOS result before it answers TimedOut. */
	static constexpr float CallTimeoutSeconds = 30.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual FName GetProviderName() const override { return ProviderName; }

	/** The EOS platform is running, the user is logged in (Connect, or an Epic account for presence) and the feature is enabled. */
	virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const override;

	/** Every feature, except achievements or leaderboards turned off in Project Settings > Extended EOS. */
	virtual bool SupportsFeature(EEFPlatformFeature Feature) const override;

	/** The Product User ID and display name; bSignedIn while there is a Product User ID. */
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

	/** Success when Feature can be served now, otherwise the NotAvailable / NotSignedIn failure a call would answer. */
	FEFResult CheckFeature(EEFPlatformFeature Feature) const;

	/** Calls still waiting for an answer. */
	int32 GetPendingCallCount() const { return Calls.Num(); }

private:
	EEOSPlatform::EFeatureState GetFeatureState(EEFPlatformFeature Feature) const;
	const UEEOSSubsystem* GetFeatureSubsystem(EEFPlatformFeature Feature) const;
	FString GetLocalProductUserId() const;

	// Calls

	int64 AddCall(EEOSPlatform::FCall&& Call);
	void StartCall(int64 CallId);
	EEOSPlatform::EStart RunStep(int64 CallId);
	void AdvanceCall(int64 CallId);
	bool TakeCall(int64 CallId, EEOSPlatform::FCall& OutCall);
	void SucceedCall(int64 CallId);
	void FailCall(int64 CallId, const FEFResult& Result);
	static void AnswerFailure(EEOSPlatform::FCall& Call, const FEFResult& Result);

	/** The call CallId when it is of Kind, running Step (INDEX_NONE for any) for Key (null for any) and started. */
	EEOSPlatform::FCall* FindStartedCall(int64 CallId, EEOSPlatform::ECallKind Kind, int32 Step = INDEX_NONE, const FString* Key = nullptr);

	static bool IsFileListStep(const EEOSPlatform::FCall& Call);
	void FinishFileListStep(int64 CallId, const TArray<FString>& FileNames, bool bListed);
	FEFLeaderboardEntry ToEntry(const FEEOSLeaderboardEntry& Row, const FString& LocalProductUserId) const;

	/** A generic EOS failure, converted with FEEOSResult::ToEFResult. */
	FEFResult Failed(const FString& Message) const;
	FEFResult FromOutcome(const FEEOSOperationOutcome& Outcome) const;

	bool TickCalls(float DeltaTime);

	// Subsystem delegates

	UFUNCTION()
	void HandleAchievementUnlocked(bool bSuccess, const FString& AchievementId);

	UFUNCTION()
	void HandleAchievementsQueried(bool bSuccess, const TArray<FEEOSAchievement>& Achievements);

	UFUNCTION()
	void HandleStatIngested(bool bSuccess);

	UFUNCTION()
	void HandleStatsQueried(bool bSuccess, const TArray<FEEOSStat>& Stats);

	UFUNCTION()
	void HandleScoreUploaded(bool bSuccess, const FString& LeaderboardId);

	UFUNCTION()
	void HandleLeaderboardQueried(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Entries);

	UFUNCTION()
	void HandlePlayerDataWritten(bool bSuccess, const FString& FileName);

	UFUNCTION()
	void HandlePlayerDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data);

	UFUNCTION()
	void HandlePlayerDataDeleted(bool bSuccess, const FString& FileName);

	UFUNCTION()
	void HandlePlayerFilesQueried(const TArray<FString>& FileNames);

	UFUNCTION()
	void HandlePresenceOutcome(const FEEOSOperationOutcome& Outcome);

	TMap<int64, EEOSPlatform::FCall> Calls;

	TWeakObjectPtr<UEEOSAchievementSubsystem> AchievementSubsystem;
	TWeakObjectPtr<UEEOSStatsSubsystem> StatsSubsystem;
	TWeakObjectPtr<UEEOSLeaderboardSubsystem> LeaderboardSubsystem;
	TWeakObjectPtr<UEEOSPlayerStorageSubsystem> StorageSubsystem;
	TWeakObjectPtr<UEEOSPresenceSubsystem> PresenceSubsystem;
	TWeakObjectPtr<UEEOSAuthSubsystem> AuthSubsystem;
	TWeakObjectPtr<UEEOSConnectSubsystem> ConnectSubsystem;
	TWeakObjectPtr<UEEOSUserInfoSubsystem> UserInfoSubsystem;

	FTSTicker::FDelegateHandle TickerHandle;
	bool bShuttingDown = false;
};

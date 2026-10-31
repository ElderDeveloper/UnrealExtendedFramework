// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/EFPlatformProvider.h"
#include "EFLocalPlatformProvider.generated.h"

class FJsonObject;

/**
 * The "Local" provider: achievements, stats, leaderboards and (optionally) cloud storage kept on
 * this machine under Saved/ExtendedPlatform.
 *
 * It lets the platform layer work in the editor and in builds sold without a store, and makes
 * every platform call testable offline. Leaderboards hold the local user's best scores (higher is
 * better), so Top returns a personal best table and Friends returns the same.
 *
 * Calls answer on the next tick, like a real service. Enabled with bEnableLocalProvider; cloud
 * storage only with bLocalProviderCloudStorage.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFLocalPlatformProvider : public UEFPlatformProviderSubsystem
{
	GENERATED_BODY()

public:
	static const FName ProviderName;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual FName GetProviderName() const override { return ProviderName; }
	virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const override;
	virtual bool IsFallbackProvider() const override { return true; }
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

	/** Folder holding Profile.json and the Cloud folder. */
	static FString GetStorageRoot();

	/** Points storage at another folder, e.g. a throwaway one in tests. Empty restores the default. Takes effect for providers initialized afterwards. */
	static void SetStorageRootForTests(const FString& Root);

	/** Best scores kept per leaderboard. */
	static constexpr int32 MaxScoresPerLeaderboard = 100;

	const FString& GetPresenceStatus() const { return PresenceStatus; }

private:
	struct FAchievement
	{
		bool bUnlocked = false;
		float Progress = 0.f;
		FDateTime UnlockTimeUtc;
	};

	struct FScore
	{
		int64 Score = 0;
		FDateTime TimeUtc;
	};

	void LoadProfile();
	bool SaveProfile() const;
	FString GetProfilePath() const;
	FString GetCloudFolder() const;
	bool IsValidCloudFileName(const FString& FileName) const;

	/** Runs Work on the next tick if this provider still exists then. */
	void Later(TFunction<void(UEFLocalPlatformProvider& This)> Work);

	FEFResult Ok() const;
	FEFResult Fail(const FString& Code, const FString& Message) const;

	FString StorageRoot;
	FString UserId;
	TMap<FString, FAchievement> Achievements;
	TMap<FString, int64> Stats;
	TMap<FString, TArray<FScore>> Leaderboards;
	FString PresenceStatus;
	TMap<FString, FString> PresenceProperties;
};

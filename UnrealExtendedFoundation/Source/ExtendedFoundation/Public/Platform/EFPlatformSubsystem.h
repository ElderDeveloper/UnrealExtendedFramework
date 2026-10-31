// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Platform/EFPlatformProvider.h"
#include "Platform/EFPlatformTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFPlatformSubsystem.generated.h"

class UEFPlatformCatalog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEFOnAchievementUnlocked, FName, AchievementId, FName, Provider);

/**
 * One API for achievements, stats, leaderboards, cloud storage and presence on every store.
 *
 *     UEFPlatformSubsystem::Get(this)->UnlockAchievement(TEXT("FirstBlood"));
 *     UEFPlatformSubsystem::Get(this)->SubmitScore(TEXT("HighScore"), Score, [](const FEFResult& Result) { ... });
 *
 * Each call goes to the provider chosen for its feature (Project Settings > Extended Framework >
 * Extended Platform): a provider override for the feature, else the first provider in
 * ProviderOrder that is registered and can serve it now. Ids are the game's own and are mapped to
 * each store's ids through the catalog.
 *
 * - Every call answers its callback exactly once, on the game thread, never before the call
 *   returns. Callbacks may be empty for fire-and-forget writes.
 * - Achievement, stat and leaderboard writes made while no provider can take them are queued and
 *   sent once one can (bQueueWritesWhileUnavailable). Reads and cloud storage fail at once with
 *   NotAvailable instead.
 * - Successful unlocks and score submissions are broadcast on OnAchievementUnlocked and as
 *   Extended.Platform.* messages.
 * - Extended.Status shows which provider serves each feature.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFPlatformSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnResult = UEFPlatformProviderSubsystem::FOnResult;
	using FOnAchievements = UEFPlatformProviderSubsystem::FOnAchievements;
	using FOnStats = UEFPlatformProviderSubsystem::FOnStats;
	using FOnLeaderboard = UEFPlatformProviderSubsystem::FOnLeaderboard;
	using FOnCloudFile = UEFPlatformProviderSubsystem::FOnCloudFile;
	using FOnCloudFiles = UEFPlatformProviderSubsystem::FOnCloudFiles;

	DECLARE_MULTICAST_DELEGATE(FOnProvidersChanged);

	static UEFPlatformSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Providers

	/** Called by UEFPlatformProviderSubsystem::Initialize. */
	void RegisterProvider(UEFPlatformProviderSubsystem* Provider);

	/** Called by UEFPlatformProviderSubsystem::Deinitialize. */
	void UnregisterProvider(UEFPlatformProviderSubsystem* Provider);

	/** Registered providers in the order features are offered to them. */
	TArray<UEFPlatformProviderSubsystem*> GetProviders() const;

	UEFPlatformProviderSubsystem* FindProvider(FName ProviderName) const;

	/** The provider that serves Feature now, or null when none can. */
	UEFPlatformProviderSubsystem* GetProvider(EEFPlatformFeature Feature) const;

	/** Name of the provider that serves Feature now, or None. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform")
	FName GetProviderName(EEFPlatformFeature Feature) const;

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform")
	bool IsFeatureAvailable(EEFPlatformFeature Feature) const { return GetProvider(Feature) != nullptr; }

	/** The user of the first provider in order that has one signed in; bSignedIn is false when none has. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Platform")
	FEFPlatformUser GetLocalUser() const;

	// Achievements

	void UnlockAchievement(FName AchievementId, FOnResult OnDone = nullptr);
	void SetAchievementProgress(FName AchievementId, float Progress, FOnResult OnDone = nullptr);
	void QueryAchievements(FOnAchievements OnDone);

	// Stats

	void UpdateStat(FName StatId, int64 Value, EEFStatUpdateMode Mode = EEFStatUpdateMode::Set, FOnResult OnDone = nullptr);
	void UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone = nullptr);
	void QueryStats(const TArray<FName>& StatIds, FOnStats OnDone);

	// Leaderboards

	void SubmitScore(FName LeaderboardId, int64 Score, FOnResult OnDone = nullptr);
	void QueryLeaderboard(FName LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone);

	// Cloud storage

	void WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone);
	void ReadCloudFile(const FString& FileName, FOnCloudFile OnDone);
	void DeleteCloudFile(const FString& FileName, FOnResult OnDone);
	void ListCloudFiles(FOnCloudFiles OnDone);

	// Presence

	void SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone = nullptr);

	// Queue

	/** Writes waiting for a provider. */
	int32 GetQueuedWriteCount() const { return QueuedWrites.Num(); }

	/** Sends queued writes whose feature has a provider now. Runs on its own when a provider registers and periodically. */
	void FlushQueuedWrites();

	// Catalog

	/** The catalog from the settings, loaded on first use, or null. */
	const UEFPlatformCatalog* GetCatalog() const;

	/** Replaces the catalog from the settings, e.g. in tests. Null returns to the settings' catalog. */
	void SetCatalogOverride(const UEFPlatformCatalog* InCatalog);

	FString ToProviderId(EEFPlatformCatalogKind Kind, FName Id, FName ProviderName) const;
	FName FromProviderId(EEFPlatformCatalogKind Kind, const FString& ProviderId, FName ProviderName) const;

	/** An achievement was unlocked (or reached progress 1) on Provider. */
	UPROPERTY(BlueprintAssignable, Category = "Extended Framework|Platform")
	FEFOnAchievementUnlocked OnAchievementUnlocked;

	/** A provider registered or unregistered. */
	FOnProvidersChanged OnProvidersChanged;

private:
	struct FQueuedWrite
	{
		EEFPlatformFeature Feature = EEFPlatformFeature::Achievements;
		FString Description;
		TFunction<void(UEFPlatformProviderSubsystem& Provider, FOnResult Done)> Send;
		FOnResult OnDone;
	};

	/** Runs Write on Feature's provider now, or queues it; OnDone is answered asynchronously either way. */
	void RouteWrite(EEFPlatformFeature Feature, const FString& Description, TFunction<void(UEFPlatformProviderSubsystem& Provider, FOnResult Done)> Write, FOnResult OnDone);

	/** Calls OnDone with Result on the next tick. */
	void AnswerLater(FOnResult OnDone, const FEFResult& Result);

	FEFResult NoProvider(EEFPlatformFeature Feature) const;
	void BroadcastAchievementUnlocked(FName AchievementId, FName ProviderName);
	void BroadcastScoreSubmitted(FName LeaderboardId, int64 Score, FName ProviderName);
	void EnsureQueueTicker();
	bool TickQueue(float DeltaTime);

	TArray<TWeakObjectPtr<UEFPlatformProviderSubsystem>> Providers;
	TArray<FQueuedWrite> QueuedWrites;
	FTSTicker::FDelegateHandle QueueTickerHandle;

	/** Held so the loaded catalog is not garbage collected. Written from const GetCatalog(). */
	UPROPERTY(Transient)
	TObjectPtr<UEFPlatformCatalog> LoadedCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UEFPlatformCatalog> CatalogOverride;

	bool bCatalogLoadAttempted = false;
	int32 StatusHandle = 0;
};

// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/EFPlatformTypes.h"
#include "Result/EFResult.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFPlatformProvider.generated.h"

/**
 * Base for a store or back end that serves the platform layer: achievements, stats, leaderboards,
 * cloud storage and presence behind one API (UEFPlatformSubsystem).
 *
 * Each online plugin adds one subclass (Steam, EOS, PlayFab); the foundation ships a Local
 * provider. The subclass is a game instance subsystem, so it exists for every game instance and can
 * reach that game instance's other subsystems. It registers itself with UEFPlatformSubsystem in
 * Initialize; the platform subsystem then picks a provider per feature from the project settings.
 *
 *     UCLASS()
 *     class UESteamPlatformProvider : public UEFPlatformProviderSubsystem
 *     {
 *         virtual FName GetProviderName() const override { return TEXT("Steam"); }
 *         virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const override { ... }
 *         virtual void UnlockAchievement(const FString& AchievementId, FOnResult OnDone) override { ... }
 *     };
 *
 * Rules for implementations:
 * - Ids arrive already mapped to this provider's names (see UEFPlatformCatalog).
 * - Every call answers OnDone exactly once, on the game thread, with an FEFResult whose Source is
 *   the provider's name. OnDone is never empty when the platform subsystem calls; it may be called
 *   before the call returns.
 * - Features a provider does not implement keep the default, which answers NotAvailable.
 * - Answer calls still pending in Deinitialize with FEFResult::Cancelled.
 */
UCLASS(Abstract)
class EXTENDEDFOUNDATION_API UEFPlatformProviderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnResult = TFunction<void(const FEFResult& Result)>;
	using FOnAchievements = TFunction<void(const FEFResult& Result, const TArray<FEFAchievementState>& Achievements)>;
	using FOnStats = TFunction<void(const FEFResult& Result, const TArray<FEFStatValue>& Stats)>;
	using FOnLeaderboard = TFunction<void(const FEFResult& Result, const TArray<FEFLeaderboardEntry>& Entries)>;
	using FOnCloudFile = TFunction<void(const FEFResult& Result, const TArray<uint8>& Data)>;
	using FOnCloudFiles = TFunction<void(const FEFResult& Result, const TArray<FEFCloudFileInfo>& Files)>;

	/** Registers with the platform subsystem. Subclasses that override it must call Super first. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unregisters. Subclasses that override it must call Super last. */
	virtual void Deinitialize() override;

	/** The name used in settings, the catalog and results: Steam, EOS, PlayFab, Local. */
	virtual FName GetProviderName() const PURE_VIRTUAL(UEFPlatformProviderSubsystem::GetProviderName, return NAME_None;);

	/**
	 * Whether the provider can serve Feature now: SDK initialized, user signed in, feature enabled
	 * in its settings. The platform subsystem skips providers that cannot, so this must be cheap.
	 */
	virtual bool IsFeatureAvailable(EEFPlatformFeature Feature) const { return false; }

	/**
	 * Whether the provider implements Feature at all, available now or not. Return false for
	 * features it never serves (PlayFab has no achievements). While a registered store supports a
	 * feature, the Local provider does not take it over, so writes wait for that store instead.
	 */
	virtual bool SupportsFeature(EEFPlatformFeature Feature) const { return true; }

	/** True only for the Local provider: it serves a feature when no other registered provider supports it. */
	virtual bool IsFallbackProvider() const { return false; }

	/** The user the provider acts for. bSignedIn is false when there is none. */
	virtual FEFPlatformUser GetLocalUser() const;

	bool IsAnyFeatureAvailable() const;

	// Achievements

	/** Unlocks the achievement permanently. */
	virtual void UnlockAchievement(const FString& AchievementId, FOnResult OnDone);

	/**
	 * Reports progress from 0 to 1. Reaching 1 unlocks. The default unlocks at 1 and answers
	 * NotAvailable below it; providers that show progress override it.
	 */
	virtual void SetAchievementProgress(const FString& AchievementId, float Progress, FOnResult OnDone);

	/** Every achievement the provider knows for the local user, with ProviderId filled in. */
	virtual void QueryAchievements(FOnAchievements OnDone);

	// Stats

	/** Applies the updates (ProviderId and Value are set). Answer once for the whole batch. */
	virtual void UpdateStats(const TArray<FEFStatUpdate>& Updates, FOnResult OnDone);

	/** Current values of StatIds for the local user, with ProviderId filled in. */
	virtual void QueryStats(const TArray<FString>& StatIds, FOnStats OnDone);

	// Leaderboards

	/** Submits Score for the local user. Whether a lower score replaces a higher one is the board's own setting. */
	virtual void SubmitScore(const FString& LeaderboardId, int64 Score, FOnResult OnDone);

	/** Up to Count entries in Range, best first. */
	virtual void QueryLeaderboard(const FString& LeaderboardId, EEFLeaderboardRange Range, int32 Count, FOnLeaderboard OnDone);

	// Cloud storage (per user)

	virtual void WriteCloudFile(const FString& FileName, const TArray<uint8>& Data, FOnResult OnDone);

	/** Fails with code NotFound when the file does not exist. */
	virtual void ReadCloudFile(const FString& FileName, FOnCloudFile OnDone);

	/** Succeeds when the file does not exist. */
	virtual void DeleteCloudFile(const FString& FileName, FOnResult OnDone);

	virtual void ListCloudFiles(FOnCloudFiles OnDone);

	// Presence

	/** Sets the text friends see (Status) and any provider-specific key/value properties. */
	virtual void SetPresence(const FString& Status, const TMap<FString, FString>& Properties, FOnResult OnDone);

protected:
	/** A NotAvailable result from this provider saying Operation is not supported. */
	FEFResult NotSupported(const TCHAR* Operation) const;
};

/** Codes platform results use beyond EFResultCodes. */
namespace EFPlatformResultCodes
{
	/** The achievement, stat, leaderboard or cloud file does not exist. */
	inline constexpr const TCHAR* NotFound = TEXT("NotFound");
	/** The provider has no signed-in user. */
	inline constexpr const TCHAR* NotSignedIn = TEXT("NotSignedIn");
	/** The write was queued and dropped because the queue was full or the game shut down. */
	inline constexpr const TCHAR* QueueFull = TEXT("QueueFull");
}

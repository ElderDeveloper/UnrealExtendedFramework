// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"
#include "Identity/OnlineIdentityExtendedSteam.h"

class FOnlineSubsystemExtendedSteam;
class FOnlineAchievementsExtendedSteamCallbacks;

/**
 * Achievements interface backed by ISteamUserStats.
 *
 * Local player only: the Steam client API can only read/write achievements of the signed-in
 * user, so every method validates the passed player id against the local CSteamID and fails
 * (through the delegate where one exists) for any other id.
 *
 * Query model: since Steamworks SDK 1.61 the Steam client synchronizes the local user's
 * stats/achievements before the game process starts, so QueryAchievements /
 * QueryAchievementDescriptions read the SDK synchronously, fill the caches and fire their
 * delegate inside the call. On pre-1.61 SDKs QueryAchievements issues RequestCurrentStats and
 * completes asynchronously via UserStatsReceived_t.
 *
 * Write model: WriteAchievements supports ONE in-flight write. Each stat in the write object
 * whose value reaches the unlock threshold (float/double progress >= 100, bool true, or
 * int >= 100 — the common OSS convention of 100.0 meaning unlocked) is applied with
 * SetAchievement, then a single StoreStats commits the batch; the write delegate fires when
 * Steam confirms via UserStatsStored_t. Starting a second write while one is pending fails
 * immediately. Steam raises UserAchievementStored_t per fully unlocked achievement, which is
 * forwarded to OnAchievementUnlocked.
 */
class FOnlineAchievementsExtendedSteam : public IOnlineAchievements
{
public:
	explicit FOnlineAchievementsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineAchievementsExtendedSteam();

	//~ Begin IOnlineAchievements
	virtual void WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate = FOnAchievementsWrittenDelegate()) override;
	virtual void QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual void QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate = FOnQueryAchievementsCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement) override;
	virtual EOnlineCachedResult::Type GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement>& OutAchievements) override;
	virtual EOnlineCachedResult::Type GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc) override;
#if !UE_BUILD_SHIPPING
	virtual bool ResetAchievements(const FUniqueNetId& PlayerId) override;
#endif
	//~ End IOnlineAchievements

private:
	friend class FOnlineAchievementsExtendedSteamCallbacks;

	/** SteamID64 of the local user; 0 when the Steam client API is unavailable. */
	uint64 GetLocalSteamId64() const;

	/** True when PlayerId is a valid id equal to the local Steam user. */
	bool IsLocalPlayer(const FUniqueNetId& PlayerId) const;

	/** Reads the full achievement list from the SDK into CachedAchievements. False when Steam is unavailable. */
	bool CacheAchievements();

	/** Reads name/desc/hidden/unlock time per achievement into CachedAchievementDescs. False when Steam is unavailable. */
	bool CacheAchievementDescriptions();

	/** Completes the pending QueryAchievements (pre-1.61 async path and shared failure path). */
	void CompletePendingQueries(bool bSuccess);

	/** Completes the pending WriteAchievements. */
	void CompletePendingWrite(bool bSuccess);

	/** UserStatsStored_t arrived for this app (forwarded by the callback holder). */
	void HandleUserStatsStored(bool bSuccess);

	/** UserAchievementStored_t arrived (forwarded by the callback holder); empty progress means fully unlocked. */
	void HandleUserAchievementStored(const FString& AchievementApiName, bool bFullyUnlocked);

	/** UserStatsReceived_t for the local user arrived (pre-1.61 RequestCurrentStats completion). */
	void HandleUserStatsReceived(bool bSuccess);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Native Steam callback listeners (SDK-gated, defined in the cpp). */
	TSharedPtr<FOnlineAchievementsExtendedSteamCallbacks> Callbacks;

	/** Achievement list cached by the last successful QueryAchievements. Id = Steam API name. */
	TArray<FOnlineAchievement> CachedAchievements;
	bool bAchievementsCached = false;

	/** Descriptions cached by the last successful QueryAchievementDescriptions, keyed by API name. */
	TMap<FString, FOnlineAchievementDesc> CachedAchievementDescs;
	bool bDescriptionsCached = false;

	/** Pending pre-1.61 QueryAchievements delegates, completed on UserStatsReceived_t. */
	TArray<FOnQueryAchievementsCompleteDelegate> PendingQueryDelegates;

	/** One in-flight write: state kept until UserStatsStored_t confirms the StoreStats. */
	bool bWriteInFlight = false;
	FOnlineAchievementsWritePtr PendingWriteObject;
	FOnAchievementsWrittenDelegate PendingWriteDelegate;
};

typedef TSharedPtr<FOnlineAchievementsExtendedSteam, ESPMode::ThreadSafe> FOnlineAchievementsExtendedSteamPtr;

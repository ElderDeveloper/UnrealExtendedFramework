// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamGameServerStatsSubsystem.generated.h"

/** Fired when a RequestUserStats call completed for a user (stats getters are usable on success). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamGSUserStatsReceived, bool, bSuccess, FESteamId, User);

/** Fired when a StoreUserStats call completed for a user. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamGSUserStatsStored, bool, bSuccess, FESteamId, User);

/** Fired when a user's stats were unloaded; call RequestUserStats again before reading them. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamGSStatsUnloaded, FESteamId, User);

/**
 * Wraps ISteamGameServerStats: server-authoritative reads/writes of user stats and achievements.
 *
 * Availability: like UESteamGameServerSubsystem this wraps a GAME SERVER interface
 * (SteamGameServerStats()), which only exists after FExtendedSteamSharedModule::
 * InitializeSteamGameServer succeeded. The base-class client hooks do not apply; this subsystem
 * binds to the shared module's OnSteamGameServerInitialized / OnSteamGameServerShutdown delegates
 * in Initialize/Deinitialize instead. All methods return false while the game server API is down.
 *
 * Writes only work for stats the game server is allowed to edit, on servers declared as
 * officially controlled in the Steamworks settings. Call RequestUserStats and wait for
 * OnUserStatsReceived before reading; call StoreUserStats to persist writes.
 *
 * Concurrency: same-type async requests are serialized via an internal per-operation FIFO
 * queue (request stats, store stats each have their own). They complete in order and none are
 * dropped — issuing several before earlier ones finish is safe.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamGameServerStatsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Downloads the stats of a user. Result arrives on OnUserStatsReceived (bSuccess false when
	 * the user has no stats). Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool RequestUserStats(FESteamId User);

	/** Reads an int stat of a user (requires a successful RequestUserStats first). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool GetUserStatInt(FESteamId User, const FString& StatName, int32& OutValue) const;

	/** Reads a float stat of a user (requires a successful RequestUserStats first). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool GetUserStatFloat(FESteamId User, const FString& StatName, float& OutValue) const;

	/** Updates an int stat of a user locally; persist with StoreUserStats. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool SetUserStatInt(FESteamId User, const FString& StatName, int32 Value);

	/** Updates a float stat of a user locally; persist with StoreUserStats. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool SetUserStatFloat(FESteamId User, const FString& StatName, float Value);

	/** Updates an AVGRATE stat of a user with new session data locally; persist with StoreUserStats. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool UpdateUserAvgRateStat(FESteamId User, const FString& StatName, float CountThisSession, double SessionLength);

	/** Reads whether a user unlocked an achievement (requires a successful RequestUserStats first). */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool GetUserAchievement(FESteamId User, const FString& AchievementName, bool& bOutAchieved) const;

	/** Unlocks an achievement for a user locally; persist with StoreUserStats. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool SetUserAchievement(FESteamId User, const FString& AchievementName);

	/** Relocks an achievement for a user locally; persist with StoreUserStats. */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool ClearUserAchievement(FESteamId User, const FString& AchievementName);

	/**
	 * Persists the pending stat/achievement writes for a user on the Steam servers.
	 * Result arrives on OnUserStatsStored. Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|GameServerStats")
	bool StoreUserStats(FESteamId User);

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServerStats")
	FOnSteamGSUserStatsReceived OnUserStatsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServerStats")
	FOnSteamGSUserStatsStored OnUserStatsStored;

	UPROPERTY(BlueprintAssignable, Category = "Steam|GameServerStats")
	FOnSteamGSStatsUnloaded OnStatsUnloaded;

private:
	/** Called when the shared module finished initializing the game server API (or immediately when already up). */
	void HandleGameServerInitialized();

	/** Called when the game server API shuts down. */
	void HandleGameServerShutdown();

	/** True when the game server API is initialized and the ISteamGameServerStats interface is reachable. */
	bool IsGameServerStatsAvailable() const;

	friend class FESteamGameServerStatsCallbacks;
	TSharedPtr<class FESteamGameServerStatsCallbacks> Callbacks;

	FDelegateHandle GameServerInitializedHandle;
	FDelegateHandle GameServerShutdownHandle;
};

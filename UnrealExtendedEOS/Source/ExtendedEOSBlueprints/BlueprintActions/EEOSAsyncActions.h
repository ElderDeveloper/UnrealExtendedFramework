// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EEOSAsyncActionBase.h"
#include "Shared/EEOSTypes.h"
#include "EEOSAsyncActions.generated.h"

class USaveGame;
class UEEOSStatsSubsystem;
class UEEOSAchievementSubsystem;
class UEEOSAuthSubsystem;
class UEEOSPlayerStorageSubsystem;
class UEEOSEcomSubsystem;
class UEEOSSessionSubsystem;
class UEEOSLobbySubsystem;
class UEEOSMatchmakingSubsystem;
class UEEOSLeaderboardSubsystem;
class UEEOSFriendsSubsystem;
class UEEOSChatSubsystem;
class UEEOSSanctionsSubsystem;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Shared OnFailure pin
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Every node derives from UEEOSAsyncActionBase (EEOSAsyncActionBase.h), which owns the
//  FOnAsyncFailed OnFailure pin. It also fires for cancellation (the node's Async Task pin),
//  timeouts and a missing subsystem.

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Completion pattern (uniform across all nodes)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Every node binds its handler(s) FIRST, then calls the subsystem entry point inside
//  CallWithContext(). The subsystems carry that call context to their completion broadcast, so
//  each handler returns early unless IsResultForThisCall(): two identical nodes running at once
//  each finish on their own result. Broadcasts without a context are still accepted.
//  Subsystem entry points return bool: false means the call was rejected (an
//  operation is already in flight — log-only, NO delegate ever fires for that
//  call) or failed pre-flight (some pre-flight failures DO broadcast, and do so
//  synchronously during the call — reaching the already-bound handler).
//  On a false return the node unbinds and fires OnFailure exactly once. FinishAction()
//  guards against double-firing: a pre-flight broadcast that already completed the node
//  during the call suppresses the false-return OnFailure path (IsFinished()).
//  CancelWork() unbinds the node on cancellation and timeout.
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Stats
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncStatsQueried, const TArray<FEEOSStat>&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsyncStatIngested);

/**
 * Query player stats from EOS.
 * Returns the stat values (e.g., GoldCoins, EnemiesKilled) on the OnSuccess pin.
 * Use GetStatValue() on the subsystem to read individual values after this completes.
 * OnFailure fires if the stat query fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Query Stats"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncQueryStats : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query stats for the local player.
	 * @param StatNames  Array of stat names to query (must match names in EOS DevPortal).
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Query Stats"), Category = "EOS|Async|Stats")
	static UEOSAsyncQueryStats* QueryStats(UObject* WorldContext, const TArray<FString>& StatNames);

	/** Fires when stats are successfully retrieved. Provides the stat array. */
	UPROPERTY(BlueprintAssignable) FOnAsyncStatsQueried OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TArray<FString> StatNames;
	TWeakObjectPtr<UEEOSStatsSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const TArray<FEEOSStat>& Stats);
};

/**
 * Add a value to a player stat (e.g., +100 gold, +1 kill).
 * The stat must exist in the EOS DevPortal with SUM aggregation.
 * OnFailure fires if the stat ingestion fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Ingest Stat"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncIngestStat : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Ingest (add) a value to a stat for the local player.
	 * @param StatName  The stat name as configured in the EOS DevPortal.
	 * @param Amount    The value to add (positive integer).
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Ingest Stat"), Category = "EOS|Async|Stats")
	static UEOSAsyncIngestStat* IngestStat(UObject* WorldContext, const FString& StatName, int32 Amount);

	/** Fires when the stat is successfully ingested. */
	UPROPERTY(BlueprintAssignable) FOnAsyncStatIngested OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString StatName;
	int32 Amount = 0;
	TWeakObjectPtr<UEEOSStatsSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Achievements
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncAchievementsQueried, const TArray<FEEOSAchievement>&, Achievements);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncAchievementUnlocked, const FString&, AchievementId);

/**
 * Query all achievements and their unlock progress for the local player.
 * Use the returned array to build achievement UI, check unlock status, and gate content.
 * OnFailure fires if the achievement query fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Query Achievements"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncQueryAchievements : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Query all achievements and progress for the local player. */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Query Achievements"), Category = "EOS|Async|Achievements")
	static UEOSAsyncQueryAchievements* QueryAchievements(UObject* WorldContext);

	/** Fires when achievements are successfully retrieved. Provides the full achievement array. */
	UPROPERTY(BlueprintAssignable) FOnAsyncAchievementsQueried OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TWeakObjectPtr<UEEOSAchievementSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const TArray<FEEOSAchievement>& Achievements);
};

/**
 * Instantly unlock (complete) an achievement for the local player.
 * The achievement must exist in the EOS DevPortal.
 * OnFailure fires if the unlock fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Unlock Achievement"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncUnlockAchievement : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Unlock an achievement by ID.
	 * @param AchievementId  The achievement ID as configured in the EOS DevPortal.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Unlock Achievement"), Category = "EOS|Async|Achievements")
	static UEOSAsyncUnlockAchievement* UnlockAchievement(UObject* WorldContext, const FString& AchievementId);

	/** Fires when the achievement is successfully unlocked. */
	UPROPERTY(BlueprintAssignable) FOnAsyncAchievementUnlocked OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString AchievementId;
	TWeakObjectPtr<UEEOSAchievementSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& Id);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Connect Login
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncConnectLoginSuccess, const FString&, ProductUserId);

/**
 * Log in to EOS Game Services (Connect) using a platform account.
 * Supports Steam, PSN, Xbox, Nintendo, Discord, DeviceId, Epic, and more.
 * Returns the ProductUserId on success, which is needed for all game services.
 *
 * For DeviceId login, leave Token empty — it creates the device ID automatically.
 * OnFailure fires if login fails or could not be started (e.g. a Connect login is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Connect Login"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncConnectLogin : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Perform EOS Connect Login.
	 * @param LoginType    The platform to authenticate with (Steam, DeviceId, Epic, etc.)
	 * @param Token        The platform authentication token. Leave empty for DeviceId.
	 * @param DisplayName  Display name for DeviceId logins. Ignored for other types.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Connect Login"), Category = "EOS|Async|Auth")
	static UEOSAsyncConnectLogin* ConnectLogin(UObject* WorldContext, EEOSConnectLoginType LoginType, const FString& Token = TEXT(""), const FString& DisplayName = TEXT(""));

	/** Fires on successful login. Provides the ProductUserId for game services. */
	UPROPERTY(BlueprintAssignable) FOnAsyncConnectLoginSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	EEOSConnectLoginType LoginType = EEOSConnectLoginType::DeviceId;
	FString Token;
	FString DisplayName;
	TWeakObjectPtr<UEEOSAuthSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& ProductUserId, const FString& Error);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Player Storage (SaveGame)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncSaveGameRead, USaveGame*, SaveGame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsyncSaveGameWritten);

/**
 * Read a USaveGame object from EOS cloud storage.
 * The file must have been previously written with "EOS: Write Save Game".
 * Cast the returned SaveGame to your custom class.
 * OnFailure fires if the read fails (file not found, network error, etc.) or could not be
 * started (an operation on this file is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Read Save Game"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncReadSaveGame : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Read a save game from EOS Player Data Storage.
	 * @param FileName  The cloud file name (e.g., "inventory", "progress").
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Read Save Game"), Category = "EOS|Async|Storage")
	static UEOSAsyncReadSaveGame* ReadSaveGame(UObject* WorldContext, const FString& FileName);

	/** Fires when the save game is successfully loaded. Cast the result to your SaveGame class. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSaveGameRead OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString FileName;
	TWeakObjectPtr<UEEOSPlayerStorageSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InFileName, USaveGame* SaveGame);
};

/**
 * Write a USaveGame object to EOS cloud storage.
 * The object is serialized automatically. Use this for inventory, progress, settings, etc.
 * OnFailure fires if the write fails or could not be started (an operation on this file is
 * already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Write Save Game"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncWriteSaveGame : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Write a save game to EOS Player Data Storage.
	 * @param FileName       The cloud file name (e.g., "inventory", "progress").
	 * @param SaveGameObject The USaveGame object to serialize and upload.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Write Save Game"), Category = "EOS|Async|Storage")
	static UEOSAsyncWriteSaveGame* WriteSaveGame(UObject* WorldContext, const FString& FileName, USaveGame* SaveGameObject);

	/** Fires when the save game is successfully written to cloud. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSaveGameWritten OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString FileName;
	UPROPERTY() USaveGame* SaveGameObject = nullptr;
	TWeakObjectPtr<UEEOSPlayerStorageSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InFileName);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Ecom (Store)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncOffersQueried, const TArray<FEEOSCatalogOffer>&, Offers);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncCheckoutComplete, const FString&, TransactionId);

/**
 * Query available store offers (catalog) from the EOS DevPortal.
 * Returns the list of purchasable items with prices.
 * OnFailure fires if the query fails or could not be started (an offers query is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Query Offers"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncQueryOffers : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Query all available offers from the EOS store catalog. */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Query Offers"), Category = "EOS|Async|Ecom")
	static UEOSAsyncQueryOffers* QueryOffers(UObject* WorldContext);

	/** Fires when offers are successfully retrieved. */
	UPROPERTY(BlueprintAssignable) FOnAsyncOffersQueried OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TWeakObjectPtr<UEEOSEcomSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const TArray<FEEOSCatalogOffer>& Offers);
};

/**
 * Initiate a checkout/purchase for a store offer.
 * Opens the Epic overlay for payment. Returns the transaction ID on success.
 * OnFailure fires if the checkout fails, is cancelled, or could not be started (a checkout is
 * already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Checkout"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncCheckout : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Purchase an offer from the EOS store.
	 * @param OfferId  The offer ID from QueryOffers results.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Checkout"), Category = "EOS|Async|Ecom")
	static UEOSAsyncCheckout* Checkout(UObject* WorldContext, const FString& OfferId);

	/** Fires on successful purchase. Provides the TransactionId. */
	UPROPERTY(BlueprintAssignable) FOnAsyncCheckoutComplete OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString OfferId;
	TWeakObjectPtr<UEEOSEcomSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& TransactionId);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Sessions (Dedicated/Listen Server)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncSessionCreated, const FString&, SessionName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncSessionsFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncSessionJoined, const FString&, SessionName);

/**
 * Create a new EOS game session for dedicated or listen server gameplay.
 * Use this for server-browser style games. The session becomes searchable by other players.
 *
 * For pre-game lobbies with member management, use "EOS: Create Lobby" instead.
 * OnFailure fires if session creation fails or could not be started (a create/destroy is
 * already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Create Session"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncCreateSession : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Create a game session.
	 * @param MaxPlayers   Maximum number of players allowed (default 4).
	 * @param bIsLAN       True for LAN-only sessions.
	 * @param bIsPresence  True to show session in presence (friends can see you).
	 * @param SessionName  Internal session name (default "GameSession").
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Create Session"), Category = "EOS|Async|Sessions")
	static UEOSAsyncCreateSession* CreateSession(UObject* WorldContext, int32 MaxPlayers = 4, bool bIsLAN = false, bool bIsPresence = true, const FString& SessionName = TEXT("GameSession"));

	/** Fires when the session is successfully created. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSessionCreated OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 MaxPlayers = 4;
	bool bIsLAN = false;
	bool bIsPresence = true;
	FString SessionName;
	TWeakObjectPtr<UEEOSSessionSubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InSessionName);
};

/**
 * Search for available EOS game sessions.
 * Returns a list of joinable sessions — use the index to join one with "EOS: Join Session".
 * OnFailure fires on admission rejection, preflight failure, or native search failure.
 */
UCLASS(meta = (DisplayName = "EOS: Find Sessions"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncFindSessions : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Search for available sessions.
	 * @param MaxResults  Maximum number of results to return (default 20).
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Find Sessions"), Category = "EOS|Async|Sessions")
	static UEOSAsyncFindSessions* FindSessions(UObject* WorldContext, int32 MaxResults = 20);

	/** Fires after a successful native search, including an empty results array. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSessionsFound OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 MaxResults = 20;
	TWeakObjectPtr<UEEOSSessionSubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(const TArray<FEEOSSessionSearchResult>& Results);
};

/**
 * Join a game session from the search results.
 * Use the index from the "EOS: Find Sessions" results array.
 * After joining, use ClientTravel on the Session Subsystem to travel to the host.
 * OnFailure fires if the join fails (session full, no longer exists, etc.) or could not be
 * started (a join is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Join Session"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncJoinSession : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Join a session by search result index.
	 * @param SearchResultIndex  The index in the search results array from "EOS: Find Sessions".
	 * @param SessionName        Internal session name (default "GameSession").
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Join Session"), Category = "EOS|Async|Sessions")
	static UEOSAsyncJoinSession* JoinSession(UObject* WorldContext, int32 SearchResultIndex, const FString& SessionName = TEXT("GameSession"));

	/** Fires when successfully joined. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSessionJoined OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 SearchResultIndex = 0;
	FString SessionName;
	TWeakObjectPtr<UEEOSSessionSubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InSessionName);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Lobbies (Pre-Game Social Rooms)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncLobbyCreated, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncLobbiesFound, const TArray<FEEOSSessionSearchResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncLobbyJoined, const FString&, LobbyId);

/**
 * Create a new EOS lobby for pre-game gathering.
 * Lobbies support member management, synced attributes, voice chat, and invites.
 * Use lobbies for character select, ready-up screens, party systems.
 *
 * For dedicated/listen server gameplay, use "EOS: Create Session" instead.
 * OnFailure fires if lobby creation fails or could not be started (a lobby create/destroy is
 * already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Create Lobby"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncCreateLobby : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Create a lobby.
	 * @param MaxMembers    Maximum lobby members (default 4).
	 * @param bIsPublic     True = searchable, False = invite-only.
	 * @param bUseVoiceChat True to enable built-in voice chat.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Create Lobby"), Category = "EOS|Async|Lobbies")
	static UEOSAsyncCreateLobby* CreateLobby(UObject* WorldContext, int32 MaxMembers = 4, bool bIsPublic = true, bool bUseVoiceChat = false);

	/** Fires when the lobby is successfully created. Provides the LobbyId. */
	UPROPERTY(BlueprintAssignable) FOnAsyncLobbyCreated OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 MaxMembers = 4;
	bool bIsPublic = true;
	bool bUseVoiceChat = false;
	TWeakObjectPtr<UEEOSLobbySubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& LobbyId);
};

/**
 * Search for available EOS lobbies.
 * Returns a list of joinable lobbies — use the index to join one with "EOS: Join Lobby".
 * OnFailure fires on admission rejection, preflight failure, or native search failure.
 */
UCLASS(meta = (DisplayName = "EOS: Find Lobbies"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncFindLobbies : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Search for available lobbies.
	 * @param MaxResults  Maximum number of results to return (default 20).
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Find Lobbies"), Category = "EOS|Async|Lobbies")
	static UEOSAsyncFindLobbies* FindLobbies(UObject* WorldContext, int32 MaxResults = 20);

	/** Fires after a successful native search, including an empty results array. */
	UPROPERTY(BlueprintAssignable) FOnAsyncLobbiesFound OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 MaxResults = 20;
	TWeakObjectPtr<UEEOSLobbySubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(const TArray<FEEOSSessionSearchResult>& Results);
};

/**
 * Join an existing EOS lobby from search results.
 * After joining, use lobby attributes and member events for pre-game coordination.
 * OnFailure fires if the join fails (lobby full, no longer exists, etc.) or could not be
 * started (a join-lobby is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Join Lobby"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncJoinLobby : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Join a lobby by search result index.
	 * @param SearchResultIndex  The index in the search results from "EOS: Find Lobbies".
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Join Lobby"), Category = "EOS|Async|Lobbies")
	static UEOSAsyncJoinLobby* JoinLobby(UObject* WorldContext, int32 SearchResultIndex);

	/** Fires when successfully joined. Provides the LobbyId. */
	UPROPERTY(BlueprintAssignable) FOnAsyncLobbyJoined OnSuccess;

	/** Additive typed terminal pin; includes rejection codes and optional native/SDK detail. */
	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	int32 SearchResultIndex = 0;
	TWeakObjectPtr<UEEOSLobbySubsystem> Subsystem;
	int64 IgnoredRequestId = 0;
	int64 ExpectedRequestId = 0;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& LobbyId);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Matchmaking (Automatic Queue-Based)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncMatchFound, const FString&, SessionId);

/**
 * Start search-based matchmaking on EOS.
 * Repeatedly searches for sessions advertising the given pool (with retry and reject-exclusion —
 * see UEEOSMatchmakingSubsystem for the retry settings). Hosts advertise a pool by adding a
 * "MATCHMAKINGPOOL" custom setting in CreateSessionAdvanced.
 * Fires OnSuccess when a match is found — use AcceptMatch/RejectMatch on the subsystem.
 * Exactly one pin fires: found / failed / cancelled / could-not-start all complete the node.
 * OnFailure fires if matchmaking fails, times out, is cancelled ("Matchmaking cancelled"), or
 * could not be started (a matchmaking cycle is already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Start Matchmaking"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncStartMatchmaking : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Start matchmaking.
	 * @param QueueName  The matchmaking queue (configured in EOS DevPortal, default "Default").
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Start Matchmaking"), Category = "EOS|Async|Matchmaking")
	static UEOSAsyncStartMatchmaking* StartMatchmaking(UObject* WorldContext, const FString& QueueName = TEXT("Default"));

	/** Fires when a match is found. Provides the SessionId to join. NOTE: if the cycle ends
	 *  successfully without this node observing a match-found event (e.g. the match was already
	 *  accepted and joined), OnSuccess fires with an EMPTY SessionId — the completion delegate
	 *  carries no id. */
	UPROPERTY(BlueprintAssignable) FOnAsyncMatchFound OnSuccess;

	UPROPERTY(BlueprintAssignable) FOnEEOSOperationOutcome OnDetailedCompleted;

	/** Also cancels the matchmaking cycle this node started (OnFailure: "Matchmaking cancelled"). */
	virtual void Cancel() override;

protected:
	virtual void ExecuteAction() override;
	/** Unbind all three subsystem delegates this node binds. */
	virtual void CancelWork() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;

private:
	FString QueueName;
	TWeakObjectPtr<UEEOSMatchmakingSubsystem> Subsystem;
	int64 IgnoredCycleId = 0;
	int64 ExpectedCycleId = 0;

	UFUNCTION() void HandleMatchFound(const FString& SessionId);
	UFUNCTION() void HandleComplete(bool bSuccess, const FString& ErrorMessage);
	UFUNCTION() void HandleCancelled();
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Leaderboards
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncLeaderboardQueried, const TArray<FEEOSLeaderboardEntry>&, Entries);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncScoreUploaded, const FString&, LeaderboardId);

/**
 * Query a leaderboard from EOS.
 * Returns ranked entries with player names and scores.
 * OnFailure fires if the query fails or could not be started (another leaderboard query is
 * already in flight).
 */
UCLASS(meta = (DisplayName = "EOS: Query Leaderboard"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncQueryLeaderboard : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query a global leaderboard.
	 * @param LeaderboardId  The leaderboard ID as configured in the EOS DevPortal.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Query Leaderboard"), Category = "EOS|Async|Leaderboards")
	static UEOSAsyncQueryLeaderboard* QueryLeaderboard(UObject* WorldContext, const FString& LeaderboardId);

	/** Fires when the leaderboard is retrieved. Provides the ranked entries. */
	UPROPERTY(BlueprintAssignable) FOnAsyncLeaderboardQueried OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString LeaderboardId;
	TWeakObjectPtr<UEEOSLeaderboardSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const TArray<FEEOSLeaderboardEntry>& Entries);
};

/**
 * Upload a score to an EOS leaderboard.
 * The leaderboard must exist in the EOS DevPortal.
 * OnFailure fires if the upload fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Upload Score"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncUploadScore : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Upload a score.
	 * @param LeaderboardId  The leaderboard to upload to.
	 * @param Score          The score value.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Upload Score"), Category = "EOS|Async|Leaderboards")
	static UEOSAsyncUploadScore* UploadScore(UObject* WorldContext, const FString& LeaderboardId, int32 Score);

	/** Fires on successful upload. */
	UPROPERTY(BlueprintAssignable) FOnAsyncScoreUploaded OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString LeaderboardId;
	int32 Score = 0;
	TWeakObjectPtr<UEEOSLeaderboardSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InLeaderboardId);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Friends
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncFriendsListReady, const TArray<FEEOSFriendInfo>&, Friends);

/**
 * Read (refresh) the friends list from EOS.
 * Returns the full friends list with online status and display names.
 * OnFailure fires if the read could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Read Friends List"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncReadFriendsList : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Read the friends list for the local player. */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Read Friends List"), Category = "EOS|Async|Friends")
	static UEOSAsyncReadFriendsList* ReadFriendsList(UObject* WorldContext);

	/** Fires when the friends list is ready (an empty list may indicate a failed read — the
	 *  subsystem broadcasts an empty list on failure). */
	UPROPERTY(BlueprintAssignable) FOnAsyncFriendsListReady OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TWeakObjectPtr<UEEOSFriendsSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(const TArray<FEEOSFriendInfo>& Friends);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Chat
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncChatChannelJoined, const FString&, ChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncChatMessageSent, const FString&, ChannelName);

/**
 * Join a text chat channel.
 * After joining, messages are received via the ChatSubsystem's OnChatMessageReceived delegate.
 * OnFailure fires if the join fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Join Chat Channel"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncJoinChatChannel : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Join a chat channel.
	 * @param ChannelName  The channel to join (e.g., "General", "Team_Red", "Trade").
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Join Chat Channel"), Category = "EOS|Async|Chat")
	static UEOSAsyncJoinChatChannel* JoinChatChannel(UObject* WorldContext, const FString& ChannelName);

	/** Fires when the channel is joined. */
	UPROPERTY(BlueprintAssignable) FOnAsyncChatChannelJoined OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString ChannelName;
	TWeakObjectPtr<UEEOSChatSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InChannelName);
};

/**
 * Send a text message to a chat channel.
 * The channel must be joined first with "EOS: Join Chat Channel".
 * OnFailure fires if the send fails or could not be started.
 */
UCLASS(meta = (DisplayName = "EOS: Send Chat Message"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncSendChatMessage : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Send a message to a channel.
	 * @param ChannelName  The channel to send to (must be joined).
	 * @param Message      The text message content.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Send Chat Message"), Category = "EOS|Async|Chat")
	static UEOSAsyncSendChatMessage* SendChatMessage(UObject* WorldContext, const FString& ChannelName, const FString& Message);

	/** Fires when the message is sent. */
	UPROPERTY(BlueprintAssignable) FOnAsyncChatMessageSent OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString ChannelName;
	FString Message;
	TWeakObjectPtr<UEEOSChatSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& InChannelName);
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Sanctions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAsyncSanctionsQueried, const TArray<FEEOSSanction>&, Sanctions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAsyncPlayerReported);

/**
 * Query active sanctions for a player.
 * Returns bans, mutes, and other restrictions. Check on login to enforce sanctions.
 * Correlates by target: only the completion carrying THIS node's target PUID completes the
 * node — concurrent queries for other targets are ignored (no cross-target wrong data).
 * OnFailure fires if the query fails (including pre-flight failures such as EOS unavailable or
 * an unparseable target id).
 */
UCLASS(meta = (DisplayName = "EOS: Query Sanctions"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncQuerySanctions : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query active sanctions.
	 * @param TargetUserId  The user to query sanctions for — a bare Product User ID or a
	 *                      composite "<EpicAccountId>|<ProductUserId>" net-id string.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Query Sanctions"), Category = "EOS|Async|Sanctions")
	static UEOSAsyncQuerySanctions* QuerySanctions(UObject* WorldContext, const FString& TargetUserId);

	/** Fires when this node's target's sanctions are retrieved. */
	UPROPERTY(BlueprintAssignable) FOnAsyncSanctionsQueried OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString TargetUserId;
	/** Bare target PUID extracted at ExecuteAction (empty when TargetUserId had no parseable PUID).
	 *  The subsystem broadcasts the bare PUID on every completion — compared for correlation. */
	FString TargetPUID;
	TWeakObjectPtr<UEEOSSanctionsSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& CompletedTargetUserId, const TArray<FEEOSSanction>& Sanctions);
};

/**
 * Report a player for bad behavior.
 * The report is sent to EOS for review. Use categories from EOS DevPortal.
 * Correlates by target: only the completion carrying THIS node's target PUID completes the
 * node — concurrent reports against other targets are ignored.
 * OnFailure fires if the report fails (including pre-flight failures such as EOS unavailable,
 * no logged-in reporter, or an unparseable target id).
 */
UCLASS(meta = (DisplayName = "EOS: Report Player"))
class EXTENDEDEOSBLUEPRINTS_API UEOSAsyncReportPlayer : public UEEOSAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Report a player.
	 * @param TargetUserId  The user to report — a bare Product User ID or a composite
	 *                      "<EpicAccountId>|<ProductUserId>" net-id string.
	 * @param Reason        Short reason code (e.g., "CHEATING", "HARASSMENT").
	 * @param Message       Detailed description from the reporter.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "EOS: Report Player"), Category = "EOS|Async|Sanctions")
	static UEOSAsyncReportPlayer* ReportPlayer(UObject* WorldContext, const FString& TargetUserId, const FString& Reason, const FString& Message);

	/** Fires when the report is successfully sent. */
	UPROPERTY(BlueprintAssignable) FOnAsyncPlayerReported OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString TargetUserId;
	/** Bare target PUID extracted at ExecuteAction (empty when TargetUserId had no parseable PUID).
	 *  The subsystem broadcasts the bare PUID on every completion — compared for correlation. */
	FString TargetPUID;
	FString Reason;
	FString Message;
	TWeakObjectPtr<UEEOSSanctionsSubsystem> Subsystem;

	UFUNCTION() void HandleComplete(bool bSuccess, const FString& CompletedTargetUserId);
};

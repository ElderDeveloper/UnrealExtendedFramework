// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Matchmaking/ESteamMatchmakingSubsystem.h"
#include "ESteamMatchmakingAsyncActions.generated.h"

/** Completion pin for lobby create/join nodes (LobbyId is invalid on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncLobbyPin, FESteamId, LobbyId);

/** Completion pin for the lobby list node (Lobbies is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncLobbyListPin, const TArray<FESteamId>&, Lobbies);

/**
 * Creates a lobby on the Steam servers and completes when the matching result
 * arrives from UESteamMatchmakingSubsystem (the local user has joined the lobby
 * when OnSuccess fires).
 */
UCLASS()
class USteamAsyncCreateLobby : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Creates a lobby of the given type with room for MaxMembers users.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Create Lobby"))
	static USteamAsyncCreateLobby* CreateLobby(UObject* WorldContext, EESteamLobbyType LobbyType, int32 MaxMembers, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The lobby was created and joined; LobbyId identifies it. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyPin OnSuccess;

	/** Steam is unavailable or the lobby could not be created; LobbyId is invalid. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyPin OnFailure;

private:
	UFUNCTION()
	void HandleLobbyCreated(bool bSuccess, FESteamId LobbyId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, FESteamId LobbyId);

	TWeakObjectPtr<UESteamMatchmakingSubsystem> MatchmakingSubsystem;
	EESteamLobbyType LobbyType = EESteamLobbyType::Private;
	int32 MaxMembers = 0;
};

/**
 * Joins an existing lobby and completes when the matching enter result arrives
 * from UESteamMatchmakingSubsystem.
 */
UCLASS()
class USteamAsyncJoinLobby : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Joins the given lobby; lobby metadata is usable when OnSuccess fires.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Join Lobby"))
	static USteamAsyncJoinLobby* JoinLobby(UObject* WorldContext, FESteamId LobbyId, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The lobby was joined; LobbyId identifies it. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyPin OnSuccess;

	/** Steam is unavailable or the lobby could not be joined; LobbyId echoes the request. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyPin OnFailure;

private:
	UFUNCTION()
	void HandleLobbyEntered(bool bSuccess, FESteamId EnteredLobbyId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, FESteamId EnteredLobbyId);

	TWeakObjectPtr<UESteamMatchmakingSubsystem> MatchmakingSubsystem;
	FESteamId LobbyId;
};

/**
 * Searches for lobbies with a distance and result-count filter and completes when
 * the list arrives from UESteamMatchmakingSubsystem. Filters added on the subsystem
 * before this node runs (string/numerical) also apply to the search.
 */
UCLASS()
class USteamAsyncRequestLobbyList : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Searches for joinable lobbies (full lobbies are never returned).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Matchmaking", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Lobby List"))
	static USteamAsyncRequestLobbyList* RequestLobbyList(UObject* WorldContext, EESteamLobbyDistanceFilter DistanceFilter, int32 MaxResults, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The search completed; Lobbies holds the matches (possibly empty). */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyListPin OnSuccess;

	/** Steam is unavailable or the search failed; Lobbies is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncLobbyListPin OnFailure;

private:
	UFUNCTION()
	void HandleLobbyListReceived(bool bSuccess, const TArray<FESteamId>& Lobbies);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamId>& Lobbies);

	TWeakObjectPtr<UESteamMatchmakingSubsystem> MatchmakingSubsystem;
	EESteamLobbyDistanceFilter DistanceFilter = EESteamLobbyDistanceFilter::Default;
	int32 MaxResults = 0;
};

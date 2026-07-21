// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "MatchmakingServers/ESteamMatchmakingServersSubsystem.h"
#include "ESteamMatchmakingServersAsyncActions.generated.h"

/** Completion pin for the ping-server node (Server is default-initialized on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncServerPingPin, const FESteamServerInfo&, Server);

/** Completion pin for the server-rules node (Keys and Values are parallel arrays, empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncServerRulesPin, const TArray<FString>&, Keys, const TArray<FString>&, Values);

/**
 * Pings a single game server and completes when the matching response arrives from
 * UESteamMatchmakingServersSubsystem.
 */
UCLASS()
class USteamAsyncPingServer : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Pings a server ("a.b.c.d" + query port) for updated details.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|ServerBrowser", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Ping Server"))
	static USteamAsyncPingServer* PingServer(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The server responded; Server holds its details. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncServerPingPin OnSuccess;

	/** Steam is unavailable or the server did not respond; Server is default-initialized. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncServerPingPin OnFailure;

private:
	UFUNCTION()
	void HandlePingResponded(bool bSuccess, const FESteamServerInfo& Server);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FESteamServerInfo& Server);

	TWeakObjectPtr<UESteamMatchmakingServersSubsystem> ServersSubsystem;
	FString Ip;
	int32 Port = 0;
};

/**
 * Requests the rules (key/value pairs) of a single game server and completes when the matching
 * response arrives from UESteamMatchmakingServersSubsystem.
 */
UCLASS()
class USteamAsyncRequestServerRules : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests the rules of a server ("a.b.c.d" + query port).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|ServerBrowser", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Server Rules"))
	static USteamAsyncRequestServerRules* RequestServerRules(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The rules arrived; Keys and Values are parallel arrays. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncServerRulesPin OnSuccess;

	/** Steam is unavailable or the server did not respond; Keys and Values are empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncServerRulesPin OnFailure;

private:
	UFUNCTION()
	void HandleRulesReceived(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values);

	TWeakObjectPtr<UESteamMatchmakingServersSubsystem> ServersSubsystem;
	FString Ip;
	int32 Port = 0;
};

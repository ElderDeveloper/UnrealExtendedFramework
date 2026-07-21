// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Parties/ESteamPartiesSubsystem.h"
#include "ESteamPartiesAsyncActions.generated.h"

/** Completion pin for the join-party node (ConnectString is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncJoinPartyPin, int64, BeaconId, const FString&, ConnectString);

/** Completion pin for the create-beacon node (BeaconId is 0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncCreateBeaconPin, int64, BeaconId);

/**
 * Asks Steam to reserve a slot on a party beacon and completes when the matching join
 * result arrives from UESteamPartiesSubsystem (use ConnectString to connect on success).
 */
UCLASS()
class USteamAsyncJoinParty : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Joins the party behind a beacon id (enumerate beacons with the Parties subsystem first).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Parties", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Join Party"))
	static USteamAsyncJoinParty* JoinParty(UObject* WorldContext, int64 BeaconId, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** A slot was reserved; ConnectString holds the game-specific connect instructions. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncJoinPartyPin OnSuccess;

	/** Steam is unavailable or the join failed; ConnectString is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncJoinPartyPin OnFailure;

private:
	UFUNCTION()
	void HandlePartyJoined(bool bSuccess, int64 InBeaconId, const FString& ConnectString);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 InBeaconId, const FString& ConnectString);

	TWeakObjectPtr<UESteamPartiesSubsystem> PartiesSubsystem;
	int64 BeaconId = 0;
};

/**
 * Creates a party beacon and completes when the matching create result arrives from
 * UESteamPartiesSubsystem (OnSuccess carries the new beacon id).
 */
UCLASS()
class USteamAsyncCreateBeacon : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Creates a beacon with OpenSlots open slots. ConnectString is handed to joiners; Metadata is
	 * free-form beacon data. The first available beacon location is used (see the subsystem).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Parties", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Create Beacon"))
	static USteamAsyncCreateBeacon* CreateBeacon(UObject* WorldContext, int32 OpenSlots, const FString& ConnectString, const FString& Metadata, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The beacon is live; BeaconId identifies it. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncCreateBeaconPin OnSuccess;

	/** Steam is unavailable or the beacon could not be created; BeaconId is 0. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncCreateBeaconPin OnFailure;

private:
	UFUNCTION()
	void HandleBeaconCreated(bool bSuccess, int64 InBeaconId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 InBeaconId);

	TWeakObjectPtr<UESteamPartiesSubsystem> PartiesSubsystem;
	int32 OpenSlots = 0;
	FString ConnectString;
	FString Metadata;
};

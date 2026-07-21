// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamPartiesSubsystem.generated.h"

/** Which piece of display data to read for a beacon location (mirrors ESteamPartyBeaconLocationData). */
UENUM(BlueprintType)
enum class EESteamPartyBeaconLocationData : uint8
{
	Invalid,
	/** Localized name of the location (e.g. the chat group name). */
	Name,
	/** URL to the small location icon. */
	IconURLSmall,
	/** URL to the medium location icon. */
	IconURLMedium,
	/** URL to the large location icon. */
	IconURLLarge
};

/**
 * A place a party beacon can be posted (mirrors SteamPartyBeaconLocation_t). Treat Type and
 * LocationId as opaque; only feed back values received from GetAvailableBeaconLocations.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamPartyBeaconLocation
{
	GENERATED_BODY()

	/** Location type (ESteamPartyBeaconLocationType); 1 = chat group. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Parties")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Parties")
	int64 LocationId = 0;
};

/** Fired when a JoinParty request completes. ConnectString carries the game-specific connect instructions on success. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamPartyJoined, bool, bSuccess, int64, BeaconId, const FString&, ConnectString);

/** Fired when a CreateBeacon request completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPartyBeaconCreated, bool, bSuccess, int64, BeaconId);

/** Fired when a ChangeNumOpenSlots request completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPartySlotsChanged, bool, bSuccess, int64, BeaconId);

/** Fired when a user responds to your beacon and Steam reserved a slot for them (they are in-flight to your game). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPartyReservationRequested, int64, BeaconId, FESteamId, Requester);

/** Fired when the list of active party beacons may have changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamPartyBeaconsUpdated);

/** Fired when the list of available beacon locations to host at may have changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamPartyBeaconLocationsUpdated);

/**
 * Wraps ISteamParties: party beacons that surface in Steam chat groups so friends
 * and group members can join a running game session with one click.
 *
 * Host flow: CreateBeacon -> OnPartyReservationRequested per joiner ->
 * OnReservationCompleted / CancelReservation -> DestroyBeacon.
 * Client flow: enumerate beacons -> JoinParty -> connect using the connect string.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamPartiesSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Number of party beacons currently visible to this user. */
	UFUNCTION(BlueprintPure, Category = "Steam|Parties")
	int32 GetNumActiveBeacons() const;

	/** Beacon id at the given index (0 when out of bounds). */
	UFUNCTION(BlueprintPure, Category = "Steam|Parties")
	int64 GetBeaconByIndex(int32 Index) const;

	// ---- Beacon locations (host) ----

	/** Number of beacon locations this user can currently host a party at (0 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Parties")
	int32 GetNumAvailableBeaconLocations() const;

	/**
	 * Lists the beacon locations this user can host a party at. Returns the number written to
	 * OutLocations. Pass an entry to CreateBeaconAt / GetBeaconLocationData.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	int32 GetAvailableBeaconLocations(TArray<FESteamPartyBeaconLocation>& OutLocations) const;

	/**
	 * Reads display data (name / icon URLs) for a beacon location returned by
	 * GetAvailableBeaconLocations. Returns false when the data is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool GetBeaconLocationData(FESteamPartyBeaconLocation Location, EESteamPartyBeaconLocationData DataType, FString& OutData) const;

	/**
	 * Details of a visible beacon. The beacon location is intentionally not exposed.
	 * Note: ISteamParties reports no open-slot count for foreign beacons; only owner and metadata are available.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool GetBeaconDetails(int64 BeaconId, FESteamId& OutOwner, FString& OutMetadata) const;

	/**
	 * Asks Steam to reserve a slot on the given beacon. Result arrives on OnPartyJoined.
	 * Returns true when the request was issued. Only one join request is tracked at a time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool JoinParty(int64 BeaconId);

	/**
	 * Creates a party beacon with the given number of open slots. Result arrives on OnPartyBeaconCreated.
	 * ConnectString is handed to joiners (e.g. "+connect ip:port"); Metadata is free-form beacon data.
	 * Location: the first location reported by GetAvailableBeaconLocations is used, falling back to a
	 * generic chat-group location when Steam reports none (the simplest valid approach; per-location
	 * selection is out of scope). Only one create request is tracked at a time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool CreateBeacon(int32 OpenSlots, const FString& ConnectString, const FString& Metadata);

	/** Call when a reserved user (see OnPartyReservationRequested) successfully joined your party. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	void OnReservationCompleted(int64 BeaconId, FESteamId User);

	/** Cancels a reservation (timeout, kick...). Steam opens a new reservation slot. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	void CancelReservation(int64 BeaconId, FESteamId User);

	/**
	 * Changes the number of open beacon slots. The result arrives on OnPartySlotsChanged.
	 * Returns true when the request was issued. Only one change request is tracked at a time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool ChangeNumOpenSlots(int64 BeaconId, int32 NewSlots);

	/** Turns your beacon off. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Parties")
	bool DestroyBeacon(int64 BeaconId);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartyJoined OnPartyJoined;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartyBeaconCreated OnPartyBeaconCreated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartySlotsChanged OnPartySlotsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartyReservationRequested OnPartyReservationRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartyBeaconsUpdated OnPartyBeaconsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Parties")
	FOnSteamPartyBeaconLocationsUpdated OnPartyBeaconLocationsUpdated;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamPartiesCallbacks;
	TSharedPtr<class FESteamPartiesCallbacks> Callbacks;
};

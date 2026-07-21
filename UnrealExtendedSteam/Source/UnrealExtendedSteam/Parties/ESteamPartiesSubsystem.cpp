// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Parties/ESteamPartiesSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamPartiesCallbacks
{
public:
	explicit FESteamPartiesCallbacks(UESteamPartiesSubsystem* InOwner)
		: Owner(InOwner)
		, ReservationNotification(this, &FESteamPartiesCallbacks::HandleReservationNotification)
		, ActiveBeaconsUpdated(this, &FESteamPartiesCallbacks::HandleActiveBeaconsUpdated)
		, AvailableBeaconLocationsUpdated(this, &FESteamPartiesCallbacks::HandleAvailableBeaconLocationsUpdated)
	{
	}

	void TrackJoinRequest(SteamAPICall_t Call)
	{
		JoinPartyResult.Set(Call, this, &FESteamPartiesCallbacks::HandleJoinPartyResult);
	}

	void TrackCreateRequest(SteamAPICall_t Call)
	{
		CreateBeaconResult.Set(Call, this, &FESteamPartiesCallbacks::HandleCreateBeaconResult);
	}

	void TrackChangeSlotsRequest(SteamAPICall_t Call, int64 BeaconId)
	{
		// ChangeNumOpenSlotsCallback_t carries only an EResult, so remember which beacon this was for.
		PendingSlotsBeaconId = BeaconId;
		ChangeNumOpenSlotsResult.Set(Call, this, &FESteamPartiesCallbacks::HandleChangeNumOpenSlotsResult);
	}

	// Single-slot CallResults: a second same-type request before the first completes would cancel it
	// (the first's callback would never fire). Call sites check these and reject the overlap.
	bool IsJoinBusy() const { return JoinPartyResult.IsActive(); }
	bool IsCreateBusy() const { return CreateBeaconResult.IsActive(); }
	bool IsChangeSlotsBusy() const { return ChangeNumOpenSlotsResult.IsActive(); }

private:
	void HandleReservationNotification(ReservationNotificationCallback_t* Data)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPartyReservationRequested.Broadcast(
				static_cast<int64>(Data->m_ulBeaconID),
				FESteamId(Data->m_steamIDJoiner.ConvertToUint64()));
		}
	}

	void HandleActiveBeaconsUpdated(ActiveBeaconsUpdated_t* Data)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPartyBeaconsUpdated.Broadcast();
		}
	}

	void HandleAvailableBeaconLocationsUpdated(AvailableBeaconLocationsUpdated_t* Data)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPartyBeaconLocationsUpdated.Broadcast();
		}
	}

	void HandleJoinPartyResult(JoinPartyCallback_t* Data, bool bIOFailure)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPartyJoined.Broadcast(
				bSuccess,
				static_cast<int64>(Data->m_ulBeaconID),
				bSuccess ? FString(UTF8_TO_TCHAR(Data->m_rgchConnectString)) : FString());
		}
	}

	void HandleCreateBeaconResult(CreateBeaconCallback_t* Data, bool bIOFailure)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPartyBeaconCreated.Broadcast(bSuccess, static_cast<int64>(Data->m_ulBeaconID));
		}
	}

	void HandleChangeNumOpenSlotsResult(ChangeNumOpenSlotsCallback_t* Data, bool bIOFailure)
	{
		if (UESteamPartiesSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPartySlotsChanged.Broadcast(bSuccess, PendingSlotsBeaconId);
		}
	}

	TWeakObjectPtr<UESteamPartiesSubsystem> Owner;
	int64 PendingSlotsBeaconId = 0;
	CCallback<FESteamPartiesCallbacks, ReservationNotificationCallback_t> ReservationNotification;
	CCallback<FESteamPartiesCallbacks, ActiveBeaconsUpdated_t> ActiveBeaconsUpdated;
	CCallback<FESteamPartiesCallbacks, AvailableBeaconLocationsUpdated_t> AvailableBeaconLocationsUpdated;
	CCallResult<FESteamPartiesCallbacks, JoinPartyCallback_t> JoinPartyResult;
	CCallResult<FESteamPartiesCallbacks, CreateBeaconCallback_t> CreateBeaconResult;
	CCallResult<FESteamPartiesCallbacks, ChangeNumOpenSlotsCallback_t> ChangeNumOpenSlotsResult;
};
#else
class FESteamPartiesCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamPartiesSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamPartiesSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamPartiesCallbacks>(this);
	}
#endif
}

void UESteamPartiesSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

int32 UESteamPartiesSubsystem::GetNumActiveBeacons() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		return static_cast<int32>(SteamParties()->GetNumActiveBeacons());
	}
#endif
	return 0;
}

int64 UESteamPartiesSubsystem::GetBeaconByIndex(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties() && Index >= 0)
	{
		return static_cast<int64>(SteamParties()->GetBeaconByIndex(static_cast<uint32>(Index)));
	}
#endif
	return 0;
}

int32 UESteamPartiesSubsystem::GetNumAvailableBeaconLocations() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		uint32 NumLocations = 0;
		if (SteamParties()->GetNumAvailableBeaconLocations(&NumLocations))
		{
			return static_cast<int32>(NumLocations);
		}
	}
#endif
	return 0;
}

int32 UESteamPartiesSubsystem::GetAvailableBeaconLocations(TArray<FESteamPartyBeaconLocation>& OutLocations) const
{
	OutLocations.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		uint32 NumLocations = 0;
		if (!SteamParties()->GetNumAvailableBeaconLocations(&NumLocations) || NumLocations == 0)
		{
			return 0;
		}

		TArray<SteamPartyBeaconLocation_t> Locations;
		Locations.SetNumUninitialized(static_cast<int32>(NumLocations));
		if (!SteamParties()->GetAvailableBeaconLocations(Locations.GetData(), NumLocations))
		{
			return 0;
		}

		OutLocations.Reserve(static_cast<int32>(NumLocations));
		for (const SteamPartyBeaconLocation_t& Location : Locations)
		{
			FESteamPartyBeaconLocation Entry;
			Entry.Type = static_cast<int32>(Location.m_eType);
			Entry.LocationId = static_cast<int64>(Location.m_ulLocationID);
			OutLocations.Add(Entry);
		}
		return OutLocations.Num();
	}
#endif
	return 0;
}

bool UESteamPartiesSubsystem::GetBeaconLocationData(FESteamPartyBeaconLocation Location, EESteamPartyBeaconLocationData DataType, FString& OutData) const
{
	OutData.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamParties())
	{
		return false;
	}

	ESteamPartyBeaconLocationData eData = k_ESteamPartyBeaconLocationDataInvalid;
	switch (DataType)
	{
	case EESteamPartyBeaconLocationData::Name:           eData = k_ESteamPartyBeaconLocationDataName;           break;
	case EESteamPartyBeaconLocationData::IconURLSmall:   eData = k_ESteamPartyBeaconLocationDataIconURLSmall;   break;
	case EESteamPartyBeaconLocationData::IconURLMedium:  eData = k_ESteamPartyBeaconLocationDataIconURLMedium;  break;
	case EESteamPartyBeaconLocationData::IconURLLarge:   eData = k_ESteamPartyBeaconLocationDataIconURLLarge;   break;
	default:                                             eData = k_ESteamPartyBeaconLocationDataInvalid;        break;
	}

	SteamPartyBeaconLocation_t SteamLocation;
	SteamLocation.m_eType = static_cast<ESteamPartyBeaconLocationType>(Location.Type);
	SteamLocation.m_ulLocationID = static_cast<uint64>(Location.LocationId);

	char Buffer[2048] = { 0 };
	if (!SteamParties()->GetBeaconLocationData(SteamLocation, eData, Buffer, static_cast<int>(sizeof(Buffer))))
	{
		return false;
	}

	OutData = UTF8_TO_TCHAR(Buffer);
	return true;
#else
	return false;
#endif
}

bool UESteamPartiesSubsystem::GetBeaconDetails(int64 BeaconId, FESteamId& OutOwner, FString& OutMetadata) const
{
	OutOwner = FESteamId();
	OutMetadata.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamParties())
	{
		return false;
	}

	CSteamID Owner;
	SteamPartyBeaconLocation_t Location;
	// ISteamParties has no metadata-length query, so a generous fixed buffer stands in for a two-call idiom.
	char Metadata[8192] = { 0 };
	if (!SteamParties()->GetBeaconDetails(static_cast<PartyBeaconID_t>(BeaconId), &Owner, &Location, Metadata, static_cast<int>(sizeof(Metadata))))
	{
		return false;
	}

	OutOwner = FESteamId(Owner.ConvertToUint64());
	OutMetadata = UTF8_TO_TCHAR(Metadata);
	return true;
#else
	return false;
#endif
}

bool UESteamPartiesSubsystem::JoinParty(int64 BeaconId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamParties() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("JoinParty"));
		return false;
	}

	if (Callbacks->IsJoinBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinParty: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamParties()->JoinParty(static_cast<PartyBeaconID_t>(BeaconId));
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinParty failed to issue the request for beacon %lld"), BeaconId);
		return false;
	}
	Callbacks->TrackJoinRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("JoinParty"));
	return false;
#endif
}

bool UESteamPartiesSubsystem::CreateBeacon(int32 OpenSlots, const FString& ConnectString, const FString& Metadata)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamParties() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("CreateBeacon"));
		return false;
	}

	// Prefer the first beacon location Steam reports as available; when the list is empty
	// (or unavailable), fall back to a generic chat-group location, the only public location type.
	SteamPartyBeaconLocation_t Location;
	Location.m_eType = k_ESteamPartyBeaconLocationType_ChatGroup;
	Location.m_ulLocationID = 0;

	uint32 NumLocations = 0;
	if (SteamParties()->GetNumAvailableBeaconLocations(&NumLocations) && NumLocations > 0)
	{
		TArray<SteamPartyBeaconLocation_t> Locations;
		Locations.SetNumUninitialized(static_cast<int32>(NumLocations));
		if (SteamParties()->GetAvailableBeaconLocations(Locations.GetData(), NumLocations))
		{
			Location = Locations[0];
		}
	}

	if (Callbacks->IsCreateBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("CreateBeacon: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamParties()->CreateBeacon(
		static_cast<uint32>(FMath::Max(OpenSlots, 0)),
		&Location,
		TCHAR_TO_UTF8(*ConnectString),
		TCHAR_TO_UTF8(*Metadata));
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("CreateBeacon failed to issue the request"));
		return false;
	}
	Callbacks->TrackCreateRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("CreateBeacon"));
	return false;
#endif
}

void UESteamPartiesSubsystem::OnReservationCompleted(int64 BeaconId, FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		SteamParties()->OnReservationCompleted(static_cast<PartyBeaconID_t>(BeaconId), CSteamID(User.Value));
	}
#endif
}

void UESteamPartiesSubsystem::CancelReservation(int64 BeaconId, FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		SteamParties()->CancelReservation(static_cast<PartyBeaconID_t>(BeaconId), CSteamID(User.Value));
	}
#endif
}

bool UESteamPartiesSubsystem::ChangeNumOpenSlots(int64 BeaconId, int32 NewSlots)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamParties() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("ChangeNumOpenSlots"));
		return false;
	}

	if (Callbacks->IsChangeSlotsBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ChangeNumOpenSlots: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamParties()->ChangeNumOpenSlots(
		static_cast<PartyBeaconID_t>(BeaconId),
		static_cast<uint32>(FMath::Max(NewSlots, 0)));
	if (Call == k_uAPICallInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ChangeNumOpenSlots failed to issue the request for beacon %lld"), BeaconId);
		return false;
	}
	Callbacks->TrackChangeSlotsRequest(Call, BeaconId);
	return true;
#else
	LogSteamUnavailable(TEXT("ChangeNumOpenSlots"));
	return false;
#endif
}

bool UESteamPartiesSubsystem::DestroyBeacon(int64 BeaconId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamParties())
	{
		return SteamParties()->DestroyBeacon(static_cast<PartyBeaconID_t>(BeaconId));
	}
#endif
	return false;
}

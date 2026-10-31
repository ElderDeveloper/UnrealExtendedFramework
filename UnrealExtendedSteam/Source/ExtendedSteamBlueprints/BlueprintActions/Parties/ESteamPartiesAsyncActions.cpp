// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Parties/ESteamPartiesAsyncActions.h"

// ---- USteamAsyncJoinParty ----

USteamAsyncJoinParty* USteamAsyncJoinParty::JoinParty(UObject* WorldContext, int64 BeaconId, float Timeout)
{
	USteamAsyncJoinParty* Action = NewObject<USteamAsyncJoinParty>();
	Action->BeaconId = BeaconId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncJoinParty::ExecuteAction()
{
	UESteamPartiesSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamPartiesSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	PartiesSubsystem = Subsystem;
	Subsystem->OnPartyJoined.AddDynamic(this, &USteamAsyncJoinParty::HandlePartyJoined);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->JoinParty(BeaconId); });
	if (!bStarted)
	{
		Complete(false, BeaconId, FString());
	}
}

void USteamAsyncJoinParty::HandlePartyJoined(bool bSuccess, int64 InBeaconId, const FString& ConnectString)
{
	// Only one join is tracked at a time by the subsystem; still guard against unrelated beacons.
	if (!IsResultForThisCall() || (bSuccess && InBeaconId != BeaconId))
	{
		return;
	}
	Complete(bSuccess, InBeaconId, ConnectString);
}

void USteamAsyncJoinParty::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(BeaconId, FString());
}

void USteamAsyncJoinParty::CancelWork()
{
	if (UESteamPartiesSubsystem* Subsystem = PartiesSubsystem.Get())
	{
		Subsystem->OnPartyJoined.RemoveDynamic(this, &USteamAsyncJoinParty::HandlePartyJoined);
	}
}

void USteamAsyncJoinParty::Complete(bool bSuccess, int64 InBeaconId, const FString& ConnectString)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(InBeaconId, ConnectString);
	}
	else
	{
		OnFailure.Broadcast(InBeaconId, ConnectString);
	}
}

// ---- USteamAsyncCreateBeacon ----

USteamAsyncCreateBeacon* USteamAsyncCreateBeacon::CreateBeacon(UObject* WorldContext, int32 OpenSlots, const FString& ConnectString, const FString& Metadata, float Timeout)
{
	USteamAsyncCreateBeacon* Action = NewObject<USteamAsyncCreateBeacon>();
	Action->OpenSlots = OpenSlots;
	Action->ConnectString = ConnectString;
	Action->Metadata = Metadata;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncCreateBeacon::ExecuteAction()
{
	UESteamPartiesSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamPartiesSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	PartiesSubsystem = Subsystem;
	Subsystem->OnPartyBeaconCreated.AddDynamic(this, &USteamAsyncCreateBeacon::HandleBeaconCreated);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->CreateBeacon(OpenSlots, ConnectString, Metadata); });
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncCreateBeacon::HandleBeaconCreated(bool bSuccess, int64 InBeaconId)
{
	// The shared delegate carries no request id; the subsystem tags the result with the call
	// context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, InBeaconId);
}

void USteamAsyncCreateBeacon::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0);
}

void USteamAsyncCreateBeacon::CancelWork()
{
	if (UESteamPartiesSubsystem* Subsystem = PartiesSubsystem.Get())
	{
		Subsystem->OnPartyBeaconCreated.RemoveDynamic(this, &USteamAsyncCreateBeacon::HandleBeaconCreated);
	}
}

void USteamAsyncCreateBeacon::Complete(bool bSuccess, int64 InBeaconId)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(InBeaconId);
	}
	else
	{
		OnFailure.Broadcast(InBeaconId);
	}
}

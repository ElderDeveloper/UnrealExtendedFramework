// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Parties/ESteamPartiesAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamPartiesSubsystem* GetPartiesSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamPartiesSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncJoinParty ----

USteamAsyncJoinParty* USteamAsyncJoinParty::JoinParty(UObject* WorldContext, int64 BeaconId, float Timeout)
{
	USteamAsyncJoinParty* Action = NewObject<USteamAsyncJoinParty>();
	Action->WorldContextObject = WorldContext;
	Action->BeaconId = BeaconId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncJoinParty::Activate()
{
	UESteamPartiesSubsystem* Subsystem = GetPartiesSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, BeaconId, FString());
		return;
	}

	PartiesSubsystem = Subsystem;
	Subsystem->OnPartyJoined.AddDynamic(this, &USteamAsyncJoinParty::HandlePartyJoined);

	if (!Subsystem->JoinParty(BeaconId))
	{
		Complete(false, BeaconId, FString());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncJoinParty::HandlePartyJoined(bool bSuccess, int64 InBeaconId, const FString& ConnectString)
{
	// Only one join is tracked at a time by the subsystem; still guard against unrelated beacons.
	if (bSuccess && InBeaconId != BeaconId)
	{
		return;
	}
	Complete(bSuccess, InBeaconId, ConnectString);
}

void USteamAsyncJoinParty::OnTimeoutFailure()
{
	Complete(false, BeaconId, FString());
}

void USteamAsyncJoinParty::Complete(bool bSuccess, int64 InBeaconId, const FString& ConnectString)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamPartiesSubsystem* Subsystem = PartiesSubsystem.Get())
	{
		Subsystem->OnPartyJoined.RemoveDynamic(this, &USteamAsyncJoinParty::HandlePartyJoined);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(InBeaconId, ConnectString);
	}
	else
	{
		OnFailure.Broadcast(InBeaconId, ConnectString);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncCreateBeacon ----

USteamAsyncCreateBeacon* USteamAsyncCreateBeacon::CreateBeacon(UObject* WorldContext, int32 OpenSlots, const FString& ConnectString, const FString& Metadata, float Timeout)
{
	USteamAsyncCreateBeacon* Action = NewObject<USteamAsyncCreateBeacon>();
	Action->WorldContextObject = WorldContext;
	Action->OpenSlots = OpenSlots;
	Action->ConnectString = ConnectString;
	Action->Metadata = Metadata;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncCreateBeacon::Activate()
{
	UESteamPartiesSubsystem* Subsystem = GetPartiesSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, 0);
		return;
	}

	PartiesSubsystem = Subsystem;
	Subsystem->OnPartyBeaconCreated.AddDynamic(this, &USteamAsyncCreateBeacon::HandleBeaconCreated);

	if (!Subsystem->CreateBeacon(OpenSlots, ConnectString, Metadata))
	{
		Complete(false, 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncCreateBeacon::HandleBeaconCreated(bool bSuccess, int64 InBeaconId)
{
	// NOTE: shared delegate carries no request id; relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, InBeaconId);
}

void USteamAsyncCreateBeacon::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncCreateBeacon::Complete(bool bSuccess, int64 InBeaconId)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamPartiesSubsystem* Subsystem = PartiesSubsystem.Get())
	{
		Subsystem->OnPartyBeaconCreated.RemoveDynamic(this, &USteamAsyncCreateBeacon::HandleBeaconCreated);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(InBeaconId);
	}
	else
	{
		OnFailure.Broadcast(InBeaconId);
	}

	SetReadyToDestroy();
}

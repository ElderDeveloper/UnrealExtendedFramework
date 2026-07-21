// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/MatchmakingServers/ESteamMatchmakingServersAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamMatchmakingServersSubsystem* GetServersSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamMatchmakingServersSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncPingServer ----

USteamAsyncPingServer* USteamAsyncPingServer::PingServer(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout)
{
	USteamAsyncPingServer* Action = NewObject<USteamAsyncPingServer>();
	Action->WorldContextObject = WorldContext;
	Action->Ip = Ip;
	Action->Port = Port;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncPingServer::Activate()
{
	UESteamMatchmakingServersSubsystem* Subsystem = GetServersSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FESteamServerInfo());
		return;
	}

	ServersSubsystem = Subsystem;
	Subsystem->OnServerPingResponded.AddDynamic(this, &USteamAsyncPingServer::HandlePingResponded);

	if (!Subsystem->PingServer(Ip, Port))
	{
		Complete(false, FESteamServerInfo());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncPingServer::HandlePingResponded(bool bSuccess, const FESteamServerInfo& Server)
{
	// NOTE: FOnSteamServerPingResponded carries no request id; the subsystem allows one ping in
	// flight, so this relies on Timeout + that one-in-flight limit to correlate.
	Complete(bSuccess, Server);
}

void USteamAsyncPingServer::OnTimeoutFailure()
{
	Complete(false, FESteamServerInfo());
}

void USteamAsyncPingServer::Complete(bool bSuccess, const FESteamServerInfo& Server)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamMatchmakingServersSubsystem* Subsystem = ServersSubsystem.Get())
	{
		Subsystem->OnServerPingResponded.RemoveDynamic(this, &USteamAsyncPingServer::HandlePingResponded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Server);
	}
	else
	{
		OnFailure.Broadcast(Server);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncRequestServerRules ----

USteamAsyncRequestServerRules* USteamAsyncRequestServerRules::RequestServerRules(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout)
{
	USteamAsyncRequestServerRules* Action = NewObject<USteamAsyncRequestServerRules>();
	Action->WorldContextObject = WorldContext;
	Action->Ip = Ip;
	Action->Port = Port;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestServerRules::Activate()
{
	UESteamMatchmakingServersSubsystem* Subsystem = GetServersSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<FString>(), TArray<FString>());
		return;
	}

	ServersSubsystem = Subsystem;
	Subsystem->OnServerRulesReceived.AddDynamic(this, &USteamAsyncRequestServerRules::HandleRulesReceived);

	if (!Subsystem->ServerRules(Ip, Port))
	{
		Complete(false, TArray<FString>(), TArray<FString>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestServerRules::HandleRulesReceived(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values)
{
	// NOTE: FOnSteamServerRulesReceived carries no request id; the subsystem allows one rules query
	// in flight, so this relies on Timeout + that one-in-flight limit to correlate.
	Complete(bSuccess, Keys, Values);
}

void USteamAsyncRequestServerRules::OnTimeoutFailure()
{
	Complete(false, TArray<FString>(), TArray<FString>());
}

void USteamAsyncRequestServerRules::Complete(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamMatchmakingServersSubsystem* Subsystem = ServersSubsystem.Get())
	{
		Subsystem->OnServerRulesReceived.RemoveDynamic(this, &USteamAsyncRequestServerRules::HandleRulesReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Keys, Values);
	}
	else
	{
		OnFailure.Broadcast(Keys, Values);
	}

	SetReadyToDestroy();
}

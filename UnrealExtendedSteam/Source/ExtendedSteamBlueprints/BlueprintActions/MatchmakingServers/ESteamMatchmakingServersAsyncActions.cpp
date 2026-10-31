// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/MatchmakingServers/ESteamMatchmakingServersAsyncActions.h"

// ---- USteamAsyncPingServer ----

USteamAsyncPingServer* USteamAsyncPingServer::PingServer(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout)
{
	USteamAsyncPingServer* Action = NewObject<USteamAsyncPingServer>();
	Action->Ip = Ip;
	Action->Port = Port;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncPingServer::ExecuteAction()
{
	UESteamMatchmakingServersSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamMatchmakingServersSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ServersSubsystem = Subsystem;
	Subsystem->OnServerPingResponded.AddDynamic(this, &USteamAsyncPingServer::HandlePingResponded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->PingServer(Ip, Port); });
	if (!bStarted)
	{
		Complete(false, FESteamServerInfo());
	}
}

void USteamAsyncPingServer::HandlePingResponded(bool bSuccess, const FESteamServerInfo& Server)
{
	// FOnSteamServerPingResponded carries no request id. The subsystem keeps one ping in flight and
	// tags its response with the call context of the request, so a ping another node started (which
	// replaces this one's query) is not mistaken for this node's result.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Server);
}

void USteamAsyncPingServer::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FESteamServerInfo());
}

void USteamAsyncPingServer::CancelWork()
{
	if (UESteamMatchmakingServersSubsystem* Subsystem = ServersSubsystem.Get())
	{
		Subsystem->OnServerPingResponded.RemoveDynamic(this, &USteamAsyncPingServer::HandlePingResponded);
	}
}

void USteamAsyncPingServer::Complete(bool bSuccess, const FESteamServerInfo& Server)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Server);
	}
	else
	{
		OnFailure.Broadcast(Server);
	}
}

// ---- USteamAsyncRequestServerRules ----

USteamAsyncRequestServerRules* USteamAsyncRequestServerRules::RequestServerRules(UObject* WorldContext, const FString& Ip, int32 Port, float Timeout)
{
	USteamAsyncRequestServerRules* Action = NewObject<USteamAsyncRequestServerRules>();
	Action->Ip = Ip;
	Action->Port = Port;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestServerRules::ExecuteAction()
{
	UESteamMatchmakingServersSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamMatchmakingServersSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ServersSubsystem = Subsystem;
	Subsystem->OnServerRulesReceived.AddDynamic(this, &USteamAsyncRequestServerRules::HandleRulesReceived);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->ServerRules(Ip, Port); });
	if (!bStarted)
	{
		Complete(false, TArray<FString>(), TArray<FString>());
	}
}

void USteamAsyncRequestServerRules::HandleRulesReceived(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values)
{
	// FOnSteamServerRulesReceived carries no request id. The subsystem keeps one rules query in
	// flight and tags its response with the call context of the request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Keys, Values);
}

void USteamAsyncRequestServerRules::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FString>(), TArray<FString>());
}

void USteamAsyncRequestServerRules::CancelWork()
{
	if (UESteamMatchmakingServersSubsystem* Subsystem = ServersSubsystem.Get())
	{
		Subsystem->OnServerRulesReceived.RemoveDynamic(this, &USteamAsyncRequestServerRules::HandleRulesReceived);
	}
}

void USteamAsyncRequestServerRules::Complete(bool bSuccess, const TArray<FString>& Keys, const TArray<FString>& Values)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Keys, Values);
	}
	else
	{
		OnFailure.Broadcast(Keys, Values);
	}
}

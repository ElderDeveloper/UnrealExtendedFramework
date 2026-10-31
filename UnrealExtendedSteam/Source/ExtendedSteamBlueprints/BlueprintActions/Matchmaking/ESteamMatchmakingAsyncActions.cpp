// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Matchmaking/ESteamMatchmakingAsyncActions.h"

// ---- USteamAsyncCreateLobby ----

USteamAsyncCreateLobby* USteamAsyncCreateLobby::CreateLobby(UObject* WorldContext, EESteamLobbyType LobbyType, int32 MaxMembers, float Timeout)
{
	USteamAsyncCreateLobby* Action = NewObject<USteamAsyncCreateLobby>();
	Action->LobbyType = LobbyType;
	Action->MaxMembers = MaxMembers;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncCreateLobby::ExecuteAction()
{
	UESteamMatchmakingSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamMatchmakingSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyCreated.AddDynamic(this, &USteamAsyncCreateLobby::HandleLobbyCreated);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->CreateLobby(LobbyType, MaxMembers); });
	if (!bStarted)
	{
		Complete(false, FESteamId());
	}
}

void USteamAsyncCreateLobby::HandleLobbyCreated(bool bSuccess, FESteamId LobbyId)
{
	// FOnSteamLobbyCreated's LobbyId is the result (unknown before the call), so it cannot correlate
	// concurrent creates; the subsystem tags each result with the call context of its request instead.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, LobbyId);
}

void USteamAsyncCreateLobby::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FESteamId());
}

void USteamAsyncCreateLobby::CancelWork()
{
	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyCreated.RemoveDynamic(this, &USteamAsyncCreateLobby::HandleLobbyCreated);
	}
}

void USteamAsyncCreateLobby::Complete(bool bSuccess, FESteamId LobbyId)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(LobbyId);
	}
	else
	{
		OnFailure.Broadcast(LobbyId);
	}
}

// ---- USteamAsyncJoinLobby ----

USteamAsyncJoinLobby* USteamAsyncJoinLobby::JoinLobby(UObject* WorldContext, FESteamId LobbyId, float Timeout)
{
	USteamAsyncJoinLobby* Action = NewObject<USteamAsyncJoinLobby>();
	Action->LobbyId = LobbyId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncJoinLobby::ExecuteAction()
{
	UESteamMatchmakingSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamMatchmakingSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyEntered.AddDynamic(this, &USteamAsyncJoinLobby::HandleLobbyEntered);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->JoinLobby(LobbyId); });
	if (!bStarted)
	{
		Complete(false, LobbyId);
	}
}

void USteamAsyncJoinLobby::HandleLobbyEntered(bool bSuccess, FESteamId EnteredLobbyId)
{
	// Defensive: only react to the lobby this node requested, in case another
	// join was issued on the shared subsystem delegate in the meantime.
	if (!IsResultForThisCall() || EnteredLobbyId != LobbyId)
	{
		return;
	}
	Complete(bSuccess, EnteredLobbyId);
}

void USteamAsyncJoinLobby::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(LobbyId);
}

void USteamAsyncJoinLobby::CancelWork()
{
	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyEntered.RemoveDynamic(this, &USteamAsyncJoinLobby::HandleLobbyEntered);
	}
}

void USteamAsyncJoinLobby::Complete(bool bSuccess, FESteamId EnteredLobbyId)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(EnteredLobbyId);
	}
	else
	{
		OnFailure.Broadcast(EnteredLobbyId);
	}
}

// ---- USteamAsyncRequestLobbyList ----

USteamAsyncRequestLobbyList* USteamAsyncRequestLobbyList::RequestLobbyList(UObject* WorldContext, EESteamLobbyDistanceFilter DistanceFilter, int32 MaxResults, float Timeout)
{
	USteamAsyncRequestLobbyList* Action = NewObject<USteamAsyncRequestLobbyList>();
	Action->DistanceFilter = DistanceFilter;
	Action->MaxResults = MaxResults;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestLobbyList::ExecuteAction()
{
	UESteamMatchmakingSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamMatchmakingSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyListReceived.AddDynamic(this, &USteamAsyncRequestLobbyList::HandleLobbyListReceived);

	Subsystem->AddRequestLobbyListDistanceFilter(DistanceFilter);
	if (MaxResults > 0)
	{
		Subsystem->AddRequestLobbyListResultCountFilter(MaxResults);
	}

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->RequestLobbyList(); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamId>());
	}
}

void USteamAsyncRequestLobbyList::HandleLobbyListReceived(bool bSuccess, const TArray<FESteamId>& Lobbies)
{
	// FOnSteamLobbyListReceived carries no request id; the subsystem tags each result with the call
	// context of its request, so concurrent searches are told apart here.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Lobbies);
}

void USteamAsyncRequestLobbyList::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamId>());
}

void USteamAsyncRequestLobbyList::CancelWork()
{
	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyListReceived.RemoveDynamic(this, &USteamAsyncRequestLobbyList::HandleLobbyListReceived);
	}
}

void USteamAsyncRequestLobbyList::Complete(bool bSuccess, const TArray<FESteamId>& Lobbies)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Lobbies);
	}
	else
	{
		OnFailure.Broadcast(Lobbies);
	}
}

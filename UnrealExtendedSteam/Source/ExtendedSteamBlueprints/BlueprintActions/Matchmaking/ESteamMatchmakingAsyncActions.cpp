// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Matchmaking/ESteamMatchmakingAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamMatchmakingSubsystem* GetMatchmakingSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamMatchmakingSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncCreateLobby ----

USteamAsyncCreateLobby* USteamAsyncCreateLobby::CreateLobby(UObject* WorldContext, EESteamLobbyType LobbyType, int32 MaxMembers, float Timeout)
{
	USteamAsyncCreateLobby* Action = NewObject<USteamAsyncCreateLobby>();
	Action->WorldContextObject = WorldContext;
	Action->LobbyType = LobbyType;
	Action->MaxMembers = MaxMembers;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncCreateLobby::Activate()
{
	UESteamMatchmakingSubsystem* Subsystem = GetMatchmakingSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FESteamId());
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyCreated.AddDynamic(this, &USteamAsyncCreateLobby::HandleLobbyCreated);

	if (!Subsystem->CreateLobby(LobbyType, MaxMembers))
	{
		Complete(false, FESteamId());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncCreateLobby::HandleLobbyCreated(bool bSuccess, FESteamId LobbyId)
{
	// NOTE: FOnSteamLobbyCreated's LobbyId is the result (unknown before the call), so it cannot
	// correlate concurrent creates. Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, LobbyId);
}

void USteamAsyncCreateLobby::OnTimeoutFailure()
{
	Complete(false, FESteamId());
}

void USteamAsyncCreateLobby::Complete(bool bSuccess, FESteamId LobbyId)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyCreated.RemoveDynamic(this, &USteamAsyncCreateLobby::HandleLobbyCreated);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(LobbyId);
	}
	else
	{
		OnFailure.Broadcast(LobbyId);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncJoinLobby ----

USteamAsyncJoinLobby* USteamAsyncJoinLobby::JoinLobby(UObject* WorldContext, FESteamId LobbyId, float Timeout)
{
	USteamAsyncJoinLobby* Action = NewObject<USteamAsyncJoinLobby>();
	Action->WorldContextObject = WorldContext;
	Action->LobbyId = LobbyId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncJoinLobby::Activate()
{
	UESteamMatchmakingSubsystem* Subsystem = GetMatchmakingSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, LobbyId);
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyEntered.AddDynamic(this, &USteamAsyncJoinLobby::HandleLobbyEntered);

	if (!Subsystem->JoinLobby(LobbyId))
	{
		Complete(false, LobbyId);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncJoinLobby::HandleLobbyEntered(bool bSuccess, FESteamId EnteredLobbyId)
{
	// Defensive: only react to the lobby this node requested, in case another
	// join was issued on the shared subsystem delegate in the meantime.
	if (EnteredLobbyId != LobbyId)
	{
		return;
	}
	Complete(bSuccess, EnteredLobbyId);
}

void USteamAsyncJoinLobby::OnTimeoutFailure()
{
	Complete(false, LobbyId);
}

void USteamAsyncJoinLobby::Complete(bool bSuccess, FESteamId EnteredLobbyId)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyEntered.RemoveDynamic(this, &USteamAsyncJoinLobby::HandleLobbyEntered);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(EnteredLobbyId);
	}
	else
	{
		OnFailure.Broadcast(EnteredLobbyId);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncRequestLobbyList ----

USteamAsyncRequestLobbyList* USteamAsyncRequestLobbyList::RequestLobbyList(UObject* WorldContext, EESteamLobbyDistanceFilter DistanceFilter, int32 MaxResults, float Timeout)
{
	USteamAsyncRequestLobbyList* Action = NewObject<USteamAsyncRequestLobbyList>();
	Action->WorldContextObject = WorldContext;
	Action->DistanceFilter = DistanceFilter;
	Action->MaxResults = MaxResults;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestLobbyList::Activate()
{
	UESteamMatchmakingSubsystem* Subsystem = GetMatchmakingSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<FESteamId>());
		return;
	}

	MatchmakingSubsystem = Subsystem;
	Subsystem->OnLobbyListReceived.AddDynamic(this, &USteamAsyncRequestLobbyList::HandleLobbyListReceived);

	Subsystem->AddRequestLobbyListDistanceFilter(DistanceFilter);
	if (MaxResults > 0)
	{
		Subsystem->AddRequestLobbyListResultCountFilter(MaxResults);
	}

	if (!Subsystem->RequestLobbyList())
	{
		Complete(false, TArray<FESteamId>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestLobbyList::HandleLobbyListReceived(bool bSuccess, const TArray<FESteamId>& Lobbies)
{
	// NOTE: FOnSteamLobbyListReceived carries no request id; cannot discriminate. Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Lobbies);
}

void USteamAsyncRequestLobbyList::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamId>());
}

void USteamAsyncRequestLobbyList::Complete(bool bSuccess, const TArray<FESteamId>& Lobbies)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamMatchmakingSubsystem* Subsystem = MatchmakingSubsystem.Get())
	{
		Subsystem->OnLobbyListReceived.RemoveDynamic(this, &USteamAsyncRequestLobbyList::HandleLobbyListReceived);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Lobbies);
	}
	else
	{
		OnFailure.Broadcast(Lobbies);
	}

	SetReadyToDestroy();
}

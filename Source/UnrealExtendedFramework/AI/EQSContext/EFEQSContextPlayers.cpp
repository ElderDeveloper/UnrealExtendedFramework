// Fill out your copyright notice in the Description page of Project Settings.


#include "EFEQSContextPlayers.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"


void UEFEQSContextPlayers::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	Super::ProvideContext(QueryInstance, ContextData);

	UWorld* World = QueryInstance.World;
	if (!World)
	{
		return;
	}
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return;
	}

	TArray<AActor*> Players;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (PlayerState)
		{
			if (APawn* PlayerPawn = PlayerState->GetPawn())
			{
				Players.Add(PlayerPawn);
			}
		}
	}
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, Players);
	
}


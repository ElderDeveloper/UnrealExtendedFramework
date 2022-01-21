// Fill out your copyright notice in the Description page of Project Settings.


#include "EFEQSContextPlayer.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "Kismet/GameplayStatics.h"


void UEFEQSContextPlayer::ProvideContext(FEnvQueryInstance& QueryInstance,FEnvQueryContextData& ContextData) const
{
	if (const auto actor = UGameplayStatics::GetPlayerPawn(QueryInstance.Owner.Get()->GetWorld(),0))
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, actor);
	}
}

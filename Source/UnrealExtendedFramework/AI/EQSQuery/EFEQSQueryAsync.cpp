﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFEQSQueryAsync.h"

#include "EnvironmentQuery/EnvQueryManager.h"

UEFEQSQueryAsync* UEFEQSQueryAsync::RunEQSQueryAsync(UObject* WorldContextObject,UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode,TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass)
{
	UEFEQSQueryAsync* Node = NewObject<UEFEQSQueryAsync>();
	if (Node)
	{
		Node->WorldContextObject = WorldContextObject;
		Node->QueryTemplate = QueryTemplate;
		Node->Querier = Querier;
		Node->RunMode = RunMode;
		Node->WrapperClass = WrapperClass;
	}
	return Node;
}

void UEFEQSQueryAsync::OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus)
{
	QueryFinished.Broadcast(QueryInstance,QueryStatus);
	DestroyAsync();
}


void UEFEQSQueryAsync::DestroyAsync()
{
	SetReadyToDestroy();
	ConditionalBeginDestroy();
}

void UEFEQSQueryAsync::Activate()
{
	Super::Activate();

	if (WorldContextObject , QueryTemplate , Querier )
	{
		if( const auto Query = UEnvQueryManager::RunEQSQuery(WorldContextObject , QueryTemplate , Querier , RunMode , WrapperClass))
		{
			Query->GetOnQueryFinishedEvent().AddDynamic(this, &UEFEQSQueryAsync::OnQueryFinished);
			return;
		}
	}
	DestroyAsync();
	
}

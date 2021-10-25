// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedRunEQSQueryAsync.h"

#include "EnvironmentQuery/EnvQueryManager.h"

UUEExtendedRunEQSQueryAsync* UUEExtendedRunEQSQueryAsync::RunEQSQueryAsync(UObject* WorldContextObject,UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode,TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass)
{
	UUEExtendedRunEQSQueryAsync* Node = NewObject<UUEExtendedRunEQSQueryAsync>();
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

void UUEExtendedRunEQSQueryAsync::OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus)
{
	QueryFinished.Broadcast(QueryInstance,QueryStatus);
	DestroyAsync();
}


void UUEExtendedRunEQSQueryAsync::DestroyAsync()
{
	SetReadyToDestroy();
	MarkPendingKill();
}

void UUEExtendedRunEQSQueryAsync::Activate()
{
	Super::Activate();

	if (WorldContextObject , QueryTemplate , Querier )
	{
		if( const auto Query = UEnvQueryManager::RunEQSQuery(WorldContextObject , QueryTemplate , Querier , RunMode , WrapperClass))
		{
			Query->GetOnQueryFinishedEvent().AddDynamic(this, &UUEExtendedRunEQSQueryAsync::OnQueryFinished);
			return;
		}
	}
	DestroyAsync();
	
}

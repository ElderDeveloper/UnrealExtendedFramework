// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/Object.h"
#include "UEExtendedRunEQSQueryAsync.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQueryCompleate , UEnvQueryInstanceBlueprintWrapper*, QueryInstance, EEnvQueryStatus::Type, QueryStatus);


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedRunEQSQueryAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Run EQS Query Async", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true" , AdvancedDisplay = "WrapperClass"), Category = "AsyncNode")
	static UUEExtendedRunEQSQueryAsync* RunEQSQueryAsync(UObject* WorldContextObject, UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode, TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass);

	UFUNCTION()
	void OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);

	UPROPERTY(BlueprintAssignable)
	FQueryCompleate QueryFinished;

	UPROPERTY()
	UObject* WorldContextObject;
	UPROPERTY()
	UEnvQuery* QueryTemplate;
	UPROPERTY()
	UObject* Querier;
	UPROPERTY()
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode;
	UPROPERTY()
	TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass;

	void DestroyAsync();

	//Overriding BP async action base
	virtual void Activate() override;
	
};

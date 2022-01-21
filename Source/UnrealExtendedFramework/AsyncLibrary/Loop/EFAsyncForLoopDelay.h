// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncForLoopDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FForLoopArrayDelegate,int32,ArrayIndex);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncForLoopDelay : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

		
	/**
	* Internal tick. Will also broadcast the Tick Execution pin in the editor
	*/
	UFUNCTION()
	void InternalTick();

	/**
	* Internal completed. Clears timers and flags and broadcasts Completed Execution pin in the editor
	*/
	UFUNCTION()
	void InternalCompleted();

	/**
	* Static property to prevent restarting the async node multiple times before execution has finished
	*/
	
	float LoopTime = 0.1;
	int32 LoopIndex = 0;

	int32 FirstArrayIndex;
	int32 LastArrayIndex;

	
	const UObject* WorldContext;
	FTimerHandle TimerHandle;

	
public:

	
	UPROPERTY(BlueprintAssignable)
	FForLoopArrayDelegate Loop;

	UPROPERTY(BlueprintAssignable)
	FForLoopArrayDelegate Completed;

	
	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended For Loop With Delay", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncForLoopDelay* EFForLoopWithDelay(const int32 FirstIndex , const int32 LastIndex ,const UObject* WorldContextObject , float Delay = 0.1);
	
	
	//Overriding BP async action base
	virtual void Activate() override;
};

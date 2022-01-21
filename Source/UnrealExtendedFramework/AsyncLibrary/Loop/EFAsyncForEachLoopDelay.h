// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncForEachLoopDelay.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArrayOutputPin,int32,ArrayIndex);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncForEachLoopDelay : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

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

	UPROPERTY()
	TArray<UObject*> Array;
	const UObject* WorldContext;
	FTimerHandle TimerHandle;

public:
	
	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Loop;

	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Completed;


	
	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Framework For Each Loop Delay", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncForEachLoopDelay* ForEachLoopDelayObject(const TArray<UObject*>& TargetArray ,const UObject* WorldContextObject , float Delay = 0.1);


	
	//Overriding BP async action base
	virtual void Activate() override;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UEExtendedForEachLoopDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArrayOutputPin,int32,ArrayIndex);

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedForEachLoopDelay : public UBlueprintAsyncActionBase
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
	static bool bActive;

	float LoopTime;

	int32 LoopIndex = 0;
	
	TArray<FProperty*> Array;
	
	/**
	* World context object to grab a reference of the world 
	*/
	const UObject* WorldContext;

	/**
	* Timer handle of internal tick
	*/
	FTimerHandle TimerHandle;

	public:
	
	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Loop;

	UPROPERTY(BlueprintAssignable)
	FArrayOutputPin Completed;

	UFUNCTION(BlueprintImplementableEvent)
	void OnTookTake(int32 aaa);

	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended For Each Loop Delay", WorldContext = "WorldContextObject" ,  ArrayParm  = "TargetArray",BlueprintThreadSafe), Category = "AsyncNode")
	static UUEExtendedForEachLoopDelay* ForEachLoopDelay(const TArray<UProperty*>& TargetArray ,const UObject* WorldContextObject , float Delay);

	//Overriding BP async action base
	virtual void Activate() override;
	
};

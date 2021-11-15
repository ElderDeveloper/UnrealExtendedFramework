// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/Object.h"
#include "UEExtendedFloatTimeline.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTimelineFloatLoop,float,Alpha);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTimelineFloatLoopComplete);

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedFloatTimeline : public UBlueprintAsyncActionBase
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
	float TimePassed = 0.f;
	float CurveLastTime = 0;
	
	const UObject* WorldContext;
	FTimerHandle TimerHandle;

public:
	
	UPROPERTY(BlueprintAssignable)
	FTimelineFloatLoop Loop;

	UPROPERTY(BlueprintAssignable)
	FTimelineFloatLoopComplete Completed;

	UPROPERTY()
	UCurveFloat* SelectedFloatCurve;

	
	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Float Timeline", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UUEExtendedFloatTimeline* ExtendedFloatTimeline(UCurveFloat* FloatCurve ,const UObject* WorldContextObject , float PassTime = 0.005);


	
	//Overriding BP async action base
	virtual void Activate() override;
};

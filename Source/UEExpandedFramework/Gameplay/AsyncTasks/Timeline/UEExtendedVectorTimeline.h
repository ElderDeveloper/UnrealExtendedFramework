// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveVector.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UEExtendedVectorTimeline.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTimelineVectorLoop,FVector,Vector);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTimelineVectorLoopComplete);


class UUEExtendedVectorTimeline;
static TMap<int32,TWeakObjectPtr<UUEExtendedVectorTimeline>> ExtendedVectorTimelines;


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedVectorTimeline : public UBlueprintAsyncActionBase
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
	FTimelineVectorLoop Loop;

	UPROPERTY(BlueprintAssignable)
	FTimelineVectorLoopComplete Completed;

	UPROPERTY()
	UCurveVector* SelectedFloatCurve;

	
	/**
	* InternalUseOnly to hide sync version in BPs
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Vector Curve Timeline", WorldContext = "WorldContextObject" , BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UUEExtendedVectorTimeline* ExtendedFloatTimeline(UCurveVector* VectorCurve ,const UObject* WorldContextObject , float PassTime = 0.005 );


	
	//Overriding BP async action base
	virtual void Activate() override;
};

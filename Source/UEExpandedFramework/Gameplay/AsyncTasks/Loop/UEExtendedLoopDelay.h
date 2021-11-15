// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/Object.h"
#include "UEExtendedLoopDelay.generated.h"

/**
 * 
 */

class UUEExtendedLoopDelay;
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDelayStart,UUEExtendedLoopDelay*,LoopReferance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLoopDelay);


UCLASS(BlueprintType,Blueprintable, meta = (ExposedAsyncProxy = AsyncTask))
class UEEXPANDEDFRAMEWORK_API UUEExtendedLoopDelay : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()


public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Loop Delay", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UUEExtendedLoopDelay* LoopDelay(const UObject* WorldContextObject ,bool shouldLoop = false, float Delay = 1.f);

	UFUNCTION(BlueprintCallable)
	void StopLoop();
	
protected:

	UPROPERTY(BlueprintAssignable)
	FLoopDelay Loop;
	
	
	FTimerHandle Handle;
	bool ShouldLoop;
	const UObject* WorldContext;
	float LoopTime;

	void TickLoop();
	
	//Overriding BP async action base
	virtual void Activate() override;
};

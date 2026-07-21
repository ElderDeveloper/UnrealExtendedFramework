// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncDelay.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExtendedDelay);


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncDelay : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

		
public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Delay", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncDelay* EFAsyncDelay(const UObject* WorldContextObject , float Delay = 1.f);

	
protected:

	UPROPERTY(BlueprintAssignable)
	FExtendedDelay Finished;
	
	FTimerHandle Handle;
	float DelayTime;
	
	UPROPERTY()
	const UObject* WorldContext;

	void FinishDelay();

	
	//Overriding BP async action base
	virtual void Activate() override;
};

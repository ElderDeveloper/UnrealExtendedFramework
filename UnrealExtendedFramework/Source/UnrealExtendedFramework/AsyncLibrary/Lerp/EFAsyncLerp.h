// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncLerp.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExtendedLerp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLerpOutputPin , float , Alpha);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncLerp : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

	
public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName=" Extended Lerp", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "AsyncNode")
	static UEFAsyncLerp* AsyncLerp(const UObject* WorldContextObject , float Duration = 1.f ,  bool Reversed = false , bool EaseInEaseOut = false);

protected:

	UPROPERTY(BlueprintAssignable)
	FLerpOutputPin Loop;
	
	UPROPERTY(BlueprintAssignable)
	FExtendedLerp Finished;



	bool Reversed;
	bool EaseInEaseOut;
	bool IgnoreTimeDilation;

	float CreatedGameTime;
	float TargetGameTime;
	bool FinalLoop;
	
	UPROPERTY()
	const UObject* WorldContext;

	FTimerHandle LerpHandle;

	float LerpDuration;
	float LerpAlpha;

	UFUNCTION()
	void Timer();

	//Overriding BP async action base
	virtual void Activate() override;

};

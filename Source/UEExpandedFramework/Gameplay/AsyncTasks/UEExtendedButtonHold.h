// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/Object.h"
#include "UEExtendedButtonHold.generated.h"


UENUM()
enum EButtonAction
{
	Press,
	Release
};

class UUEExtendedButtonHold;
static TArray<UUEExtendedButtonHold*> LoopReferenceArray;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonAction);
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedButtonHold : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()


	public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UUEExtendedButtonHold* ExtendedStartButtonAction(const UObject* WorldContextObject, float holdTime = 1.f , int32 ActionIndex = 0);

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void ExtendedEndButtonAction(const UObject* WorldContextObject, int32 ActionIndex = 0);

	
protected:

	UPROPERTY(BlueprintAssignable)
	FButtonAction Press;
	UPROPERTY(BlueprintAssignable)
	FButtonAction Hold;
	UPROPERTY(BlueprintAssignable)
	FButtonAction HoldExceeded;
	
	FTimerHandle Handle;
	const UObject* WorldContext;

	bool isHold = false;
	float HoldTime = 0;
	float TimePassed = 0;
	

	void PassTime();
	void StopAction() ;
	
	FORCEINLINE	void BroadcastHold()  {	Hold.Broadcast();	StopAction();};
	FORCEINLINE	void BroadcastPress() {	Press.Broadcast();	StopAction();};
	
	//Overriding BP async action base
	virtual void Activate() override;
};

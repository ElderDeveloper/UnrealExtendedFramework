// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/Object.h"
#include "UEExtendedButtonHoldName.generated.h"

/**
 * 
 */

class UUEExtendedButtonHoldName;
static TMap<FName,TWeakObjectPtr<UUEExtendedButtonHoldName>> ButtonHoldsName;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonAction);
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedButtonHoldName : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()


	
	public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UUEExtendedButtonHoldName* ExtendedStartButtonActionName(const UObject* WorldContextObject, FName ActionName , float holdTime = 1.f);

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void ExtendedEndButtonActionName(const UObject* WorldContextObject, FName ActionName);

	
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
	
	FORCEINLINE	void BroadcastHold()  {	Hold.Broadcast();};
	FORCEINLINE	void BroadcastPress() {	Press.Broadcast();};
	
	//Overriding BP async action base
	virtual void Activate() override;
};

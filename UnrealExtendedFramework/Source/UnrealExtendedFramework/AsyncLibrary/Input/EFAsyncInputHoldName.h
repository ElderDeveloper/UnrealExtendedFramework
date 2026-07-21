// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncInputHoldName.generated.h"

class UEFAsyncInputHoldName;
static TMap<FName,TWeakObjectPtr<UEFAsyncInputHoldName>>InputHoldsName;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonAction);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncInputHoldName : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

	
	
public:

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UEFAsyncInputHoldName* EFStartInputActionName(const UObject* WorldContextObject, FName ActionName , float holdTime = 1.f);

	UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject"))
	static void EFEndInputActionName(const UObject* WorldContextObject, FName ActionName);

	
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

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UEExtendedButtonHold.generated.h"



class UUEExtendedButtonHold;
static TMap<int32,TWeakObjectPtr<UUEExtendedButtonHold>> ButtonHolds;



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonActionName);
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
	FButtonActionName Press;
	UPROPERTY(BlueprintAssignable)
	FButtonActionName Hold;
	UPROPERTY(BlueprintAssignable)
	FButtonActionName HoldExceeded;
	
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

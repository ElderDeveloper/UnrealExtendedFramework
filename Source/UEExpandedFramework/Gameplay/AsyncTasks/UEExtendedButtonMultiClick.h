﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "UEExtendedButtonMultiClick.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonClickAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FButtonEndAction,int32,ClickIndex);



class UUEExtendedButtonMultiClick;
static TMap<FName,TWeakObjectPtr<UUEExtendedButtonMultiClick>> ButtonClicksName;


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedButtonMultiClick : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
    
    UFUNCTION(BlueprintCallable,meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "true") , Category = "AsyncNode")
    static UUEExtendedButtonMultiClick* ExtendedButtonMultiClick(const UObject* WorldContextObject, FName ActionName , float clickWaitTime = 0.5f , int32 maxClickCount = 3);


	void BroadcastClickIndex();
	
    UPROPERTY(BlueprintAssignable)
    FButtonEndAction DoubleClick;
	
    UPROPERTY(BlueprintAssignable)
    FButtonEndAction TripleClick;
	
    UPROPERTY(BlueprintAssignable)
    FButtonEndAction End;


protected:
	
    FTimerHandle Handle;
    const UObject* WorldContext;


	FName ActionName;
	int32 ClickIndex = 0;
	int32 MaxClickCount = 3;
    float ClickWaitTime = 0;

    	
    UFUNCTION()
    void PassTime();
    	
    //Overriding BP async action base
   	virtual void Activate() override;
};

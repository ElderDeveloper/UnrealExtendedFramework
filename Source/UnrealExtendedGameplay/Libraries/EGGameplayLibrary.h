// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGGameplayLibrary.generated.h"


UENUM()
enum EExtendedLog
{
	Log,
	Warning,
	Error
};

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintCallable , Category="Extended Log")
	static void ExtendedBlueprintLOG(TEnumAsByte<EExtendedLog> ExtendedLogType , FString Log);
	
	UFUNCTION(BlueprintCallable, Category = "Reflection" , meta=(DefaultToSelf="RequestOwner" , HidePin="RequestOwner"))
	static void ExecuteFunction(UObject* RequestOwner , UObject* TargetObject , const FString FunctionToExecute);

	
	/*
	 * Return Game FPS
	 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static float GetGameplayFramePerSecond(const UObject* WorldContextObject);
};

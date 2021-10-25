// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedGameplayLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedGameplayLibrary : public UObject
{
	GENERATED_BODY()

	public:


	UFUNCTION(BlueprintCallable, Category = "Reflection" , meta=(DefaultToSelf="RequestOwner" , HidePin="RequestOwner"))
	static void ExecuteFunction(UObject* RequestOwner , UObject* TargetObject , const FString FunctionToExecute);

	static void RotateToObjectYaw(AActor* From , AActor* To);

	static void RotateToObject(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterp(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);
};

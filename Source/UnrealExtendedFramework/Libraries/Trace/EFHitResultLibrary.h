// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFHitResultLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFHitResultLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Location From Hit Result", CompactNodeTitle = "Location"), Category="UEExtended|Hit Result")
	static FVector GetHitLocationFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Trace End From Hit Result", CompactNodeTitle = "TraceEnd"), Category="UEExtended|Hit Result")
	static FVector GetTraceEndFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Actor From Hit Result", CompactNodeTitle = "HitActor"), Category="UEExtended|Hit Result")
	static AActor* GetHitActorFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Component From Hit Result", CompactNodeTitle = "HitComponent"), Category="UEExtended|Hit Result")
	static USceneComponent* GetHitComponentFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Distance From Hit Result", CompactNodeTitle = "Distance"), Category="UEExtended|Hit Result")
	static float GetHitDistance(FHitResult HitResult);
};

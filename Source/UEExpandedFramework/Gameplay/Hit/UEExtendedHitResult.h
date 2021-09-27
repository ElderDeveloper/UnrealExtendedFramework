// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "UEExtendedHitResult.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedHitResult : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Location From Hit Result", CompactNodeTitle = "Location", ScriptMethod = "Location+", ScriptOperator = "="), Category="UEExtended|Hit Result")
	static FVector GetHitLocationFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Trace End From Hit Result", CompactNodeTitle = "TraceEnd", ScriptMethod = "TraceEnd+", ScriptOperator = "="), Category="UEExtended|Hit Result")
	static FVector GetTraceEndFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Actor From Hit Result", CompactNodeTitle = "TraceEnd", ScriptMethod = "TraceEnd+", ScriptOperator = "="), Category="UEExtended|Hit Result")
	static AActor* GetHitActorFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Component From Hit Result", CompactNodeTitle = "TraceEnd", ScriptMethod = "TraceEnd+", ScriptOperator = "="), Category="UEExtended|Hit Result")
	static USceneComponent* GetHitComponentFromHitResult(FHitResult HitResult);
};

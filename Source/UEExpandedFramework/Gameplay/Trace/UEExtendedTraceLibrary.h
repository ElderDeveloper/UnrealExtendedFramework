// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedTraceData.h"
#include "UObject/Object.h"
#include "UEExtendedTraceLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTraceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedLineTraceSingle(const UObject* WorldContextObject , UPARAM(ref) FLineTraceStruct& LineTraceStruct);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedLineTraceMulti(const UObject* WorldContextObject , UPARAM(ref) FLineTraceStruct& LineTraceStruct);


	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedBoxTraceSingle(const UObject* WorldContextObject , UPARAM(ref) FBoxTraceStruct& BoxTraceStruct );

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedBoxTraceMulti(const UObject* WorldContextObject , UPARAM(ref) FBoxTraceStruct& BoxTraceStruct);


	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedSphereTraceSingle(const UObject* WorldContextObject , UPARAM(ref) FSphereTraceStruct& SphereTraceStruct);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedSphereTraceMulti(const UObject* WorldContextObject , UPARAM(ref) FSphereTraceStruct& SphereTraceStruct);


	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedCapsuleTraceSingle(const UObject* WorldContextObject , UPARAM(ref) FCapsuleTraceStruct& CapsuleTraceStruct);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static bool ExtendedCapsuleTraceMulti(const UObject* WorldContextObject , UPARAM(ref) FCapsuleTraceStruct& CapsuleTraceStruct);
	
};

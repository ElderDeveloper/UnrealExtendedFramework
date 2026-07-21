// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "EFTraceLibrary.generated.h"

/**
 * Blueprint function library wrapping UKismetSystemLibrary trace functions
 * with configurable trace structs (Line, Box, Sphere, Capsule).
 * Supports Channel, Profile, and Object trace types via a single node.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFTraceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Performs a single line trace using the settings in the LineTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedLineTraceSingle(const UObject* WorldContextObject, UPARAM(ref) FLineTraceStruct& LineTraceStruct);

	/** Performs a multi line trace using the settings in the LineTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedLineTraceMulti(const UObject* WorldContextObject, UPARAM(ref) FLineTraceStruct& LineTraceStruct);

	/** Performs a single box trace using the settings in the BoxTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedBoxTraceSingle(const UObject* WorldContextObject, UPARAM(ref) FBoxTraceStruct& BoxTraceStruct);

	/** Performs a multi box trace using the settings in the BoxTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedBoxTraceMulti(const UObject* WorldContextObject, UPARAM(ref) FBoxTraceStruct& BoxTraceStruct);

	/** Performs a single sphere trace using the settings in the SphereTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedSphereTraceSingle(const UObject* WorldContextObject, UPARAM(ref) FSphereTraceStruct& SphereTraceStruct);

	/** Performs a multi sphere trace using the settings in the SphereTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedSphereTraceMulti(const UObject* WorldContextObject, UPARAM(ref) FSphereTraceStruct& SphereTraceStruct);

	/** Performs a single capsule trace using the settings in the CapsuleTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedCapsuleTraceSingle(const UObject* WorldContextObject, UPARAM(ref) FCapsuleTraceStruct& CapsuleTraceStruct);

	/** Performs a multi capsule trace using the settings in the CapsuleTraceStruct. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedCapsuleTraceMulti(const UObject* WorldContextObject, UPARAM(ref) FCapsuleTraceStruct& CapsuleTraceStruct);

	/**
	 * Performs a line trace from the player camera's location in its forward direction.
	 * Automatically fills Start and End on the LineTraceStruct.
	 * @param PlayerController The player controller whose camera viewpoint is used
	 * @param Distance The trace distance from the camera
	 * @param LineTraceStruct The trace struct (Start/End will be overwritten)
	 * @return True if the trace hit something
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "ExtendedFramework|Trace")
	static bool ExtendedLineTraceFromCamera(const UObject* WorldContextObject, APlayerController* PlayerController, float Distance, UPARAM(ref) FLineTraceStruct& LineTraceStruct);
};

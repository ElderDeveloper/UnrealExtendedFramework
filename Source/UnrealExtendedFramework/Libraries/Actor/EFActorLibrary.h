// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFActorLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFActorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GET LOCATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location X", CompactNodeTitle = "(Self)LocationX"  , DefaultToSelf = "Actor")  , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationX(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Y", CompactNodeTitle = "(Self)LocationY"  , DefaultToSelf = "Actor")  , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationY(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Z", CompactNodeTitle = "(Self)LocationZ"  , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationZ(AActor* Actor);


	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GET ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Yaw", CompactNodeTitle = "(Self)RotationYaw" , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationYaw(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Pitch", CompactNodeTitle = "(Self)RotationPitch"  , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationPitch(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Roll", CompactNodeTitle = "(Self)RotationRoll" , DefaultToSelf = "Actor" )   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationRoll(AActor* Actor);



	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SET ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/* Rotates Object To Given Actor With Only Yaw Rotation , This Can Be Used As a Find Look At Replacement */
	UFUNCTION(BlueprintCallable , meta=(Keywords="Set Look At Rotation Only Yaw"), Category="ExtendedFramework|Actor|Rotation")
	static void RotateToObjectYaw(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|Actor|Rotation")
	static void RotateToObject(AActor* From , AActor* To);

	/**
	 * Rotates Object To Given Actor With Only Yaw Rotation With Interpolation , This Can Be Used As a Find Look At Replacement
	 * @param UseFindLookAtRotation uses built in FindLookAtRotation but if its false function will custom Find Rotation ( this will prevent some find look at rotation issues )
	 **/
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject" , Keywords="Set Look At Rotation Only Yaw")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed = 3.f , bool UseFindLookAtRotation = true);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToObjectInterp(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);





	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< NETWORKING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "Is Actor Local" , DefaultToSelf = "Actor") , Category="ExtendedFramework|Actor|Networking")
	static bool IsActorLocal(AActor* Actor);

	UFUNCTION(BlueprintCallable, meta = (CompactNodeTitle = "Switch Actor Local" , DefaultToSelf = "Actor" , ExpandEnumAsExecs = "OutPins") , Category="ExtendedFramework|Actor|Networking")
	static void SwitchIsLocallyControlled(AActor* Actor , TEnumAsByte<EFConditionOutput>& OutPins);


	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< CLASS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Actor|Class")
	static bool IsClassEqualOrChildOfOtherClass(TSubclassOf<AActor> TestClass , TSubclassOf<AActor> ParentClass);


	UFUNCTION(BlueprintPure, meta = ( DefaultToSelf = "Pawn" , Keywords = "Controller , Forward , Right ") , Category="ExtendedFramework|Actor|Networking")
	static void GetActorControlRotationDirection(APawn* Pawn , FVector& Forward , FVector& Right);
};



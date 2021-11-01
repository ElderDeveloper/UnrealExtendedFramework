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
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location X", CompactNodeTitle = "(Self)LocationX" , ScriptMethod = "ActorLocationX", ScriptOperator = "X" , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationX(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Y", CompactNodeTitle = "(Self)LocationY" ,ScriptMethod = "ActorLocationY", ScriptOperator = "Y" , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationY(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Z", CompactNodeTitle = "(Self)LocationZ" , ScriptMethod = "ActorLocationZ", ScriptOperator = "Z" , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationZ(AActor* Actor);


	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Yaw", CompactNodeTitle = "(Self)RotationYaw" , ScriptMethod = "ActorRotationYaw", ScriptOperator = "Yaw" , DefaultToSelf = "Actor") , Category="Actor|Rotation")
	static float GetActorRotationYaw(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Pitch", CompactNodeTitle = "(Self)RotationPitch" , ScriptMethod = "ActorRotationPitch", ScriptOperator = "Pitch" , DefaultToSelf = "Actor") , Category="Actor|Rotation")
	static float GetActorRotationPitch(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Roll", CompactNodeTitle = "(Self)RotationRoll" , ScriptMethod = "ActorRotationRoll", ScriptOperator = "Roll" , DefaultToSelf = "Actor" ) , Category="Actor|Rotation")
	static float GetActorRotationRoll(AActor* Actor);


	UFUNCTION(BlueprintCallable, Category = "Reflection" , meta=(DefaultToSelf="RequestOwner" , HidePin="RequestOwner"))
	static void ExecuteFunction(UObject* RequestOwner , UObject* TargetObject , const FString FunctionToExecute);

	UFUNCTION(BlueprintCallable)
	static void RotateToObjectYaw(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable)
	static void RotateToObject(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterp(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);
};

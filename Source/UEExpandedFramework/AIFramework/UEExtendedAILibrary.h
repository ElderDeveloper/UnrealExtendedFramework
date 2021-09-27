// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExtendedAILibrary.generated.h"

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedAILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD GETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Bool", CompactNodeTitle = "Board Bool", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static bool ExtendedGetBlackboardBool(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Class", CompactNodeTitle = "Board Class", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static TSubclassOf<UObject> ExtendedGetBlackboardClass(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Enum", CompactNodeTitle = "Board Enum", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static uint8 ExtendedGetBlackboardEnum(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Float", CompactNodeTitle = "Board Float", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static float ExtendedGetBlackboardFloat(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Int", CompactNodeTitle = "Board Int", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static int32 ExtendedGetBlackboardInt(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Name", CompactNodeTitle = "Board Name", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FName ExtendedGetBlackboardName(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Object", CompactNodeTitle = "Board Object", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static UObject* ExtendedGetBlackboardObject(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Rotator", CompactNodeTitle = "Board Rotator", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static FRotator ExtendedGetBlackboardRotator(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard String", CompactNodeTitle = "Board String", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FString ExtendedGetBlackboardString(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Vector", CompactNodeTitle = "Board Vector", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FVector ExtendedGetBlackboardVector(AActor* OwningActor , FName KeyName);


		//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintCallable , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardBool(AActor* OwningActor , FName KeyName , bool Value);

	UFUNCTION(BlueprintCallable, Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardClass(AActor* OwningActor , FName KeyName , TSubclassOf<UObject> Value);

	UFUNCTION(BlueprintCallable , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardEnum(AActor* OwningActor , FName KeyName , uint8 Value);

	UFUNCTION(BlueprintCallable , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardFloat(AActor* OwningActor , FName KeyName , float Value);

	UFUNCTION(BlueprintCallable , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardInt(AActor* OwningActor , FName KeyName , int32 Value);

	UFUNCTION(BlueprintCallable ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardName(AActor* OwningActor , FName KeyName , FName Value);

	UFUNCTION(BlueprintCallable , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardObject(AActor* OwningActor , FName KeyName , UObject* Value);

	UFUNCTION(BlueprintCallable ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardRotator(AActor* OwningActor , FName KeyName ,FRotator Value);

	UFUNCTION(BlueprintCallable ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardString(AActor* OwningActor , FName KeyName , FString Value);

	UFUNCTION(BlueprintCallable ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardVector(AActor* OwningActor , FName KeyName , FVector Value);
};

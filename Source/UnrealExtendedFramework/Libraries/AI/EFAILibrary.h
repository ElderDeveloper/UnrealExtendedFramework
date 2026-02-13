// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFAILibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
public:
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|AI" )
	static void ForceRebuildNavigationMesh(const UObject* WorldContextObject);
	
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD GETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Bool",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Bool", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static bool ExtendedGetBlackboardBool(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Class",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Class", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static TSubclassOf<UObject> ExtendedGetBlackboardClass(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Enum",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Enum", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static uint8 ExtendedGetBlackboardEnum(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Float",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Float", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static float ExtendedGetBlackboardFloat(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Int",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Int", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static int32 ExtendedGetBlackboardInt(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Name",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Name", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FName ExtendedGetBlackboardName(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Object", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Object", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static UObject* ExtendedGetBlackboardObject(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Rotator", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Rotator", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static FRotator ExtendedGetBlackboardRotator(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard String", DefaultToSelf="OwningActor",CompactNodeTitle = "Board String", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FString ExtendedGetBlackboardString(AActor* OwningActor , FName KeyName);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Vector", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Vector", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FVector ExtendedGetBlackboardVector(AActor* OwningActor , FName KeyName);

	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardBool(AActor* OwningActor , FName KeyName , bool Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardClass(AActor* OwningActor , FName KeyName , TSubclassOf<UObject> Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardEnum(AActor* OwningActor , FName KeyName , uint8 Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardFloat(AActor* OwningActor , FName KeyName , float Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardInt(AActor* OwningActor , FName KeyName , int32 Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardName(AActor* OwningActor , FName KeyName , FName Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardObject(AActor* OwningActor , FName KeyName , UObject* Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardRotator(AActor* OwningActor , FName KeyName ,FRotator Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardString(AActor* OwningActor , FName KeyName , FString Value);

	// Update Blackboard Value of Owning Actor With Key Name And Value
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardVector(AActor* OwningActor , FName KeyName , FVector Value);



	//float AcceptanceRadius , bool bStopOnOverlap , bool bUsePathfinding , bool bProjectDestinationToNavigation , bool bCanStrafe , bool bAllowPartialPath , TEnumAsByte<EAIOptionFlag::Type> Filter

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|AI|Movement" )
	static void CustomMoveAIToLocations( const UObject* WorldContextObject, APawn* Pawn , TArray<FVector> Locations);

	UFUNCTION(BlueprintCallable, Category="ExtendedFramework|AI|Movement" )
	static bool CustomAIMovetoLocation(APawn* Pawn ,const FVector& Location , const float& AcceptedRadius = 50);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFAILibrary.generated.h"

class UBlackboardComponent;

/**
 * Blueprint function library providing extended AI utilities including
 * navigation mesh control, blackboard accessors, and custom AI movement.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	
public:

	/**
	 * Forces the navigation system to rebuild the navigation mesh in the given world.
	 * @param WorldContextObject The context object indicating the world
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|AI" )
	static void ForceRebuildNavigationMesh(const UObject* WorldContextObject);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD HELPERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the Blackboard Component for the given AI-controlled actor.
	 * Useful for batch operations when you need to set/get multiple values
	 * without repeated lookups.
	 * @param OwningActor The actor that owns the AI controller with a blackboard
	 * @return The Blackboard Component, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, meta=(DefaultToSelf="OwningActor", CompactNodeTitle = "Get Board"), Category="AI|Blackboard")
	static UBlackboardComponent* GetBlackboardComponent(AActor* OwningActor);

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD GETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Gets a boolean value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The boolean value, or false if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Bool",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Bool", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static bool ExtendedGetBlackboardBool(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a class value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The class value, or nullptr if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Class",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Class", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static TSubclassOf<UObject> ExtendedGetBlackboardClass(AActor* OwningActor , FName KeyName);

	/**
	 * Gets an enum (uint8) value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The enum value as uint8, or 0 if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Enum",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Enum", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static uint8 ExtendedGetBlackboardEnum(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a float value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The float value, or 0.0f if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Float",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Float", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static float ExtendedGetBlackboardFloat(AActor* OwningActor , FName KeyName);

	/**
	 * Gets an integer value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The integer value, or 0 if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Int",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Int", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static int32 ExtendedGetBlackboardInt(AActor* OwningActor , FName KeyName);

	/**
	 * Gets an FName value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The name value, or NAME_None if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Name",DefaultToSelf="OwningActor", CompactNodeTitle = "Board Name", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FName ExtendedGetBlackboardName(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a UObject pointer from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The object pointer, or nullptr if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Object", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Object", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static UObject* ExtendedGetBlackboardObject(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a rotator value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The rotator value, or FRotator::ZeroRotator if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Rotator", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Rotator", BlueprintThreadSafe),Category="AI|Blackboard|Get")
	static FRotator ExtendedGetBlackboardRotator(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a string value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The string value, or empty string if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard String", DefaultToSelf="OwningActor",CompactNodeTitle = "Board String", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FString ExtendedGetBlackboardString(AActor* OwningActor , FName KeyName);

	/**
	 * Gets a vector value from the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to read
	 * @return The vector value, or FVector::ZeroVector if the blackboard or key is invalid
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Extended Get Blackboard Vector", DefaultToSelf="OwningActor",CompactNodeTitle = "Board Vector", BlueprintThreadSafe), Category="AI|Blackboard|Get")
	static FVector ExtendedGetBlackboardVector(AActor* OwningActor , FName KeyName);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	/**
	 * Sets a boolean value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The boolean value to set
	 * @return True if the value was set successfully, false if the blackboard was not found
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardBool(AActor* OwningActor , FName KeyName , bool Value);

	/**
	 * Sets a class value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The class value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardClass(AActor* OwningActor , FName KeyName , TSubclassOf<UObject> Value);

	/**
	 * Sets an enum (uint8) value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The enum value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardEnum(AActor* OwningActor , FName KeyName , uint8 Value);

	/**
	 * Sets a float value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The float value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardFloat(AActor* OwningActor , FName KeyName , float Value);

	/**
	 * Sets an integer value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The integer value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardInt(AActor* OwningActor , FName KeyName , int32 Value);

	/**
	 * Sets an FName value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The name value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardName(AActor* OwningActor , FName KeyName , FName Value);

	/**
	 * Sets a UObject pointer on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The object pointer to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") , Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardObject(AActor* OwningActor , FName KeyName , UObject* Value);

	/**
	 * Sets a rotator value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The rotator value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardRotator(AActor* OwningActor , FName KeyName ,FRotator Value);

	/**
	 * Sets a string value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The string value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardString(AActor* OwningActor , FName KeyName , FString Value);

	/**
	 * Sets a vector value on the blackboard of the owning actor's AI controller.
	 * @param OwningActor The actor that owns the AI controller
	 * @param KeyName The blackboard key name to write
	 * @param Value The vector value to set
	 * @return True if the value was set successfully
	 */
	UFUNCTION(BlueprintCallable ,meta=(DefaultToSelf="OwningActor") ,Category="AI|Blackboard|Set")
	static bool ExtendedSetBlackboardVector(AActor* OwningActor , FName KeyName , FVector Value);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MOVEMENT >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Moves AI sequentially along path points found via Navigation System to each of the given locations.
	 * Note: This function issues AddMovementInput calls within a single frame, so only the first
	 * movement segment will be effective per call. Call this every tick for continuous movement.
	 * @param WorldContextObject The context object
	 * @param Pawn The pawn to move
	 * @param Locations The target locations to navigate through
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|AI|Movement" )
	static void CustomMoveAIToLocations( const UObject* WorldContextObject, APawn* Pawn , TArray<FVector> Locations);

	/**
	 * Instantly computes path and moves AI towards a single target location.
	 * Designed to be called every tick for continuous movement.
	 * @param Pawn The pawn to move
	 * @param Location Target destination
	 * @param AcceptedRadius Stop distance to the target
	 * @return True if the Pawn is within the accepted radius of the target location
	 */
	UFUNCTION(BlueprintCallable, Category="ExtendedFramework|AI|Movement" )
	static bool CustomAIMovetoLocation(APawn* Pawn ,const FVector& Location , const float& AcceptedRadius = 50);


private:

	/**
	 * Internal helper to safely retrieve the Blackboard Component from an actor's AI controller.
	 * @param OwningActor The actor to query
	 * @return The Blackboard Component, or nullptr if the actor or its AI controller is invalid
	 */
	static UBlackboardComponent* GetBlackboardSafe(AActor* OwningActor);
};

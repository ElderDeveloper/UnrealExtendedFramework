// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "EFActorLibrary.generated.h"

/**
 * Blueprint function library providing extended actor utilities including
 * location/rotation accessors, rotation interpolation, networking checks,
 * direction vectors, distance helpers, and coordinate space conversion.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFActorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GET LOCATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the X component of the actor's world location.
	 * @param Actor The actor to query (defaults to self)
	 * @return X coordinate, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location X", CompactNodeTitle = "(Self)LocationX"  , DefaultToSelf = "Actor")  , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationX(AActor* Actor);

	/**
	 * Returns the Y component of the actor's world location.
	 * @param Actor The actor to query (defaults to self)
	 * @return Y coordinate, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Y", CompactNodeTitle = "(Self)LocationY"  , DefaultToSelf = "Actor")  , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationY(AActor* Actor);

	/**
	 * Returns the Z component of the actor's world location.
	 * @param Actor The actor to query (defaults to self)
	 * @return Z coordinate, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Z", CompactNodeTitle = "(Self)LocationZ"  , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Location")
	static float GetActorLocationZ(AActor* Actor);


	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< GET ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the Yaw component of the actor's world rotation.
	 * @param Actor The actor to query (defaults to self)
	 * @return Yaw in degrees, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Yaw", CompactNodeTitle = "(Self)RotationYaw" , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationYaw(AActor* Actor);

	/**
	 * Returns the Pitch component of the actor's world rotation.
	 * @param Actor The actor to query (defaults to self)
	 * @return Pitch in degrees, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Pitch", CompactNodeTitle = "(Self)RotationPitch"  , DefaultToSelf = "Actor")   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationPitch(AActor* Actor);

	/**
	 * Returns the Roll component of the actor's world rotation.
	 * @param Actor The actor to query (defaults to self)
	 * @return Roll in degrees, or 0 if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Roll", CompactNodeTitle = "(Self)RotationRoll" , DefaultToSelf = "Actor" )   , Category="ExtendedFramework|Actor|Rotation")
	static float GetActorRotationRoll(AActor* Actor);



	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DIRECTION VECTORS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the actor's forward direction vector (unit length).
	 * @param Actor The actor to query (defaults to self)
	 * @return Forward vector, or FVector::ZeroVector if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Forward Vector", CompactNodeTitle = "(Self)Forward", DefaultToSelf = "Actor"), Category="ExtendedFramework|Actor|Direction")
	static FVector GetActorForwardVector(AActor* Actor);

	/**
	 * Returns the actor's right direction vector (unit length).
	 * @param Actor The actor to query (defaults to self)
	 * @return Right vector, or FVector::ZeroVector if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Right Vector", CompactNodeTitle = "(Self)Right", DefaultToSelf = "Actor"), Category="ExtendedFramework|Actor|Direction")
	static FVector GetActorRightVector(AActor* Actor);

	/**
	 * Returns the actor's up direction vector (unit length).
	 * @param Actor The actor to query (defaults to self)
	 * @return Up vector, or FVector::ZeroVector if Actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Up Vector", CompactNodeTitle = "(Self)Up", DefaultToSelf = "Actor"), Category="ExtendedFramework|Actor|Direction")
	static FVector GetActorUpVector(AActor* Actor);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SET ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Instantly rotates From actor to face To actor using only Yaw rotation.
	 * Preserves the From actor's current Pitch and Roll.
	 * Can be used as a Find Look At Rotation replacement.
	 * @param From The actor to rotate
	 * @param To The target actor to look at
	 */
	UFUNCTION(BlueprintCallable , meta=(Keywords="Set Look At Rotation Only Yaw"), Category="ExtendedFramework|Actor|Rotation")
	static void RotateToObjectYaw(AActor* From , AActor* To);

	/**
	 * Instantly rotates From actor to face To actor on all axes.
	 * Can be used as a Find Look At Rotation replacement.
	 * @param From The actor to rotate
	 * @param To The target actor to look at
	 */
	UFUNCTION(BlueprintCallable , Category="ExtendedFramework|Actor|Rotation")
	static void RotateToObject(AActor* From , AActor* To);

	/**
	 * Smoothly rotates From actor to face To actor using only Yaw rotation with interpolation.
	 * Uses NormalizeAxis to always take the shortest angular path, preventing 270-degree spins.
	 * @param WorldContextObject World context
	 * @param From The actor to rotate
	 * @param To The target actor to look at
	 * @param InterpSpeed Interpolation speed multiplier
	 * @param UseFindLookAtRotation When true, preserves existing Pitch/Roll; when false, zeros Pitch/Roll
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject" , Keywords="Set Look At Rotation Only Yaw")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed = 3.f , bool UseFindLookAtRotation = true);

	/**
	 * Smoothly rotates From actor to face a target location using only Yaw rotation with interpolation.
	 * Uses NormalizeAxis to always take the shortest angular path, preventing 270-degree spins.
	 * @param WorldContextObject World context
	 * @param From The actor to rotate
	 * @param To The target location to look at
	 * @param InterpSpeed Interpolation speed multiplier
	 * @param UseFindLookAtRotation When true, preserves existing Pitch/Roll; when false, zeros Pitch/Roll
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject" , Keywords="Set Look At Rotation Only Yaw")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToLocationInterpYaw(const UObject* WorldContextObject, AActor* From , const FVector& To , float InterpSpeed = 3.f , bool UseFindLookAtRotation = true);

	/**
	 * Smoothly rotates From actor to face To actor on all axes with interpolation.
	 * @param WorldContextObject World context
	 * @param From The actor to rotate
	 * @param To The target actor to look at
	 * @param InterpSpeed Interpolation speed multiplier
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToObjectInterp(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);

	/**
	 * Smoothly rotates From actor to face a target location on all axes with interpolation,
	 * clamped by a maximum degrees-per-second rate.
	 * @param WorldContextObject World context
	 * @param From The actor to rotate
	 * @param To The target location to look at
	 * @param InterpSpeed Interpolation speed multiplier
	 * @param MaxDegreePerSecond Maximum rotation rate in degrees per second
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject")  , Category="ExtendedFramework|Actor|Rotation" )
	static void RotateToLocationInterp(const UObject* WorldContextObject, AActor* From , const FVector& To , float InterpSpeed = 3 , float MaxDegreePerSecond = 180.f);

	
	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< NETWORKING >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Checks if the given actor is locally controlled (e.g. possessed by a local player controller).
	 * Supports Pawn, Controller, and PlayerState actor types.
	 * @param Actor The actor to check (defaults to self)
	 * @return True if the actor is locally controlled
	 */
	UFUNCTION(BlueprintPure, meta = (CompactNodeTitle = "Is Actor Local" , DefaultToSelf = "Actor") , Category="ExtendedFramework|Actor|Networking")
	static bool IsActorLocal(AActor* Actor);

	/**
	 * Execution pin switch based on whether the given actor is locally controlled.
	 * @param Actor The actor to check (defaults to self)
	 * @param OutPins The execution path determined by the local status
	 */
	UFUNCTION(BlueprintCallable, meta = (CompactNodeTitle = "Switch Actor Local" , DefaultToSelf = "Actor" , ExpandEnumAsExecs = "OutPins") , Category="ExtendedFramework|Actor|Networking")
	static void SwitchIsLocallyControlled(AActor* Actor , TEnumAsByte<EFConditionOutput>& OutPins);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< CLASS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns true if TestClass is the same as ParentClass or is a child of ParentClass.
	 * @param TestClass The class to evaluate
	 * @param ParentClass The class to check against
	 * @return True if TestClass equals or derives from ParentClass
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Actor|Class")
	static bool IsClassEqualOrChildOfOtherClass(TSubclassOf<AActor> TestClass , TSubclassOf<AActor> ParentClass);

	/**
	 * Gets the Forward and Right direction vectors from the pawn's Control Rotation.
	 * @param Pawn The pawn to evaluate (defaults to self)
	 * @param Forward The computed forward vector
	 * @param Right The computed right vector
	 * @param YawOnly If true, evaluates using only the Yaw (ignoring pitch and roll)
	 */
	UFUNCTION(BlueprintPure, meta = ( DefaultToSelf = "Pawn" , Keywords = "Controller , Forward , Right ") , Category="ExtendedFramework|Actor")
	static void GetActorControlRotationDirection(APawn* Pawn  , FVector& Forward , FVector& Right , bool YawOnly = true);

	/**
	 * Retrieves the pawn's Control Rotation, isolated to its Yaw axis only.
	 * @param Pawn The pawn to get Control Rotation from (defaults to self)
	 * @return FRotator with the Yaw set, Pitch and Roll are zero
	 */
	UFUNCTION(BlueprintPure, meta = ( DefaultToSelf = "Pawn" , CompactNodeTitle = "ControlRotationYaw" , Keywords = "Controller , Forward , Right ") , Category="ExtendedFramework|Actor")
	static FRotator GetActorControlRotationYaw(APawn* Pawn);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< COORDINATE SPACE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Transforms a world-space location to actor-local space.
	 * @param Actor The actor acting as the local space reference
	 * @param WorldLocation The world-space coordinates
	 * @return Location vector relative to the actor, or ZeroVector if Actor is null
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Actor")
	static FVector WorldToLocal(const AActor* Actor , const FVector& WorldLocation);

	/**
	 * Transforms an actor-local space location to world-space.
	 * @param Actor The actor acting as the local space reference
	 * @param LocalLocation The location relative to the actor
	 * @return Coordinates in world space, or ZeroVector if Actor is null
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Actor")
	static FVector LocalToWorld(const AActor* Actor , const FVector& LocalLocation);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DISTANCE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the 3D distance between two actors.
	 * @param ActorA First actor
	 * @param ActorB Second actor
	 * @return Distance in world units, or -1.0 if either actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "Distance", Keywords = "Distance Between"), Category="ExtendedFramework|Actor|Distance")
	static float DistanceBetweenActors(const AActor* ActorA, const AActor* ActorB);

	/**
	 * Returns the 2D distance between two actors (ignoring Z axis).
	 * @param ActorA First actor
	 * @param ActorB Second actor
	 * @return 2D distance in world units, or -1.0 if either actor is null
	 */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "Distance 2D", Keywords = "Distance Between 2D"), Category="ExtendedFramework|Actor|Distance")
	static float DistanceBetweenActors2D(const AActor* ActorA, const AActor* ActorB);

};

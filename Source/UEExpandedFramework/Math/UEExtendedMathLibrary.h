// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExpandedFramework/UEExpandedFramework.h"
#include "UEExtendedMathLibrary.generated.h"






UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure)
	static FRotator GetRotationBetweenActors(const AActor* From, const AActor* To, const FRotator PlusRotator = FRotator::ZeroRotator);
	
	UFUNCTION(BlueprintPure)
	static void GetAngleBetweenActors(AActor* From, AActor* To , float& Yaw , float& Pitch);



	
	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DISTANCE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure)
	static float GetDistanceBetweenActors(const AActor* From,const AActor* To);

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Components", CompactNodeTitle = "Distance", ScriptMethod = "Distance", ScriptOperator = "="))
	static float GetDistanceBetweenComponents(const USceneComponent* From,const USceneComponent* To);

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Component And Actor", CompactNodeTitle = "Distance", ScriptMethod = "Distance", ScriptOperator = "="))
	static float GetDistanceBetweenComponentAndActor(const USceneComponent* From,const AActor* To);

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Vectors", CompactNodeTitle = "Distance", ScriptMethod = "Distance", ScriptOperator = "="))
	static float GetDistanceBetweenVectors(const FVector From, const FVector To);

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Vectors No Square Root", CompactNodeTitle = "DistanceNoSqrt", ScriptMethod = "DistanceNoSqrt", ScriptOperator = "="))
	static float GetDistanceBetweenVectorsNoSquareRoot(const FVector From, const FVector To);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor Get Closest Actor", CompactNodeTitle = "Actor Closest Actor", BlueprintThreadSafe), Category="Math|Distance")
	static AActor* GetClosestActorFromActorArray(const AActor* OwnerActor ,UPARAM(ref) const TArray<AActor*>& TargetArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor  Get Closest Component", CompactNodeTitle = "Actor Closest Component", BlueprintThreadSafe), Category="Math|Distance")
	static void GetClosestComponentFromComponentArray(const AActor* OwnerActor ,UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Actor", CompactNodeTitle = "Component Closest Actor", BlueprintThreadSafe), Category="Math|Distance")
	static void ComponentGetClosestActorFromActorArray(const USceneComponent* OwnerComponent ,UPARAM(ref) const TArray<AActor*>& TargetArray, AActor*& Item);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Component", CompactNodeTitle = "Component Closest Component", BlueprintThreadSafe), Category="Math|Distance")
	static void ComponentGetClosestComponentFromComponentArray(const USceneComponent* OwnerComponent ,UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);

	
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DIRECTION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure)
	static FVector GetDirectionBetweenActors(const AActor* From , const AActor* To, float scaleVector = 1);
	
	UFUNCTION(BlueprintPure)
	static TEnumAsByte<EHitDirection> CalculateHitDirectionYaw (float Yaw , float& Angle);
	
	UFUNCTION(BlueprintPure)
	static TEnumAsByte<EHitDirection> CalculateHitDirectionActors (AActor* From, AActor* To, float& Angle);
	
	UFUNCTION(BlueprintPure)
	static void GetComponentForwardVectorPlus(USceneComponent* Component , float Distance , FVector& CurrentLocation , FVector& ForwardLocation);
	
	UFUNCTION(BlueprintPure)
	static void GetActorForwardVectorPlus(AActor* Actor , float Distance , FVector& CurrentLocation , FVector& ForwardLocation);

	/*
	**Calculates a forward location from given actors target location like a spine points 
	*/
	UFUNCTION(BlueprintCallable , Category = "Math Function Library")
	static FVector FCalculateDirectionalLocation(const FVector targetLocation , const FVector startPosition , float distance , bool forward	= true) ;

	/*
	** Trying to find a look at rotation with Dot Product , Completely opposite = 3 Completely Looking = 0
	*/
	UFUNCTION(BlueprintCallable , Category = "Math Function Library")
	static bool FCalculateIsLookingAt(const FVector actorForward, const FVector target, const FVector start, float& returnAngle, float limit = 1.7);

	
	UFUNCTION(BlueprintPure)
	static float FindLookAtRotationYaw(const FVector& Start, const FVector& Target);

	/*
	** Simple check if two directions similar with tolerance in mind. This is useful in the ai naw mesh connectors to know if ai wants to jump or slide
	*/
	UFUNCTION(BlueprintPure , Category = "Math Function Library")
	static bool FCalculateIsTheSameDirection(const FVector firstForwardDirection , const FVector secondForwardDirection , const float tolerance);

	/**
	 * 0: FRONT
	 * 1: BACK
	 * 2: LEFT
	 * 3: RIGHT
	 * -1: ERROR
	 **/
	UFUNCTION(BlueprintPure , Category = "Math Function Library")
	static uint8 CalculateDirectionBetweenActors(const AActor* Target , const AActor* From , const float ForwardTolerance = 0.5 );



	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SCREEN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor In The Center Of The Screen", CompactNodeTitle = "CenterActor", ScriptMethod = "CenterActor+", ScriptOperator = "+"), Category="Math|Screen")
	static AActor* GetActorInTheCenterOfTheScreen(TMap<AActor*,float> ActorScreenMap , bool& Found);
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Object Screen Position Clamped", CompactNodeTitle = "Screen Position", ScriptMethod = "Screen Position+", ScriptOperator = "=" , WorldContext = "WorldContextObject"))
	static FVector2D GetObjectScreenPositionClamped(UObject* WorldContextObject , FVector Position);




	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< LOCATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	/*
	Find a location close to center point in a circle with given min-max radius
	*/
	UFUNCTION(BlueprintPure , Category = "Math Function Library")
	static FVector FindRandomCircleLocation(float innerRadius , float outerRadius , FVector centerPont , FVector forwardVector);

	/*
	Find a location close to center point in a circle with given min-max radius. The purpose is not selecting a location behind the center point
	*/
	UFUNCTION(BlueprintPure , Category = "Math Function Library")
	static FVector FindRandomCircleLocationWithDirection(float innerRadius , float outerRadius , FVector centerPont , FVector targetPoint,float angle);




	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< PHYSICS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	/*
	**Calculates a launch velocity for characters with how long should they travel. The Z value and forward vector calculated based on duration
	*/
	UFUNCTION(BlueprintCallable , Category = "Math Function Library")
	static FVector FCalculateLaunchVelocity(const FVector targetLocation , const FVector startPosition ,const float duration);



	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< VARIABLE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "+1", CompactNodeTitle = "+1", ScriptMethod = "+", ScriptOperator = "+") , Category="Math|Float")
	static float FloatPlusOne(const float Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-1", CompactNodeTitle = "-1", ScriptMethod = "-", ScriptOperator = "-") , Category="Math|Float")
	static float FloatMinusOne(const float Value) {return Value - 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "*2", CompactNodeTitle = "*2", ScriptMethod = "*", ScriptOperator = "*") , Category="Math|Float")
	static float FloatMultiplyByTwo(const float Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "/2", CompactNodeTitle = "/2", ScriptMethod = "/", ScriptOperator = "/") , Category="Math|Float")
	static float FloatDivideByTwo(const float Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Invert", CompactNodeTitle = "Invert", ScriptMethod = "*", ScriptOperator = "*") , Category="Math|Float")
	static float FloatInvert(const float Value) {return Value*-1; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS", ScriptMethod = "-ABS", ScriptOperator = "-ABS") , Category="Math|Float")
	static float FloatMinusABS(const float Value) {return Value>0 ? Value*-1 : Value;  }

	

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "+1", CompactNodeTitle = "+1", ScriptMethod = "+", ScriptOperator = "+") , Category="Math|Int32")
	static int32 IntPlusOne(const int32 Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-1", CompactNodeTitle = "-1", ScriptMethod = "-", ScriptOperator = "-") , Category="Math|Int32")
	static int32 IntMinusOne(const int32 Value) {return Value - 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "*2", CompactNodeTitle = "*2", ScriptMethod = "*", ScriptOperator = "*") , Category="Math|Int32")
	static int32 IntMultiplyByTwo(const int32 Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "/2", CompactNodeTitle = "/2", ScriptMethod = "/", ScriptOperator = "/") , Category="Math|Int32")
	static int32 IntDivideByTwo(const int32 Value) {return Value + 1;}

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Invert", CompactNodeTitle = "Invert", ScriptMethod = "*", ScriptOperator = "*") , Category="Math|Int32")
	static int32 IntInvert(const int32 Value) {return Value*-1; }

	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS", ScriptMethod = "-ABS", ScriptOperator = "-ABS") , Category="Math|Int32")
	static int32 IntMinusABS(const int32 Value) {return Value>0 ? Value*-1 : Value;  }

	
	
};

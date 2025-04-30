// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMathLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Random Point In Sphere
	UFUNCTION(BlueprintPure , Category="ExendedFramework|Math|Random")
	static FVector RandPointInSphere(const FVector& Center, float Radius);
	
	
	// Returns a Rotator with given Yaw and Pitch values
	UFUNCTION(BlueprintPure , Category="ExendedFramework|Math|Rotation")
	static FRotator GetRotationBetweenActors(const AActor* From, const AActor* To, const FRotator PlusRotator = FRotator::ZeroRotator);

	// Returns a Rotator with given Yaw and Pitch values
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Rotation")
	static void GetAngleBetweenActors(AActor* From, AActor* To , float& Yaw , float& Pitch);

	// Returns a Rotator with given Yaw and Pitch values
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Rotation")
	static FRotator GetRotationBetweenVectors(const FVector& From, const FVector& To, const FRotator PlusRotator = FRotator::ZeroRotator);
	

	//Returns Distance Between Two Actors With Simple Minus Operation. Checks If Valid For References If Not Valid Returns 0
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Distance")
	static float GetDistanceBetweenActors(const AActor* From,const AActor* To);

	
	//Returns Distance Between Two Components With Simple Minus Operation. Checks If Valid For References If Not Valid Returns 0
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Components", CompactNodeTitle = "Distance") , Category="ExendedFramework|Math|Distance")
	static float GetDistanceBetweenComponents(const USceneComponent* From,const USceneComponent* To);

	
	//Returns Distance Between a Component and Actor With Simple Minus Operation. Checks If Valid For References If Not Valid Returns 0
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Component And Actor", CompactNodeTitle = "Distance") , Category="ExendedFramework|Math|Distance")
	static float GetDistanceBetweenComponentAndActor(const USceneComponent* From,const AActor* To);

	
	//Returns Distance Between Two Vectors With Simple Minus Operation
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Vectors", CompactNodeTitle = "Distance") , Category="ExendedFramework|Math|Distance")
	static float GetDistanceBetweenVectors(const FVector From, const FVector To);

	
	//Returns Distance Between Two Vectors With Simple Minus Operation But For Better Performance There is No Square Root Applied
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Get Distance Between Vectors No Square Root", CompactNodeTitle = "DistanceNoSqrt") , Category="ExendedFramework|Math|Distance")
	static float GetDistanceBetweenVectorsNoSquareRoot(const FVector From, const FVector To);

	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor Get Closest Actor", CompactNodeTitle = "Actor Closest Actor", BlueprintThreadSafe), Category="ExendedFramework|Math|Distance")
	static AActor* GetClosestActorFromActorArray(const AActor* OwnerActor ,UPARAM(ref) const TArray<AActor*>& TargetArray);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor  Get Closest Component", CompactNodeTitle = "Actor Closest Component", BlueprintThreadSafe), Category="ExendedFramework|Math|Distance")
	static void GetClosestComponentFromComponentArray(const AActor* OwnerActor ,UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Actor", CompactNodeTitle = "Component Closest Actor", BlueprintThreadSafe), Category="ExendedFramework|Math|Distance")
	static void ComponentGetClosestActorFromActorArray(const USceneComponent* OwnerComponent ,UPARAM(ref) const TArray<AActor*>& TargetArray, AActor*& Item);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Component", CompactNodeTitle = "Component Closest Component", BlueprintThreadSafe), Category="ExendedFramework|Math|Distance")
	static void ComponentGetClosestComponentFromComponentArray(const USceneComponent* OwnerComponent ,UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);


	

	// Get Direction Between Actors With Simple Minus Operation
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Direction")
	static FVector GetDirectionBetweenActors(const AActor* From , const AActor* To, float scaleVector = 1);

	
	// Get Direction Between Components With Simple Minus Operation
	static FVector GetDirectionBetweenComponents(const USceneComponent* From , const USceneComponent* To, float scaleVector = 1);

	
	// Return Component forward Vector with Multiplied Distance using Built In GetForwardVector Function
	UFUNCTION(BlueprintPure ,meta=( CompactNodeTitle = "CompForward+Distance"), Category="ExendedFramework|Math|Direction")
	static UPARAM(DisplayName = "ForwardLocation") FVector GetComponentForwardVectorPlus(USceneComponent* Component , float Distance , FVector& CurrentLocation);

	
	// Return Actor forward Vector with Multiplied Distance using Built In GetActorForwardVector Function
	UFUNCTION(BlueprintPure ,  meta= (CompactNodeTitle = "ActorForward+Distance"), meta=(DefaultToSelf = Actor) , Category="ExendedFramework|Math|Direction")
	static UPARAM(DisplayName = "ForwardLocation") FVector GetActorForwardVectorPlus(AActor* Actor , float Distance , FVector& CurrentLocation);

	
	// Return Actor forward Vector with Multiplied Distance and Added Rotation using Actor Rotation
	UFUNCTION(BlueprintPure ,meta= ( CompactNodeTitle = "ActorForward+Distance"), meta=(DefaultToSelf = Actor), Category="ExendedFramework|Math|Direction")
	static void GetActorForwardVectorPlusWithRotation(AActor* Actor , float Distance , FRotator PlusRotator , FVector&ForwardLocation , FVector& CurrentLocation , FVector& ForwardVector);

	
	// Return Component forward Vector with Multiplied Distance and Added Rotation using Component Rotation
	UFUNCTION(BlueprintPure, meta= (CompactNodeTitle = "CompForward+Distance" ),  Category="ExendedFramework|Math|Direction")
	static void GetComponentForwardVectorPlusWithRotation(USceneComponent* Component , float Distance , FRotator PlusRotator , FVector&ForwardLocation , FVector& CurrentLocation , FVector& ForwardVector );

	
	// Calculates a forward location from given actors target location like a spine points 
	UFUNCTION(BlueprintCallable , Category="ExendedFramework|Math|Direction")
	static FVector FCalculateDirectionalLocation(const FVector targetLocation , const FVector startPosition , float distance , bool forward	= true) ;


	// Trying to find a look at rotation with Dot Product , Completely opposite = 3 Completely Looking = 0
	UFUNCTION(BlueprintCallable , Category="ExendedFramework|Math|Direction")
	static bool FCalculateIsLookingAt(const FVector actorForward, const FVector target, const FVector start, float& returnAngle, float limit = 1.7);


	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Direction")
	static float FindLookAtRotationYaw(const FVector& Start, const FVector& Target);
	

	// Simple check if two directions similar with tolerance in mind
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Direction")
	static bool CalculateIsTheSameDirection(const FVector firstForwardDirection , const FVector secondForwardDirection , const float tolerance);

	
	/**
	 * 0: FRONT
	 * 1: BACK
	 * 2: LEFT
	 * 3: RIGHT
	 * -1: ERROR
	 **/
	UFUNCTION(BlueprintPure , Category="ExendedFramework|Math|Direction")
	static uint8 CalculateDirectionBetweenActors(const AActor* Target , const AActor* From , const float ForwardTolerance = 0.5 );

	
	/**
	 * 0: FRONT
	 * 1: BACK
	 * 2: RIGHT
	 * 3: LEFT
	 * 4: NONE
	 **/
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Direction")
	static uint8 GetControllerLookAtDirection(const APawn* Pawn);
	
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor In The Center Of The Screen", CompactNodeTitle = "CenterActor"), Category="ExendedFramework|Math|Screen")
	static AActor* GetActorInTheCenterOfTheScreen(TMap<AActor*,float> ActorScreenMap , const FVector2D ClampMinMax = FVector2D(0,1));

	//  Get the screen position of the object clamped to the screen
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Object Screen Position Clamped", CompactNodeTitle = "Screen Position", WorldContext = "WorldContextObject") , Category="ExendedFramework|Math|Screen")
	static FVector2D GetObjectScreenPositionClamped(UObject* WorldContextObject , FVector Position);


	
	// Find a location close to center point in a circle with given min-max radius
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Location")
	static FVector FindRandomCircleLocation(float innerRadius , float outerRadius , FVector centerPont , FVector forwardVector);


	// Find a location close to center point in a circle with given min-max radius. The purpose is not selecting a location behind the center point
	UFUNCTION(BlueprintPure, Category="ExendedFramework|Math|Location")
	static FVector FindRandomCircleLocationWithDirection(float innerRadius , float outerRadius , FVector centerPont , FVector targetPoint,float angle);

	
	//Calculates a launch velocity for characters with how long should they travel. The Z value and forward vector calculated based on duration
	UFUNCTION(BlueprintCallable , Category="ExendedFramework|Math|Physics")
	static FVector CalculateLaunchVelocity(const FVector targetLocation , const FVector startPosition ,const float duration);
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "+1", CompactNodeTitle = "+1") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatPlusOne(const float Value) {return Value + 1;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-1", CompactNodeTitle = "-1") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatMinusOne(const float Value) {return Value - 1;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "*2", CompactNodeTitle = "*2") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatMultiplyByTwo(const float Value) {return Value * 2 ;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "/2", CompactNodeTitle = "/2") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatDivideByTwo(const float Value) {return Value / 2 ;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Invert", CompactNodeTitle = "Invert") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatInvert(const float Value) {return Value * -1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS") , Category="ExendedFramework|Math|Variable|Float")
	static float FloatMinusABS(const float Value) {return Value>0 ? Value*-1 : Value;  }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "+1", CompactNodeTitle = "+1") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntPlusOne(const int32 Value) {return Value + 1;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-1", CompactNodeTitle = "-1") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntMinusOne(const int32 Value) {return Value - 1;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "*2", CompactNodeTitle = "*2") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntMultiplyByTwo(const int32 Value) {return Value * 2 ;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "/2", CompactNodeTitle = "/2") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntDivideByTwo(const int32 Value) {return Value / 2 ;}
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Invert", CompactNodeTitle = "Invert") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntInvert(const int32 Value) {return Value*-1; }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS") , Category="ExendedFramework|Math|Variable|Int32")
	static int32 IntMinusABS(const int32 Value) {return Value>0 ? Value*-1 : Value;  }
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "RotatorToQuad", CompactNodeTitle = "RotToQuad") , Category="ExendedFramework|Math")
	static FQuat RotatorToQuad(const FRotator Rotator);
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Clamp 2D"), Category="ExendedFramework|Math")
	static FVector2D ClampVector2D(const FVector2D Vector , const float Min , const float Max);
	
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Map Range Clamp 2D"), Category="ExendedFramework|Math")
	static FVector2D MapRangeClampVector2D(const FVector2D Value , const FVector2D InRangeA ,  const FVector2D InRangeB , const FVector2D OutRangeA , const FVector2D OutRangeB );
	
	// Calculates the speed and direction of the character based on the velocity of the character Direction is 360 degrees
	UFUNCTION(BlueprintPure ,meta=(DisplayName = "Calculate Speed And Direction") , Category="ExendedFramework|Math|Animation")
	static void CalculateSpeedAndDirection(const UAnimInstance* AnimInstance , float& Speed , float& Direction);
	
	// Returns a bool value using the Uniform method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Bool Uniform" ), Category="ExendedFramework|Math|Random" )
	static bool RandomBoolUniform();
	
	// Returns a bool value using the Bernoulli method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Bool with Bernoulli" ), Category="ExendedFramework|Math|Random" )
	static bool RandomBoolBernoulli( const float Bias = 0.5f );

	
	// Returns a bool value using the Bernoulli Twister method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Bool with Mersenne Twister" ), Category="ExendedFramework|Math|Random" )
	static bool RandomBoolMersenneTwister( const float Bias = 0.5f );
	
	// Returns a uint8 in the range 0 to X value using the Uniform method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Byte Uniform" ), Category="ExendedFramework|Math|Random" )
	static uint8 RandomByteUniform( const uint8 Max );
	
	// Returns a uint8 in the range 0 to 1 value using the Bernoulli method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Byte with Bernoulli" ), Category="ExendedFramework|Math|Random" )
	static uint8 RandomByteBernoulli( const float Bias = 0.5f );
	
	// Returns a uint8 in the range 0 to 1 value using the Bernoulli Twister method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Byte with Mersenne Twister" ), Category="ExendedFramework|Math|Random" )
	static uint8 RandomByteMersenneTwister( const float Bias = 0.5f );
	
	// Returns a int32 in the range 0 to X value using the Uniform method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Integer with Uniform" ), Category="ExendedFramework|Math|Random" )
	static int32 RandomIntUniform( const int32 Max );
	
	// Returns a int32 in the range 0 to 1 value using the Bernoulli method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Integer with Bernoulli" ), Category="ExendedFramework|Math|Random" )
	static int32 RandomIntBernoulli( const float Bias = 0.5f );
	
	// Returns a int32 in the range 0 to 1 value using the Bernoulli Twister method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Integer with Mersenne Twister" ), Category="ExendedFramework|Math|Random" )
	static int32 RandomIntMersenneTwister( const float Bias = 0.5f );
	
	// Returns a float in the range 0.0 to X.X value using the Uniform method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Float Uniform" ), Category="ExendedFramework|Math|Random" )
	static float RandomFloatUniform( const float Max );
	
	// Returns a float in the range 0.0 to 1.0 value using the Canonical method 
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Random Float with Canonical" ), Category="ExendedFramework|Math|Random" )
	static float RandomFloatCanonical();
	
	// Get Random Float Between Min and Max But Has A Limitations
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "RandFloat Min Max Range" ), Category="ExendedFramework|Math|Random" )
	static float RandomFloatRangeMinMax( const float Min , const float MinMax , const float Max , const float MaxMax );

	// Get Random Float with Positive or Negative One
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "RandFloat Positive Negative One" ), Category="ExendedFramework|Math|Random" )
	static float RandomFloatPositiveNegativeOne();
};
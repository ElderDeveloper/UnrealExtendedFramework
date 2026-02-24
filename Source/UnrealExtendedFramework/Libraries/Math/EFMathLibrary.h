// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMathLibrary.generated.h"

/**
 * Blueprint function library providing extended math utilities for rotation,
 * distance, direction, screen-space, location, physics, and random number generation.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Random >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns a random point uniformly distributed inside a sphere. */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Random")
	static FVector RandPointInSphere(const FVector& Center, float Radius);
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Rotation >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns a look-at rotation from one actor to another, with an optional additive rotator.
	 * @param From Source actor
	 * @param To Target actor
	 * @param PlusRotator Optional rotation offset to add
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Rotation")
	static FRotator GetRotationBetweenActors(const AActor* From, const AActor* To, const FRotator PlusRotator = FRotator::ZeroRotator);

	/**
	 * Returns the yaw and pitch angles between two actors (normalized delta rotation).
	 * @param From Source actor
	 * @param To Target actor
	 * @param Yaw Output yaw angle
	 * @param Pitch Output pitch angle
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Rotation")
	static void GetAngleBetweenActors(AActor* From, AActor* To, float& Yaw, float& Pitch);

	/**
	 * Returns a look-at rotation between two world positions, with an optional additive rotator.
	 * @param From Source position
	 * @param To Target position
	 * @param PlusRotator Optional rotation offset to add
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Rotation")
	static FRotator GetRotationBetweenVectors(const FVector& From, const FVector& To, const FRotator PlusRotator = FRotator::ZeroRotator);
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Distance >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns the distance between two actors. Returns 0 if either is invalid. */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Distance")
	static float GetDistanceBetweenActors(const AActor* From, const AActor* To);

	/** Returns the distance between two scene components. Returns 0 if either is null. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Distance Between Components", CompactNodeTitle = "Distance"), Category="ExtendedFramework|Math|Distance")
	static float GetDistanceBetweenComponents(const USceneComponent* From, const USceneComponent* To);

	/** Returns the distance between a component and an actor. Returns 0 if either is null. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Distance Between Component And Actor", CompactNodeTitle = "Distance"), Category="ExtendedFramework|Math|Distance")
	static float GetDistanceBetweenComponentAndActor(const USceneComponent* From, const AActor* To);

	/** Returns the euclidean distance between two vectors. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Distance Between Vectors", CompactNodeTitle = "Distance"), Category="ExtendedFramework|Math|Distance")
	static float GetDistanceBetweenVectors(const FVector From, const FVector To);

	/** Returns the squared distance between two vectors (no square root — faster for comparisons). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Distance Between Vectors No Square Root", CompactNodeTitle = "DistanceNoSqrt"), Category="ExtendedFramework|Math|Distance")
	static float GetDistanceBetweenVectorsNoSquareRoot(const FVector From, const FVector To);

	/** Returns the closest actor to OwnerActor from the given array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor Get Closest Actor", CompactNodeTitle = "Actor Closest Actor"), Category="ExtendedFramework|Math|Distance")
	static AActor* GetClosestActorFromActorArray(const AActor* OwnerActor, UPARAM(ref) const TArray<AActor*>& TargetArray);

	/** Returns the closest component to OwnerActor from the given array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Actor Get Closest Component", CompactNodeTitle = "Actor Closest Component"), Category="ExtendedFramework|Math|Distance")
	static void GetClosestComponentFromComponentArray(const AActor* OwnerActor, UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);

	/** Returns the closest actor to OwnerComponent from the given array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Actor", CompactNodeTitle = "Component Closest Actor"), Category="ExtendedFramework|Math|Distance")
	static void ComponentGetClosestActorFromActorArray(const USceneComponent* OwnerComponent, UPARAM(ref) const TArray<AActor*>& TargetArray, AActor*& Item);

	/** Returns the closest component to OwnerComponent from the given array. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Component Get Closest Component", CompactNodeTitle = "Component Closest Component"), Category="ExtendedFramework|Math|Distance")
	static void ComponentGetClosestComponentFromComponentArray(const USceneComponent* OwnerComponent, UPARAM(ref) const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Direction >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the normalized direction from one actor to another, optionally scaled.
	 * @param scaleVector Multiplier for the direction vector (default 1)
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static FVector GetDirectionBetweenActors(const AActor* From, const AActor* To, float scaleVector = 1);

	/**
	 * Returns the normalized direction from one component to another, optionally scaled.
	 * @param scaleVector Multiplier for the direction vector (default 1)
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static FVector GetDirectionBetweenComponents(const USceneComponent* From, const USceneComponent* To, float scaleVector = 1);

	/**
	 * Returns a world position at the given distance along a component's forward vector.
	 * @param Component The source component
	 * @param Distance How far ahead along the forward vector
	 * @param CurrentLocation Output: the component's current world position
	 */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "CompForward+Distance"), Category="ExtendedFramework|Math|Direction")
	static UPARAM(DisplayName = "ForwardLocation") FVector GetComponentForwardVectorPlus(USceneComponent* Component, float Distance, FVector& CurrentLocation);

	/**
	 * Returns a world position at the given distance along an actor's forward vector.
	 * @param Actor The source actor
	 * @param Distance How far ahead along the forward vector
	 * @param CurrentLocation Output: the actor's current world position
	 */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "ActorForward+Distance", DefaultToSelf = Actor), Category="ExtendedFramework|Math|Direction")
	static UPARAM(DisplayName = "ForwardLocation") FVector GetActorForwardVectorPlus(AActor* Actor, float Distance, FVector& CurrentLocation);

	/** Returns a world position along an actor's rotated forward vector (rotation + offset). */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "ActorForward+Distance", DefaultToSelf = Actor), Category="ExtendedFramework|Math|Direction")
	static void GetActorForwardVectorPlusWithRotation(AActor* Actor, float Distance, FRotator PlusRotator, FVector& ForwardLocation, FVector& CurrentLocation, FVector& ForwardVector);

	/** Returns a world position along a component's rotated forward vector (rotation + offset). */
	UFUNCTION(BlueprintPure, meta=(CompactNodeTitle = "CompForward+Distance"), Category="ExtendedFramework|Math|Direction")
	static void GetComponentForwardVectorPlusWithRotation(USceneComponent* Component, float Distance, FRotator PlusRotator, FVector& ForwardLocation, FVector& CurrentLocation, FVector& ForwardVector);

	/**
	 * Calculates a directional location (like a chain/spine point) from target to start.
	 * @param forward If true, project forward; if false, project backward
	 */
	UFUNCTION(BlueprintCallable, Category="ExtendedFramework|Math|Direction")
	static FVector FCalculateDirectionalLocation(const FVector targetLocation, const FVector startPosition, float distance, bool forward = true);

	/**
	 * Tests if an actor is "looking at" a target using dot product angle.
	 * Completely opposite = ~3.14, completely looking = ~0.
	 * @param returnAngle Output angle in radians
	 * @param limit Angle threshold (default 1.7 radians)
	 */
	UFUNCTION(BlueprintCallable, Category="ExtendedFramework|Math|Direction")
	static bool FCalculateIsLookingAt(const FVector actorForward, const FVector target, const FVector start, float& returnAngle, float limit = 1.7);

	/** Returns just the Yaw component of a look-at rotation between two points. */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static float FindLookAtRotationYaw(const FVector& Start, const FVector& Target);

	/**
	 * Checks if two direction vectors are similar within a tolerance.
	 * @param tolerance Comparison tolerance for the normalized vectors
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static bool CalculateIsTheSameDirection(const FVector firstForwardDirection, const FVector secondForwardDirection, const float tolerance);

	/**
	 * Determines the relative direction (FRONT/BACK/LEFT/RIGHT) of one actor to another.
	 * @return 0=FRONT, 1=BACK, 2=LEFT, 3=RIGHT, 4=ERROR
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static uint8 CalculateDirectionBetweenActors(const AActor* Target, const AActor* From, const float ForwardTolerance = 0.5);

	/**
	 * Determines the direction the controller is looking relative to the pawn.
	 * @return 0=FRONT, 1=BACK, 2=RIGHT, 3=LEFT, 4=NONE
	 */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Direction")
	static uint8 GetControllerLookAtDirection(const APawn* Pawn);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Screen >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Returns the actor closest to the center of the screen from a map of (Actor, ScreenX%).
	 * @param ActorScreenMap Map of actors to their horizontal screen percentage
	 * @param ClampMinMax Min/Max range to filter valid screen positions
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor In The Center Of The Screen", CompactNodeTitle = "CenterActor"), Category="ExtendedFramework|Math|Screen")
	static AActor* GetActorInTheCenterOfTheScreen(TMap<AActor*, float> ActorScreenMap, const FVector2D ClampMinMax = FVector2D(0, 1));

	/**
	 * Returns the screen position of a world location, clamped to 0-1 range.
	 * @param Position World position to project
	 * @return Normalized screen coordinates (0,0 = top-left, 1,1 = bottom-right)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Object Screen Position Clamped", CompactNodeTitle = "Screen Position", WorldContext = "WorldContextObject"), Category="ExtendedFramework|Math|Screen")
	static FVector2D GetObjectScreenPositionClamped(UObject* WorldContextObject, FVector Position);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Location >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns a random point in an annulus (ring) around a center, using forwardVector as reference. */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Location")
	static FVector FindRandomCircleLocation(float innerRadius, float outerRadius, FVector centerPoint, FVector forwardVector);

	/** Returns a random point in an annulus, biased toward a target direction within a given angle arc. */
	UFUNCTION(BlueprintPure, Category="ExtendedFramework|Math|Location")
	static FVector FindRandomCircleLocationWithDirection(float innerRadius, float outerRadius, FVector centerPoint, FVector targetPoint, float angle);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Physics >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Calculates a launch velocity to travel from start to target over a given duration.
	 * Accounts for gravity (980 cm/s²). Returns ZeroVector if duration <= 0.
	 * @param targetLocation Desired landing position
	 * @param startPosition Launch origin
	 * @param duration Travel time in seconds
	 */
	UFUNCTION(BlueprintCallable, Category="ExtendedFramework|Math|Physics")
	static FVector CalculateLaunchVelocity(const FVector targetLocation, const FVector startPosition, const float duration);
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Quick Math >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns Value + 1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "+1", CompactNodeTitle = "+1"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatPlusOne(const float Value) { return Value + 1; }
	
	/** Returns Value - 1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "-1", CompactNodeTitle = "-1"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatMinusOne(const float Value) { return Value - 1; }
	
	/** Returns Value * 2. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "*2", CompactNodeTitle = "*2"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatMultiplyByTwo(const float Value) { return Value * 2; }
	
	/** Returns Value / 2. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "/2", CompactNodeTitle = "/2"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatDivideByTwo(const float Value) { return Value / 2; }
	
	/** Returns Value * -1 (negation). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Invert", CompactNodeTitle = "Invert"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatInvert(const float Value) { return Value * -1; }
	
	/** Returns the value as negative (if positive, negates; if already negative, unchanged). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS"), Category="ExtendedFramework|Math|Variable|Float")
	static float FloatMinusABS(const float Value) { return Value > 0 ? Value * -1 : Value; }
	
	/** Returns Value + 1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "+1", CompactNodeTitle = "+1"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntPlusOne(const int32 Value) { return Value + 1; }
	
	/** Returns Value - 1. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "-1", CompactNodeTitle = "-1"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntMinusOne(const int32 Value) { return Value - 1; }
	
	/** Returns Value * 2. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "*2", CompactNodeTitle = "*2"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntMultiplyByTwo(const int32 Value) { return Value * 2; }
	
	/** Returns Value / 2 (integer division). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "/2", CompactNodeTitle = "/2"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntDivideByTwo(const int32 Value) { return Value / 2; }
	
	/** Returns Value * -1 (negation). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Invert", CompactNodeTitle = "Invert"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntInvert(const int32 Value) { return Value * -1; }
	
	/** Returns the value as negative (if positive, negates; if already negative, unchanged). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "-ABS", CompactNodeTitle = "-ABS"), Category="ExtendedFramework|Math|Variable|Int32")
	static int32 IntMinusABS(const int32 Value) { return Value > 0 ? Value * -1 : Value; }
	

	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Conversion / Utility >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Converts a rotator to a quaternion using the engine's built-in conversion.
	 * @note DisplayName kept as "RotatorToQuad" for backward compatibility.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RotatorToQuad", CompactNodeTitle = "RotToQuat"), Category="ExtendedFramework|Math")
	static FQuat RotatorToQuat(const FRotator Rotator);
	
	/** Clamps each component of a Vector2D independently to [Min, Max]. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp 2D"), Category="ExtendedFramework|Math")
	static FVector2D ClampVector2D(const FVector2D Vector, const float Min, const float Max);
	
	/** Applies MapRangeClamped independently to X and Y of a Vector2D. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Map Range Clamp 2D"), Category="ExtendedFramework|Math")
	static FVector2D MapRangeClampVector2D(const FVector2D Value, const FVector2D InRangeA, const FVector2D InRangeB, const FVector2D OutRangeA, const FVector2D OutRangeB);
	
	/**
	 * Calculates movement speed and direction from an AnimInstance's owning pawn.
	 * Direction is in degrees (-180 to 180).
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Calculate Speed And Direction"), Category="ExtendedFramework|Math|Animation")
	static void CalculateSpeedAndDirection(const UAnimInstance* AnimInstance, float& Speed, float& Direction);

	/**
	 * Clamps a vector's length to a maximum radius. If shorter, returns unchanged.
	 * @param Value The input vector
	 * @param Radius Maximum allowed length
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Clamp To Radius", CompactNodeTitle = "ClampRadius"), Category="ExtendedFramework|Math")
	static FVector ClampVectorToRadius(const FVector Value, const float Radius);

	/**
	 * Returns where Value falls between A and B as a 0-1 ratio (unclamped).
	 * InverseLerp(0, 10, 5) = 0.5, InverseLerp(0, 10, 15) = 1.5
	 * @param A Range start
	 * @param B Range end
	 * @param Value The value to evaluate
	 * @return The interpolation ratio (can be outside 0-1 if Value is outside A-B)
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Inverse Lerp", CompactNodeTitle = "InvLerp"), Category="ExtendedFramework|Math")
	static float InverseLerp(const float A, const float B, const float Value);


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Random (Advanced) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/** Returns a random bool using std::uniform_int_distribution. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Bool Uniform"), Category="ExtendedFramework|Math|Random")
	static bool RandomBoolUniform();
	
	/** Returns a random bool using std::bernoulli_distribution with the given bias (0-1). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Bool with Bernoulli"), Category="ExtendedFramework|Math|Random")
	static bool RandomBoolBernoulli(const float Bias = 0.5f);

	/** Returns a random bool using Mersenne Twister with the given bias (0-1). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Bool with Mersenne Twister"), Category="ExtendedFramework|Math|Random")
	static bool RandomBoolMersenneTwister(const float Bias = 0.5f);
	
	/** Returns a random byte in [0, Max] using uniform distribution. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Byte Uniform"), Category="ExtendedFramework|Math|Random")
	static uint8 RandomByteUniform(const uint8 Max);
	
	/** Returns a random byte (0 or 1) using Bernoulli distribution with the given bias. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Byte with Bernoulli"), Category="ExtendedFramework|Math|Random")
	static uint8 RandomByteBernoulli(const float Bias = 0.5f);
	
	/** Returns a random byte (0 or 1) using Mersenne Twister with the given bias. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Byte with Mersenne Twister"), Category="ExtendedFramework|Math|Random")
	static uint8 RandomByteMersenneTwister(const float Bias = 0.5f);
	
	/** Returns a random int32 in [0, Max] using uniform distribution. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Integer with Uniform"), Category="ExtendedFramework|Math|Random")
	static int32 RandomIntUniform(const int32 Max);
	
	/** Returns a random int32 (0 or 1) using Bernoulli distribution with the given bias. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Integer with Bernoulli"), Category="ExtendedFramework|Math|Random")
	static int32 RandomIntBernoulli(const float Bias = 0.5f);
	
	/** Returns a random int32 (0 or 1) using Mersenne Twister with the given bias. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Integer with Mersenne Twister"), Category="ExtendedFramework|Math|Random")
	static int32 RandomIntMersenneTwister(const float Bias = 0.5f);
	
	/** Returns a random float in [0, Max] using uniform distribution. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Float Uniform"), Category="ExtendedFramework|Math|Random")
	static float RandomFloatUniform(const float Max);
	
	/** Returns a random float in [0, 1) using std::generate_canonical. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Random Float with Canonical"), Category="ExtendedFramework|Math|Random")
	static float RandomFloatCanonical();
	
	/**
	 * Returns a random float from one of two sub-ranges: [Min, MinMax] or [Max, MaxMax].
	 * Randomly picks which sub-range to sample from (50/50).
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RandFloat Min Max Range"), Category="ExtendedFramework|Math|Random")
	static float RandomFloatRangeMinMax(const float Min, const float MinMax, const float Max, const float MaxMax);

	/** Returns either -1.0 or 1.0 randomly. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "RandFloat Positive Negative One"), Category="ExtendedFramework|Math|Random")
	static float RandomFloatPositiveNegativeOne();


	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Mapping >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	/**
	 * Maps a value from one range to another, clamping the result to [OutMin, OutMax].
	 * @param Value The input value to remap
	 * @param InMin Input range minimum
	 * @param InMax Input range maximum
	 * @param OutMin Output range minimum
	 * @param OutMax Output range maximum
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Map Range Clamped"), Category="ExtendedFramework|Math|Mapping")
	static float MapRangeClamped(float Value, float InMin, float InMax, float OutMin, float OutMax);

	/**
	 * Maps a value from one range to another without clamping.
	 * The result can exceed [OutMin, OutMax] if Value is outside [InMin, InMax].
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Map Range Unclamped"), Category="ExtendedFramework|Math|Mapping")
	static float MapRangeUnclamped(float Value, float InMin, float InMax, float OutMin, float OutMax);
};
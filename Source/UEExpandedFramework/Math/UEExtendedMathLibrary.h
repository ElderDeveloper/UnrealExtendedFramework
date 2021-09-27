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




	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SCREEN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor In The Center Of The Screen", CompactNodeTitle = "CenterActor", ScriptMethod = "CenterActor+", ScriptOperator = "+"), Category="Math|Screen")
	static AActor* GetActorInTheCenterOfTheScreen(TMap<AActor*,float> ActorScreenMap , bool& Found);
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Object Screen Position Clamped", CompactNodeTitle = "Screen Position", ScriptMethod = "Screen Position+", ScriptOperator = "=" , WorldContext = "WorldContextObject"))
	static FVector2D GetObjectScreenPositionClamped(UObject* WorldContextObject , FVector Position);
	
};

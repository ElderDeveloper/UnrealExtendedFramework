#pragma once

#include "CoreMinimal.h"

#include "EGActorLibrary.generated.h"

/**
 * 
 */
UCLASS()
class  UEGActorLibrary : public UBlueprintFunctionLibrary
{
  GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location X", CompactNodeTitle = "(Self)LocationX"  , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationX(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Y", CompactNodeTitle = "(Self)LocationY"  , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationY(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Location Z", CompactNodeTitle = "(Self)LocationZ"  , DefaultToSelf = "Actor") , Category="Actor|Location")
	static float GetActorLocationZ(AActor* Actor);


	

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Yaw", CompactNodeTitle = "(Self)RotationYaw" , DefaultToSelf = "Actor") , Category="Actor|Rotation")
	static float GetActorRotationYaw(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Pitch", CompactNodeTitle = "(Self)RotationPitch"  , DefaultToSelf = "Actor") , Category="Actor|Rotation")
	static float GetActorRotationPitch(AActor* Actor);

	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Actor Rotation Roll", CompactNodeTitle = "(Self)RotationRoll" , DefaultToSelf = "Actor" ) , Category="Actor|Rotation")
	static float GetActorRotationRoll(AActor* Actor);




	UFUNCTION(BlueprintCallable)
	static void RotateToObjectYaw(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable)
	static void RotateToObject(AActor* From , AActor* To);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void RotateToObjectInterp(const UObject* WorldContextObject, AActor* From , AActor* To , float InterpSpeed);
	
};
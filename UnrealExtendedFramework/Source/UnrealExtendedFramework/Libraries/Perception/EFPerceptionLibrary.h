// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionTypes.h"
#include "EFPerceptionLibrary.generated.h"


class UAIPerceptionComponent;
class UAISenseConfig;
class UAISense;

/**
 * Blueprint function library for runtime modification of AI Perception Component
 * sense configurations (Sight, Hearing). Provides getters and setters for
 * sense parameters that are normally only editable in the editor.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFPerceptionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Returns the sense config for the specified sense class on a perception component.
	 * @param Perception The AI perception component
	 * @param SenseClass The sense class to look up
	 * @return The sense config, or nullptr if not found
	 */
	static UAISenseConfig* GetPerceptionSenseConfig(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);
	
	/**
	 * Forces the perception component to forget a specific actor.
	 * @param Perception The AI perception component
	 * @param Actor The actor to forget
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool ForgetActor(UAIPerceptionComponent* Perception, AActor* Actor);

	/**
	 * Forces the perception component to forget all perceived actors.
	 * @param Perception The AI perception component
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool ForgetAll(UAIPerceptionComponent* Perception);

	/**
	 * Returns the dominant sense class of the perception component.
	 * @param Perception The AI perception component
	 * @return The dominant sense class, or nullptr if unavailable
	 */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static TSubclassOf<UAISense> GetDominantSense(UAIPerceptionComponent* Perception);

	/**
	 * Sets the dominant sense for the perception component.
	 * @param Perception The AI perception component
	 * @param SenseClass The sense class to set as dominant
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetDominantSense(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	/**
	 * Returns the detection-by-affiliation filter for the specified sense.
	 * Supports Sight and Hearing sense types.
	 * @param Perception The AI perception component
	 * @param SenseClass The sense class to query
	 */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static FAISenseAffiliationFilter GetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	/**
	 * Sets the detection-by-affiliation filter for the specified sense.
	 * Supports Sight and Hearing sense types.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, bool DetectEnemies, bool DetectNeutrals, bool DetectFriendlies);

	/**
	 * Returns the MaxAge for the specified sense config.
	 * @return MaxAge in seconds, or -1 on failure
	 */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	/**
	 * Sets the MaxAge for the specified sense config.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, float MaxAge);

	/** Returns the current Sight Radius. Returns 0 on failure. */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetSightRange(UAIPerceptionComponent* Perception);

	/**
	 * Sets a new Sight Radius, automatically adjusting LoseSightRadius
	 * to maintain the same offset. Calls RequestStimuliListenerUpdate.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetSightRange(UAIPerceptionComponent* Perception, float SightRange);

	/** Returns the current Lose Sight Radius. Returns 0 on failure. */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetLoseSightRange(UAIPerceptionComponent* Perception);

	/**
	 * Sets a new Lose Sight Radius. Clamped to be >= SightRadius.
	 * Calls RequestStimuliListenerUpdate.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetLoseSightRange(UAIPerceptionComponent* Perception, float LoseSightRange);

	/** Returns the full peripheral vision angle in degrees (diameter). Returns 0 on failure. */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetVisionAngle(UAIPerceptionComponent* Perception);

	/**
	 * Sets the peripheral vision angle in degrees (full cone, divided by 2 internally).
	 * Calls RequestStimuliListenerUpdate.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetVisionAngle(UAIPerceptionComponent* Perception, float VisionAngle);

	/** Returns the current Hearing Range. Returns 0 on failure. */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetHearingRange(UAIPerceptionComponent* Perception);

	/**
	 * Sets a new Hearing Range. Calls RequestStimuliListenerUpdate.
	 * @return True on success
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetHearingRange(UAIPerceptionComponent* Perception, float HearingRange);

	/** Returns the current Line-of-Sight Hearing Range. Returns 0 on failure. @deprecated LoSHearingRange is deprecated since UE 5.2. */
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception", meta=(DeprecatedFunction, DeprecationMessage="LoSHearingRange is deprecated since UE 5.2. Use HearingRange instead."))
	static float GetLoSHearingRange(UAIPerceptionComponent* Perception);

	/**
	 * Sets a new Line-of-Sight Hearing Range. Calls RequestStimuliListenerUpdate.
	 * @return True on success
	 * @deprecated LoSHearingRange is deprecated since UE 5.2.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception", meta=(DeprecatedFunction, DeprecationMessage="LoSHearingRange is deprecated since UE 5.2. Use HearingRange instead."))
	static bool SetLoSHearingRange(UAIPerceptionComponent* Perception, float LoSHearingRange);
};

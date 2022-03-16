// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionTypes.h"
#include "EFPerceptionLibrary.generated.h"

/**
 * 
 */
class UAIPerceptionComponent;
class UAISenseConfig;
class UAISense;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFPerceptionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	static UAISenseConfig* GetPerceptionSenseConfig(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);
	
	//Force the AI Perception Component to forget a specific perceived Actor.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool ForgetActor(UAIPerceptionComponent* Perception, AActor* Actor);

	
	//Force the AI Perception Component to forget all perceived Actors.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool ForgetAll(UAIPerceptionComponent* Perception);

	
	//Get the current Dominant Sense of the AI Perception Component.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static TSubclassOf<UAISense> GetDominantSense(UAIPerceptionComponent* Perception);

	
	//Set a new Dominant Sense for the AI Perception Component.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetDominantSense(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	
	//Get the Detection By Affiliation values for the chosen Sense of the AI Perception Component.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static FAISenseAffiliationFilter GetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	
	//Set the Detection By Affiliation values for the chosen Sense of the AI Perception Component.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetDetectionByAffiliation(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, bool DetectEnemies, bool DetectNeutrals, bool DetectFriendlies);

	
	//Get the Max Age for the chosen Sense of the AI Perception Component.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass);

	
	//Set the Max Age for the chosen Sense of the AI Perception Component.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetMaxAge(UAIPerceptionComponent* Perception, TSubclassOf<UAISense> SenseClass, float MaxAge);

	
	//Get the current Sight Radius of the AI Perception Component's Sight Config.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetSightRange(UAIPerceptionComponent* Perception);

	
	//Set a new Sight Radius for the AI Perception Component's Sight Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetSightRange(UAIPerceptionComponent* Perception, float SightRange);

	
	//Get the current Lose Sight Radius of the AI Perception Component's Sight Config
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetLoseSightRange(UAIPerceptionComponent* Perception);

	
	//Set a new Lose Sight Radius for the AI Perception Component's Sight Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetLoseSightRange(UAIPerceptionComponent* Perception, float LoseSightRange);

	
	//Get the current Peripheral Vision Angle of the AI Perception Component's Sight Config.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetVisionAngle(UAIPerceptionComponent* Perception);

	
	//Set a new Peripheral Vision Angle for the AI Perception Component's Sight Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetVisionAngle(UAIPerceptionComponent* Perception, float VisionAngle);

	
	//Get the current Hearing Range of the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetHearingRange(UAIPerceptionComponent* Perception);

	
	//Set a new Hearing Range for the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetHearingRange(UAIPerceptionComponent* Perception, float HearingRange);

	
	//Get the current value of Use LoS Hearing of the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static bool GetUseLoSHearing(UAIPerceptionComponent* Perception);

	
	//Set a new Use LoS Hearing value for the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetUseLoSHearing(UAIPerceptionComponent* Perception, bool UseLoSHearing);

	
	//Get the current LoS Hearing Range of the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintPure, Category = "ExtendedFramework|AI|Perception")
	static float GetLoSHearingRange(UAIPerceptionComponent* Perception);

	
	//Set a new LoS Hearing Range for the AI Perception Component's Hearing Config.
	UFUNCTION(BlueprintCallable, Category = "ExtendedFramework|AI|Perception")
	static bool SetLoSHearingRange(UAIPerceptionComponent* Perception, float LoSHearingRange);
};

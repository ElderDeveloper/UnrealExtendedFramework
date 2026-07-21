// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EGPawnSensingActor.generated.h"

class UAIPerceptionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSeePawn, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHearNoise, APawn*, Instigator, const FVector&, Location, float, Volume);


UCLASS(Blueprintable , BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API AEGPawnSensingActor : public APawn
{
	GENERATED_BODY()

public:
	AEGPawnSensingActor();

	UPROPERTY(BlueprintReadOnly , VisibleDefaultsOnly , Category = "PawnSensing")
	UAIPerceptionComponent* PawnSensingComponent;
	
	UPROPERTY(BlueprintAssignable , Category = "PawnSensing")
	FOnSeePawn OnSeePawn;

	UPROPERTY(BlueprintAssignable , Category = "PawnSensing")
	FOnHearNoise OnHearNoise;
	
	UFUNCTION()
	void OnReceiveSeePawn(AActor* Actor,FAIStimulus Stimulus);
	UFUNCTION()
	void OnReceiveHearNoise(APawn* HearInstigator, const FVector& Location, float Volume);

	UFUNCTION()
	void PawnSensingComponentUpdated(const TArray<AActor*>& UpdatedActors);
	
	
	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float HearingThreshold;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float LOSHearingThreshold;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float SightRadius;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float SensingInterval;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float HearingMaxSoundAge;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	float PeripheralVisionAngle;
	
	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	bool EnableSensingUpdates;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	bool OnlySensePlayers;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	bool bSeePawns;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "PawnSensing")
	bool bHearNoises;
	
	UFUNCTION(BlueprintCallable, Category = "PawnSensing")
	void UpdatePawnSensingComponent();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "EGPerceptionStimuliSource.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGPerceptionStimuliSource : public UAIPerceptionStimuliSourceComponent
{
	GENERATED_BODY()

public:

	UEGPerceptionStimuliSource(const FObjectInitializer& ObjectInitializer);

protected:

	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer ActorFaction;

	UPROPERTY(EditDefaultsOnly ,	meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float ActorDetectionStrength = 1;

	
public:

	UFUNCTION(BlueprintCallable)
	void SetActorDetectionStrength(float DetectionStrength) { ActorDetectionStrength = FMath::Clamp(DetectionStrength , (float)0.0 , (float)1.0); }

	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetActorDetectionStrength() const { return ActorDetectionStrength; }


	
	UFUNCTION(BlueprintPure , Category="ExtendedGameplay|Perception|Stimuli")
	FORCEINLINE FGameplayTagContainer GetActorFaction() const { return ActorFaction; }

	UFUNCTION(BlueprintPure , Category="ExtendedGameplay|Perception|Stimuli")
	FORCEINLINE bool GetIsHostile(FGameplayTagContainer EnemyTags) const { return ActorFaction.HasAnyExact(EnemyTags); }

	UFUNCTION(BlueprintPure , Category="ExtendedGameplay|Perception|Stimuli")
	FORCEINLINE bool GetIsAlly(FGameplayTagContainer AllyTags) const { return ActorFaction.HasAnyExact(AllyTags); }


};

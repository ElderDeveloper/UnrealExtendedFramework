// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGPerceptionComponent.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionComponent.h"
#include "EGPerceptionComponentHear.generated.h"


class UAISenseConfig_Hearing;




UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGPerceptionComponentHear : public UEGPerceptionComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGPerceptionComponentHear(const FObjectInitializer& ObjectInitializer);

protected:
	
	UPROPERTY()
	UAISenseConfig_Hearing* HearConfig;
	
};

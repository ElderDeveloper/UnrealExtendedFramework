// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPerceptionComponentHear.h"

#include "Perception/AISenseConfig_Hearing.h"
#include "UnrealExtendedGameplay/AI/StimuliSource/EGPerceptionStimuliSource.h"


// Sets default values for this component's properties
UEGPerceptionComponentHear::UEGPerceptionComponentHear(const FObjectInitializer& ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	HearConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearConfig"));
	HearConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearConfig->DetectionByAffiliation.bDetectFriendlies = true;

	ConfigureSense(*HearConfig);
	SetDominantSense(HearConfig->GetSenseImplementation());
}


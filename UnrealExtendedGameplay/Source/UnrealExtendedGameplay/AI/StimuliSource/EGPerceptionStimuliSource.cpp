// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPerceptionStimuliSource.h"

#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"


// Sets default values for this component's properties
UEGPerceptionStimuliSource::UEGPerceptionStimuliSource(const FObjectInitializer& ObjectInitializer) : UAIPerceptionStimuliSourceComponent(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	bAutoRegisterAsSource = true;

	RegisterAsSourceForSenses.Add(UAISense_Hearing::StaticClass());
	RegisterAsSourceForSenses.Add(UAISense_Sight::StaticClass());
	RegisterAsSourceForSenses.Add(UAISense_Damage::StaticClass());

}



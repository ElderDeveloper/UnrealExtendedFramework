// Fill out your copyright notice in the Description page of Project Settings.


#include "EGPawnSensingActor.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/PawnSensingComponent.h"


// Sets default values for this component's properties
AEGPawnSensingActor::AEGPawnSensingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PawnSensingComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("ProjectPawnSenseComponent"));

	PawnSensingComponent->OnPerceptionUpdated.AddDynamic(this, &AEGPawnSensingActor::PawnSensingComponentUpdated);
	PawnSensingComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AEGPawnSensingActor::OnReceiveSeePawn);
	
	HearingThreshold = 1400;
	LOSHearingThreshold = 2800;
	SightRadius = 5000;
	SensingInterval = 0.5;
	HearingMaxSoundAge = 1;
	PeripheralVisionAngle = 90;
	EnableSensingUpdates = true;
	OnlySensePlayers = true;
	bSeePawns = true;
	bHearNoises = true;
}

void AEGPawnSensingActor::OnReceiveSeePawn(AActor* Actor,FAIStimulus Stimulus)
{
	OnSeePawn.Broadcast(Cast<APawn>(Actor));
}

void AEGPawnSensingActor::OnReceiveHearNoise(APawn* HearInstigator, const FVector& Location, float Volume)
{
	OnHearNoise.Broadcast(HearInstigator, Location, Volume);
}

void AEGPawnSensingActor::PawnSensingComponentUpdated(const TArray<AActor*>& UpdatedActors)
{
}

void AEGPawnSensingActor::UpdatePawnSensingComponent()
{
	/*
	PawnSensingComponent->HearingThreshold = HearingThreshold;
	PawnSensingComponent->LOSHearingThreshold = LOSHearingThreshold;
	PawnSensingComponent->SightRadius = SightRadius;
	PawnSensingComponent->SensingInterval = SensingInterval;
	PawnSensingComponent->HearingMaxSoundAge = HearingMaxSoundAge;
	PawnSensingComponent->SetPeripheralVisionAngle(PeripheralVisionAngle);
	PawnSensingComponent->bEnableSensingUpdates = EnableSensingUpdates;
	PawnSensingComponent->bOnlySensePlayers = OnlySensePlayers;
	PawnSensingComponent->bSeePawns = bSeePawns;
	PawnSensingComponent->bHearNoises = bHearNoises;
	*/
}


﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedAreaDamage.h"

#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AUEExtendedAreaDamage::AUEExtendedAreaDamage()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}


void AUEExtendedAreaDamage::SpawnEffects(USoundBase* SoundBase, UParticleSystem* ParticleSystem,UNiagaraSystem* NiagaraEmitter) const
{
	if (SoundBase)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(),SoundBase,GetActorLocation());
	if (ParticleSystem)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),ParticleSystem,GetActorLocation(),GetActorRotation());
	if (NiagaraEmitter)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),NiagaraEmitter,GetActorLocation(),GetActorRotation());
}

void AUEExtendedAreaDamage::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(1.f);
}



void AUEExtendedAreaDamage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetGameTimeSinceCreation() > Lifetime)
	{
		if (GetOwner())
		{
			SpawnEffects(DamageSoundEnd,DamageEffectEnd,DamageNiagaraEnd);
		}
		Destroy();
	}
	
}
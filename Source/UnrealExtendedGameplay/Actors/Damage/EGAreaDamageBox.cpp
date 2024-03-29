﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EGAreaDamageBox.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AEGAreaDamageBox::AEGAreaDamageBox()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	RootComponent = BoxComponent;
}

void AEGAreaDamageBox::InitializeDamage(float damageAmount, FVector damageExtend, bool isLooping, float loopTime,float lifetime, TSubclassOf<UDamageType> damageType)
{
	if (GetOwner())
	{
		SetLifeSpan(lifetime + 0.1);
		BoxComponent->SetBoxExtent(damageExtend,true);
		DamageAmount = damageAmount;
		DamageType = damageType;
		Lifetime = lifetime;
	
		if (isLooping)
		{
			SpawnEffects(DamageSoundStart,DamageEffectStart,DamageNiagaraStart);
			GetWorldTimerManager().SetTimer(LoopHandle,this,&AEGAreaDamageBox::ApplyAreaDamage,loopTime,isLooping);
			ApplyAreaDamage();
		}
		else
		{
			SpawnEffects(DamageSoundStart,DamageEffectStart,DamageNiagaraStart);
			ApplyAreaDamage();
			Destroy();
		}
	}
	else
	{
		Destroy();
	}
}

void AEGAreaDamageBox::ApplyAreaDamage()
{
	if (GetOwner())
	{
		TArray<AActor*> OverlappingActors;
		BoxComponent->GetOverlappingActors(OverlappingActors);
		
		for (const auto actor : OverlappingActors)
		{
			UGameplayStatics::ApplyDamage(actor,DamageAmount,GetOwner()->GetInstigatorController(),GetOwner(),DamageType);
		}
		
		SpawnEffects(DamageSoundLoop,DamageEffectLoop,DamageNiagaraLoop);
	}
}


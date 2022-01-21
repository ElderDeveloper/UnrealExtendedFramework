// Fill out your copyright notice in the Description page of Project Settings.


#include "EGAreaDamageSphere.h"

#include "Kismet/GameplayStatics.h"


AEGAreaDamageSphere::AEGAreaDamageSphere()
{

}

void AEGAreaDamageSphere::InitializeDamage(float damageAmount, float radius, bool isLooping, float loopTime,float lifetime, TSubclassOf<UDamageType> damageType)
{
	if (GetOwner())
	{
		SetLifeSpan(lifetime + 0.1);
		DamageSphereRadius = radius;
		DamageAmount = damageAmount;
		DamageType = damageType;
		Lifetime = lifetime;
	
		if (isLooping)
		{
			SpawnEffects(DamageSoundStart,DamageEffectStart,DamageNiagaraStart);
			GetWorldTimerManager().SetTimer(LoopHandle,this,&AEGAreaDamageSphere::ApplyAreaDamage,loopTime,isLooping);
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

void AEGAreaDamageSphere::ApplyAreaDamage()
{
	TArray<AActor*> Ignore;
	Ignore.Add(GetOwner());
	UGameplayStatics::ApplyRadialDamage(GetWorld(),DamageAmount,GetActorLocation(),DamageSphereRadius,DamageType,Ignore,GetOwner(),GetOwner()->GetInstigatorController());
	SpawnEffects(DamageSoundLoop,DamageEffectLoop,DamageNiagaraLoop);
}






// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEExtendedAreaDamage.generated.h"

class USoundBase;
class UParticleSystem;
class UNiagaraSystem;
UCLASS()
class UEEXPANDEDFRAMEWORK_API AUEExtendedAreaDamage : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AUEExtendedAreaDamage();


	float DamageAmount;
	TSubclassOf<UDamageType> DamageType = nullptr;
	float Lifetime;
	FTimerHandle LoopHandle;
	float AliveTime;
		
	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	USoundBase* DamageSoundStart;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	USoundBase* DamageSoundLoop;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	USoundBase* DamageSoundEnd;


	
	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UParticleSystem* DamageEffectStart;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UParticleSystem* DamageEffectLoop;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UParticleSystem* DamageEffectEnd;


	
	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UNiagaraSystem* DamageNiagaraStart;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UNiagaraSystem* DamageNiagaraLoop;

	UPROPERTY(EditDefaultsOnly, Category="Damage Sphere|Effects")
	UNiagaraSystem* DamageNiagaraEnd;




	void SpawnEffects(USoundBase* SoundBase , UParticleSystem*ParticleSystem , class UNiagaraSystem* NiagaraEmitter) const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	
};

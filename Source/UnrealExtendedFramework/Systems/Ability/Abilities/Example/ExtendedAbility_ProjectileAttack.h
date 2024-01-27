// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Systems/Ability/Abilities/EFExtendedAbility.h"
#include "ExtendedAbility_ProjectileAttack.generated.h"



UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UExtendedAbility_ProjectileAttack : public UEFExtendedAbility
{
	GENERATED_UCLASS_BODY()

protected:

	UPROPERTY(EditAnywhere , Category="Projectile Ability")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere , Category="Projectile Ability")
	FName HandSocketName;

	UPROPERTY(EditAnywhere , Category="Projectile Ability")
	float AttackAnimDelay;

	UPROPERTY(EditAnywhere , Category="Projectile Ability")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditAnywhere , Category="Projectile Ability")
	UParticleSystem* CastParticleEffect;

	UFUNCTION()
	void AttackDelay_Elapsed(ACharacter* InstigatorCharacter);
public:

	UExtendedAbility_ProjectileAttack();

	virtual void StartExtendedAbility_Implementation(AActor* Instigator) override;
};

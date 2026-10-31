// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMeleeDamagePolicy.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

UWorld* UEGMeleeDamagePolicy::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

bool UEGMeleeDamagePolicy::CanHit_Implementation(AActor* Attacker, AActor* Target) const
{
	if (!IsValid(Target) || Target == Attacker)
	{
		return false;
	}
	return !Attacker || !Target->IsAttachedTo(Attacker);
}

void UEGMeleeDamagePolicy::ApplyHit_Implementation(AActor* Attacker, const FEGMeleeHit& Hit)
{
}

void UEGMeleeDamagePolicy_EngineDamage::ApplyHit_Implementation(AActor* Attacker, const FEGMeleeHit& Hit)
{
	AActor* const Target = Hit.Target;
	if (!IsValid(Target))
	{
		return;
	}

	AController* const Instigator = Attacker ? Attacker->GetInstigatorController() : nullptr;
	const float Damage = BaseDamage * Hit.DamageMultiplier;
	if (Damage > 0.0f)
	{
		if (bPointDamage)
		{
			UGameplayStatics::ApplyPointDamage(Target, Damage, Hit.SwingDirection, Hit.HitResult, Instigator, Attacker, DamageType);
		}
		else
		{
			UGameplayStatics::ApplyDamage(Target, Damage, Instigator, Attacker, DamageType);
		}
	}

	if (KnockbackSpeed > 0.0f || KnockbackUpSpeed > 0.0f)
	{
		if (ACharacter* const Character = Cast<ACharacter>(Target))
		{
			// A velocity, not a force: the same result at any frame rate.
			const FVector Horizontal = FVector(Hit.SwingDirection.X, Hit.SwingDirection.Y, 0.0).GetSafeNormal() * KnockbackSpeed;
			Character->LaunchCharacter(Horizontal + FVector::UpVector * KnockbackUpSpeed, true, KnockbackUpSpeed > 0.0f);
		}
	}
}

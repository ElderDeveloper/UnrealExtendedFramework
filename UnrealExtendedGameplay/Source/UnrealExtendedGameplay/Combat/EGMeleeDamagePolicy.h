// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGMeleeTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "EGMeleeDamagePolicy.generated.h"

class UDamageType;

/**
 * What a melee hit does. UEGMeleeTraceComponent finds the hits; its policy decides who can be hit
 * and applies the result, so a project swaps damage handling (engine damage, a GameplayEffect, a
 * custom health component) without touching detection. Subclass in Blueprint or C++.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGMeleeDamagePolicy : public UObject
{
	GENERATED_BODY()

public:
	/** Filter before a hit is recorded. Default: anything but the attacker and actors attached to it. */
	UFUNCTION(BlueprintNativeEvent, Category = "Melee")
	bool CanHit(AActor* Attacker, AActor* Target) const;

	/** Called once per actor per window, and only for authoritative hits. */
	UFUNCTION(BlueprintNativeEvent, Category = "Melee")
	void ApplyHit(AActor* Attacker, const FEGMeleeHit& Hit);

	virtual UWorld* GetWorld() const override;
};

/** Engine damage (ApplyPointDamage / ApplyDamage) plus an optional knockback launch. */
UCLASS(meta = (DisplayName = "Engine Damage"))
class UNREALEXTENDEDGAMEPLAY_API UEGMeleeDamagePolicy_EngineDamage : public UEGMeleeDamagePolicy
{
	GENERATED_BODY()

public:
	/** Multiplied by the window's DamageMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	/** Point damage carries the hit and the swing direction; off applies plain damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bPointDamage = true;

	/** Horizontal launch speed given to a hit character along the swing, in cm/s. 0 is none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float KnockbackSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float KnockbackUpSpeed = 0.0f;

	virtual void ApplyHit_Implementation(AActor* Attacker, const FEGMeleeHit& Hit) override;
};

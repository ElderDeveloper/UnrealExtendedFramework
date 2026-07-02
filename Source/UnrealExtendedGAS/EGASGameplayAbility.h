#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"

#include "EGASGameplayAbility.generated.h"

class UEGASAbilitySystemComponent;

UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAS_API UEGASGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEGASGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS|Ability")
	UEGASAbilitySystemComponent* GetEGASAbilitySystemComponentFromActorInfo() const;

	AActor* GetAvatarActorFromActorInfo() const;

	AActor* GetOwnerActorFromActorInfo() const;
};

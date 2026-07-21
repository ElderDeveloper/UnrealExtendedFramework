#pragma once

#include "Abilities/GameplayAbility.h"
#include "CoreMinimal.h"

#include "EGASGameplayAbility.generated.h"

class UEGASAbilitySystemComponent;
class UInputAction;

UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAS_API UEGASGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEGASGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Enhanced Input action automatically routed through the owning EGAS ability system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unreal Extended GAS|Input")
	TSoftObjectPtr<UInputAction> ActivationInputAction;

	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS|Ability")
	UEGASAbilitySystemComponent* GetEGASAbilitySystemComponentFromActorInfo() const;

	AActor* GetAvatarActorFromActorInfo() const;

	AActor* GetOwnerActorFromActorInfo() const;
};

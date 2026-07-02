#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"

#include "EGASAbilitySystemComponent.generated.h"

class UAttributeSet;
class UGameplayAbility;
class UGameplayEffect;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAS_API UEGASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel = 1, int32 InputID = -1, UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Effects")
	FActiveGameplayEffectHandle GrantGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Attributes")
	UAttributeSet* GrantAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	void RevokeAbility(FGameplayAbilitySpecHandle AbilityHandle);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Effects")
	void RevokeGameplayEffect(FActiveGameplayEffectHandle EffectHandle, int32 StacksToRemove = -1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Attributes")
	void RevokeAttributeSet(UAttributeSet* AttributeSet);

	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS")
	bool HasAuthorityToGrant() const;
};

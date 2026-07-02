#pragma once

#include "CoreMinimal.h"
#include "EGASAbilitySet.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EGASAbilityFunctionLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAbilityFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unreal Extended GAS")
	static UAbilitySystemComponent* GetAbilitySystemComponentFromActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Ability Set")
	static bool GiveAbilitySetToActor(AActor* Actor, const UEGASAbilitySet* AbilitySet, UPARAM(ref) FEGASAbilitySetGrantedHandles& OutGrantedHandles, UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Ability Set")
	static bool RemoveAbilitySetFromActor(AActor* Actor, const UEGASAbilitySet* AbilitySet, UPARAM(ref) FEGASAbilitySetGrantedHandles& GrantedHandles);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Effects")
	static FActiveGameplayEffectHandle ApplyGameplayEffectToActor(AActor* SourceActor, AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level = 1.0f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unreal Extended GAS|Attributes")
	static float GetNumericAttribute(AActor* Actor, FGameplayAttribute Attribute, bool& bFound);

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Gameplay Tags")
	static bool AddLooseGameplayTagsToActor(AActor* Actor, FGameplayTagContainer GameplayTags);

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Gameplay Tags")
	static bool RemoveLooseGameplayTagsFromActor(AActor* Actor, FGameplayTagContainer GameplayTags);
};

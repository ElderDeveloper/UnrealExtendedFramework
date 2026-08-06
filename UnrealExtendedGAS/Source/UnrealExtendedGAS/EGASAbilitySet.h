#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"

#include "EGASAbilitySet.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UDataTable;
class UGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	int32 InputID = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effect")
	float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAbilitySet_AttributeSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Set")
	TSubclassOf<UAttributeSet> AttributeSet = nullptr;

	/**
	 * Optional starting values for the set this entry spawns, as an FAttributeMetaData data table
	 * (the engine's own default-attributes format, same as UAbilitySystemComponent::DefaultStartingData).
	 * Rows are named "<AttributeSetClass>.<AttributeName>", e.g. "MyAttributeSet.Health".
	 *
	 * Applies only to the set granted by THIS entry. Attribute sets created elsewhere — a
	 * CreateDefaultSubobject on the owning actor, for instance — are not touched by it.
	 * Leave null to keep the values the attribute set's own constructor established.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Set")
	TObjectPtr<UDataTable> DefaultAttributeTable = nullptr;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAbilitySetGrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* AttributeSet);
	void TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

UCLASS(BlueprintType, Const)
class UNREALEXTENDEDGAS_API UEGASAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Ability Set")
	void GiveToAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, FEGASAbilitySetGrantedHandles& OutGrantedHandles, UObject* SourceObject = nullptr) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Ability Set")
	void TakeFromAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent, UPARAM(ref) FEGASAbilitySetGrantedHandles& GrantedHandles) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<FEGASAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<FEGASAbilitySet_GameplayEffect> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes")
	TArray<FEGASAbilitySet_AttributeSet> GrantedAttributeSets;
};

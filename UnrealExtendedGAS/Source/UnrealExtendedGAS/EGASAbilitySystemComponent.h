#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "EGASAbilitySet.h"
#include "InputActionValue.h"

#include "EGASAbilitySystemComponent.generated.h"

class UAttributeSet;
class UEnhancedInputComponent;
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;

struct FEGASAbilityInputBindingHandles
{
	uint32 Pressed = 0;
	uint32 Completed = 0;
	uint32 Canceled = 0;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAS_API UEGASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginDestroy() override;
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

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

	/** Rebuilds ability-owned Enhanced Input bindings against the current avatar input component. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Input")
	void RefreshAbilityInputBindings();

	/** Removes all Enhanced Input bindings created by this ability system. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Input")
	void ClearAbilityInputBindings();

	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS|Input")
	bool HasAbilityBindingForInputAction(const UInputAction* InputAction) const;

	/**
	 * Abilities granted once on authority when InitAbilityActorInfo runs
	 * (same role as EFExtendedAbilityComponent::StartupExtendedAbilities).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unreal Extended GAS|Startup")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Ability sets granted once on authority when InitAbilityActorInfo runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unreal Extended GAS|Startup")
	TArray<TObjectPtr<UEGASAbilitySet>> DefaultAbilitySets;

protected:
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	/** Grants DefaultAbilities / DefaultAbilitySets once on authority. */
	virtual void GrantStartupAbilities();

private:
	void BindAbilityInput(FGameplayAbilitySpec& AbilitySpec);
	void UnbindAbilityInput(FGameplayAbilitySpec& AbilitySpec);
	int32 GetOrCreateInputID(UInputAction* InputAction);
	void HandleAbilityInputPressed(const FInputActionValue& Value, int32 InputID);
	void HandleAbilityInputReleased(const FInputActionValue& Value, int32 InputID);
	UEnhancedInputComponent* GetEnhancedInputComponent() const;
	void RemoveBindingsForInputID(int32 InputID);

	TMap<TObjectPtr<UInputAction>, int32> InputActionToIDMap;
	TMap<int32, FEGASAbilityInputBindingHandles> InputIDBindingHandles;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	int32 NextAutoInputID = 100;

	UPROPERTY(Transient)
	bool bStartupAbilitiesGranted = false;

	UPROPERTY(Transient)
	FEGASAbilitySetGrantedHandles DefaultAbilitySetHandles;
};

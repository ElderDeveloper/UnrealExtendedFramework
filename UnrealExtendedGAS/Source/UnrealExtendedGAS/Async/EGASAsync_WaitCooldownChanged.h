#pragma once

#include "Abilities/Async/AbilityAsync.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASAsync_WaitCooldownChanged.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGASAsyncCooldownChangedDelegate, FGameplayTag, CooldownTag, float, TimeRemaining, float, Duration);

/** Listens for predicted or authoritative cooldown Gameplay Effects. */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncTask))
class UNREALEXTENDEDGAS_API UEGASAsync_WaitCooldownChanged : public UAbilityAsync
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitCooldownChanged* GAListenForCooldownChange(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTagContainer CooldownTags,
		bool UseServerCooldown);

	virtual void Activate() override;
	virtual void EndAction() override;

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async")
	void EndTask();

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncCooldownChangedDelegate OnCooldownBegin;

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncCooldownChangedDelegate OnCooldownEnd;

private:
	void HandleActiveGameplayEffectAdded(
		UAbilitySystemComponent* Target,
		const FGameplayEffectSpec& SpecApplied,
		FActiveGameplayEffectHandle ActiveHandle);
	void HandleCooldownTagChanged(FGameplayTag CooldownTag, int32 NewCount);
	bool GetCooldownRemainingForTag(FGameplayTag CooldownTag, float& TimeRemaining, float& CooldownDuration) const;

	FGameplayTagContainer CooldownTags;
	bool bUseServerCooldown = false;
	FDelegateHandle ActiveEffectAddedHandle;
	TMap<FGameplayTag, FDelegateHandle> TagDelegateHandles;
};

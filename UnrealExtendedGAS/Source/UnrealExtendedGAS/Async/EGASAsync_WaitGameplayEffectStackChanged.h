#pragma once

#include "Abilities/Async/AbilityAsync.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASAsync_WaitGameplayEffectStackChanged.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FEGASAsyncGameplayEffectStackChangedDelegate,
	FGameplayTag, EffectGameplayTag,
	FActiveGameplayEffectHandle, Handle,
	int32, NewStackCount,
	int32, OldStackCount);

/** Listens for stack changes on Gameplay Effects selected by an asset or granted tag. */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncTask))
class UNREALEXTENDEDGAS_API UEGASAsync_WaitGameplayEffectStackChanged : public UAbilityAsync
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitGameplayEffectStackChanged* GAListenForGameplayEffectStackChange(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag EffectGameplayTag);

	virtual void Activate() override;
	virtual void EndAction() override;

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async")
	void EndTask();

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncGameplayEffectStackChangedDelegate OnGameplayEffectStackChange;

private:
	void HandleActiveGameplayEffectAdded(
		UAbilitySystemComponent* Target,
		const FGameplayEffectSpec& SpecApplied,
		FActiveGameplayEffectHandle ActiveHandle);
	void HandleGameplayEffectRemoved(const FActiveGameplayEffect& EffectRemoved);
	void HandleGameplayEffectStackChanged(FActiveGameplayEffectHandle EffectHandle, int32 NewStackCount, int32 PreviousStackCount);
	bool MatchesEffectTag(const FGameplayEffectSpec& EffectSpec) const;
	void BindStackDelegate(FActiveGameplayEffectHandle EffectHandle);

	FGameplayTag EffectGameplayTag;
	FDelegateHandle ActiveEffectAddedHandle;
	FDelegateHandle EffectRemovedHandle;
	TMap<FActiveGameplayEffectHandle, FDelegateHandle> StackDelegateHandles;
};

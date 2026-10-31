// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Async/EFAsyncActionBase.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "EGASAsync_WaitGameplayTagChangeOnActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGASAsyncGameplayTagChangedDelegate, FGameplayTag, Tag, int32, NewCount);

/**
 * Fires OnChanged whenever Tag's count on TargetActor's ability system changes (once, with
 * bOnlyTriggerOnce). Keeps listening until EndTask or Cancel (Async Task pin), or until the game
 * instance shuts down; neither fires anything.
 */
UCLASS()
class UNREALEXTENDEDGAS_API UEGASAsync_WaitGameplayTagChangeOnActor : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitGameplayTagChangeOnActor* WaitGameplayTagChangeOnActor(AActor* TargetActor, FGameplayTag Tag, bool bOnlyTriggerOnce = false);

	/** Stops listening, without firing anything. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async")
	void EndTask();

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncGameplayTagChangedDelegate OnChanged;

protected:
	virtual void ExecuteAction() override;
	/** Unbinds from the ability system's tag event. */
	virtual void CancelWork() override;
	/** No failure pin: an invalid actor or tag, a missing ability system and cancellation end the node silently. */
	virtual void BroadcastFailure(const FEFResult& /*Result*/) override {}
	virtual FName GetResultSource() const override { return TEXT("GAS"); }

private:
	void HandleTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	FGameplayTag Tag;
	bool bOnlyTriggerOnce = false;
	FDelegateHandle DelegateHandle;
	TWeakObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;
};

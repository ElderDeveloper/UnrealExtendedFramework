#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "EGASAsync_WaitGameplayTagChangeOnActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGASAsyncGameplayTagChangedDelegate, FGameplayTag, Tag, int32, NewCount);

UCLASS()
class UNREALEXTENDEDGAS_API UEGASAsync_WaitGameplayTagChangeOnActor : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitGameplayTagChangeOnActor* WaitGameplayTagChangeOnActor(AActor* TargetActor, FGameplayTag Tag, bool bOnlyTriggerOnce = false);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async")
	void EndTask();

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncGameplayTagChangedDelegate OnChanged;

private:
	void HandleTagChanged(const FGameplayTag Tag, int32 NewCount);

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	FGameplayTag Tag;
	bool bOnlyTriggerOnce = false;
	FDelegateHandle DelegateHandle;
	TWeakObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;
};

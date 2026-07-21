#pragma once

#include "Abilities/Async/AbilityAsync.h"
#include "CoreMinimal.h"

#include "EGASAsync_WaitAttributeChanged.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGASAsyncAttributeChangedDelegate, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);

/** Listens for one or more attribute changes outside an ability lifetime. */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncTask))
class UNREALEXTENDEDGAS_API UEGASAsync_WaitAttributeChanged : public UAbilityAsync
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitAttributeChanged* GAListenForAttributeChange(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute);

	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async", meta = (BlueprintInternalUseOnly = "TRUE"))
	static UEGASAsync_WaitAttributeChanged* GAListenForAttributesChange(UAbilitySystemComponent* AbilitySystemComponent, TArray<FGameplayAttribute> Attributes);

	virtual void Activate() override;
	virtual void EndAction() override;

	/** Compatibility alias for the legacy async node. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Async")
	void EndTask();

	UPROPERTY(BlueprintAssignable)
	FEGASAsyncAttributeChangedDelegate OnAttributeChanged;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	TArray<FGameplayAttribute> AttributesToListen;
	TMap<FGameplayAttribute, FDelegateHandle> DelegateHandles;
};

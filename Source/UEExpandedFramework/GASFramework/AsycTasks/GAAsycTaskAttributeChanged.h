// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AbilitySystemComponent.h"
#include "GAAsycTaskAttributeChanged.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAttributeChanged, FGameplayAttribute, Attribute, float, NewValue, float, OldValue);

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncTask))
class UEEXPANDEDFRAMEWORK_API UGAAsycTaskAttributeChanged : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChanged OnAttributeChanged;

	// Listens for an attribute changing.
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGAAsycTaskAttributeChanged* GAListenForAttributeChange(UAbilitySystemComponent* AbilitySystemComponent , FGameplayAttribute Attribute);

	// Listens for an attribute changing.
	// Version that takes in an array of Attributes. Check the Attribute output for which Attribute changed.
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UGAAsycTaskAttributeChanged* GAListenForAttributesChange(UAbilitySystemComponent* AbilitySystemComponent , TArray<FGameplayAttribute> Attributes);


	UFUNCTION(BlueprintCallable)
	void EndTask();

	protected:

	UPROPERTY()
	UAbilitySystemComponent* ASC;

	FGameplayAttribute AttributeToListen;
	TArray<FGameplayAttribute> AttributesToListen;

	void AttributeChanged(const FOnAttributeChangeData& Data);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncAttributeChanged.generated.h"

class UEFAttributeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAsyncOnAttributeChanged , float , AttributeValue);



UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncAttributeChanged : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()


public:
	
	/**
	 * 	Wait until the specified gameplay tag is Added. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedAttributes|Async", meta = (BlueprintInternalUseOnly = "true"))
	static UEFAsyncAttributeChanged* EFAsyncAttributeChanged(UEFAttributeComponent* AttributeComponent , FGameplayTag AttributeTag, bool OnlyTriggerOnce = false);


	UFUNCTION()
	void OnAttributeChanged(const FExtendedAttribute& Attribute);

	
protected:
	
	UPROPERTY()
	UEFAttributeComponent* OwningAttributeComponent;

	FGameplayTag CheckAttributeTag;
	bool bOnlyTriggerOnce;


	UPROPERTY(BlueprintAssignable)
	FAsyncOnAttributeChanged AsyncAttributeChanged;

	
	virtual void Activate() override;
};

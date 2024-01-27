// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncWaitGameplayTag.generated.h"


class UEFExtendedAbility;
class UEFAsyncWaitGameplayTagAdded;
class UEFAsyncWaitGameplayTagRemoved;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayTagDelegateAdded , UEFAsyncWaitGameplayTagAdded* , WaitGameplayTagAdded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayTagDelegateRemoved , UEFAsyncWaitGameplayTagRemoved* , WaitGameplayTagRemoved);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncWaitGameplayTagAdded : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	/**
	 * 	Wait until the specified gameplay tag is Added. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedAbility|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UEFAsyncWaitGameplayTagAdded* EFAsyncWaitGameplayTagAdded(UEFExtendedAbility* OwningAbility, FGameplayTag Tag, bool OnlyTriggerOnce = false);

	UFUNCTION()
	void OnGameplayTagAdded(UEFExtendedAbility* Instigator , FGameplayTag ChangedTag);

protected:
	
	UPROPERTY()
	UEFExtendedAbility* Ability;

	bool bShouldDestroyOnce = false;
	FGameplayTag SearchTag;
	
	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagDelegateAdded Added;


	virtual void Activate() override;
	
};




UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncWaitGameplayTagRemoved : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	/**
	 * 	Wait until the specified gameplay tag is Added. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  If the tag is already present when this task is started, it will immediately broadcast the Added event. It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "ExtendedAbility|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UEFAsyncWaitGameplayTagRemoved* EFAsyncWaitGameplayTagRemoved(UEFExtendedAbility* OwningAbility, FGameplayTag Tag, bool OnlyTriggerOnce = false);
	
	UFUNCTION()
	void OnGameplayTagRemoved(UEFExtendedAbility* Instigator , FGameplayTag ChangedTag);
	
protected:

	UPROPERTY()
	UEFExtendedAbility* Ability;

	bool bShouldDestroyOnce = false;
	FGameplayTag SearchTag;
	
	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagDelegateRemoved Removed;
	

	virtual void Activate() override;
};

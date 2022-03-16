// Fill out your copyright notice in the Description page of Project Settings.


#include "EGAAsyncEffectStackChanged.h"


UEGAAsyncEffectStackChanged* UEGAAsyncEffectStackChanged::GAListenForGameplayEffectStackChange(
	UAbilitySystemComponent* AbilitySystemComponent, FGameplayTag InEffectGameplayTag)
{
	UEGAAsyncEffectStackChanged* ListenForGameplayEffectStackChange = NewObject<UEGAAsyncEffectStackChanged>();
	ListenForGameplayEffectStackChange->ASC = AbilitySystemComponent;
	ListenForGameplayEffectStackChange->EffectGameplayTag = InEffectGameplayTag;

	if (!IsValid(AbilitySystemComponent) || !InEffectGameplayTag.IsValid())
	{
		ListenForGameplayEffectStackChange->EndTask();
		return nullptr;
	}

	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		ListenForGameplayEffectStackChange, 
		&UEGAAsyncEffectStackChanged::OnActiveGameplayEffectAddedCallback);
	
	AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		ListenForGameplayEffectStackChange, 
		&UEGAAsyncEffectStackChanged::OnRemoveGameplayEffectCallback);

	return ListenForGameplayEffectStackChange;
}

void UEGAAsyncEffectStackChanged::EndTask()
{
	if (IsValid(ASC))
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
		ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
	}

	SetReadyToDestroy();
	ConditionalBeginDestroy();
}

void UEGAAsyncEffectStackChanged::OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target,
	const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);

	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);

	if (AssetTags.HasTagExact(EffectGameplayTag) || GrantedTags.HasTagExact(EffectGameplayTag))
	{
		ASC->OnGameplayEffectStackChangeDelegate(ActiveHandle)->AddUObject(this, &UEGAAsyncEffectStackChanged::GameplayEffectStackChanged);

		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag, 
			ActiveHandle, 
			1, 
			0);
	}
}

void UEGAAsyncEffectStackChanged::OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved)
{
	FGameplayTagContainer AssetTags;
	EffectRemoved.Spec.GetAllAssetTags(AssetTags);

	FGameplayTagContainer GrantedTags;
	EffectRemoved.Spec.GetAllGrantedTags(GrantedTags);

	if (AssetTags.HasTagExact(EffectGameplayTag) || GrantedTags.HasTagExact(EffectGameplayTag))
	{
		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag, 
			EffectRemoved.Handle, 
			0, 
			1);
	}
}

void UEGAAsyncEffectStackChanged::GameplayEffectStackChanged(FActiveGameplayEffectHandle EffectHandle,
	int32 NewStackCount, int32 PreviousStackCount)
{
	OnGameplayEffectStackChange.Broadcast(
		EffectGameplayTag, 
		EffectHandle, 
		NewStackCount, 
		PreviousStackCount);
}
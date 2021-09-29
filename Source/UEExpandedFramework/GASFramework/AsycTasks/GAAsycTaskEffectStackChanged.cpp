// Fill out your copyright notice in the Description page of Project Settings.


#include "GAAsycTaskEffectStackChanged.h"


UGAAsycTaskEffectStackChanged* UGAAsycTaskEffectStackChanged::GAListenForGameplayEffectStackChange(
	UAbilitySystemComponent* AbilitySystemComponent, FGameplayTag InEffectGameplayTag)
{
	UGAAsycTaskEffectStackChanged* ListenForGameplayEffectStackChange = NewObject<UGAAsycTaskEffectStackChanged>();
	ListenForGameplayEffectStackChange->ASC = AbilitySystemComponent;
	ListenForGameplayEffectStackChange->EffectGameplayTag = InEffectGameplayTag;

	if (!IsValid(AbilitySystemComponent) || !InEffectGameplayTag.IsValid())
	{
		ListenForGameplayEffectStackChange->EndTask();
		return nullptr;
	}

	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		ListenForGameplayEffectStackChange, 
		&UGAAsycTaskEffectStackChanged::OnActiveGameplayEffectAddedCallback);
	
	AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		ListenForGameplayEffectStackChange, 
		&UGAAsycTaskEffectStackChanged::OnRemoveGameplayEffectCallback);

	return ListenForGameplayEffectStackChange;
}

void UGAAsycTaskEffectStackChanged::EndTask()
{
	if (IsValid(ASC))
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
		ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
	}

	SetReadyToDestroy();
	MarkPendingKill();
}

void UGAAsycTaskEffectStackChanged::OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target,
	const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);

	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);

	if (AssetTags.HasTagExact(EffectGameplayTag) || GrantedTags.HasTagExact(EffectGameplayTag))
	{
		ASC->OnGameplayEffectStackChangeDelegate(ActiveHandle)->AddUObject(this, &UGAAsycTaskEffectStackChanged::GameplayEffectStackChanged);

		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag, 
			ActiveHandle, 
			1, 
			0);
	}
}

void UGAAsycTaskEffectStackChanged::OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved)
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

void UGAAsycTaskEffectStackChanged::GameplayEffectStackChanged(FActiveGameplayEffectHandle EffectHandle,
	int32 NewStackCount, int32 PreviousStackCount)
{
	OnGameplayEffectStackChange.Broadcast(
		EffectGameplayTag, 
		EffectHandle, 
		NewStackCount, 
		PreviousStackCount);
}
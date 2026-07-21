#include "Async/EGASAsync_WaitGameplayEffectStackChanged.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UEGASAsync_WaitGameplayEffectStackChanged* UEGASAsync_WaitGameplayEffectStackChanged::GAListenForGameplayEffectStackChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTag InEffectGameplayTag)
{
	UEGASAsync_WaitGameplayEffectStackChanged* AsyncAction = NewObject<UEGASAsync_WaitGameplayEffectStackChanged>();
	AsyncAction->SetAbilitySystemComponent(AbilitySystemComponent);
	AsyncAction->EffectGameplayTag = InEffectGameplayTag;
	return AsyncAction;
}

void UEGASAsync_WaitGameplayEffectStackChanged::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !EffectGameplayTag.IsValid())
	{
		EndAction();
		return;
	}

	ActiveEffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &ThisClass::HandleActiveGameplayEffectAdded);
	EffectRemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		this, &ThisClass::HandleGameplayEffectRemoved);
}

void UEGASAsync_WaitGameplayEffectStackChanged::EndAction()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ActiveEffectAddedHandle.IsValid())
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(ActiveEffectAddedHandle);
		}
		if (EffectRemovedHandle.IsValid())
		{
			ASC->OnAnyGameplayEffectRemovedDelegate().Remove(EffectRemovedHandle);
		}

		for (const TPair<FActiveGameplayEffectHandle, FDelegateHandle>& Pair : StackDelegateHandles)
		{
			if (FOnActiveGameplayEffectStackChange* Delegate = ASC->OnGameplayEffectStackChangeDelegate(Pair.Key))
			{
				Delegate->Remove(Pair.Value);
			}
		}
	}

	ActiveEffectAddedHandle.Reset();
	EffectRemovedHandle.Reset();
	StackDelegateHandles.Reset();
	Super::EndAction();
}

void UEGASAsync_WaitGameplayEffectStackChanged::EndTask()
{
	EndAction();
}

void UEGASAsync_WaitGameplayEffectStackChanged::HandleActiveGameplayEffectAdded(
	UAbilitySystemComponent* Target,
	const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveHandle)
{
	if (Target != GetAbilitySystemComponent() || !MatchesEffectTag(SpecApplied))
	{
		return;
	}

	BindStackDelegate(ActiveHandle);
	if (ShouldBroadcastDelegates())
	{
		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag,
			ActiveHandle,
			SpecApplied.GetStackCount(),
			0);
	}
}

void UEGASAsync_WaitGameplayEffectStackChanged::HandleGameplayEffectRemoved(const FActiveGameplayEffect& EffectRemoved)
{
	if (!MatchesEffectTag(EffectRemoved.Spec))
	{
		return;
	}

	StackDelegateHandles.Remove(EffectRemoved.Handle);
	if (ShouldBroadcastDelegates())
	{
		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag,
			EffectRemoved.Handle,
			0,
			EffectRemoved.Spec.GetStackCount());
	}
}

void UEGASAsync_WaitGameplayEffectStackChanged::HandleGameplayEffectStackChanged(
	FActiveGameplayEffectHandle EffectHandle,
	int32 NewStackCount,
	int32 PreviousStackCount)
{
	if (ShouldBroadcastDelegates())
	{
		OnGameplayEffectStackChange.Broadcast(
			EffectGameplayTag,
			EffectHandle,
			NewStackCount,
			PreviousStackCount);
	}
}

bool UEGASAsync_WaitGameplayEffectStackChanged::MatchesEffectTag(const FGameplayEffectSpec& EffectSpec) const
{
	FGameplayTagContainer EffectTags;
	EffectSpec.GetAllAssetTags(EffectTags);
	EffectSpec.GetAllGrantedTags(EffectTags);
	return EffectTags.HasTagExact(EffectGameplayTag);
}

void UEGASAsync_WaitGameplayEffectStackChanged::BindStackDelegate(FActiveGameplayEffectHandle EffectHandle)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || StackDelegateHandles.Contains(EffectHandle))
	{
		return;
	}

	if (FOnActiveGameplayEffectStackChange* Delegate = ASC->OnGameplayEffectStackChangeDelegate(EffectHandle))
	{
		FDelegateHandle Handle = Delegate->AddUObject(this, &ThisClass::HandleGameplayEffectStackChanged);
		StackDelegateHandles.Add(EffectHandle, Handle);
	}
}

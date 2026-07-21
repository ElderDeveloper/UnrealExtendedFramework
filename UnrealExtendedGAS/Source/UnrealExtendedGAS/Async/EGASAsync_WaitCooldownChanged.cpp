#include "Async/EGASAsync_WaitCooldownChanged.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UEGASAsync_WaitCooldownChanged* UEGASAsync_WaitCooldownChanged::GAListenForCooldownChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayTagContainer InCooldownTags,
	bool UseServerCooldown)
{
	UEGASAsync_WaitCooldownChanged* AsyncAction = NewObject<UEGASAsync_WaitCooldownChanged>();
	AsyncAction->SetAbilitySystemComponent(AbilitySystemComponent);
	AsyncAction->CooldownTags = MoveTemp(InCooldownTags);
	AsyncAction->bUseServerCooldown = UseServerCooldown;
	return AsyncAction;
}

void UEGASAsync_WaitCooldownChanged::Activate()
{
	Super::Activate();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || CooldownTags.IsEmpty())
	{
		EndAction();
		return;
	}

	ActiveEffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &ThisClass::HandleActiveGameplayEffectAdded);

	for (const FGameplayTag& CooldownTag : CooldownTags)
	{
		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(
			CooldownTag, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this, &ThisClass::HandleCooldownTagChanged);
		TagDelegateHandles.Add(CooldownTag, Handle);
	}
}

void UEGASAsync_WaitCooldownChanged::EndAction()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ActiveEffectAddedHandle.IsValid())
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(ActiveEffectAddedHandle);
		}

		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : TagDelegateHandles)
		{
			ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
	}

	ActiveEffectAddedHandle.Reset();
	TagDelegateHandles.Reset();
	Super::EndAction();
}

void UEGASAsync_WaitCooldownChanged::EndTask()
{
	EndAction();
}

void UEGASAsync_WaitCooldownChanged::HandleActiveGameplayEffectAdded(
	UAbilitySystemComponent* Target,
	const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || Target != ASC || !ShouldBroadcastDelegates())
	{
		return;
	}

	FGameplayTagContainer EffectTags;
	SpecApplied.GetAllAssetTags(EffectTags);
	SpecApplied.GetAllGrantedTags(EffectTags);

	const bool bIsAuthority = ASC->GetOwnerRole() == ROLE_Authority;
	const bool bIsPredictedEffect = SpecApplied.GetContext().GetAbilityInstance_NotReplicated() != nullptr;

	for (const FGameplayTag& CooldownTag : CooldownTags)
	{
		if (!EffectTags.HasTagExact(CooldownTag))
		{
			continue;
		}

		float TimeRemaining = 0.0f;
		float Duration = 0.0f;
		GetCooldownRemainingForTag(CooldownTag, TimeRemaining, Duration);

		if (bIsAuthority || (!bUseServerCooldown && bIsPredictedEffect) || (bUseServerCooldown && !bIsPredictedEffect))
		{
			OnCooldownBegin.Broadcast(CooldownTag, TimeRemaining, Duration);
		}
		else if (bUseServerCooldown && bIsPredictedEffect)
		{
			OnCooldownBegin.Broadcast(CooldownTag, -1.0f, -1.0f);
		}
	}
}

void UEGASAsync_WaitCooldownChanged::HandleCooldownTagChanged(FGameplayTag CooldownTag, int32 NewCount)
{
	if (NewCount == 0 && ShouldBroadcastDelegates())
	{
		OnCooldownEnd.Broadcast(CooldownTag, -1.0f, -1.0f);
	}
}

bool UEGASAsync_WaitCooldownChanged::GetCooldownRemainingForTag(
	FGameplayTag CooldownTag,
	float& TimeRemaining,
	float& CooldownDuration) const
{
	TimeRemaining = 0.0f;
	CooldownDuration = 0.0f;

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !CooldownTag.IsValid())
	{
		return false;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
		FGameplayTagContainer(CooldownTag));
	const TArray<TPair<float, float>> Durations = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	if (Durations.IsEmpty())
	{
		return false;
	}

	const TPair<float, float>* Longest = &Durations[0];
	for (const TPair<float, float>& Candidate : Durations)
	{
		if (Candidate.Key > Longest->Key)
		{
			Longest = &Candidate;
		}
	}

	TimeRemaining = Longest->Key;
	CooldownDuration = Longest->Value;
	return true;
}

#include "AI/EGASBTTask_ApplyGameplayEffect.h"

#include "AI/EGASBTAbilityTargeting.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameplayEffect.h"

UEGASBTTask_ApplyGameplayEffect::UEGASBTTask_ApplyGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Apply Gameplay Effect");
}

EBTNodeResult::Type UEGASBTTask_ApplyGameplayEffect::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!GameplayEffect)
	{
		return EBTNodeResult::Failed;
	}

	UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return EBTNodeResult::Failed;
	}

	// Build the spec on the AI's own ASC so the effect is attributed to the AI. When
	// targeting self these are the same component and this collapses to applying to self.
	UAbilitySystemComponent* SourceAbilitySystem = EGASBTAbilityTargeting::ResolveInstigatorAbilitySystem(OwnerComp);
	if (!SourceAbilitySystem)
	{
		SourceAbilitySystem = TargetAbilitySystem;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(SourceAbilitySystem->GetAvatarActor());

	const FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(GameplayEffect, GameplayEffectLevel, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	const FActiveGameplayEffectHandle EffectHandle = SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetAbilitySystem);
	if (EffectHandle.IsValid())
	{
		return EBTNodeResult::Succeeded;
	}

	// An instant effect is never added to the active list, so it always hands back an
	// invalid handle even though it applied. Only treat that as success for instants.
	const UGameplayEffect* EffectDefinition = SpecHandle.Data->Def;
	const bool bIsInstant = EffectDefinition && EffectDefinition->DurationPolicy == EGameplayEffectDurationType::Instant;

	return bIsInstant ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

FString UEGASBTTask_ApplyGameplayEffect::GetStaticDescription() const
{
	if (!GameplayEffect)
	{
		return TEXT("No gameplay effect set");
	}

	return FString::Printf(TEXT("Apply %s (level %d)\nto %s"), *GameplayEffect->GetName(), GameplayEffectLevel, *GetTargetDescription());
}

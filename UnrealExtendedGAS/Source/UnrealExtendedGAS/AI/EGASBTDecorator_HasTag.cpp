#include "AI/EGASBTDecorator_HasTag.h"

#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTDecorator_HasTag::UEGASBTDecorator_HasTag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Has Gameplay Tag");
}

bool UEGASBTDecorator_HasTag::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	if (SearchTags.IsEmpty())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return false;
	}

	return bRequireAllTags
		? TargetAbilitySystem->HasAllMatchingGameplayTags(SearchTags)
		: TargetAbilitySystem->HasAnyMatchingGameplayTags(SearchTags);
}

void UEGASBTDecorator_HasTag::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (!bTrackTagChanges || SearchTags.IsEmpty())
	{
		return;
	}

	UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return;
	}

	CachedOwnerComp = &OwnerComp;
	ObservedAbilitySystem = TargetAbilitySystem;

	for (const FGameplayTag& Tag : SearchTags)
	{
		TargetAbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEGASBTDecorator_HasTag::HandleTagChanged);
	}
}

void UEGASBTDecorator_HasTag::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UAbilitySystemComponent* TargetAbilitySystem = ObservedAbilitySystem.Get())
	{
		for (const FGameplayTag& Tag : SearchTags)
		{
			TargetAbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		}
	}

	ObservedAbilitySystem.Reset();
	CachedOwnerComp.Reset();

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

void UEGASBTDecorator_HasTag::HandleTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get())
	{
		OwnerComp->RequestExecution(this);
	}
}

FString UEGASBTDecorator_HasTag::GetStaticDescription() const
{
	if (SearchTags.IsEmpty())
	{
		return TEXT("No tags set");
	}

	return FString::Printf(TEXT("%s has %s of:\n%s"),
		*GetTargetDescription(),
		bRequireAllTags ? TEXT("all") : TEXT("any"),
		*SearchTags.ToStringSimple());
}

#include "AI/EGASBTTask_HasTag.h"

#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UEGASBTTask_HasTag::UEGASBTTask_HasTag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Has Gameplay Tag");
}

EBTNodeResult::Type UEGASBTTask_HasTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (SearchTags.IsEmpty())
	{
		return EBTNodeResult::Failed;
	}

	const UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return EBTNodeResult::Failed;
	}

	const bool bMatches = bRequireAllTags
		? TargetAbilitySystem->HasAllMatchingGameplayTags(SearchTags)
		: TargetAbilitySystem->HasAnyMatchingGameplayTags(SearchTags);

	return bMatches ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

FString UEGASBTTask_HasTag::GetStaticDescription() const
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

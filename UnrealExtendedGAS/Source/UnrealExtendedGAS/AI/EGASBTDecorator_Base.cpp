#include "AI/EGASBTDecorator_Base.h"

#include "AI/EGASBTAbilityTargeting.h"
#include "BehaviorTree/BlackboardData.h"

UEGASBTDecorator_Base::UEGASBTDecorator_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Per-agent observer state lives in members, so each agent needs its own node object.
	bCreateNodeInstance = true;

	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UEGASBTDecorator_Base, TargetActorKey), AActor::StaticClass());
}

void UEGASBTDecorator_Base::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

AActor* UEGASBTDecorator_Base::ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp) const
{
	return EGASBTAbilityTargeting::ResolveTargetActor(OwnerComp, bTargetSelf, TargetActorKey);
}

UAbilitySystemComponent* UEGASBTDecorator_Base::ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const
{
	return EGASBTAbilityTargeting::ResolveTargetAbilitySystem(OwnerComp, bTargetSelf, TargetActorKey);
}

FString UEGASBTDecorator_Base::GetTargetDescription() const
{
	return bTargetSelf ? TEXT("Self") : TargetActorKey.SelectedKeyName.ToString();
}

#include "AI/EGASBTTask_Base.h"

#include "AI/EGASBTAbilityTargeting.h"
#include "BehaviorTree/BlackboardData.h"

UEGASBTTask_Base::UEGASBTTask_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UEGASBTTask_Base, TargetActorKey), AActor::StaticClass());
}

void UEGASBTTask_Base::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

AActor* UEGASBTTask_Base::ResolveTargetActor(const UBehaviorTreeComponent& OwnerComp) const
{
	return EGASBTAbilityTargeting::ResolveTargetActor(OwnerComp, bTargetSelf, TargetActorKey);
}

UAbilitySystemComponent* UEGASBTTask_Base::ResolveTargetAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const
{
	return EGASBTAbilityTargeting::ResolveTargetAbilitySystem(OwnerComp, bTargetSelf, TargetActorKey);
}

FString UEGASBTTask_Base::GetTargetDescription() const
{
	return bTargetSelf ? TEXT("Self") : TargetActorKey.SelectedKeyName.ToString();
}

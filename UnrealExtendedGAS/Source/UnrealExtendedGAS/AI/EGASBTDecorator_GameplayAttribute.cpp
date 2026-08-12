#include "AI/EGASBTDecorator_GameplayAttribute.h"

#include "AI/EGASBTAbilityTargeting.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardData.h"

UEGASBTDecorator_GameplayAttribute::UEGASBTDecorator_GameplayAttribute(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Compare Gameplay Attribute");

	SecondTargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UEGASBTDecorator_GameplayAttribute, SecondTargetActorKey), AActor::StaticClass());
}

void UEGASBTDecorator_GameplayAttribute::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		SecondTargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

UAbilitySystemComponent* UEGASBTDecorator_GameplayAttribute::ResolveSecondAbilitySystem(const UBehaviorTreeComponent& OwnerComp) const
{
	return EGASBTAbilityTargeting::ResolveTargetAbilitySystem(OwnerComp, bSecondTargetSelf, SecondTargetActorKey);
}

bool UEGASBTDecorator_GameplayAttribute::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	if (!AttributeToCheck.IsValid())
	{
		return false;
	}

	const UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp);
	if (!TargetAbilitySystem)
	{
		return false;
	}

	const float AttributeValue = TargetAbilitySystem->GetNumericAttribute(AttributeToCheck);

	if (!bCompareWithAttribute)
	{
		return Compare(AttributeValue, ComparisonValue);
	}

	if (!SecondAttributeToCheck.IsValid())
	{
		return false;
	}

	const UAbilitySystemComponent* SecondAbilitySystem = ResolveSecondAbilitySystem(OwnerComp);
	if (!SecondAbilitySystem)
	{
		return false;
	}

	return Compare(AttributeValue, SecondAbilitySystem->GetNumericAttribute(SecondAttributeToCheck));
}

void UEGASBTDecorator_GameplayAttribute::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (!bTrackAttributeChanges)
	{
		return;
	}

	CachedOwnerComp = &OwnerComp;

	if (AttributeToCheck.IsValid())
	{
		if (UAbilitySystemComponent* TargetAbilitySystem = ResolveTargetAbilitySystem(OwnerComp))
		{
			ObservedAbilitySystem = TargetAbilitySystem;
			AttributeChangeHandle = TargetAbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeToCheck)
				.AddUObject(this, &UEGASBTDecorator_GameplayAttribute::HandleAttributeChanged);
		}
	}

	if (bCompareWithAttribute && SecondAttributeToCheck.IsValid())
	{
		if (UAbilitySystemComponent* SecondAbilitySystem = ResolveSecondAbilitySystem(OwnerComp))
		{
			SecondObservedAbilitySystem = SecondAbilitySystem;
			SecondAttributeChangeHandle = SecondAbilitySystem->GetGameplayAttributeValueChangeDelegate(SecondAttributeToCheck)
				.AddUObject(this, &UEGASBTDecorator_GameplayAttribute::HandleAttributeChanged);
		}
	}
}

void UEGASBTDecorator_GameplayAttribute::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AttributeChangeHandle.IsValid())
	{
		if (UAbilitySystemComponent* TargetAbilitySystem = ObservedAbilitySystem.Get())
		{
			TargetAbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeToCheck).Remove(AttributeChangeHandle);
		}
		AttributeChangeHandle.Reset();
	}

	if (SecondAttributeChangeHandle.IsValid())
	{
		if (UAbilitySystemComponent* SecondAbilitySystem = SecondObservedAbilitySystem.Get())
		{
			SecondAbilitySystem->GetGameplayAttributeValueChangeDelegate(SecondAttributeToCheck).Remove(SecondAttributeChangeHandle);
		}
		SecondAttributeChangeHandle.Reset();
	}

	ObservedAbilitySystem.Reset();
	SecondObservedAbilitySystem.Reset();
	CachedOwnerComp.Reset();

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

void UEGASBTDecorator_GameplayAttribute::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	if (UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get())
	{
		OwnerComp->RequestExecution(this);
	}
}

bool UEGASBTDecorator_GameplayAttribute::Compare(float Left, float Right) const
{
	switch (Comparison)
	{
	case EEGASAttributeCompareOp::Equal:
		return FMath::IsNearlyEqual(Left, Right, UE_KINDA_SMALL_NUMBER);
	case EEGASAttributeCompareOp::NotEqual:
		return !FMath::IsNearlyEqual(Left, Right, UE_KINDA_SMALL_NUMBER);
	case EEGASAttributeCompareOp::Greater:
		return Left > Right;
	case EEGASAttributeCompareOp::GreaterOrEqual:
		return Left >= Right;
	case EEGASAttributeCompareOp::Less:
		return Left < Right;
	case EEGASAttributeCompareOp::LessOrEqual:
		return Left <= Right;
	}

	return false;
}

FString UEGASBTDecorator_GameplayAttribute::GetComparisonOperator() const
{
	switch (Comparison)
	{
	case EEGASAttributeCompareOp::Equal:          return TEXT("==");
	case EEGASAttributeCompareOp::NotEqual:       return TEXT("!=");
	case EEGASAttributeCompareOp::Greater:        return TEXT(">");
	case EEGASAttributeCompareOp::GreaterOrEqual: return TEXT(">=");
	case EEGASAttributeCompareOp::Less:           return TEXT("<");
	case EEGASAttributeCompareOp::LessOrEqual:    return TEXT("<=");
	}

	return TEXT("?");
}

FString UEGASBTDecorator_GameplayAttribute::GetStaticDescription() const
{
	const FString LeftOperand = FString::Printf(TEXT("%s.%s"),
		*GetTargetDescription(),
		AttributeToCheck.IsValid() ? *AttributeToCheck.GetName() : TEXT("<no attribute>"));

	if (!bCompareWithAttribute)
	{
		return FString::Printf(TEXT("%s %s %.2f"), *LeftOperand, *GetComparisonOperator(), ComparisonValue);
	}

	const FString SecondTarget = bSecondTargetSelf ? TEXT("Self") : SecondTargetActorKey.SelectedKeyName.ToString();
	const FString RightOperand = FString::Printf(TEXT("%s.%s"),
		*SecondTarget,
		SecondAttributeToCheck.IsValid() ? *SecondAttributeToCheck.GetName() : TEXT("<no attribute>"));

	return FString::Printf(TEXT("%s %s %s"), *LeftOperand, *GetComparisonOperator(), *RightOperand);
}

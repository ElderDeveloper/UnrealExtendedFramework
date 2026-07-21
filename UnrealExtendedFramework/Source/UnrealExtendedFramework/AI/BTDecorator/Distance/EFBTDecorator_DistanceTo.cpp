// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTDecorator_DistanceTo.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Actor.h"

UEFBTDecorator_DistanceTo::UEFBTDecorator_DistanceTo()
{
	NodeName = "Distance To";

	From.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UEFBTDecorator_DistanceTo, From), AActor::StaticClass());
	To.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UEFBTDecorator_DistanceTo, To), AActor::StaticClass());
}

bool UEFBTDecorator_DistanceTo::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const auto Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) { return false; }

	const auto FromActor = Cast<AActor>(Blackboard->GetValueAsObject(From.SelectedKeyName));
	if (!FromActor) { return false; }

	const auto ToActor = Cast<AActor>(Blackboard->GetValueAsObject(To.SelectedKeyName));
	if (!ToActor) { return false; }

	const auto CalculatedDistance = FVector::Dist(FromActor->GetActorLocation(), ToActor->GetActorLocation());

	if (DistanceCheck == EEFDecoratorDistanceCheck::Less)
	{
		return CalculatedDistance < Distance;
	}
	return CalculatedDistance > Distance;
}

FString UEFBTDecorator_DistanceTo::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s to %s \ndist %s %.1f"), *From.SelectedKeyName.ToString(), *To.SelectedKeyName.ToString(), DistanceCheck == EEFDecoratorDistanceCheck::Less ? TEXT("<") : TEXT(">"), Distance);
}

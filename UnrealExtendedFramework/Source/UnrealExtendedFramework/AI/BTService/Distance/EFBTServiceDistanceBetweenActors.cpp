// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTServiceDistanceBetweenActors.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"


// Sets default values
UEFBTServiceDistanceBetweenActors::UEFBTServiceDistanceBetweenActors()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	bNotifyTick = true;
}


void UEFBTServiceDistanceBetweenActors::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		const auto OwnerActor = Cast<AActor>(AIController->GetBlackboardComponent()->GetValueAsObject(OwnerActorKey.SelectedKeyName));
		const auto TargetActor = Cast<AActor>(AIController->GetBlackboardComponent()->GetValueAsObject(OwnerTargetKey.SelectedKeyName));
		if (OwnerActor && TargetActor)
		{
			AIController->GetBlackboardComponent()->SetValueAsFloat(DistanceKey.SelectedKeyName,FVector(OwnerActor->GetActorLocation()-TargetActor->GetActorLocation()).Size());
			return;
		}
		AIController->GetBlackboardComponent()->SetValueAsFloat(DistanceKey.SelectedKeyName,-1.f);
	}
}

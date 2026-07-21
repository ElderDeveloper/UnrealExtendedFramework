// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_SaveLocationVelocity.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Kismet/KismetSystemLibrary.h"


EBTNodeResult::Type UEFBTTask_SaveLocationVelocity::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (const auto Actor = Cast<AActor>(AIController->GetBlackboardComponent()->GetValue<UBlackboardKeyType_Object>(TargetActorKey.SelectedKeyName)))
		{
			const FVector PredictedLocation = Actor->GetActorLocation() + (Actor->GetVelocity().GetSafeNormal() * VelocityScale);
			AIController->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(SaveVectorKey.SelectedKeyName,PredictedLocation);
			return EBTNodeResult::Succeeded;
		}
	}
	return EBTNodeResult::Failed;
}

FString UEFBTTask_SaveLocationVelocity::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("Save predicted location of %s\nin %s\nScale: %.2f"),
		*TargetActorKey.SelectedKeyName.ToString(),
		*SaveVectorKey.SelectedKeyName.ToString(),
		VelocityScale
	);
}
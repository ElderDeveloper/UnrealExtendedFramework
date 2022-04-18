// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTServiceRotateToActor.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Actor/EFActorLibrary.h"

void UEFBTServiceRotateToActor::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	if (const auto AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (AIController->GetPawn())
		{
			if (IsRotateBlocked.SelectedKeyName != "SelfActor")
			{
				if (AIController->GetBlackboardComponent()->GetValueAsBool(IsRotateBlocked.SelectedKeyName)) return;
			}
			
			const auto Target =Cast<AActor>(AIController->GetBlackboardComponent()->GetValueAsObject(TargetActorKey.SelectedKeyName));
			if (!Target) return;
				
			if (bUseFullRotation)
			{
				const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(AIController->GetPawn()->GetActorLocation() , Target->GetActorLocation());
				const FRotator TargetRotation = UKismetMathLibrary::RInterpTo(AIController->GetPawn()->GetActorRotation() , LookAtRotation , DeltaSeconds , InterpSpeed);
				AIController->GetPawn()->SetActorRotation(TargetRotation);
				return;
			}
			UEFActorLibrary::RotateToObjectInterpYaw(GetWorld(),AIController->GetPawn(),Target,InterpSpeed,!bUseCustomRotation);
		}
	}
}

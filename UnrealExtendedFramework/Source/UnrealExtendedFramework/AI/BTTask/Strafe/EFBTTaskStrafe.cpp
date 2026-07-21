// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTaskStrafe.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UEFBTTaskStrafe::UEFBTTaskStrafe()
{
	bNotifyTick = true;
}


EBTNodeResult::Type UEFBTTaskStrafe::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	TaskFinished = false;
	
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		StrafeValue = UKismetMathLibrary::RandomBool() ? 1 : -1 ;
		GetWorld()->GetTimerManager().SetTimer(TaskFinishHandle,this,&UEFBTTaskStrafe::FinishThisTask,UKismetMathLibrary::RandomFloatInRange(StrafeTimeMin,StrafeTimeMax) , false );

		if (const auto Character = Cast<ACharacter>(AIController->GetPawn()))
			Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		
		return EBTNodeResult::InProgress;
	}
	return EBTNodeResult::Failed;
}





void UEFBTTaskStrafe::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	
	if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
	{
		if (AIController->GetPawn())
			AIController->GetPawn()->AddMovementInput(AIController->GetPawn()->GetActorRightVector() , StrafeValue);
	}
	
	if (TaskFinished)
	{
		if (auto const AIController = Cast<AAIController>(OwnerComp.GetOwner()))
		{
			if (const auto Character = Cast<ACharacter>(AIController->GetPawn()))
				Character->GetCharacterMovement()->bOrientRotationToMovement = SetOrientRotationToMovementAtFinish;
			
			AIController->GetBlackboardComponent()->SetValueAsBool(StrafeBlackboardKey.SelectedKeyName , false );
			FinishLatentTask(OwnerComp,EBTNodeResult::Succeeded);
		}
	}
}


void UEFBTTaskStrafe::FinishThisTask()
{
	TaskFinished = true;
}

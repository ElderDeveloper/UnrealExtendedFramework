// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "EFAIPatrolTask.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAIPatrolTask : public UBTTaskNode
{
	GENERATED_BODY()

	public:
    UEFAIPatrolTask()
    {
        NodeName = "MoveToLocations";
    }
/*
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override
    {
        APawn* Pawn = Cast<APawn>(OwnerComp.GetAIOwner()->GetPawn());
        if (!Pawn)
        {
            return EBTNodeResult::Failed;
        }

        TArray<FVector> Locations;
        OwnerComp.GetBlackboardComponent()->GetValueAsArray("TargetLocations", Locations);
        if (Locations.Num() == 0)
        {
            return EBTNodeResult::Failed;
        }

        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
        ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
        if (!NavData)
        {
            return EBTNodeResult::Failed;
        }

        FVector CurrentLocation = Pawn->GetActorLocation();
        
		FPathFindingQuery Query(Pawn, *NavData, CurrentLocation, Locations[0]);
		FPathFindingResult Result;
		NavSys->FindPathSync(Query, Result);

		if (Result.Path.IsValid())
		{
			FVector Direction = (Result.Path->PathPoints[0].Location - CurrentLocation).GetSafeNormal();
			FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
			Pawn->SetActorRotation(NewRotation);

			for (int32 i = 0; i < Result.Path->PathPoints.Num(); i++)
			{
				FVector PointDirection = (Result.Path->PathPoints[i].Location - CurrentLocation).GetSafeNormal();
				if (i == Result.Path->PathPoints.Num() - 1)
				{
					Pawn->AddMovementInput(PointDirection);
				}
				else
				{
					FVector NextDirection = (Result.Path->PathPoints[i + 1].Location - Result.Path->PathPoints[i].Location).GetSafeNormal();
					float DotProduct = FVector::DotProduct(PointDirection, NextDirection);
					if (DotProduct > 0.9f)
					{
						Pawn->AddMovementInput(PointDirection);
					}
					else
					{
						break;
					}
				}
			}

			if ((Pawn->GetActorLocation() - Locations[0]).SizeSquared() < FMath::Square(50.0f))
			{
				TArray<FVector> RemainingLocations;
				for (int32 i = 1; i < Locations.Num(); i++)
				{
					RemainingLocations.Add(Locations[i]);
				}

                OwnerComp.GetBlackboardComponent()->SetValueAsArray("TargetLocations", RemainingLocations);

                return EBTNodeResult::Succeeded;
			}
			
            return EBTNodeResult::InProgress;
        }

        return EBTNodeResult::Failed;
    }
    */
};

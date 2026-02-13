// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAILibrary.h"

#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"


void UEFAILibrary::ForceRebuildNavigationMesh(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject))
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			NavSys->Build();
		}
	}
}

bool UEFAILibrary::ExtendedGetBlackboardBool(AActor* OwningActor , FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsBool(KeyName);
	return false;
}

TSubclassOf<UObject> UEFAILibrary::ExtendedGetBlackboardClass(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsClass(KeyName);
	return nullptr;
}

uint8 UEFAILibrary::ExtendedGetBlackboardEnum(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsEnum(KeyName);
	return -1;
}

float UEFAILibrary::ExtendedGetBlackboardFloat(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsFloat(KeyName);
	return 0;
}

int32 UEFAILibrary::ExtendedGetBlackboardInt(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsInt(KeyName);
	return 0;
}

FName UEFAILibrary::ExtendedGetBlackboardName(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsName(KeyName);
	return FName();
}

UObject* UEFAILibrary::ExtendedGetBlackboardObject(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsObject(KeyName);
	return nullptr;
}

FRotator UEFAILibrary::ExtendedGetBlackboardRotator(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsRotator(KeyName);
	return FRotator();
}

FString UEFAILibrary::ExtendedGetBlackboardString(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsString(KeyName);
	return FString();
}

FVector UEFAILibrary::ExtendedGetBlackboardVector(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsVector(KeyName);
	return FVector();
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

bool UEFAILibrary::ExtendedSetBlackboardBool(AActor* OwningActor, FName KeyName, bool Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsBool(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardClass(AActor* OwningActor, FName KeyName, TSubclassOf<UObject> Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsClass(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardEnum(AActor* OwningActor, FName KeyName, uint8 Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsEnum(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardFloat(AActor* OwningActor, FName KeyName, float Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsFloat(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardInt(AActor* OwningActor, FName KeyName, int32 Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsInt(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardName(AActor* OwningActor, FName KeyName, FName Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsName(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardObject(AActor* OwningActor, FName KeyName, UObject* Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsObject(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardRotator(AActor* OwningActor, FName KeyName, FRotator Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsRotator(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardString(AActor* OwningActor, FName KeyName, FString Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsString(KeyName,Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardVector(AActor* OwningActor, FName KeyName, FVector Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsVector(KeyName,Value);
		return true;
	}
	return false;
}

void UEFAILibrary::CustomMoveAIToLocations(const UObject* WorldContextObject, APawn* Pawn, TArray<FVector> Locations)
{
    if (!Pawn || Locations.Num() == 0)
    {
        return;
    }

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
    ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
    if (!NavData)
    {
        return;
    }

    for (FVector Location : Locations)
    {
        FPathFindingQuery Query(Pawn, *NavData, Pawn->GetActorLocation(), Location);
        FPathFindingResult Result = NavSys->FindPathSync(FNavAgentProperties{}, Query);

    	TArray<FNavPathPoint> PathPoints = Result.Path->GetPathPoints();
    	
        if (Result.Path.IsValid())
        {
            // Rotate AI to face towards the first point in the path
            FVector Direction = (PathPoints[0].Location - Pawn->GetActorLocation()).GetSafeNormal();
            FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
            Pawn->SetActorRotation(NewRotation);

            // Use AddMovementInput to move towards each point in the path
            for (int32 i = 0; i < PathPoints.Num(); i++)
            {
                FVector PointDirection = (PathPoints[i].Location - Pawn->GetActorLocation()).GetSafeNormal();
                if (i == PathPoints.Num() - 1)
                {
                    // For the last point, always move forward
                    Pawn->AddMovementInput(PointDirection);
                }
                else
                {
                    // Move towards the next point in the path
                    FVector NextDirection = (PathPoints[i + 1].Location - PathPoints[i].Location).GetSafeNormal();
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
        }
    }
}



bool UEFAILibrary::CustomAIMovetoLocation(APawn* Pawn, const FVector& Location, const float& AcceptedRadius)
{
	if (!Pawn)
	{
		return false;
	}
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld()))
	{
		if (ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
		{
			FVector CurrentLocation = Pawn->GetActorLocation();

			FPathFindingQuery Query(Pawn, *NavData, CurrentLocation, Location);
			FPathFindingResult Result = NavSys->FindPathSync(FNavAgentProperties{}, Query);
			
			if (Result.Path.IsValid())
			{
				TArray<FNavPathPoint> PathPoints = Result.Path->GetPathPoints();

				if (!PathPoints.IsValidIndex(0))
				{
					return false;
				}
				FVector Direction = (PathPoints[0].Location - CurrentLocation).GetSafeNormal();
				FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
				Pawn->SetActorRotation(NewRotation);

				for (int32 i = 0; i < PathPoints.Num(); i++)
				{
					FVector PointDirection = (PathPoints[i].Location - CurrentLocation).GetSafeNormal();
					if (i == PathPoints.Num() - 1)
					{
						Pawn->AddMovementInput(PointDirection);
					}
					else
					{
						FVector NextDirection = (PathPoints[i + 1].Location - PathPoints[i].Location).GetSafeNormal();
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
			}
		}
		
		if ((Pawn->GetActorLocation() - Location).SizeSquared() < FMath::Square(AcceptedRadius))
		{
			return true;
		}
	}

	return false;
}

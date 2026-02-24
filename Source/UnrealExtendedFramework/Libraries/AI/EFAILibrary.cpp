// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAILibrary.h"

#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"


// ================================ PRIVATE HELPER ================================

UBlackboardComponent* UEFAILibrary::GetBlackboardSafe(AActor* OwningActor)
{
	return OwningActor ? UAIBlueprintHelperLibrary::GetBlackboard(OwningActor) : nullptr;
}


// ================================ BLACKBOARD HELPER ================================

UBlackboardComponent* UEFAILibrary::GetBlackboardComponent(AActor* OwningActor)
{
	return GetBlackboardSafe(OwningActor);
}


// ================================ NAV MESH ================================

void UEFAILibrary::ForceRebuildNavigationMesh(const UObject* WorldContextObject)
{
	// UE 5.6: GetWorldFromContextObjectChecked is deprecated.
	// Use GetWorldFromContextObject with LogAndReturnNull instead.
	if (!WorldContextObject)
	{
		return;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			NavSys->Build();
		}
	}
}


// ================================ BLACKBOARD GETTERS ================================

bool UEFAILibrary::ExtendedGetBlackboardBool(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsBool(KeyName);
	return false;
}

TSubclassOf<UObject> UEFAILibrary::ExtendedGetBlackboardClass(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsClass(KeyName);
	return nullptr;
}

uint8 UEFAILibrary::ExtendedGetBlackboardEnum(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsEnum(KeyName);
	// BUG FIX: Previously returned -1 which wraps to 255 as uint8.
	// Return 0 for consistency with other getters.
	return 0;
}

float UEFAILibrary::ExtendedGetBlackboardFloat(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsFloat(KeyName);
	return 0.f;
}

int32 UEFAILibrary::ExtendedGetBlackboardInt(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsInt(KeyName);
	return 0;
}

FName UEFAILibrary::ExtendedGetBlackboardName(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsName(KeyName);
	return FName();
}

UObject* UEFAILibrary::ExtendedGetBlackboardObject(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsObject(KeyName);
	return nullptr;
}

FRotator UEFAILibrary::ExtendedGetBlackboardRotator(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsRotator(KeyName);
	return FRotator();
}

FString UEFAILibrary::ExtendedGetBlackboardString(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsString(KeyName);
	return FString();
}

FVector UEFAILibrary::ExtendedGetBlackboardVector(AActor* OwningActor, FName KeyName)
{
	if (const auto* Blackboard = GetBlackboardSafe(OwningActor))
		return Blackboard->GetValueAsVector(KeyName);
	return FVector();
}


// ================================ BLACKBOARD SETTERS ================================

bool UEFAILibrary::ExtendedSetBlackboardBool(AActor* OwningActor, FName KeyName, bool Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsBool(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardClass(AActor* OwningActor, FName KeyName, TSubclassOf<UObject> Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsClass(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardEnum(AActor* OwningActor, FName KeyName, uint8 Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsEnum(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardFloat(AActor* OwningActor, FName KeyName, float Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsFloat(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardInt(AActor* OwningActor, FName KeyName, int32 Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsInt(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardName(AActor* OwningActor, FName KeyName, FName Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsName(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardObject(AActor* OwningActor, FName KeyName, UObject* Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsObject(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardRotator(AActor* OwningActor, FName KeyName, FRotator Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsRotator(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardString(AActor* OwningActor, FName KeyName, FString Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsString(KeyName, Value);
		return true;
	}
	return false;
}

bool UEFAILibrary::ExtendedSetBlackboardVector(AActor* OwningActor, FName KeyName, FVector Value)
{
	if (auto* Blackboard = GetBlackboardSafe(OwningActor))
	{
		Blackboard->SetValueAsVector(KeyName, Value);
		return true;
	}
	return false;
}


// ================================ MOVEMENT ================================

void UEFAILibrary::CustomMoveAIToLocations(const UObject* WorldContextObject, APawn* Pawn, TArray<FVector> Locations)
{
	if (!Pawn || Locations.Num() == 0)
	{
		return;
	}

	// BUG FIX: Added null check for NavSys — crashes if no navigation system exists in the world.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	if (!NavSys)
	{
		return;
	}

	ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!NavData)
	{
		return;
	}

	for (const FVector& Location : Locations)
	{
		FPathFindingQuery Query(Pawn, *NavData, Pawn->GetActorLocation(), Location);
		FPathFindingResult Result = NavSys->FindPathSync(FNavAgentProperties{}, Query);

		if (Result.Path.IsValid())
		{
			const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();
			if (PathPoints.Num() == 0) continue;

			// Rotate AI to face towards the first point in the path
			FVector Direction = (PathPoints[0].Location - Pawn->GetActorLocation()).GetSafeNormal();
			FRotator NewRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
			Pawn->SetActorRotation(NewRotation);

			// NOTE: AddMovementInput only queues input for this frame.
			// Only the first movement segment will be effective per call.
			// This function must be called every tick for continuous movement.
			for (int32 i = 0; i < PathPoints.Num(); i++)
			{
				FVector PointDirection = (PathPoints[i].Location - Pawn->GetActorLocation()).GetSafeNormal();
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
}

bool UEFAILibrary::CustomAIMovetoLocation(APawn* Pawn, const FVector& Location, const float& AcceptedRadius)
{
	if (!Pawn)
	{
		return false;
	}

	// BUG FIX: Check accepted radius BEFORE computing path.
	// Previously the early-out only happened after path-following, wasting resources.
	if ((Pawn->GetActorLocation() - Location).SizeSquared() < FMath::Square(AcceptedRadius))
	{
		return true;
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
				const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();

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
	}

	return false;
}

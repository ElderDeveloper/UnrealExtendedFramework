// Fill out your copyright notice in the Description page of Project Settings.


#include "EGEnemySpawner.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Data/EGEnemySpawnerInterface.h"
#include "Engine/DataTable.h"


// Sets default values for this component's properties
UEGEnemySpawner::UEGEnemySpawner()
{
	PrimaryComponentTick.bCanEverTick = true;
}



void UEGEnemySpawner::ActivateSpawnBehavior()
{
	bIsSpawning = true;
	OnSpawnBehaviorBegin();
	GetWorld()->GetTimerManager().SetTimer( SpawnTimerHandle , this , &UEGEnemySpawner::TimerSpawnPawn , SpawnWorldActorLoopTime , true , 0.f);
}



void UEGEnemySpawner::DeActivateSpawnBehavior()
{
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	bIsSpawning = false;
	if (bDestroyAllOnDeactivation)
	{
		for (int32 i = 0; i < SpawnedActors.Num(); i++)
		{
			if (SpawnedActors[i])
			{
				if (SpawnedActors[i]->GetClass()->ImplementsInterface(UEGEnemySpawnerInterface::StaticClass()))
				{
					IEGEnemySpawnerInterface::Execute_OnSpawnerDestroyPawn(SpawnedActors[i]);
				}
				else
				{
					SpawnedActors[i]->Destroy();
				}
			}
		}
	}
}

void UEGEnemySpawner::PauseSpawnBehavior()
{
	if (bIsSpawning)
	{
		GetWorld()->GetTimerManager().PauseTimer(SpawnTimerHandle);
	}
}

void UEGEnemySpawner::UnPauseSpawnBehavior()
{
	if(bIsSpawning)
	{
		GetWorld()->GetTimerManager().UnPauseTimer(SpawnTimerHandle);
	}
}


void UEGEnemySpawner::OnSpawnBehaviorBegin()
{
	if (OnBeginSystemSpawnerDataTable)
	{
		TArray<FName> Names = OnBeginSystemSpawnerDataTable->GetRowNames();

		for (const auto& Name : Names)
		{
			if (const FEnemySpawnerEnemyData* Data = OnBeginSystemSpawnerDataTable->FindRow<FEnemySpawnerEnemyData>(Name, ""))
			{
				for (int32 i = 0 ; i < Data->SpawnCount ; ++i)
				{
					FVector SpawnLocation;
					if (FindSpawnLocation(SpawnLocation))
					{
						Spawn(SpawnLocation , *Data);
					}
				}
			}
		}
	}
}


void UEGEnemySpawner::TimerSpawnPawn()
{
	FEnemySpawnerEnemyData SpawnData;
	if (SelectSpawnData(SpawnData))
	{
		FVector SpawnLocation;
		if (FindSpawnLocation(SpawnLocation))
		{
			Spawn(SpawnLocation , SpawnData);
		}
	}
}



void UEGEnemySpawner::GetSpawnData()
{
	if (EnemySpawnerDataTable)
	{
		TArray<FName> Names = EnemySpawnerDataTable->GetRowNames();

		for (const auto& Name : Names)
		{
			if (const FEnemySpawnerEnemyData* Data = EnemySpawnerDataTable->FindRow<FEnemySpawnerEnemyData>(Name, ""))
			{
				SpawnedEnemyData.Add(*Data);
			}
		}
	}
}



bool UEGEnemySpawner::SelectSpawnData(FEnemySpawnerEnemyData& OutSpawnData)
{
	const float Weight = FMath::FRand() * 100;

	TArray<FEnemySpawnerEnemyData> PossibleData;

	for (const auto& Data : SpawnedEnemyData)
	{
		if (Data.SpawnWeight >= Weight)
		{
			PossibleData.Add(Data);
		}
	}

	if (PossibleData.Num() > 0)
	{
		OutSpawnData = PossibleData[FMath::RandRange(0, PossibleData.Num() - 1)];
		return true;
	}
	
	return false;
}



bool UEGEnemySpawner::FindSpawnLocation(FVector& OutLocation) const
{
	if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation RandomPoint;
		if (NavSys->GetRandomPointInNavigableRadius(GetComponentLocation(), SphereRadius , RandomPoint))
		{
			OutLocation = RandomPoint.Location;
			return true;
		}
	}
	return false;
}



void UEGEnemySpawner::OnActorDestroyed(AActor* DestroyedActor)
{
	SpawnedActors.Remove(DestroyedActor);
	DestroyedActorCount++;
	OnEnemyDestroyed.Broadcast(DestroyedActor,DestroyedActorCount);
}



void UEGEnemySpawner::Spawn(const FVector& Location , const FEnemySpawnerEnemyData& Data)
{
	if (CheckCanSpawn())
	{
		if (APawn* Pawn = UAIBlueprintHelperLibrary::SpawnAIFromClass( GetWorld() , Data.PawnClass , Data.BehaviorTree , Location , FRotator::ZeroRotator , true , GetOwner()))
		{
			SpawnedActors.Add(Pawn);
			SpawnedActorCount++;
			Pawn->OnDestroyed.AddDynamic(this , &UEGEnemySpawner::OnActorDestroyed);
			OnEnemySpawned.Broadcast(Pawn,SpawnedActorCount);
			
			if (Pawn->GetClass()->ImplementsInterface(UEGEnemySpawnerInterface::StaticClass()))
			{
				IEGEnemySpawnerInterface::Execute_OnSpawnedBySpawner(Pawn, this);
			}
		}
	}
}



bool UEGEnemySpawner::CheckCanSpawn()
{
	if (bUnlimitedSpawn)
	{
		return true;
	}

	for(int32 i = SpawnedActors.Num() - 1 ; i >= 0 ; --i)
	{
		if (SpawnedActors[i] == nullptr)
		{
			SpawnedActors.RemoveAt(i);
		}
	}

	return SpawnedActors.Num() < SpawnLimit;
}



void UEGEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	GetSpawnData();
}


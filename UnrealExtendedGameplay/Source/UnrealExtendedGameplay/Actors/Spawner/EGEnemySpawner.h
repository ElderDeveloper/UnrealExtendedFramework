// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EGEnemySpawnerData.h"
#include "Components/SphereComponent.h"
#include "EGEnemySpawner.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemySpawned , AActor* , SpawnedActor , int32 , SpawnedActorCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDestroyed , AActor* , DestroyedActor , int32 , DestroyedActorCount);


class UDataTable;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGEnemySpawner : public USphereComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGEnemySpawner();

	UPROPERTY( BlueprintAssignable , Category = "Spawn Behavior")
	FOnEnemySpawned OnEnemySpawned;
	
	UPROPERTY( BlueprintAssignable , Category = "Spawn Behavior")
	FOnEnemyDestroyed OnEnemyDestroyed;

	UPROPERTY(EditDefaultsOnly , Category= "Spawn Behavior")
	UDataTable* OnBeginSystemSpawnerDataTable;

	UPROPERTY(EditDefaultsOnly , Category= "Spawn Behavior")
	UDataTable* EnemySpawnerDataTable;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Behavior")
	AActor* SpawnAroundActor;
	
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Behavior")
	float SpawnWorldActorLoopTime;

 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Behavior")
	bool bUnlimitedSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Behavior")
	int32 SpawnLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Behavior")
	bool bDestroyAllOnDeactivation;
	
 	UFUNCTION(BlueprintCallable , Category = "Spawn Behavior")
	void ActivateSpawnBehavior();
	
 	UFUNCTION(BlueprintCallable , Category = "Spawn Behavior")
	void DeActivateSpawnBehavior();

	UFUNCTION(BlueprintCallable , Category = "Spawn Behavior")
	void PauseSpawnBehavior();

	UFUNCTION(BlueprintCallable , Category = "Spawn Behavior")
	void UnPauseSpawnBehavior();
	
protected:
	bool bIsSpawning;
	FTimerHandle SpawnTimerHandle;
	int32 DestroyedActorCount;
	int32 SpawnedActorCount;
	
	UPROPERTY()
	TArray<AActor*> SpawnedActors;
	TArray<FEnemySpawnerEnemyData> SpawnedEnemyData;

	UFUNCTION()
	void OnActorDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void TimerSpawnPawn();
	
	void OnSpawnBehaviorBegin();
	void GetSpawnData();
	bool SelectSpawnData(FEnemySpawnerEnemyData& OutSpawnData);
	bool FindSpawnLocation(FVector& OutLocation) const;
	void Spawn(const FVector& Location , const FEnemySpawnerEnemyData& Data);
	bool CheckCanSpawn();
	
	virtual void BeginPlay() override;

public:

	UFUNCTION(BlueprintPure , Category = "Spawn Behavior")
	int32 GetTotalSpawnedActorCount() const { return SpawnedActorCount; }

	UFUNCTION(BlueprintPure , Category = "Spawn Behavior")
	int32 GetTotalDestroyedActorCount() const { return DestroyedActorCount; }

	UFUNCTION(BlueprintPure , Category = "Spawn Behavior")
	int32 GetAliveActorCount() const { return SpawnedActors.Num(); }
	
	UFUNCTION(BlueprintPure , Category = "Spawn Behavior")
	TArray<AActor*> GetSpawnedActors() const { return SpawnedActors; }
	
};

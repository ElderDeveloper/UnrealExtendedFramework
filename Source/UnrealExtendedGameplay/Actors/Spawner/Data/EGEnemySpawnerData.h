// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "EGEnemySpawnerData.generated.h"

class UBehaviorTree;

USTRUCT(BlueprintType)
struct FEnemySpawnerEnemyData :  public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UBehaviorTree* BehaviorTree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnWeight;

	// This is only used when the spawn system is activated
	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	int32 SpawnCount = 0;
};

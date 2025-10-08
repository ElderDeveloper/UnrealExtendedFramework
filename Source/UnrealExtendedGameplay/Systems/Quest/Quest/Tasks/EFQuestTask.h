// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFQuestTask.generated.h"

/**
 * 
 */
UCLASS(Blueprintable,BlueprintType,Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEFQuestTask : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Task")
	FString TaskId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Task")
	FText TaskDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Task")
	int32 CurrentProgress = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Task")
	int32 TargetProgress = 1;

	UFUNCTION(BlueprintCallable, Category = "Quest Task")
	void IncrementProgress(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Quest Task")
	bool IsTaskCompleted() const;
};

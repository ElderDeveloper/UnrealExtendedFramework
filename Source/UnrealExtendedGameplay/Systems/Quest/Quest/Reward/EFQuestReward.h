// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFQuestReward.generated.h"

/**
 * 
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDGAMEPLAY_API UEFQuestReward : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Reward")
	FString RewardId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest Reward")
	FText RewardDescription;

	// Implement reward logic here
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable, Category = "Quest Reward")
	void GrantReward(AActor* Owner);
};

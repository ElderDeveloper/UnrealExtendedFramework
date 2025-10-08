// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFQuestRequirements.generated.h"

/**
 * 
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDGAMEPLAY_API UEFQuestRequirements : public UObject
{
	GENERATED_BODY()


public:

	// Check if the requirements are met for the given actor (e.g., player)
	UFUNCTION(BlueprintNativeEvent,BlueprintPure)
	bool IsRequirementsMet(const AActor* Owner);
};

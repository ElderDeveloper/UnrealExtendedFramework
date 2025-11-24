// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "EFSettingsSaveGame.generated.h"

/**
 * SaveGame object to store all modular settings.
 * We store values as strings mapped to GameplayTags for maximum flexibility.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	
	// Map of Setting Tag -> Value as String
	UPROPERTY(VisibleAnywhere, Category = "Settings")
	TMap<FGameplayTag, FString> StoredSettings;
};

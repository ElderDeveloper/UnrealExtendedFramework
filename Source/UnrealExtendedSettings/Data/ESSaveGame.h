// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ESData.h"
#include "GameFramework/SaveGame.h"
#include "ESSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	FExtendedSettingsSaveStruct UESettingsSaveStruct;
};

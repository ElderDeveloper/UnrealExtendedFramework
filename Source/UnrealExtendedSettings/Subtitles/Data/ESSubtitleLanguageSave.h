// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ESSubtitleData.h"
#include "GameFramework/SaveGame.h"
#include "UnrealExtendedSettings/Data/ESData.h"
#include "ESSubtitleLanguageSave.generated.h"


UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSubtitleLanguageSave : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly)
	FExtendedSubtitleLanguageSettings CurrentLanguage;

	UESSubtitleLanguageSave()
	{
		CurrentLanguage = FExtendedSubtitleLanguageSettings();
	}
};

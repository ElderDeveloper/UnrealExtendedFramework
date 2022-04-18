// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ESSubtitleLanguageSave.generated.h"


UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSubtitleLanguageSave : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly)
	FExtendedSubtitleLanguageSettings CurrentLanguage;
};

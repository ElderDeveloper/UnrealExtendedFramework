// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "UObject/Object.h"
#include "ESSubtitleData.generated.h"



/*
	This is a possible language file. 
	<?xml version="1.0" encoding="UTF-8"?>
	<rooting>
	<text key="NewGame" string="New Game" />
	<text key="Settings" string="Settings" />
	<text key="QuitGame" string="Quit Game" />
	  <text key="Landmark Edition LOC" string="Landmark Edition" />
	  <text key="SaveGameTitle" string="Dear Esther Save File Changed Location" />
	  <text key="Dialouge1" string="Where am I" />
	  <text key="Disabled" string="Disabled" />
	  <text key="Start" string="Start Over" />
	</rooting>
 */

USTRUCT(BlueprintType)
struct FExtendedSubtitleLanguageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	FGameplayTag LanguageTag;
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	UDataTable* LanguageSubtitleDataTable;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	EFProjectDirectory LanguageAssetProjectDirectory;
	
	//Important! This Directory Uses Game Content Directory
	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	FString AssetDirectory;
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FSlateFontInfo Font;

	FExtendedSubtitleLanguageSettings()
	{
		LanguageTag = FGameplayTag::EmptyTag;
		LanguageSubtitleDataTable = nullptr;
		LanguageAssetProjectDirectory = EFProjectDirectory::ProjectContentDir;
		AssetDirectory = "";
		Font = FSlateFontInfo();
	}
	
};

 
USTRUCT(BlueprintType)
struct FExtendedSubtitle : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere , BlueprintReadOnly)
	FString Subtitle;

	UPROPERTY(VisibleAnywhere , BlueprintReadOnly)
	float Duration;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	USoundBase* SubtitleSound;
	
	FExtendedSubtitle()
	{
		Subtitle = "";
		Duration = 0;
		SubtitleSound = nullptr;
	}
};


UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSubtitleData : public UObject
{
	GENERATED_BODY()
};

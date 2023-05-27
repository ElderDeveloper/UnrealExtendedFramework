// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "ESLanguageSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FXmlLanguageFileLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadWrite)
	FString LanguageDisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Xml")
	EFProjectDirectory LanguageAssetProjectDirectory = EFProjectDirectory::ProjectConfigDir;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Xml")
	FString FilePath = "Languages/";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Xml")
	bool bEnabled = true;
};

UCLASS(Config = "DefaultSubtitlePlugin")
class UNREALEXTENDEDSETTINGS_API UESLanguageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category = "Language")
	TMap<FGameplayTag , FXmlLanguageFileLocation>LanguageFiles;

	UPROPERTY(Config, EditAnywhere, Category = "Language")
	FGameplayTag CurrentLanguageTag;

	UFUNCTION(BlueprintPure)
	FString GetCurrentLanguageFileDirection();
};

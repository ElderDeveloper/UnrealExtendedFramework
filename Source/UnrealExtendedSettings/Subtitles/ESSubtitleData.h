// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/Data/EFEnums.h"
#include "Engine/DataTable.h"
#include "ESSubtitleData.generated.h"





USTRUCT(BlueprintType)
struct FExtendedSubtitleLanguageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	FGameplayTag LanguageTag;
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	UDataTable* LanguageSubtitleDataTable;

	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	TEnumAsByte<EFProjectDirectory> LanguageAssetProjectDirectory = UEF_ProjectContentDir;
	
	//Important! This Directory Uses Game Content Directory
	UPROPERTY(EditAnywhere , BlueprintReadOnly )
	FString AssetDirectory;
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly)
	FSlateFontInfo Font;
	
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
};


UCLASS()
class UNREALEXTENDEDSETTINGS_API UESSubtitleData : public UObject
{
	GENERATED_BODY()
};

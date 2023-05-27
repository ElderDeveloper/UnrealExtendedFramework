// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ESLanguageLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDSETTINGS_API UESLanguageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure , meta = (WorldContext = "WorldContextObject") )
	static FText GetLanguageString(const UObject* WorldContextObject,FString Key);

	UFUNCTION(BlueprintCallable)
	static void ReadXmlFile(const FString& FilePath ,TArray<FString>& Keys , TArray<FString>& Values);
	
	UFUNCTION(BlueprintCallable)
	static FString ConvertXmlToJsonString(const FString& XmlFilePath);

	UFUNCTION(BlueprintCallable)
	static void FillSubtitleDataTable(UDataTable* DataTable ,const FString& XmlFilePath);
};

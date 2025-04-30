// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFDataTableLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUtiliesNode, Log, All);
class UDataTable;

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFDataTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

/** Output entire contents of table as CSV string*/
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV String", Category = "DataTable")
	static void GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString);

/** Output entire contents of table as CSV File */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV File", Category = "DataTable")
	static void GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath);
	
};




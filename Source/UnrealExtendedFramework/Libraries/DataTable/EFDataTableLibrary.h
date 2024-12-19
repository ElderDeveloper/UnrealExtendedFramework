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
	
	// <<<<<<<<<<<<<<<<<<<<<< Experiment >>>>>>>>>>>>>>>>>>
	/*
	static int32 GetRowNumber(UDataTable* DataTable);
	static bool AddRow(UDataTable* DataTable, FName RowName , UStruct RowData);
	
	static bool AddRows(UDataTable* DataTable , TMap<FName,UStruct> RowMap);
	static bool RemoveRow(UDataTable* DataTable, FName RowName);
	static void ClearTable(UDataTable* DataTable);
	static UProperty* GetSinglePropertyOfDataTable(UDataTable* DataTable , FName RowName , FName PropertyName );
	static bool SetSinglePropertyOfDataTable(UDataTable* DataTable , FName RowName , FName PropertyName , UProperty* Value);
	static void GetArrayPropertyOfDataTable(UDataTable* DataTable , FName RowName , FName PropertyName , TArray<UProperty*>& Values);
	static void SetArrayPropertyOfDataTable(UDataTable* DataTable , FName RowName , FName PropertyName , TArray<UProperty*> Values);
	*/
	

};




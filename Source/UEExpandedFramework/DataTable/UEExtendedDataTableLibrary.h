// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UEExtendedDataTableLibrary.generated.h"

/**
 * 
 */

// Declare General Log Category, header file .h
DECLARE_LOG_CATEGORY_EXTERN(LogUtiliesNode, Log, All);

UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedDataTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:
	/**
	* Empty and fill a Data Table from CSV string.
	* @param	CSVString	The Data that representing the contents of a CSV file.
	* @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Fill Data Table from CSV String", Category = "DataTable")
	static bool FillDataTableFromCSVString(UDataTable* DataTable, const FString& CSVString);


	/**
	* Empty and fill a Data Table from CSV file.
	* @param	CSVFilePath	The file path of the CSV file.
	* @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Fill Data Table from CSV File", Category = "DataTable")
	static bool FillDataTableFromCSVFile(UDataTable* DataTable, const FString& CSVFilePath);


	/**
	* Empty and fill a Data Table from JSON string.
	* @param	JSONString	The Data that representing the contents of a JSON file.
	* @return	True if the operation succeeds, check the log for errors if it didn't succeed.
	*/
	UFUNCTION(BlueprintCallable, DisplayName = "Fill Data Table from JSON String", Category = "DataTable")
	static bool FillDataTableFromJSONString(UDataTable* DataTable, const FString& JSONString);


	/**
		* Empty and fill a Data Table from JSON file.
		* @param	JSONFilePath	The file path of the JSON file.
		* @return	True if the operation succeeds, check the log for errors if it didn't succeed.
		*/
	UFUNCTION(BlueprintCallable, DisplayName = "Fill Data Table from JSON File", Category = "DataTable")
	static bool FillDataTableFromJSONFile(UDataTable* DataTable, const FString& JSONFilePath);


	/** Output entire contents of table as CSV string */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV String", Category = "DataTable")
		static void GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString);

	/** Output entire contents of table as CSV File */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV File", Category = "DataTable")
		static void GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath);

	
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFDataTableLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUtilitiesNode, Log, All);
class UDataTable;

/**
 * Blueprint function library providing extended DataTable utilities including
 * CSV/JSON export, CSV import, and row counting.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFDataTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Exports the entire contents of a DataTable as a CSV-formatted string.
	 * The first row contains column headers derived from the row struct properties.
	 * @param DataTable The DataTable to export
	 * @param CSVString The resulting CSV string (empty if DataTable is invalid)
	 */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV String", Category = "DataTable")
	static void GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString);

	/**
	 * Exports the entire contents of a DataTable to a CSV file on disk (UTF-8 encoded).
	 * @param DataTable The DataTable to export
	 * @param CSVFilePath The absolute file path to write the CSV file to
	 * @return True if the file was written successfully, false on failure
	 */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As CSV File", Category = "DataTable")
	static bool GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath);

	/**
	 * Exports the entire contents of a DataTable as a JSON-formatted string.
	 * Each row becomes a JSON object with property names as keys.
	 * @param DataTable The DataTable to export
	 * @param JSONString The resulting JSON string (empty if DataTable is invalid)
	 */
	UFUNCTION(BlueprintCallable, DisplayName = "Get Table As JSON String", Category = "DataTable")
	static void GetDataTableAsJSON(UDataTable* DataTable, FString& JSONString);

	/**
	 * Returns the number of rows in the given DataTable.
	 * @param DataTable The DataTable to query
	 * @return The number of rows, or 0 if the DataTable is invalid
	 */
	UFUNCTION(BlueprintPure, DisplayName = "Get Data Table Row Count", Category = "DataTable")
	static int32 GetDataTableRowCount(const UDataTable* DataTable);

	/**
	 * Imports a CSV file from disk into an existing DataTable, replacing its current contents.
	 * The CSV must match the DataTable's row struct format.
	 * @param DataTable The DataTable to import into (must already have a valid RowStruct)
	 * @param CSVFilePath The absolute file path of the CSV file to import
	 * @param ErrorMessages Any errors encountered during import (empty on success)
	 * @return True if the import succeeded without errors
	 */
	UFUNCTION(BlueprintCallable, DisplayName = "Import CSV To Data Table", Category = "DataTable")
	static bool ImportCSVToDataTable(UDataTable* DataTable, const FString& CSVFilePath, TArray<FString>& ErrorMessages);
	
};

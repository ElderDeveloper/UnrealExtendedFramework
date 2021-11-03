// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedDataTableLibrary.h"

#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Kismet/DataTableFunctionLibrary.h"
#include "DataTableUtils.h"


bool UUEExtendedDataTableLibrary::FillDataTableFromCSVString(UDataTable* DataTable, const FString& CSVString)
{
	if (!DataTable || (CSVString.Len() == 0))
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromCSVString -> Can't fill DataTable with CSVString: %."), *CSVString);
		return false;
	}
	// Call bulit-in function
	TArray<FString> Errors = DataTable->CreateTableFromCSVString(CSVString);
	if (Errors.Num())
	{
		// It has some error message
		for (const FString& Error : Errors)
		{
			UE_LOG(LogUtiliesNode, Warning, TEXT("%s"), *Error);
		}
		return false;
	}

	return true;
}

bool UUEExtendedDataTableLibrary::FillDataTableFromCSVFile(UDataTable* DataTable, const FString& CSVFilePath)
{
	FString CSVString;
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CSVFilePath))
	{
		// Supports all combination of ANSI/Unicode files and platforms.
		FFileHelper::LoadFileToString(CSVString, *CSVFilePath);
	}
	else
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromCSVFile -> Cannot find CSV file %s"), *CSVFilePath);
		return false;
	}
	return UUEExtendedDataTableLibrary::FillDataTableFromCSVString(DataTable, CSVString);
}

bool UUEExtendedDataTableLibrary::FillDataTableFromJSONString(UDataTable* DataTable, const FString& JSONString)
{
	if (!DataTable || (JSONString.Len() == 0))
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromJSONString -> Can't fill DataTable with JSONString: %."), *JSONString);
		return false;
	}
	// Call bulit-in function
	TArray<FString> Errors = DataTable->CreateTableFromJSONString(JSONString);

	if (Errors.Num())
	{
		// It has some error message
		for (const FString& Error : Errors)
		{
			UE_LOG(LogUtiliesNode, Warning, TEXT("%s"), *Error);
		}
		return false;
	}

	return true;
}

bool UUEExtendedDataTableLibrary::FillDataTableFromJSONFile(UDataTable* DataTable, const FString& JSONFilePath)
{
	FString JSONString;
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*JSONFilePath))
	{
		// Supports all combination of ANSI/Unicode files and platforms.
		FFileHelper::LoadFileToString(JSONString, *JSONFilePath);
	}
	else
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromJSONFile -> Cannot find CSV file %s"), *JSONFilePath);
		return false;
	}
	return UUEExtendedDataTableLibrary::FillDataTableFromJSONString(DataTable, JSONString);
}

void UUEExtendedDataTableLibrary::GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString)
{
	CSVString = FString();

	if (!DataTable || (DataTable->RowStruct == nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("UUEExtendedDataTableLibrary::GetTableAsCSV : Missing DataTable or RowStruct !"));
		return;
	}

	// First build array of properties
	TArray<FProperty*> StructProps;
	for (TFieldIterator<FProperty> It(DataTable->RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		check(Prop != nullptr);
		StructProps.Add(Prop);
	}

	// First row, column titles, taken from properties
	CSVString += TEXT("---");
	for (int32 PropIdx = 0; PropIdx < StructProps.Num(); PropIdx++)
	{
		CSVString += TEXT(",");
		CSVString += StructProps[PropIdx]->GetName();
	}
	CSVString += TEXT("\n");

	// Now iterate over rows
	for (auto RowIt = DataTable->GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		FName RowName = RowIt.Key();
		CSVString += RowName.ToString();

		uint8* RowData = RowIt.Value();
		for (int32 PropIdx = 0; PropIdx < StructProps.Num(); PropIdx++)
		{
			CSVString += TEXT(",");
			CSVString += DataTableUtils::GetPropertyValueAsString(StructProps[PropIdx], RowData, EDataTableExportFlags::None);
		}
		CSVString += TEXT("\n");
	}
}



void UUEExtendedDataTableLibrary::GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath)
{
	FString CSVString;
	UUEExtendedDataTableLibrary::GetDataTableAsCSVString(DataTable, CSVString);
	if (CSVString.Len() == 0)
	{
		return;
	}
	FFileHelper::SaveStringToFile(CSVString, *CSVFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
}
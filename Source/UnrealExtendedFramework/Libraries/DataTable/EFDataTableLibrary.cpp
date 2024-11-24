// Fill out your copyright notice in the Description page of Project Settings.


#include "EFDataTableLibrary.h"

#include "Engine/DataTable.h"


DEFINE_LOG_CATEGORY(LogUtiliesNode);

bool UEFDataTableLibrary::FillDataTableFromCSVString(UDataTable* DataTable, const FString& CSVString)
{
	if (!DataTable || (CSVString.Len() == 0))
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromCSVString -> Can't fill DataTable with CSVString: %s"), *CSVString);
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

bool UEFDataTableLibrary::FillDataTableFromCSVFile(UDataTable* DataTable, const FString& CSVFilePath)
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
	return UEFDataTableLibrary::FillDataTableFromCSVString(DataTable, CSVString);
}

bool UEFDataTableLibrary::FillDataTableFromJSONString(UDataTable* DataTable, const FString& JSONString)
{
	if (!DataTable || (JSONString.Len() == 0))
	{
		UE_LOG(LogUtiliesNode, Warning, TEXT("FillDataTableFromJSONString -> Can't fill DataTable with JSONString: %s"), *JSONString);
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

bool UEFDataTableLibrary::FillDataTableFromJSONFile(UDataTable* DataTable, const FString& JSONFilePath)
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
	return UEFDataTableLibrary::FillDataTableFromJSONString(DataTable, JSONString);
}

void UEFDataTableLibrary::GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString)
{
	CSVString = FString();

	if (!DataTable || (DataTable->RowStruct == nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGenericMiscLibrary::GetTableAsCSV : Missing DataTable or RowStruct !"));
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

		const uint8* RowData = RowIt.Value();
		for (int32 PropIdx = 0; PropIdx < StructProps.Num(); PropIdx++)
		{
			CSVString += TEXT(",");
			CSVString += DataTableUtils::GetPropertyValueAsString(StructProps[PropIdx], RowData, EDataTableExportFlags::None);
		}
		CSVString += TEXT("\n");
	}
}



void UEFDataTableLibrary::GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath)
{
	FString CSVString;
	UEFDataTableLibrary::GetDataTableAsCSVString(DataTable, CSVString);
	if (CSVString.Len() == 0)
	{
		return;
	}
	FFileHelper::SaveStringToFile(CSVString, *CSVFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "EFDataTableLibrary.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


DEFINE_LOG_CATEGORY(LogUtilitiesNode);


void UEFDataTableLibrary::GetDataTableAsCSVString(UDataTable* DataTable, FString& CSVString)
{
	CSVString = FString();

	if (!DataTable || (DataTable->RowStruct == nullptr))
	{
		UE_LOG(LogUtilitiesNode, Warning, TEXT("UEFDataTableLibrary::GetDataTableAsCSVString : Missing DataTable or RowStruct!"));
		return;
	}

	// Build array of properties from the row struct
	TArray<FProperty*> StructProps;
	for (TFieldIterator<FProperty> It(DataTable->RowStruct); It; ++It)
	{
		FProperty* Prop = *It;
		check(Prop != nullptr);
		StructProps.Add(Prop);
	}

	// First row: column titles derived from property names
	CSVString += TEXT("---");
	for (int32 PropIdx = 0; PropIdx < StructProps.Num(); PropIdx++)
	{
		CSVString += TEXT(",");
		CSVString += StructProps[PropIdx]->GetName();
	}
	CSVString += TEXT("\n");

	// Subsequent rows: each row's data values
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


bool UEFDataTableLibrary::GetDataTableAsCSVFile(UDataTable* DataTable, const FString& CSVFilePath)
{
	FString CSVString;
	UEFDataTableLibrary::GetDataTableAsCSVString(DataTable, CSVString);
	if (CSVString.Len() == 0)
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(CSVString, *CSVFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
}


void UEFDataTableLibrary::GetDataTableAsJSON(UDataTable* DataTable, FString& JSONString)
{
	JSONString = FString();

	if (!DataTable || (DataTable->RowStruct == nullptr))
	{
		UE_LOG(LogUtilitiesNode, Warning, TEXT("UEFDataTableLibrary::GetDataTableAsJSON : Missing DataTable or RowStruct!"));
		return;
	}

	// Build JSON array of row objects manually (GetTableAsJSON removed in UE 5.6)
	TArray<FString> RowJsonStrings;
	TArray<FProperty*> StructProps;
	for (TFieldIterator<FProperty> It(DataTable->RowStruct); It; ++It)
	{
		StructProps.Add(*It);
	}

	for (auto RowIt = DataTable->GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		const FName RowName = RowIt.Key();
		const uint8* RowData = RowIt.Value();

		FString RowJson = TEXT("{");
		RowJson += FString::Printf(TEXT("\"Name\":\"%s\""), *RowName.ToString());

		for (FProperty* Prop : StructProps)
		{
			FString ValueStr = DataTableUtils::GetPropertyValueAsString(Prop, RowData, EDataTableExportFlags::None);
			ValueStr.ReplaceInline(TEXT("\""), TEXT("\\\""));
			RowJson += FString::Printf(TEXT(",\"%s\":\"%s\""), *Prop->GetName(), *ValueStr);
		}
		RowJson += TEXT("}");
		RowJsonStrings.Add(RowJson);
	}

	JSONString = TEXT("[") + FString::Join(RowJsonStrings, TEXT(",")) + TEXT("]");
}


int32 UEFDataTableLibrary::GetDataTableRowCount(const UDataTable* DataTable)
{
	if (!DataTable)
	{
		return 0;
	}
	return DataTable->GetRowMap().Num();
}


bool UEFDataTableLibrary::ImportCSVToDataTable(UDataTable* DataTable, const FString& CSVFilePath, TArray<FString>& ErrorMessages)
{
	ErrorMessages.Empty();

	if (!DataTable)
	{
		ErrorMessages.Add(TEXT("DataTable is null."));
		return false;
	}

	if (DataTable->RowStruct == nullptr)
	{
		ErrorMessages.Add(TEXT("DataTable has no RowStruct assigned."));
		return false;
	}

	FString CSVContent;
	if (!FFileHelper::LoadFileToString(CSVContent, *CSVFilePath))
	{
		ErrorMessages.Add(FString::Printf(TEXT("Failed to load file: %s"), *CSVFilePath));
		return false;
	}

	// Use the engine's built-in CSV import which handles all column mapping
	TArray<FString> ImportProblems = DataTable->CreateTableFromCSVString(CSVContent);

	if (ImportProblems.Num() > 0)
	{
		ErrorMessages = ImportProblems;
		for (const FString& Problem : ImportProblems)
		{
			UE_LOG(LogUtilitiesNode, Warning, TEXT("UEFDataTableLibrary::ImportCSVToDataTable : %s"), *Problem);
		}
		return false;
	}

	return true;
}
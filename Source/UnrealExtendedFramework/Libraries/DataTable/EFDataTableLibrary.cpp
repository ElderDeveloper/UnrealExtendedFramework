// Fill out your copyright notice in the Description page of Project Settings.


#include "EFDataTableLibrary.h"

#include "Engine/DataTable.h"


DEFINE_LOG_CATEGORY(LogUtiliesNode);


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
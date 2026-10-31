// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: DataTable export to and import from CSV and JSON strings.

#include "EFFileLibrary.h"

#include "Engine/DataTable.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"

bool UEFFileLibrary::DatatableToCSV(UDataTable* Table, FString& Output)
{
	if (Table == nullptr || !Table->RowStruct)
	{
		return false;
	}
	return UEFFileLibrary::WriteTableToCSV(*Table, Output);
	// Output = Table->GetTableAsCSV(EDataTableExportFlags::None);
	// return true;
}

bool UEFFileLibrary::DataTableToJSON(UDataTable* Table, FString& Output)
{
	if (Table == nullptr || !Table->RowStruct)
	{
		return false;
	}
	return UEFFileLibrary::WriteTableToJSON(*Table, Output);
	// Output = Table->GetTableAsJSON(EDataTableExportFlags::UseJsonObjectsForStructs);
	// return true;
}

UDataTable* UEFFileLibrary::CSVToDataTable(FString CSV, UScriptStruct* Struct, bool& Success)
{
	Success = false;
	if (Struct == nullptr)
	{
		return nullptr;
	}
	UDataTable* DataTable = NewObject<UDataTable>();
	DataTable->RowStruct = Struct;
	auto Result = DataTable->CreateTableFromCSVString(CSV);
	if (Result.Num() == 0)
	{
		Success = true;
	}
	return DataTable;
}

UDataTable* UEFFileLibrary::JSONToDataTable(FString JSON, UScriptStruct* Struct, bool& Success)
{
	Success = false;
	if (Struct == nullptr)
	{
		return nullptr;
	}
	UDataTable* DataTable = NewObject<UDataTable>();
	DataTable->RowStruct = Struct;
	auto Result = DataTable->CreateTableFromJSONString(JSON);
	if (Result.Num() == 0)
	{
		Success = true;
	}
	return DataTable;
}

// equivalent GetTableAsCSV()

bool UEFFileLibrary::WriteTableToCSV(const UDataTable& InDataTable, FString& ExportedText)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	// Write the header (column titles)
	FString ImportKeyField;
	if (!InDataTable.ImportKeyField.IsEmpty())
	{
		// Write actual name if we have it
		ImportKeyField = InDataTable.ImportKeyField;
		ExportedText += ImportKeyField;
	}
	else
	{
		ExportedText += TEXT("---");
	}

	FProperty* SkipProperty = nullptr;
	for (TFieldIterator<FProperty> It(InDataTable.RowStruct); It; ++It)
	{
		FProperty* BaseProp = *It;
		check(BaseProp);

		FString ColumnHeader = DataTableUtils::GetPropertyExportName(BaseProp, EDataTableExportFlags::None);

		if (ColumnHeader == ImportKeyField)
		{
			// Don't write header again if this is the name field, and save for skipping later
			SkipProperty = BaseProp;
			continue;
		}

		ExportedText += TEXT(",");
		ExportedText += ColumnHeader;
	}
	ExportedText += TEXT("\n");

	// Write each row
	for (auto RowIt = InDataTable.GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		FName RowName = RowIt.Key();
		ExportedText += RowName.ToString();

		uint8* RowData = RowIt.Value();
		UEFFileLibrary::WriteRowToCSV(InDataTable.RowStruct, RowData, ExportedText);

		ExportedText += TEXT("\n");
	}

	return true;
}

bool UEFFileLibrary::WriteRowToCSV(const UScriptStruct* InRowStruct, const void* InRowData, FString& ExportedText)
{
	if (!InRowStruct)
	{
		return false;
	}

	for (TFieldIterator<FProperty> It(InRowStruct); It; ++It)
	{
		FProperty* BaseProp = *It;
		check(BaseProp);

		const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InRowData, 0);
		UEFFileLibrary::WriteStructEntryToCSV(InRowData, BaseProp, Data, ExportedText);
	}

	return true;
}

bool UEFFileLibrary::WriteStructEntryToCSV(const void* InRowData, FProperty* InProperty, const void* InPropertyData,
                                           FString& ExportedText)
{
	ExportedText += TEXT(",");

	const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(
		InProperty, (uint8*)InRowData, EDataTableExportFlags::None);
	ExportedText += TEXT("\"");
	ExportedText += PropertyValue.Replace(TEXT("\""), TEXT("\"\""));
	ExportedText += TEXT("\"");

	return true;
}

// equivalent GetTableAsJSON()

FString UEFFileLibrary::GetKeyFieldName(const UDataTable& InDataTable)
{
	FString ExplicitString = InDataTable.ImportKeyField;
	if (ExplicitString.IsEmpty())
	{
		return TEXT("Name");
	}
	else
	{
		return ExplicitString;
	}
}

bool UEFFileLibrary::WriteTableToJSON(const UDataTable& InDataTable, FString& OutExportText)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<
		TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutExportText);

	FString KeyField = UEFFileLibrary::GetKeyFieldName(InDataTable);

	JsonWriter->WriteArrayStart();

	// Iterate over rows
	for (auto RowIt = InDataTable.GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		JsonWriter->WriteObjectStart();
		{
			// RowName
			const FName RowName = RowIt.Key();
			JsonWriter->WriteValue(KeyField, RowName.ToString());

			// Now the values
			uint8* RowData = RowIt.Value();
			UEFFileLibrary::WriteRowToJSON(InDataTable.RowStruct, RowData, JsonWriter);
		}
		JsonWriter->WriteObjectEnd();
	}

	JsonWriter->WriteArrayEnd();

	JsonWriter->Close();

	return true;
}

bool UEFFileLibrary::WriteTableAsObjectToJSON(const UDataTable& InDataTable,
                                              TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	JsonWriter->WriteObjectStart(InDataTable.GetName());

	// Iterate over rows
	for (auto RowIt = InDataTable.GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		// RowName
		const FName RowName = RowIt.Key();
		JsonWriter->WriteObjectStart(RowName.ToString());
		{
			// Now the values
			uint8* RowData = RowIt.Value();
			UEFFileLibrary::WriteRowToJSON(InDataTable.RowStruct, RowData, JsonWriter);
		}
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteObjectEnd();

	return true;
}

bool UEFFileLibrary::WriteRowToJSON(const UScriptStruct* InRowStruct, const void* InRowData,
                                    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter)
{
	if (!InRowStruct)
	{
		return false;
	}

	return UEFFileLibrary::WriteStructToJSON(InRowStruct, InRowData, JsonWriter);
}

bool UEFFileLibrary::WriteStructToJSON(const UScriptStruct* InStruct, const void* InStructData,
                                       TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter)
{
	for (TFieldIterator<const FProperty> It(InStruct); It; ++It)
	{
		const FProperty* BaseProp = *It;
		check(BaseProp);

		const FString Identifier = DataTableUtils::GetPropertyExportName(
			BaseProp, EDataTableExportFlags::UseJsonObjectsForStructs);

		if (BaseProp->ArrayDim == 1)
		{
			const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, 0);
			UEFFileLibrary::WriteStructEntryToJSON(InStructData, BaseProp, Data, JsonWriter);
		}
		else
		{
			JsonWriter->WriteArrayStart(Identifier);

			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < BaseProp->ArrayDim; ++ArrayEntryIndex)
			{
				const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, ArrayEntryIndex);
				UEFFileLibrary::WriteContainerEntryToJSON(BaseProp, Data, &Identifier, JsonWriter);
			}

			JsonWriter->WriteArrayEnd();
		}
	}

	return true;
}

bool UEFFileLibrary::WriteStructEntryToJSON(const void* InRowData, const FProperty* InProperty,
                                            const void* InPropertyData,
                                            TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter)
{
	const FString Identifier = DataTableUtils::GetPropertyExportName(
		InProperty, EDataTableExportFlags::UseJsonObjectsForStructs);

	if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(
			EnumProp, (uint8*)InRowData, EDataTableExportFlags::UseJsonObjectsForStructs);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const FNumericProperty* NumProp = CastField<const FNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(
				InProperty, (uint8*)InRowData, EDataTableExportFlags::UseJsonObjectsForStructs);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
	}
	else if (const FBoolProperty* BoolProp = CastField<const FBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			UEFFileLibrary::WriteContainerEntryToJSON(ArrayProp->Inner, ArrayEntryData, &Identifier, JsonWriter);
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const FSetProperty* SetProp = CastField<const FSetProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		for (int32 SetSparseIndex = 0; SetSparseIndex < SetHelper.GetMaxIndex(); ++SetSparseIndex)
		{
			if (SetHelper.IsValidIndex(SetSparseIndex))
			{
				const uint8* SetEntryData = SetHelper.GetElementPtr(SetSparseIndex);
				UEFFileLibrary::WriteContainerEntryToJSON(SetHelper.GetElementProperty(), SetEntryData, &Identifier,
				                                          JsonWriter);
			}
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const FMapProperty* MapProp = CastField<const FMapProperty>(InProperty))
	{
		JsonWriter->WriteObjectStart(Identifier);

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		for (int32 MapSparseIndex = 0; MapSparseIndex < MapHelper.GetMaxIndex(); ++MapSparseIndex)
		{
			if (MapHelper.IsValidIndex(MapSparseIndex))
			{
				const uint8* MapKeyData = MapHelper.GetKeyPtr(MapSparseIndex);
				const uint8* MapValueData = MapHelper.GetValuePtr(MapSparseIndex);

				// JSON object keys must always be strings
				const FString KeyValue = DataTableUtils::GetPropertyValueAsStringDirect(
					MapHelper.GetKeyProperty(), (uint8*)MapKeyData, EDataTableExportFlags::UseJsonObjectsForStructs);
				UEFFileLibrary::WriteContainerEntryToJSON(MapHelper.GetValueProperty(), MapValueData, &KeyValue,
				                                          JsonWriter);
			}
		}

		JsonWriter->WriteObjectEnd();
	}
	else if (const FStructProperty* StructProp = CastField<const FStructProperty>(InProperty))
	{
		JsonWriter->WriteObjectStart(Identifier);
		UEFFileLibrary::WriteStructToJSON(StructProp->Struct, InPropertyData, JsonWriter);
		JsonWriter->WriteObjectEnd();
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(
			InProperty, (uint8*)InRowData, EDataTableExportFlags::UseJsonObjectsForStructs);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}

	return true;
}

bool UEFFileLibrary::WriteContainerEntryToJSON(const FProperty* InProperty, const void* InPropertyData,
                                               const FString* InIdentifier,
                                               TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter)
{
	if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(
			InProperty, (uint8*)InPropertyData, EDataTableExportFlags::UseJsonObjectsForStructs);
		UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, *PropertyValue);
	}
	else if (const FNumericProperty* NumProp = CastField<const FNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(
				InProperty, (uint8*)InPropertyData, EDataTableExportFlags::UseJsonObjectsForStructs);
			UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, *PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier,
			                                                     *FString::FromInt(PropertyValue));
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier,
			                                                     *FString::SanitizeFloat(PropertyValue));
		}
	}
	else if (const FBoolProperty* BoolProp = CastField<const FBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier,
		                                                     *(PropertyValue ? FString("true") : FString("false")));
	}
	else if (const FStructProperty* StructProp = CastField<const FStructProperty>(InProperty))
	{
		UEFFileLibrary::WriteJSONObjectStartWithOptionalIdentifier(JsonWriter, InIdentifier);
		UEFFileLibrary::WriteStructToJSON(StructProp->Struct, InPropertyData, JsonWriter);
		JsonWriter->WriteObjectEnd();
	}
	else if (const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else if (const FSetProperty* SetProp = CastField<const FSetProperty>(InProperty))
	{
		// Cannot nest sets
		return false;
	}
	else if (const FMapProperty* MapProp = CastField<const FMapProperty>(InProperty))
	{
		// Cannot nest maps
		return false;
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(
			InProperty, (uint8*)InPropertyData, EDataTableExportFlags::UseJsonObjectsForStructs);
		UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, *PropertyValue);
	}

	return true;
}

void UEFFileLibrary::WriteJSONObjectStartWithOptionalIdentifier(
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter, const FString* InIdentifier)
{
	if (InIdentifier)
	{
		JsonWriter->WriteObjectStart(*InIdentifier);
	}
	else
	{
		JsonWriter->WriteObjectStart();
	}
}

void UEFFileLibrary::WriteJSONValueWithOptionalIdentifier(
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter, const FString* InIdentifier,
	const TCHAR* InValue)
{
	if (InIdentifier)
	{
		JsonWriter->WriteValue(*InIdentifier, InValue);
	}
	else
	{
		JsonWriter->WriteValue(InValue);
	}
}

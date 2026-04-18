// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Blueprint/ESQLBlueprintBridge.h"

#include "PlayerData/ESQLPlayerDBComponent.h"
#include "Shared/ESQLPropertySerializer.h"
#include "TableAsset/ESQLTableAsset.h"
#include "UObject/UnrealType.h"

namespace
{
FESQLQuerySpec BuildSingleFilterQuerySpec(const FName FieldName, const EESQLFilterOperation Operation, const FESQLBindingValue& Value)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Filters.Add(FESQLFieldFilter(FieldName.ToString(), Operation, Value));
	return QuerySpec;
}

FESQLQuerySpec BuildPagedSingleFilterQuerySpec(const FName FieldName, const EESQLFilterOperation Operation, const FESQLBindingValue& Value, int32 PageIndex, int32 PageSize)
{
	FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, Operation, Value);
	QuerySpec.Limit = PageSize;
	QuerySpec.Offset = PageIndex * PageSize;
	return QuerySpec;
}

FESQLQueryResult LoadSQLBridgeRow(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FString& RowId,
	FProperty* StructProperty,
	void* StructData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Load SQL Row requires a valid output struct pin"));
	}

	return TableAsset->LoadRowIntoStruct(WorldContextObject, RowId, StructData, StructProp->Struct);
}

FESQLQueryResult FindFirstSQLBridgeRowByField(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FName FieldName,
	const EESQLFilterOperation Operation,
	const FESQLBindingValue& Value,
	FProperty* StructProperty,
	void* StructData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Find First SQL Row By Field requires a valid output struct pin"));
	}

	FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, Operation, Value);
	QuerySpec.Limit = 1;
	const FESQLQueryResult QueryResult = TableAsset->QueryRows(WorldContextObject, QuerySpec);
	if (!QueryResult.bSuccess)
	{
		return QueryResult;
	}

	return TableAsset->PopulateQueryResultIntoStruct(QueryResult, StructData, StructProp->Struct);
}

FESQLQueryResult PopulateSQLBridgeRowFromResult(
	UESQLTableAsset* TableAsset,
	const FESQLQueryResult& QueryResult,
	FProperty* StructProperty,
	void* StructData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Populate SQL Row From Result requires a valid output struct pin"));
	}

	return TableAsset->PopulateQueryResultIntoStruct(QueryResult, StructData, StructProp->Struct);
}

FESQLQueryResult LoadPlayerSQLBridgeRow(
	UESQLPlayerDBComponent* PlayerDBComponent,
	UESQLTableAsset* TableAsset,
	const FString& RowId,
	FProperty* StructProperty,
	void* StructData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Load Player SQL Row requires a valid output struct pin"));
	}

	return TableAsset->LoadPlayerRowIntoStruct(PlayerDBComponent, RowId, StructData, StructProp->Struct);
}

FESQLQueryResult LoadSQLBridgeRows(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	int32 MaxRows,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Load SQL Rows requires a valid output array pin"));
	}

	return TableAsset->LoadRowsIntoStructArray(WorldContextObject, ArrayData, ArrayProp, MaxRows);
}

FESQLQueryResult PopulateSQLBridgeRowsFromResult(
	UESQLTableAsset* TableAsset,
	const FESQLQueryResult& QueryResult,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Populate SQL Rows From Result requires a valid output array pin"));
	}

	return TableAsset->PopulateQueryResultIntoStructArray(QueryResult, ArrayData, ArrayProp);
}

FESQLQueryResult FindSQLBridgeRows(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FESQLQuerySpec& QuerySpec,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Find SQL Rows requires a valid output array pin"));
	}

	return TableAsset->QueryRowsIntoStructArray(WorldContextObject, QuerySpec, ArrayData, ArrayProp);
}

FESQLQueryResult LoadSQLBridgePage(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FESQLQuerySpec& BaseQuerySpec,
	int32 PageIndex,
	int32 PageSize,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Load SQL Page requires a valid output array pin"));
	}

	return TableAsset->LoadPageIntoStructArray(WorldContextObject, BaseQuerySpec, PageIndex, PageSize, ArrayData, ArrayProp);
}

FESQLQueryResult LoadSQLBridgePageByField(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FName FieldName,
	const EESQLFilterOperation Operation,
	const FESQLBindingValue& Value,
	int32 PageIndex,
	int32 PageSize,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Load SQL Page By Field requires a valid output array pin"));
	}

	const FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, Operation, Value);
	return TableAsset->LoadPageIntoStructArray(WorldContextObject, QuerySpec, PageIndex, PageSize, ArrayData, ArrayProp);
}

FESQLQueryResult LoadPlayerSQLBridgeRows(
	UESQLPlayerDBComponent* PlayerDBComponent,
	UESQLTableAsset* TableAsset,
	int32 MaxRows,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Load Player SQL Rows requires a valid output array pin"));
	}

	return TableAsset->LoadPlayerRowsIntoStructArray(PlayerDBComponent, ArrayData, ArrayProp, MaxRows);
}

FESQLQueryResult FindPlayerSQLBridgeRows(
	UESQLPlayerDBComponent* PlayerDBComponent,
	UESQLTableAsset* TableAsset,
	const FESQLQuerySpec& QuerySpec,
	FProperty* ArrayProperty,
	void* ArrayData)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(ArrayProperty);
	if (!ArrayProp || !ArrayData)
	{
		return FESQLQueryResult::Failure(TEXT("Find Player SQL Rows requires a valid output array pin"));
	}

	return TableAsset->QueryPlayerRowsIntoStructArray(PlayerDBComponent, QuerySpec, ArrayData, ArrayProp);
}

FESQLQueryResult SaveSQLBridgeRow(
	UObject* WorldContextObject,
	UESQLTableAsset* TableAsset,
	const FString& RowIdOverride,
	FProperty* StructProperty,
	void* StructData,
	FString& OutResolvedRowId)
{
	OutResolvedRowId.Reset();

	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Save SQL Row requires a valid input struct pin"));
	}

	return TableAsset->SaveRowFromStruct(WorldContextObject, StructData, StructProp->Struct, &OutResolvedRowId, RowIdOverride);
}

FESQLQueryResult SavePlayerSQLBridgeRow(
	UESQLPlayerDBComponent* PlayerDBComponent,
	UESQLTableAsset* TableAsset,
	const FString& RowIdOverride,
	FProperty* StructProperty,
	void* StructData,
	FString& OutResolvedRowId)
{
	OutResolvedRowId.Reset();

	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FStructProperty* StructProp = CastField<FStructProperty>(StructProperty);
	if (!StructProp || !StructData || !StructProp->Struct)
	{
		return FESQLQueryResult::Failure(TEXT("Save Player SQL Row requires a valid input struct pin"));
	}

	return TableAsset->SavePlayerRowFromStruct(PlayerDBComponent, StructData, StructProp->Struct, &OutResolvedRowId, RowIdOverride);
}
}

bool UESQLBlueprintBridge::DoesSQLRowExist(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowId, FString& OutError)
{
	if (!TableAsset)
	{
		OutError = TEXT("TableAsset is null");
		return false;
	}

	return TableAsset->DoesRowExist(WorldContextObject, RowId, &OutError);
}

bool UESQLBlueprintBridge::DoesSQLRowExistById(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FESQLId SqlId, FString& OutError)
{
	if (!TableAsset)
	{
		OutError = TEXT("TableAsset is null");
		return false;
	}

	return TableAsset->DoesRowExist(WorldContextObject, SqlId, &OutError);
}

FESQLQueryResult UESQLBlueprintBridge::DeleteSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowId)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->DeleteRowById(WorldContextObject, RowId);
}

FESQLQueryResult UESQLBlueprintBridge::DeleteSQLRowById(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FESQLId SqlId)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->DeleteRowById(WorldContextObject, SqlId);
}

FESQLQueryResult UESQLBlueprintBridge::QuerySQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->QueryRows(WorldContextObject, QuerySpec);
}

FESQLQueryResult UESQLBlueprintBridge::CountSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount)
{
	OutCount = 0;

	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->CountRows(WorldContextObject, QuerySpec, OutCount);
}

bool UESQLBlueprintBridge::IsSQLQueryResultSuccessful(const FESQLQueryResult& QueryResult)
{
	return QueryResult.bSuccess;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execMakeSQLBindingValue)
{
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	FProperty* ValueProperty = Stack.MostRecentProperty;
	void* ValueData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	FESQLBindingValue BindingValue;
	if (!ValueProperty || !ValueData || !FESQLPropertySerializer::SerializePropertyValueToBindingValue(ValueProperty, ValueData, BindingValue))
	{
		BindingValue = FESQLBindingValue::Null();
	}

	*(FESQLBindingValue*)RESULT_PARAM = BindingValue;
}

FESQLFieldFilter UESQLBlueprintBridge::MakeSQLFieldFilter(const FName FieldName, const EESQLFilterOperation Operation, const FESQLBindingValue& Value, const TArray<FESQLBindingValue>& Values)
{
	FESQLFieldFilter Filter;
	Filter.FieldName = FieldName.ToString();
	Filter.Operation = Operation;
	if (Values.Num() > 0)
	{
		Filter.Values = Values;
	}
	else
	{
		Filter.Values.Add(Value);
	}
	return Filter;
}

FESQLSortRule UESQLBlueprintBridge::MakeSQLSortRule(const FName FieldName, const bool bAscending)
{
	return FESQLSortRule(FieldName.ToString(), bAscending);
}

FESQLQuerySpec UESQLBlueprintBridge::MakeSQLQuerySpec(const TArray<FESQLFieldFilter>& Filters, const TArray<FESQLSortRule>& SortRules, const int32 Limit, const int32 Offset)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Filters = Filters;
	QuerySpec.SortRules = SortRules;
	QuerySpec.Limit = Limit;
	QuerySpec.Offset = Offset;
	return QuerySpec;
}

FESQLQueryResult UESQLBlueprintBridge::CountSQLRowsByField(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int64& OutCount)
{
	OutCount = 0;

	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	const FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, Operation, Value);
	return TableAsset->CountRows(WorldContextObject, QuerySpec, OutCount);
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execPopulateSQLRowFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = PopulateSQLBridgeRowFromResult(TableAsset, QueryResult, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execPopulateSQLRowsFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = PopulateSQLBridgeRowsFromResult(TableAsset, QueryResult, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadSQLRowById)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FStrProperty, RowId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadSQLBridgeRow(WorldContextObject, TableAsset, RowId, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadSQLRow)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLId, SqlId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadSQLBridgeRow(WorldContextObject, TableAsset, SqlId.Value, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execSaveSQLRow)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY_REF(FStrProperty, OutResolvedRowId);
	P_GET_PROPERTY(FStrProperty, RowIdOverride);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = SaveSQLBridgeRow(WorldContextObject, TableAsset, RowIdOverride, StructProperty, StructData, OutResolvedRowId);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadSQLRows)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FIntProperty, MaxRows);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadSQLBridgeRows(WorldContextObject, TableAsset, MaxRows, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execFindSQLRows)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQuerySpec, QuerySpec);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = FindSQLBridgeRows(WorldContextObject, TableAsset, QuerySpec, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execFindSQLRowsByField)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQuerySpec QuerySpec = BuildSingleFilterQuerySpec(FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value);
	const FESQLQueryResult Result = FindSQLBridgeRows(WorldContextObject, TableAsset, QuerySpec, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execFindFirstSQLRowByField)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = FindFirstSQLBridgeRowByField(WorldContextObject, TableAsset, FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadSQLPage)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQuerySpec, BaseQuerySpec);
	P_GET_PROPERTY(FIntProperty, PageIndex);
	P_GET_PROPERTY(FIntProperty, PageSize);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadSQLBridgePage(WorldContextObject, TableAsset, BaseQuerySpec, PageIndex, PageSize, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadSQLPageByField)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FNameProperty, FieldName);
	P_GET_ENUM(EESQLFilterOperation, OperationValue);
	P_GET_STRUCT(FESQLBindingValue, Value);
	P_GET_PROPERTY(FIntProperty, PageIndex);
	P_GET_PROPERTY(FIntProperty, PageSize);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadSQLBridgePageByField(WorldContextObject, TableAsset, FieldName, static_cast<EESQLFilterOperation>(OperationValue), Value, PageIndex, PageSize, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

FESQLQueryResult UESQLBlueprintBridge::DeletePlayerSQLRow(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FString& RowId)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->DeletePlayerRowById(PlayerDBComponent, RowId);
}

FESQLQueryResult UESQLBlueprintBridge::DeletePlayerSQLRowById(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLId& SqlId)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->DeletePlayerRowById(PlayerDBComponent, SqlId);
}

FESQLQueryResult UESQLBlueprintBridge::QueryPlayerSQLRows(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec)
{
	if (!TableAsset)
	{
		return FESQLQueryResult::Failure(TEXT("TableAsset is null"));
	}

	return TableAsset->QueryPlayerRows(PlayerDBComponent, QuerySpec);
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadPlayerSQLRowById)
{
	P_GET_OBJECT(UESQLPlayerDBComponent, PlayerDBComponent);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FStrProperty, RowId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadPlayerSQLBridgeRow(PlayerDBComponent, TableAsset, RowId, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadPlayerSQLRow)
{
	P_GET_OBJECT(UESQLPlayerDBComponent, PlayerDBComponent);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLId, SqlId);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadPlayerSQLBridgeRow(PlayerDBComponent, TableAsset, SqlId.Value, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execSavePlayerSQLRow)
{
	P_GET_OBJECT(UESQLPlayerDBComponent, PlayerDBComponent);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY_REF(FStrProperty, OutResolvedRowId);
	P_GET_PROPERTY(FStrProperty, RowIdOverride);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = SavePlayerSQLBridgeRow(PlayerDBComponent, TableAsset, RowIdOverride, StructProperty, StructData, OutResolvedRowId);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execLoadPlayerSQLRows)
{
	P_GET_OBJECT(UESQLPlayerDBComponent, PlayerDBComponent);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_PROPERTY(FIntProperty, MaxRows);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = LoadPlayerSQLBridgeRows(PlayerDBComponent, TableAsset, MaxRows, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintBridge::execFindPlayerSQLRows)
{
	P_GET_OBJECT(UESQLPlayerDBComponent, PlayerDBComponent);
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQuerySpec, QuerySpec);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = FindPlayerSQLBridgeRows(PlayerDBComponent, TableAsset, QuerySpec, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

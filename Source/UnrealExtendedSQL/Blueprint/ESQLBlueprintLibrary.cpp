// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Blueprint/ESQLBlueprintLibrary.h"

#include "Shared/ESQLPropertySerializer.h"
#include "TableAsset/ESQLTableAsset.h"
#include "UObject/UnrealType.h"

namespace
{
	FESQLQueryResult PopulateSQLBlueprintRowFromResult(
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

	FESQLQueryResult PopulateSQLBlueprintRowsFromResult(
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
}

bool UESQLBlueprintLibrary::IsSQLQueryResultSuccessful(const FESQLQueryResult& QueryResult)
{
	return QueryResult.bSuccess;
}

DEFINE_FUNCTION(UESQLBlueprintLibrary::execMakeSQLBindingValue)
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

FESQLFieldFilter UESQLBlueprintLibrary::MakeSQLFieldFilter(const FName FieldName, const EESQLFilterOperation Operation, const FESQLBindingValue& Value, const TArray<FESQLBindingValue>& Values)
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

FESQLSortRule UESQLBlueprintLibrary::MakeSQLSortRule(const FName FieldName, const bool bAscending)
{
	return FESQLSortRule(FieldName.ToString(), bAscending);
}

FESQLQuerySpec UESQLBlueprintLibrary::MakeSQLQuerySpec(const TArray<FESQLFieldFilter>& Filters, const TArray<FESQLSortRule>& SortRules, const int32 Limit, const int32 Offset)
{
	FESQLQuerySpec QuerySpec;
	QuerySpec.Filters = Filters;
	QuerySpec.SortRules = SortRules;
	QuerySpec.Limit = Limit;
	QuerySpec.Offset = Offset;
	return QuerySpec;
}

DEFINE_FUNCTION(UESQLBlueprintLibrary::execPopulateSQLRowFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FProperty* StructProperty = Stack.MostRecentProperty;
	void* StructData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = PopulateSQLBlueprintRowFromResult(TableAsset, QueryResult, StructProperty, StructData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}

DEFINE_FUNCTION(UESQLBlueprintLibrary::execPopulateSQLRowsFromResult)
{
	P_GET_OBJECT(UESQLTableAsset, TableAsset);
	P_GET_STRUCT(FESQLQueryResult, QueryResult);

	Stack.StepCompiledIn<FArrayProperty>(nullptr);
	FProperty* ArrayProperty = Stack.MostRecentProperty;
	void* ArrayData = Stack.MostRecentPropertyAddress;

	P_FINISH;

	const FESQLQueryResult Result = PopulateSQLBlueprintRowsFromResult(TableAsset, QueryResult, ArrayProperty, ArrayData);
	*(FESQLQueryResult*)RESULT_PARAM = Result;
}
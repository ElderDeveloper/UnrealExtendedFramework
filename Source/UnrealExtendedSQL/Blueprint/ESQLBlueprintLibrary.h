// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shared/ESQLTypes.h"
#include "ESQLBlueprintLibrary.generated.h"

class UESQLTableAsset;

UCLASS()
class UNREALEXTENDEDSQL_API UESQLBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Is SQL Query Result Successful"))
	static bool IsSQLQueryResultSuccessful(const FESQLQueryResult& QueryResult);

	UFUNCTION(
	BlueprintCallable, CustomThunk, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", DisplayName = "Make SQL Binding Value", Keywords = "construct build", NativeMakeFunc))
	static FESQLBindingValue MakeSQLBindingValue(const int32& Value);
	DECLARE_FUNCTION(execMakeSQLBindingValue);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Field Filter", Keywords = "construct build", AutoCreateRefTerm = "Values", AdvancedDisplay = "Values", NativeMakeFunc))
	static FESQLFieldFilter MakeSQLFieldFilter(FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, const TArray<FESQLBindingValue>& Values);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Sort Rule", Keywords = "construct build", NativeMakeFunc))
	static FESQLSortRule MakeSQLSortRule(FName FieldName, bool bAscending = true);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Query Spec", Keywords = "construct build", AutoCreateRefTerm = "Filters,SortRules", AdvancedDisplay = "SortRules,Limit,Offset", NativeMakeFunc))
	static FESQLQuerySpec MakeSQLQuerySpec(const TArray<FESQLFieldFilter>& Filters, const TArray<FESQLSortRule>& SortRules, int32 Limit = 0, int32 Offset = 0);

	UFUNCTION(
	BlueprintCallable, CustomThunk, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Populate SQL Row From Result"))
	static FESQLQueryResult PopulateSQLRowFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, int32& OutRow);
	DECLARE_FUNCTION(execPopulateSQLRowFromResult);

	UFUNCTION(
	BlueprintCallable, CustomThunk, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Populate SQL Rows From Result"))
	static FESQLQueryResult PopulateSQLRowsFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execPopulateSQLRowsFromResult);
};
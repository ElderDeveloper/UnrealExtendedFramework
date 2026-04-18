// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shared/ESQLId.h"
#include "Shared/ESQLTypes.h"
#include "ESQLBlueprintBridge.generated.h"

class UESQLPlayerDBComponent;
class UESQLTableAsset;

/**
 * Reflection-friendly runtime bridge for the typed SQL table layer.
 *
 * This library exposes the minimum Blueprint-callable surface K2 nodes need,
 * while keeping the real behavior on UESQLTableAsset and related C++ runtime
 * types instead of duplicating SQL logic here.
 */
UCLASS()
class UNREALEXTENDEDSQL_API UESQLBlueprintBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SQL|Bridge", meta = (DisplayName = "Is SQL Query Result Successful"))
	static bool IsSQLQueryResultSuccessful(const FESQLQueryResult& QueryResult);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", DisplayName = "Make SQL Binding Value", Keywords = "construct build", NativeMakeFunc))
	static FESQLBindingValue MakeSQLBindingValue(const int32& Value);
	DECLARE_FUNCTION(execMakeSQLBindingValue);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Field Filter", Keywords = "construct build", AutoCreateRefTerm = "Values", AdvancedDisplay = "Values", NativeMakeFunc))
	static FESQLFieldFilter MakeSQLFieldFilter(FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, const TArray<FESQLBindingValue>& Values);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Sort Rule", Keywords = "construct build", NativeMakeFunc))
	static FESQLSortRule MakeSQLSortRule(FName FieldName, bool bAscending = true);

	UFUNCTION(BlueprintPure, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Make SQL Query Spec", Keywords = "construct build", AutoCreateRefTerm = "Filters,SortRules", AdvancedDisplay = "SortRules,Limit,Offset", NativeMakeFunc))
	static FESQLQuerySpec MakeSQLQuerySpec(const TArray<FESQLFieldFilter>& Filters, const TArray<FESQLSortRule>& SortRules, int32 Limit = 0, int32 Offset = 0);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Does SQL Row Exist", AdvancedDisplay = "OutError"))
	static bool DoesSQLRowExist(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (WorldContext = "WorldContextObject", DisplayName = "Does SQL Row Exist", AdvancedDisplay = "OutError"))
	static bool DoesSQLRowExistById(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FESQLId SqlId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Delete SQL Row"))
	static FESQLQueryResult DeleteSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowId);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (WorldContext = "WorldContextObject", DisplayName = "Delete SQL Row"))
	static FESQLQueryResult DeleteSQLRowById(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FESQLId SqlId);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Query SQL Rows"))
	static FESQLQueryResult QuerySQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Count SQL Rows"))
	static FESQLQueryResult CountSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Count SQL Rows By Field"))
	static FESQLQueryResult CountSQLRowsByField(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int64& OutCount);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Populate SQL Row From Result"))
	static FESQLQueryResult PopulateSQLRowFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, int32& OutRow);
	DECLARE_FUNCTION(execPopulateSQLRowFromResult);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Populate SQL Rows From Result"))
	static FESQLQueryResult PopulateSQLRowsFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execPopulateSQLRowsFromResult);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CustomStructureParam = "OutRow", DisplayName = "Load SQL Row By Id"))
	static FESQLQueryResult LoadSQLRowById(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FString& RowId, int32& OutRow);
	DECLARE_FUNCTION(execLoadSQLRowById);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CustomStructureParam = "OutRow", DisplayName = "Load SQL Row"))
	static FESQLQueryResult LoadSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLId& SqlId, int32& OutRow);
	DECLARE_FUNCTION(execLoadSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CustomStructureParam = "RowData", DisplayName = "Save SQL Row", AdvancedDisplay = "RowIdOverride"))
	static FESQLQueryResult SaveSQLRow(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FString& OutResolvedRowId, const FString& RowIdOverride, const int32& RowData);
	DECLARE_FUNCTION(execSaveSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", ArrayParm = "OutRows", DisplayName = "Load SQL Rows", AdvancedDisplay = "MaxRows"))
	static FESQLQueryResult LoadSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, int32 MaxRows, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLRows);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", ArrayParm = "OutRows", DisplayName = "Find SQL Rows"))
	static FESQLQueryResult FindSQLRows(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execFindSQLRows);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", ArrayParm = "OutRows", DisplayName = "Find SQL Rows By Field"))
	static FESQLQueryResult FindSQLRowsByField(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execFindSQLRowsByField);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", CustomStructureParam = "OutRow", DisplayName = "Find First SQL Row By Field"))
	static FESQLQueryResult FindFirstSQLRowByField(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int32& OutRow);
	DECLARE_FUNCTION(execFindFirstSQLRowByField);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", ArrayParm = "OutRows", DisplayName = "Load SQL Page"))
	static FESQLQueryResult LoadSQLPage(UObject* WorldContextObject, UESQLTableAsset* TableAsset, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLPage);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", ArrayParm = "OutRows", DisplayName = "Load SQL Page By Field"))
	static FESQLQueryResult LoadSQLPageByField(UObject* WorldContextObject, UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int32 PageIndex, int32 PageSize, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLPageByField);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Delete Player SQL Row"))
	static FESQLQueryResult DeletePlayerSQLRow(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FString& RowId);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Delete Player SQL Row"))
	static FESQLQueryResult DeletePlayerSQLRowById(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLId& SqlId);

	UFUNCTION(BlueprintCallable, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Query Player SQL Rows"))
	static FESQLQueryResult QueryPlayerSQLRows(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Load Player SQL Row By Id"))
	static FESQLQueryResult LoadPlayerSQLRowById(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FString& RowId, int32& OutRow);
	DECLARE_FUNCTION(execLoadPlayerSQLRowById);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Load Player SQL Row"))
	static FESQLQueryResult LoadPlayerSQLRow(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLId& SqlId, int32& OutRow);
	DECLARE_FUNCTION(execLoadPlayerSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "RowData", DisplayName = "Save Player SQL Row", AdvancedDisplay = "RowIdOverride"))
	static FESQLQueryResult SavePlayerSQLRow(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, FString& OutResolvedRowId, const FString& RowIdOverride, const int32& RowData);
	DECLARE_FUNCTION(execSavePlayerSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Load Player SQL Rows", AdvancedDisplay = "MaxRows"))
	static FESQLQueryResult LoadPlayerSQLRows(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, int32 MaxRows, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadPlayerSQLRows);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Bridge", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Find Player SQL Rows"))
	static FESQLQueryResult FindPlayerSQLRows(UESQLPlayerDBComponent* PlayerDBComponent, UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execFindPlayerSQLRows);
};
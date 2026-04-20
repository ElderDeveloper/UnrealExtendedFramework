// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLId.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Shared/ESQLTypes.h"
#include "ESQLSubsystem.generated.h"

class FArrayProperty;
class UESQLTableAsset;
class UScriptStruct;

struct FESQLAsyncTicketState;
struct FESQLDatabaseContext;

/**
 * GameInstanceSubsystem that owns all runtime SQL routing.
 *
 * Phase D moves authority checks, database path resolution, async routing,
 * reader/writer concurrency, transactions, and typed table access behind this
 * single runtime front door.
 */
UCLASS()
class UNREALEXTENDEDSQL_API UESQLSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	FESQLQueryResult OpenDatabase(
		const FString& DatabaseName,
		EESQLDatabaseScope Scope = EESQLDatabaseScope::Local,
		EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent,
		const FString& FileName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void CloseDatabase(const FString& DatabaseName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void CloseAllDatabases();

	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	bool IsDatabaseOpen(const FString& DatabaseName) const;

	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	TArray<FString> GetOpenDatabaseNames() const;

	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	FESQLQueryResult DeleteDatabase(
		const FString& DatabaseName,
		EESQLDatabaseScope Scope = EESQLDatabaseScope::Local,
		const FString& FileName = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	FString GetDatabaseFilePath(const FString& DatabaseName) const;

	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	bool IsSessionDatabase(const FString& DatabaseName) const;

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool IsDedicatedServer() const;

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool IsListenServer() const;

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool HasServerAuthority() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetCachedNetModeForTesting(const ENetMode InNetMode)
	{
		CachedNetMode = InNetMode;
	}
#endif

	UFUNCTION(BlueprintCallable, Category = "SQL|Raw", meta = (BlueprintInternalUseOnly = "true"))
	FESQLQueryResult ExecuteSQL(const FString& DatabaseName, const FString& SQL);

	UFUNCTION(BlueprintCallable, Category = "SQL|Raw", meta = (BlueprintInternalUseOnly = "true"))
	FESQLQueryResult ExecuteSQLWithBindings(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FString>& Bindings);

	UFUNCTION(BlueprintCallable, Category = "SQL|Raw", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	FESQLQueryResult ExecuteSQLWithValues(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FESQLBindingValue>& Bindings);

	UFUNCTION(BlueprintCallable, Category = "SQL|Raw", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	FESQLQueryResult QuerySQL(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FESQLBindingValue>& Bindings);

	UFUNCTION(BlueprintCallable, Category = "SQL|Async", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	FESQLTicket AsyncExecuteSQL(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FESQLBindingValue>& Bindings,
		const FOnESQLQueryCompleteCallback& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "SQL|Async", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	FESQLTicket AsyncQuerySQL(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FESQLBindingValue>& Bindings,
		const FOnESQLQueryCompleteCallback& OnComplete);

	UFUNCTION(BlueprintCallable, Category = "SQL|Async")
	bool CancelTicket(const FESQLTicket& Ticket);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult QueryTable(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec);
	void AsyncQueryTable(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, TFunction<void(const FESQLQueryResult&)> OnComplete);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Query SQL Rows"))
	FESQLQueryResult QuerySQLRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult DeleteTableRow(UESQLTableAsset* TableAsset, const FString& RowId);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult CountTableRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Count SQL Rows"))
	FESQLQueryResult CountSQLRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Count SQL Rows By Field"))
	FESQLQueryResult CountSQLRowsByField(UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int64& OutCount);

	FESQLQueryResult LoadRowIntoStruct(
		UObject* WorldContextObject,
		UESQLTableAsset* TableAsset,
		const FString& RowId,
		void* OutStructData,
		const UScriptStruct* StructType);

	FESQLQueryResult LoadRowsIntoStructArray(
		UObject* WorldContextObject,
		UESQLTableAsset* TableAsset,
		const FESQLQuerySpec& QuerySpec,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	FESQLQueryResult SaveRowFromStruct(
		UObject* WorldContextObject,
		UESQLTableAsset* TableAsset,
		const void* StructData,
		const UScriptStruct* StructType,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""));
	void AsyncSaveRowFromStruct(
		UObject* WorldContextObject,
		UESQLTableAsset* TableAsset,
		const void* StructData,
		const UScriptStruct* StructType,
		TFunction<void(const FESQLQueryResult&, const FString&)> OnComplete,
		const FString& RowIdOverride = TEXT(""));

	FESQLQueryResult PopulateQueryResultIntoStruct(
		UESQLTableAsset* TableAsset,
		const FESQLQueryResult& QueryResult,
		void* OutStructData,
		const UScriptStruct* StructType) const;

	FESQLQueryResult PopulateQueryResultIntoStructArray(
		UESQLTableAsset* TableAsset,
		const FESQLQueryResult& QueryResult,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty) const;

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Populate SQL Row From Result"))
	FESQLQueryResult PopulateSQLRowFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, int32& OutRow);
	DECLARE_FUNCTION(execPopulateSQLRowFromResult);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Populate SQL Rows From Result"))
	FESQLQueryResult PopulateSQLRowsFromResult(UESQLTableAsset* TableAsset, const FESQLQueryResult& QueryResult, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execPopulateSQLRowsFromResult);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Load SQL Row By Id"))
	FESQLQueryResult LoadSQLRowById(UESQLTableAsset* TableAsset, const FString& RowId, int32& OutRow);
	DECLARE_FUNCTION(execLoadSQLRowById);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Load SQL Row"))
	FESQLQueryResult LoadSQLRow(UESQLTableAsset* TableAsset, const FESQLId& SqlId, int32& OutRow);
	DECLARE_FUNCTION(execLoadSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "RowData", DisplayName = "Save SQL Row", AdvancedDisplay = "RowIdOverride"))
	FESQLQueryResult SaveSQLRow(UESQLTableAsset* TableAsset, FString& OutResolvedRowId, const FString& RowIdOverride, const int32& RowData);
	DECLARE_FUNCTION(execSaveSQLRow);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Load SQL Rows", AdvancedDisplay = "MaxRows"))
	FESQLQueryResult LoadSQLRows(UESQLTableAsset* TableAsset, int32 MaxRows, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLRows);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Find SQL Rows"))
	FESQLQueryResult FindSQLRows(UESQLTableAsset* TableAsset, const FESQLQuerySpec& QuerySpec, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execFindSQLRows);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Find SQL Rows By Field"))
	FESQLQueryResult FindSQLRowsByField(UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execFindSQLRowsByField);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutRow", DisplayName = "Find First SQL Row By Field"))
	FESQLQueryResult FindFirstSQLRowByField(UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int32& OutRow);
	DECLARE_FUNCTION(execFindFirstSQLRowByField);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Load SQL Page"))
	FESQLQueryResult LoadSQLPage(UESQLTableAsset* TableAsset, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLPage);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL|Table", meta = (BlueprintInternalUseOnly = "true", ArrayParm = "OutRows", DisplayName = "Load SQL Page By Field"))
	FESQLQueryResult LoadSQLPageByField(UESQLTableAsset* TableAsset, FName FieldName, EESQLFilterOperation Operation, const FESQLBindingValue& Value, int32 PageIndex, int32 PageSize, TArray<int32>& OutRows);
	DECLARE_FUNCTION(execLoadSQLPageByField);

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	FESQLQueryResult BeginTransaction(const FString& DatabaseName, FESQLTransactionHandle& OutHandle);

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	FESQLQueryResult CommitTransaction(UPARAM(ref) FESQLTransactionHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	FESQLQueryResult RollbackTransaction(UPARAM(ref) FESQLTransactionHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = "SQL|Migration")
	FESQLQueryResult ApplyMigrations(const FString& DatabaseName, const FESQLMigrationSet& MigrationSet);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult BackupTo(const FString& DatabaseName, const FString& DestAbsolutePath);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult SaveSnapshot(
		const FString& DatabaseName,
		const FString& SlotName,
		const FString& DisplayName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult LoadSnapshot(const FString& DatabaseName, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult DeleteSnapshot(const FString& DatabaseName, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	TArray<FESQLSnapshotInfo> GetAllSnapshots(const FString& DatabaseName);

	UFUNCTION(BlueprintPure, Category = "SQL|Snapshot")
	bool DoesSnapshotExist(const FString& DatabaseName, const FString& SlotName);

	UPROPERTY(BlueprintAssignable, Category = "SQL")
	FOnESQLQueryComplete OnQueryComplete;

	UPROPERTY(BlueprintAssignable, Category = "SQL")
	FOnESQLQueryComplete OnQueryError;

private:
	void RefreshCachedNetMode();
	FString NormalizeLogicalDatabaseName(const FString& DatabaseName, EESQLDatabaseScope& InOutScope) const;
	TOptional<FESQLError> CheckScopeGate(EESQLDatabaseScope Scope, bool bWriteOperation, const FString& DatabaseName) const;
	FESQLQueryResult ResolveDatabaseOpenPath(
		const FString& DatabaseName,
		EESQLDatabaseScope Scope,
		EESQLDatabasePersistence Persistence,
		const FString& FileName,
		FString& OutResolvedDatabaseName,
		FString& OutResolvedPath,
		bool& OutReadOnly) const;
	TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe> FindDatabaseContext(const FString& DatabaseName) const;
	TSharedPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe> RegisterTicketState(const FString& DatabaseName);
	void RemoveTicketState(const FGuid& TicketId);
	void FinalizeAsyncTicket(const FGuid& TicketId, const FOnESQLQueryCompleteCallback& Callback, const FESQLQueryResult& Result);
	FESQLQueryResult RunRawRead(const FString& DatabaseName, const FString& SQL, const TArray<FESQLBindingValue>& Bindings);
	FESQLQueryResult RunRawWrite(const FString& DatabaseName, const FString& SQL, const TArray<FESQLBindingValue>& Bindings);
	FESQLQueryResult EnsureTableContext(
		UObject* WorldContextObject,
		UESQLTableAsset* TableAsset,
		bool bNeedsWrite,
		TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>& OutContext);
	FESQLQueryResult EnsureTablePrepared(UESQLTableAsset* TableAsset, const TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>& Context);
	FString ResolveSnapshotPath(const FString& DatabaseName, const FString& SlotName) const;
	FString ResolveSnapshotMetaPath(const FString& DatabaseName, const FString& SlotName) const;
	static bool IsLikelyReadOnlySql(const FString& SQL);

	mutable FCriticalSection DatabaseContextsCS;
	TMap<FString, TSharedPtr<FESQLDatabaseContext, ESPMode::ThreadSafe>> DatabaseContexts;

	mutable FCriticalSection TicketsCS;
	TMap<FGuid, TWeakPtr<FESQLAsyncTicketState, ESPMode::ThreadSafe>> TicketStates;

	TEnumAsByte<ENetMode> CachedNetMode = NM_Standalone;
};
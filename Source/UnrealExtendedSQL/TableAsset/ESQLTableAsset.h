// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/Object.h"
#include "Shared/ESQLId.h"
#include "Shared/ESQLTypes.h"
#include "Core/ESQLDatabase.h"
#include "TableAsset/ESQLStructValidator.h"
#include "ESQLTableAsset.generated.h"

class UESQLPlayerDBComponent;
class FArrayProperty;


/**
 * Schema-owning typed table contract for ExtendedSQL.
 *
 * This asset owns table identity, schema sync, primary-key behavior,
 * label behavior, and the typed row load/save surface used by both C++
 * and higher-level Blueprint/K2 workflows.
 *
 * Raw database lifecycle and raw SQL remain the responsibility of
 * UESQLSubsystem and the low-level SQL primitives.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDSQL_API UESQLTableAsset : public UObject
{
	GENERATED_BODY()

public:

	// ── Asset Metadata ───────────────────────────────────────────────────

	/** The struct that defines the row schema.
	    Set once during asset creation. The editor auto-migrates the
	    DB schema if the struct changes (add/remove columns). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SQL Table")
	const UScriptStruct* RowStruct = nullptr;

	/** The database this table lives in (logical name, same as OpenDatabase).
	    Defaults to "EditorData". Select from existing databases or type a new name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table", meta = (GetOptions = "GetDatabaseNameOptions"))
	FString DatabaseName = TEXT("EditorData");

	/** The SQL table name inside the database.
	    Defaults to the struct name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	FString TableName;

	/** Which column serves as the PRIMARY KEY in SQLite.
	    This column is managed by the system — it does NOT need to exist
	    in the RowStruct. It is auto-added as the first column.
	    Defaults to "RowName" (TEXT, auto-generated if left empty on insert).
	    Query results always include this column alongside the struct fields. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	FString PrimaryKeyColumn = TEXT("RowName");

	/** Default column to display as a human-readable label in FESQLId picker dropdowns.
	    Individual FESQLId properties can override this per property.
	    The picker shows "<DefaultLabelColumn value> (<PrimaryKeyColumn value>)".
	    If empty, only the PrimaryKeyColumn value is shown.
	    Example: Set to "display_name" so the picker shows "Iron Sword (weapon_42)". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table",
		meta = (DisplayName = "Default Label Column", GetOptions = "GetLabelColumnOptions"))
	FString DefaultLabelColumn;

	/** Database scope — controls file path and authority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;

	/** Persistence mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
	EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;


	/** Returns list of existing database names for the dropdown (GetOptions). */
	UFUNCTION()
	TArray<FString> GetDatabaseNameOptions() const;

	/** Returns picker column options derived from the primary key and row struct fields. */
	UFUNCTION()
	TArray<FString> GetLabelColumnOptions() const;


	// ── Runtime API ──────────────────────────────────────────────────────

	/** Open the underlying database, ensure the table exists,
	    and auto-migrate schema if the struct has changed.
	    Calls SyncSchema() internally. */
	UFUNCTION(BlueprintCallable, Category = "SQL Table")
	FESQLQueryResult Initialize(UObject* WorldContextObject);

	/** Insert a row from a struct instance. Uses reflection to extract values.
	    Blueprint-accessible via CustomThunk — accepts any UStruct as input pin. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL Table",
		meta = (CustomStructureParam = "RowData"))
	FESQLQueryResult InsertRowFromStruct(const int32& RowData);
	DECLARE_FUNCTION(execInsertRowFromStruct);

	/** Query all rows, returned as FESQLQueryResult.
	    MaxRows: 0 = unlimited (developer's responsibility to manage memory). */
	UFUNCTION(BlueprintCallable, Category = "SQL Table")
	FESQLQueryResult GetAllRows(int32 MaxRows = 0);

	/** Query a single row by this table's primary key value using a picker-friendly SQL Id. */
	UFUNCTION(BlueprintCallable, Category = "SQL Table")
	FESQLQueryResult GetRowById(UObject* WorldContextObject, UPARAM(DisplayName = "SQL Id") FESQLId SqlId);

	/** Get row count. */
	UFUNCTION(BlueprintPure, Category = "SQL Table")
	int32 GetRowCount();

	/** Check if the asset has been initialized (database connection is live). */
	UFUNCTION(BlueprintPure, Category = "SQL Table")
	bool IsInitialized() const;


	// ── Typed C++ Runtime API ───────────────────────────────────────────

	/** Load a single row into an existing struct instance. C++ only. */
	FESQLQueryResult LoadRowIntoStruct(
		UObject* WorldContextObject,
		const FString& RowId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Query a single row by the raw primary key value. C++ only. */
	FESQLQueryResult GetRowById(UObject* WorldContextObject, const FString& RowId);

	/** Load a single row from an FESQLId into an existing struct instance. C++ only. */
	FESQLQueryResult LoadRowIntoStruct(
		UObject* WorldContextObject,
		const FESQLId& SqlId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Load many rows into an existing array of struct instances. C++ only. */
	FESQLQueryResult LoadRowsIntoStructArray(
		UObject* WorldContextObject,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty,
		int32 MaxRows = 0);

	/** Load one page of rows into an existing array of struct instances. C++ only. */
	FESQLQueryResult LoadPageIntoStructArray(
		UObject* WorldContextObject,
		const FESQLQuerySpec& BaseQuerySpec,
		int32 PageIndex,
		int32 PageSize,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Resolve the best runtime display text for a row. Falls back to the row id when no label value is available. C++ only. */
	FText ResolveRowDisplayText(
		UObject* WorldContextObject,
		const FString& RowId,
		const FString& LabelColumnOverride = TEXT(""),
		FString* OutError = nullptr);

	/** Resolve the best runtime display text for an FESQLId, using its label override when present. C++ only. */
	FText ResolveRowDisplayText(
		UObject* WorldContextObject,
		const FESQLId& SqlId,
		FString* OutError = nullptr);

	/** Returns true if a row exists. On query failure returns false and fills OutError when provided. C++ only. */
	bool DoesRowExist(UObject* WorldContextObject, const FString& RowId, FString* OutError = nullptr);

	/** Returns true if a row exists for the given FESQLId. C++ only. */
	bool DoesRowExist(UObject* WorldContextObject, const FESQLId& SqlId, FString* OutError = nullptr);

	/** Delete one row by primary key. C++ only. */
	FESQLQueryResult DeleteRowById(UObject* WorldContextObject, const FString& RowId);

	/** Delete one row by FESQLId. C++ only. */
	FESQLQueryResult DeleteRowById(UObject* WorldContextObject, const FESQLId& SqlId);

	/** Save a single struct row using this table's primary key as the conflict key. C++ only. */
	FESQLQueryResult SaveRowFromStruct(
		UObject* WorldContextObject,
		const void* StructData,
		const UScriptStruct* StructType,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""));

	/** Load a single row for a player-scoped database into an existing struct instance. C++ only. */
	FESQLQueryResult LoadPlayerRowIntoStruct(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const FString& RowId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Load a single row for a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult LoadPlayerRowIntoStruct(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const FString& RowId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Load a single row from an FESQLId for a player-scoped database. C++ only. */
	FESQLQueryResult LoadPlayerRowIntoStruct(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const FESQLId& SqlId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Load a single row from an FESQLId for a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult LoadPlayerRowIntoStruct(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const FESQLId& SqlId,
		void* OutStructData,
		const UScriptStruct* StructType);

	/** Load many rows for a player-scoped database into an existing array of struct instances. C++ only. */
	FESQLQueryResult LoadPlayerRowsIntoStructArray(
		UESQLPlayerDBComponent* PlayerDBComponent,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty,
		int32 MaxRows = 0);

	/** Load one page of rows for a player-scoped database into an existing array of struct instances. C++ only. */
	FESQLQueryResult LoadPlayerPageIntoStructArray(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const FESQLQuerySpec& BaseQuerySpec,
		int32 PageIndex,
		int32 PageSize,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Load one page of rows for a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult LoadPlayerPageIntoStructArray(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const FESQLQuerySpec& BaseQuerySpec,
		int32 PageIndex,
		int32 PageSize,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Execute a structured query against a player-scoped database. C++ only. */
	FESQLQueryResult QueryPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec);

	/** Execute a structured query against a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult QueryPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec);

	/** Execute a structured query against a player-scoped database and populate an existing array of struct instances. C++ only. */
	FESQLQueryResult QueryPlayerRowsIntoStructArray(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const FESQLQuerySpec& QuerySpec,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Execute a structured query against a player-scoped database using an already-resolved player id and populate an existing array of struct instances. C++ only. */
	FESQLQueryResult QueryPlayerRowsIntoStructArray(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const FESQLQuerySpec& QuerySpec,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Save a single struct row against a player-scoped database. C++ only. */
	FESQLQueryResult SavePlayerRowFromStruct(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const void* StructData,
		const UScriptStruct* StructType,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""));

	/** Save a single struct row against a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult SavePlayerRowFromStruct(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const void* StructData,
		const UScriptStruct* StructType,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""));

	/** Delete one player-scoped row by primary key. C++ only. */
	FESQLQueryResult DeletePlayerRowById(UESQLPlayerDBComponent* PlayerDBComponent, const FString& RowId);

	/** Delete one player-scoped row by primary key using an already-resolved player id. C++ only. */
	FESQLQueryResult DeletePlayerRowById(UObject* WorldContextObject, const FString& PlayerId, const FString& RowId);

	/** Delete one player-scoped row by FESQLId. C++ only. */
	FESQLQueryResult DeletePlayerRowById(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLId& SqlId);

	/** Delete one player-scoped row by FESQLId using an already-resolved player id. C++ only. */
	FESQLQueryResult DeletePlayerRowById(UObject* WorldContextObject, const FString& PlayerId, const FESQLId& SqlId);

	/** Execute a structured query against this table without requiring raw SQL strings. C++ only. */
	FESQLQueryResult QueryRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec);

	/** Execute a structured query and populate an existing array of struct instances. C++ only. */
	FESQLQueryResult QueryRowsIntoStructArray(
		UObject* WorldContextObject,
		const FESQLQuerySpec& QuerySpec,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty);

	/** Count rows matching the structured query. Sort, limit, and offset are ignored so the result reflects the full filtered set. C++ only. */
	FESQLQueryResult CountRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	/** Count rows matching the structured query. C++ only. */
	bool TryCountRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError = nullptr);

	/** Count rows matching the structured query in a player-scoped database. C++ only. */
	FESQLQueryResult CountPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	/** Count rows matching the structured query in a player-scoped database using an already-resolved player id. C++ only. */
	FESQLQueryResult CountPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec, int64& OutCount);

	/** Count rows matching the structured query in a player-scoped database. C++ only. */
	bool TryCountPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError = nullptr);

	/** Count rows matching the structured query in a player-scoped database using an already-resolved player id. C++ only. */
	bool TryCountPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec, int64& OutCount, FString* OutError = nullptr);

	/** Run a row-by-id query asynchronously and return the raw SQL result on the game thread. C++ only. */
	void AsyncLoadRowResult(
		UObject* WorldContextObject,
		const FString& RowId,
		TFunction<void(const FESQLQueryResult&)> OnComplete);

	/** Run an all-rows query asynchronously and return the raw SQL result on the game thread. C++ only. */
	void AsyncLoadRowsResult(
		UObject* WorldContextObject,
		TFunction<void(const FESQLQueryResult&)> OnComplete,
		int32 MaxRows = 0);

	/** Run a structured query asynchronously and return the raw SQL result on the game thread. C++ only. */
	void AsyncQueryRowsResult(
		UObject* WorldContextObject,
		const FESQLQuerySpec& QuerySpec,
		TFunction<void(const FESQLQueryResult&)> OnComplete);

	/** Save a struct row asynchronously using the same generic typed-runtime path as the sync bridge. C++ only. */
	void AsyncSaveRowFromStruct(
		UObject* WorldContextObject,
		const void* StructData,
		const UScriptStruct* StructType,
		TFunction<void(const FESQLQueryResult&, const FString&)> OnComplete,
		const FString& RowIdOverride = TEXT(""));

	/** Populate a single struct instance from a previously captured raw SQL result. C++ only. */
	FESQLQueryResult PopulateQueryResultIntoStruct(
		const FESQLQueryResult& QueryResult,
		void* OutStructData,
		const UScriptStruct* StructType) const;

	/** Populate an array of struct instances from a previously captured raw SQL result. C++ only. */
	FESQLQueryResult PopulateQueryResultIntoStructArray(
		const FESQLQueryResult& QueryResult,
		void* OutArrayData,
		const FArrayProperty* ArrayProperty) const;

	template<typename T>
	FESQLQueryResult LoadTypedRow(UObject* WorldContextObject, const FString& RowId, T& OutRow)
	{
		return LoadRowIntoStruct(WorldContextObject, RowId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	bool TryLoadTypedRow(UObject* WorldContextObject, const FString& RowId, T& OutRow, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadTypedRow(WorldContextObject, RowId, OutRow);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult LoadTypedRow(UObject* WorldContextObject, const FESQLId& SqlId, T& OutRow)
	{
		return LoadRowIntoStruct(WorldContextObject, SqlId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	bool TryLoadTypedRow(UObject* WorldContextObject, const FESQLId& SqlId, T& OutRow, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadTypedRow(WorldContextObject, SqlId, OutRow);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult LoadTypedRows(UObject* WorldContextObject, TArray<T>& OutRows, int32 MaxRows = 0)
	{
		OutRows.Reset();

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			return ValidationResult;
		}

		const FESQLQueryResult InitResult = Initialize(WorldContextObject);
		if (!InitResult.bSuccess)
		{
			return InitResult;
		}

		const FESQLQueryResult QueryResult = GetAllRows(MaxRows);
		if (!QueryResult.bSuccess)
		{
			return QueryResult;
		}

		OutRows.Reserve(QueryResult.Rows.Num());
		for (const FESQLRow& Row : QueryResult.Rows)
		{
			T LoadedRow{};
			const FESQLQueryResult RowResult = PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
			if (!RowResult.bSuccess)
			{
				OutRows.Reset();
				return RowResult;
			}

			OutRows.Add(MoveTemp(LoadedRow));
		}

		return QueryResult;
	}

	template<typename T>
	bool TryLoadTypedRows(UObject* WorldContextObject, TArray<T>& OutRows, int32 MaxRows = 0, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadTypedRows(WorldContextObject, OutRows, MaxRows);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult FindRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, TArray<T>& OutRows)
	{
		OutRows.Reset();

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			return ValidationResult;
		}

		const FESQLQueryResult QueryResult = QueryRows(WorldContextObject, QuerySpec);
		if (!QueryResult.bSuccess)
		{
			return QueryResult;
		}

		OutRows.Reserve(QueryResult.Rows.Num());
		for (const FESQLRow& Row : QueryResult.Rows)
		{
			T LoadedRow{};
			const FESQLQueryResult RowResult = PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
			if (!RowResult.bSuccess)
			{
				OutRows.Reset();
				return RowResult;
			}

			OutRows.Add(MoveTemp(LoadedRow));
		}

		return QueryResult;
	}

	template<typename T>
	FESQLQueryResult FindFirstRow(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, T& OutRow)
	{
		FESQLQuerySpec FirstRowQuery = QuerySpec;
		FirstRowQuery.Limit = 1;

		TArray<T> MatchingRows;
		const FESQLQueryResult Result = FindRows(WorldContextObject, FirstRowQuery, MatchingRows);
		if (!Result.bSuccess)
		{
			return Result;
		}

		if (MatchingRows.Num() == 0)
		{
			return FESQLQueryResult::Failure(TEXT("No rows matched query"));
		}

		OutRow = MatchingRows[0];
		return Result;
	}

	template<typename T>
	bool TryFindFirstRow(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, T& OutRow, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = FindFirstRow(WorldContextObject, QuerySpec, OutRow);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	bool TryFindRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, TArray<T>& OutRows, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = FindRows(WorldContextObject, QuerySpec, OutRows);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult LoadPage(UObject* WorldContextObject, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows)
	{
		if (PageIndex < 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
		}

		if (PageSize <= 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
		}

		const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
		if (RequestedOffset > MAX_int32)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
		}

		FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
		PagedQuerySpec.Limit = PageSize;
		PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);

		return FindRows(WorldContextObject, PagedQuerySpec, OutRows);
	}

	template<typename T>
	bool TryLoadPage(UObject* WorldContextObject, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadPage(WorldContextObject, BaseQuerySpec, PageIndex, PageSize, OutRows);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRow(UESQLPlayerDBComponent* PlayerDBComponent, const FString& RowId, T& OutRow)
	{
		return LoadPlayerRowIntoStruct(PlayerDBComponent, RowId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRow(UObject* WorldContextObject, const FString& PlayerId, const FString& RowId, T& OutRow)
	{
		return LoadPlayerRowIntoStruct(WorldContextObject, PlayerId, RowId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRow(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLId& SqlId, T& OutRow)
	{
		return LoadPlayerRowIntoStruct(PlayerDBComponent, SqlId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRow(UObject* WorldContextObject, const FString& PlayerId, const FESQLId& SqlId, T& OutRow)
	{
		return LoadPlayerRowIntoStruct(WorldContextObject, PlayerId, SqlId, &OutRow, T::StaticStruct());
	}

	template<typename T>
	FESQLQueryResult FindPlayerRows(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& QuerySpec, TArray<T>& OutRows)
	{
		OutRows.Reset();

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			return ValidationResult;
		}

		const FESQLQueryResult QueryResult = QueryPlayerRows(PlayerDBComponent, QuerySpec);
		if (!QueryResult.bSuccess)
		{
			return QueryResult;
		}

		OutRows.Reserve(QueryResult.Rows.Num());
		for (const FESQLRow& Row : QueryResult.Rows)
		{
			T LoadedRow{};
			const FESQLQueryResult RowResult = PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
			if (!RowResult.bSuccess)
			{
				OutRows.Reset();
				return RowResult;
			}

			OutRows.Add(MoveTemp(LoadedRow));
		}

		return QueryResult;
	}

	template<typename T>
	FESQLQueryResult FindPlayerRows(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& QuerySpec, TArray<T>& OutRows)
	{
		OutRows.Reset();

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			return ValidationResult;
		}

		const FESQLQueryResult QueryResult = QueryPlayerRows(WorldContextObject, PlayerId, QuerySpec);
		if (!QueryResult.bSuccess)
		{
			return QueryResult;
		}

		OutRows.Reserve(QueryResult.Rows.Num());
		for (const FESQLRow& Row : QueryResult.Rows)
		{
			T LoadedRow{};
			const FESQLQueryResult RowResult = PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
			if (!RowResult.bSuccess)
			{
				OutRows.Reset();
				return RowResult;
			}

			OutRows.Add(MoveTemp(LoadedRow));
		}

		return QueryResult;
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRows(UESQLPlayerDBComponent* PlayerDBComponent, TArray<T>& OutRows, int32 MaxRows = 0)
	{
		FESQLQuerySpec QuerySpec;
		QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
		return FindPlayerRows(PlayerDBComponent, QuerySpec, OutRows);
	}

	template<typename T>
	FESQLQueryResult LoadPlayerTypedRows(UObject* WorldContextObject, const FString& PlayerId, TArray<T>& OutRows, int32 MaxRows = 0)
	{
		FESQLQuerySpec QuerySpec;
		QuerySpec.Limit = MaxRows > 0 ? MaxRows : 0;
		return FindPlayerRows(WorldContextObject, PlayerId, QuerySpec, OutRows);
	}

	template<typename T>
	FESQLQueryResult LoadPlayerPage(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows)
	{
		if (PageIndex < 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
		}

		if (PageSize <= 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
		}

		const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
		if (RequestedOffset > MAX_int32)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
		}

		FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
		PagedQuerySpec.Limit = PageSize;
		PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);
		return FindPlayerRows(PlayerDBComponent, PagedQuerySpec, OutRows);
	}

	template<typename T>
	bool TryLoadPlayerPage(UESQLPlayerDBComponent* PlayerDBComponent, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadPlayerPage(PlayerDBComponent, BaseQuerySpec, PageIndex, PageSize, OutRows);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult LoadPlayerPage(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows)
	{
		if (PageIndex < 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative"));
		}

		if (PageSize <= 0)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero"));
		}

		const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
		if (RequestedOffset > MAX_int32)
		{
			OutRows.Reset();
			return FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range"));
		}

		FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
		PagedQuerySpec.Limit = PageSize;
		PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);
		return FindPlayerRows(WorldContextObject, PlayerId, PagedQuerySpec, OutRows);
	}

	template<typename T>
	bool TryLoadPlayerPage(UObject* WorldContextObject, const FString& PlayerId, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TArray<T>& OutRows, FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = LoadPlayerPage(WorldContextObject, PlayerId, BaseQuerySpec, PageIndex, PageSize, OutRows);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}

	template<typename T>
	FESQLQueryResult SavePlayerTypedRow(
		UESQLPlayerDBComponent* PlayerDBComponent,
		const T& RowData,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""))
	{
		return SavePlayerRowFromStruct(PlayerDBComponent, &RowData, T::StaticStruct(), OutResolvedRowId, RowIdOverride);
	}

	template<typename T>
	FESQLQueryResult SavePlayerTypedRow(
		UObject* WorldContextObject,
		const FString& PlayerId,
		const T& RowData,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""))
	{
		return SavePlayerRowFromStruct(WorldContextObject, PlayerId, &RowData, T::StaticStruct(), OutResolvedRowId, RowIdOverride);
	}

	template<typename T>
	void AsyncLoadTypedRow(UObject* WorldContextObject, const FString& RowId, TFunction<void(const FESQLQueryResult&, const T&)> OnComplete)
	{
		T DefaultRow{};
		if (!OnComplete)
		{
			return;
		}

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			OnComplete(ValidationResult, DefaultRow);
			return;
		}

		const FESQLQueryResult InitResult = Initialize(WorldContextObject);
		if (!InitResult.bSuccess)
		{
			OnComplete(InitResult, DefaultRow);
			return;
		}

		if (!CachedDatabase || !CachedDatabase->IsOpen())
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")), DefaultRow);
			return;
		}

		FString SQL;
		TArray<FString> Bindings;
		const FESQLQueryResult BuildResult = BuildGetRowByIdSQL(RowId, SQL, Bindings);
		if (!BuildResult.bSuccess)
		{
			OnComplete(BuildResult, DefaultRow);
			return;
		}

		UESQLTableAsset* TableAsset = this;
		TSharedPtr<FESQLDatabase> Database = CachedDatabase;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [TableAsset, Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			const FESQLQueryResult QueryResult = Database->Execute(SQL, Bindings);

			AsyncTask(ENamedThreads::GameThread, [TableAsset, QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
			{
				T LoadedRow{};
				FESQLQueryResult FinalResult = QueryResult;

				if (!FinalResult.bSuccess)
				{
					OnComplete(FinalResult, LoadedRow);
					return;
				}

				if (!IsValid(TableAsset))
				{
					OnComplete(FESQLQueryResult::Failure(TEXT("SQL Table Asset is no longer valid")), LoadedRow);
					return;
				}

				const FESQLRow* Row = FinalResult.GetFirstRow();
				if (!Row)
				{
					OnComplete(FESQLQueryResult::Failure(FString::Printf(
						TEXT("No row found in table '%s' for id '%s'"),
						*TableAsset->TableName,
						*Bindings[0])), LoadedRow);
					return;
				}

				const FESQLQueryResult PopulateResult = TableAsset->PopulateStructFromRow(*Row, &LoadedRow, T::StaticStruct());
				OnComplete(PopulateResult.bSuccess ? FinalResult : PopulateResult, LoadedRow);
			});
		});
	}

	template<typename T>
	void AsyncLoadTypedRow(UObject* WorldContextObject, const FESQLId& SqlId, TFunction<void(const FESQLQueryResult&, const T&)> OnComplete)
	{
		AsyncLoadTypedRow<T>(WorldContextObject, SqlId.Value, MoveTemp(OnComplete));
	}

	template<typename T>
	void AsyncLoadTypedRows(UObject* WorldContextObject, TFunction<void(const FESQLQueryResult&, const TArray<T>&)> OnComplete, int32 MaxRows = 0)
	{
		if (!OnComplete)
		{
			return;
		}

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			OnComplete(ValidationResult, TArray<T>());
			return;
		}

		const FESQLQueryResult InitResult = Initialize(WorldContextObject);
		if (!InitResult.bSuccess)
		{
			OnComplete(InitResult, TArray<T>());
			return;
		}

		if (!CachedDatabase || !CachedDatabase->IsOpen())
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")), TArray<T>());
			return;
		}

		FString SQL;
		const FESQLQueryResult BuildResult = BuildGetAllRowsSQL(MaxRows, SQL);
		if (!BuildResult.bSuccess)
		{
			OnComplete(BuildResult, TArray<T>());
			return;
		}

		UESQLTableAsset* TableAsset = this;
		TSharedPtr<FESQLDatabase> Database = CachedDatabase;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [TableAsset, Database, SQL = MoveTemp(SQL), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			const FESQLQueryResult QueryResult = Database->Execute(SQL);

			AsyncTask(ENamedThreads::GameThread, [TableAsset, QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
			{
				TArray<T> LoadedRows;
				FESQLQueryResult FinalResult = QueryResult;

				if (!FinalResult.bSuccess)
				{
					OnComplete(FinalResult, LoadedRows);
					return;
				}

				if (!IsValid(TableAsset))
				{
					OnComplete(FESQLQueryResult::Failure(TEXT("SQL Table Asset is no longer valid")), LoadedRows);
					return;
				}

				LoadedRows.Reserve(FinalResult.Rows.Num());
				for (const FESQLRow& Row : FinalResult.Rows)
				{
					T LoadedRow{};
					const FESQLQueryResult PopulateResult = TableAsset->PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
					if (!PopulateResult.bSuccess)
					{
						LoadedRows.Reset();
						OnComplete(PopulateResult, LoadedRows);
						return;
					}

					LoadedRows.Add(MoveTemp(LoadedRow));
				}

				OnComplete(FinalResult, LoadedRows);
			});
		});
	}

	template<typename T>
	void AsyncFindRows(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, TFunction<void(const FESQLQueryResult&, const TArray<T>&)> OnComplete)
	{
		if (!OnComplete)
		{
			return;
		}

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			OnComplete(ValidationResult, TArray<T>());
			return;
		}

		const FESQLQueryResult InitResult = Initialize(WorldContextObject);
		if (!InitResult.bSuccess)
		{
			OnComplete(InitResult, TArray<T>());
			return;
		}

		if (!CachedDatabase || !CachedDatabase->IsOpen())
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")), TArray<T>());
			return;
		}

		FString SQL;
		TArray<FESQLBindingValue> Bindings;
		const FESQLQueryResult BuildResult = BuildQueryRowsSQL(QuerySpec, SQL, Bindings);
		if (!BuildResult.bSuccess)
		{
			OnComplete(BuildResult, TArray<T>());
			return;
		}

		UESQLTableAsset* TableAsset = this;
		TSharedPtr<FESQLDatabase> Database = CachedDatabase;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [TableAsset, Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			const FESQLQueryResult QueryResult = Bindings.Num() > 0
				? Database->Execute(SQL, Bindings)
				: Database->Execute(SQL);

			AsyncTask(ENamedThreads::GameThread, [TableAsset, QueryResult, OnComplete = MoveTemp(OnComplete)]() mutable
			{
				TArray<T> LoadedRows;
				FESQLQueryResult FinalResult = QueryResult;

				if (!FinalResult.bSuccess)
				{
					OnComplete(FinalResult, LoadedRows);
					return;
				}

				if (!IsValid(TableAsset))
				{
					OnComplete(FESQLQueryResult::Failure(TEXT("SQL Table Asset is no longer valid")), LoadedRows);
					return;
				}

				LoadedRows.Reserve(FinalResult.Rows.Num());
				for (const FESQLRow& Row : FinalResult.Rows)
				{
					T LoadedRow{};
					const FESQLQueryResult PopulateResult = TableAsset->PopulateStructFromRow(Row, &LoadedRow, T::StaticStruct());
					if (!PopulateResult.bSuccess)
					{
						LoadedRows.Reset();
						OnComplete(PopulateResult, LoadedRows);
						return;
					}

					LoadedRows.Add(MoveTemp(LoadedRow));
				}

				OnComplete(FinalResult, LoadedRows);
			});
		});
	}

	template<typename T>
	void AsyncFindFirstRow(UObject* WorldContextObject, const FESQLQuerySpec& QuerySpec, TFunction<void(const FESQLQueryResult&, const T&)> OnComplete)
	{
		if (!OnComplete)
		{
			return;
		}

		FESQLQuerySpec FirstRowQuery = QuerySpec;
		FirstRowQuery.Limit = 1;

		AsyncFindRows<T>(WorldContextObject, FirstRowQuery, [OnComplete = MoveTemp(OnComplete)](const FESQLQueryResult& Result, const TArray<T>& Rows) mutable
		{
			T LoadedRow{};
			if (!Result.bSuccess)
			{
				OnComplete(Result, LoadedRow);
				return;
			}

			if (Rows.Num() == 0)
			{
				OnComplete(FESQLQueryResult::Failure(TEXT("No rows matched query")), LoadedRow);
				return;
			}

			OnComplete(Result, Rows[0]);
		});
	}

	template<typename T>
	void AsyncLoadPage(UObject* WorldContextObject, const FESQLQuerySpec& BaseQuerySpec, int32 PageIndex, int32 PageSize, TFunction<void(const FESQLQueryResult&, const TArray<T>&)> OnComplete)
	{
		if (!OnComplete)
		{
			return;
		}

		if (PageIndex < 0)
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("PageIndex cannot be negative")), TArray<T>());
			return;
		}

		if (PageSize <= 0)
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("PageSize must be greater than zero")), TArray<T>());
			return;
		}

		const int64 RequestedOffset = static_cast<int64>(PageIndex) * static_cast<int64>(PageSize);
		if (RequestedOffset > MAX_int32)
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("Requested page offset exceeds supported range")), TArray<T>());
			return;
		}

		FESQLQuerySpec PagedQuerySpec = BaseQuerySpec;
		PagedQuerySpec.Limit = PageSize;
		PagedQuerySpec.Offset = static_cast<int32>(RequestedOffset);

		AsyncFindRows<T>(WorldContextObject, PagedQuerySpec, MoveTemp(OnComplete));
	}

	template<typename T>
	void AsyncSaveTypedRow(UObject* WorldContextObject, const T& RowData, TFunction<void(const FESQLQueryResult&, const FString&)> OnComplete, const FString& RowIdOverride = TEXT(""))
	{
		if (!OnComplete)
		{
			return;
		}

		const FESQLQueryResult ValidationResult = ValidateTypedStructAccess(T::StaticStruct());
		if (!ValidationResult.bSuccess)
		{
			OnComplete(ValidationResult, FString());
			return;
		}

		const FESQLQueryResult InitResult = Initialize(WorldContextObject);
		if (!InitResult.bSuccess)
		{
			OnComplete(InitResult, FString());
			return;
		}

		if (!CachedDatabase || !CachedDatabase->IsOpen())
		{
			OnComplete(FESQLQueryResult::Failure(TEXT("Database connection is not available")), FString());
			return;
		}

		FString SQL;
		TArray<FESQLBindingValue> Bindings;
		FString ResolvedRowId;
		const FESQLQueryResult BuildResult = BuildSaveRowSQL(&RowData, T::StaticStruct(), SQL, Bindings, ResolvedRowId, RowIdOverride);
		if (!BuildResult.bSuccess)
		{
			OnComplete(BuildResult, FString());
			return;
		}

		TSharedPtr<FESQLDatabase> Database = CachedDatabase;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQL = MoveTemp(SQL), Bindings = MoveTemp(Bindings), ResolvedRowId = MoveTemp(ResolvedRowId), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			const FESQLQueryResult SaveResult = Database->Execute(SQL, Bindings);

			AsyncTask(ENamedThreads::GameThread, [SaveResult, ResolvedRowId = MoveTemp(ResolvedRowId), OnComplete = MoveTemp(OnComplete)]() mutable
			{
				OnComplete(SaveResult, SaveResult.bSuccess ? ResolvedRowId : FString());
			});
		});
	}

	template<typename T>
	FESQLQueryResult SaveTypedRow(
		UObject* WorldContextObject,
		const T& RowData,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""))
	{
		return SaveRowFromStruct(WorldContextObject, &RowData, T::StaticStruct(), OutResolvedRowId, RowIdOverride);
	}

	template<typename T>
	bool TrySaveTypedRow(
		UObject* WorldContextObject,
		const T& RowData,
		FString* OutResolvedRowId = nullptr,
		const FString& RowIdOverride = TEXT(""),
		FString* OutError = nullptr)
	{
		const FESQLQueryResult Result = SaveTypedRow(WorldContextObject, RowData, OutResolvedRowId, RowIdOverride);
		if (!Result.bSuccess)
		{
			if (OutError)
			{
				*OutError = Result.ErrorMessage;
			}

			return false;
		}

		if (OutError)
		{
			OutError->Reset();
		}

		return true;
	}


	// ── Schema Migration ─────────────────────────────────────────────────

	/** Compare struct fields vs DB columns and apply migrations.
	    Called automatically by Initialize(). */
	FESQLQueryResult SyncSchema(TSharedPtr<FESQLDatabase> Database);


	// ── Editor Support (code only, no UI — Phase 8) ──────────────────────

	/** Returns column definitions derived from the row struct.
	    Includes PrimaryKeyColumn as first entry. */
	TArray<FESQLColumn> GetColumnDefinitions() const;

	/** Resolve an authored Blueprint field name to the actual SQL column name. */
	FString ResolveColumnName(const FString& ColumnName) const;

	/** Resolve the effective label column for runtime lookups. */
	FString GetEffectiveLabelColumn(const FString& LabelColumnOverride) const;

	/** Validate the row struct for SQLite compatibility. */
	bool ValidateStruct(TArray<FESQLStructValidator::FFieldResult>& OutResults) const;

	/** Ensure the table exists in the given database (CREATE TABLE IF NOT EXISTS).
	    Called by the editor on open so that Add Row works even before runtime Initialize(). */
	bool EnsureTableExists(TSharedPtr<FESQLDatabase> InDatabase, FString& OutError);

	/** Export all rows to a CSV file.
	    @param InDatabase Optional database to use. If null, uses CachedDatabase. */
	bool ExportToCSV(const FString& OutputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr) const;

	/** Import rows from a CSV file (UPSERT logic).
	    @param InDatabase Optional database to use. If null, uses CachedDatabase. */
	bool ImportFromCSV(const FString& InputFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr);


	// ── VCS Pipeline (.sqldump) ──────────────────────────────────────────

	/** Export the database to a .sqldump text file (VCS source of truth).
	    Writes CREATE TABLE + sorted INSERT OR REPLACE statements.
	    @param InDatabase Optional database to use. If null, uses CachedDatabase. */
	bool ExportToSQLDump(const FString& DumpFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr) const;

	/** Import from a .sqldump text file into the database.
	    The .sqldump must contain valid SQL (CREATE TABLE + INSERT statements).
	    @param InDatabase Optional database to use. If null, uses CachedDatabase. */
	bool ImportFromSQLDump(const FString& DumpFilePath, FString& OutError, TSharedPtr<FESQLDatabase> InDatabase = nullptr);

	/** Sync the .db file and .sqldump based on timestamps.
	    Called when the editor opens this asset. */
	bool SyncDatabaseAndDump(FString& OutError);

	/** Get the .sqldump path for this asset's database. */
	FString GetSQLDumpPath() const;

	/** Get the cached database connection. May be null if not initialized. */
	TSharedPtr<FESQLDatabase> GetCachedDatabase() const { return CachedDatabase; }

	/** Override the cached database connection used by editor tools. */
	void SetCachedDatabase(TSharedPtr<FESQLDatabase> InDatabase) { CachedDatabase = MoveTemp(InDatabase); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FESQLQueryResult ValidateTypedStructAccess(const UScriptStruct* StructType) const;
	FESQLQueryResult BuildStructColumnBindings(const void* StructData, const UScriptStruct* StructType, TMap<FString, FESQLBindingValue>& OutColumnValues) const;
	FESQLQueryResult PopulateStructFromRow(const FESQLRow& Row, void* OutStructData, const UScriptStruct* StructType) const;
	FESQLQueryResult PopulateStructArrayFromRows(const TArray<FESQLRow>& Rows, void* OutArrayData, const FArrayProperty* ArrayProperty) const;
	FESQLQueryResult ResolvePlayerDatabaseConnection(UESQLPlayerDBComponent* PlayerDBComponent, TSharedPtr<FESQLDatabase>& OutDatabase);
	FESQLQueryResult ResolvePlayerDatabaseConnection(UObject* WorldContextObject, const FString& PlayerId, TSharedPtr<FESQLDatabase>& OutDatabase);
	FESQLQueryResult BuildGetAllRowsSQL(int32 MaxRows, FString& OutSQL) const;
	FESQLQueryResult BuildGetRowByIdSQL(const FString& RowId, FString& OutSQL, TArray<FString>& OutBindings) const;
	FESQLQueryResult BuildSaveRowSQL(const void* StructData, const UScriptStruct* StructType, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings, FString& OutResolvedRowId, const FString& RowIdOverride) const;
	bool TryResolveQueryColumn(const FString& FieldName, FString& OutColumnName, FString* OutError = nullptr) const;
	FESQLQueryResult BuildQuerySQL(const FString& SelectClause, const FESQLQuerySpec& QuerySpec, bool bIncludeSorting, bool bIncludePaging, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings) const;
	FESQLQueryResult BuildQueryRowsSQL(const FESQLQuerySpec& QuerySpec, FString& OutSQL, TArray<FESQLBindingValue>& OutBindings) const;

	/** Cached database connection — set by Initialize(), cleared on GC or shutdown. */
	TSharedPtr<FESQLDatabase> CachedDatabase;

	/** Whether Initialize() has been called successfully. */
	bool bInitialized = false;
};

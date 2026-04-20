// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DefaultValueHelper.h"
#include "ESQLTypes.generated.h"

namespace ESQLTypeParsing
{
	inline bool TryParseInt64(const FString& InValue, int64& OutValue)
	{
		return FDefaultValueHelper::ParseInt64(InValue, OutValue);
	}

	inline bool TryParseDouble(const FString& InValue, double& OutValue)
	{
		return FDefaultValueHelper::ParseDouble(InValue, OutValue);
	}
}


// ── Column Type ──────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EESQLColumnType : uint8
{
	Null,
	Integer,
	Float,
	Text,
	Blob
};


// ── Database Scope (multiplayer) ─────────────────────────────────────────────

/**
 * Controls WHERE the database file lives and WHO may access it.
 * This is the key enum that makes the system multiplayer-aware.
 */
UENUM(BlueprintType)
enum class EESQLDatabaseScope : uint8
{
	/**
	 * Local to whatever process opens it.
	 * Standalone: Saved/Databases/<Name>.db
	 * Client:     Saved/Databases/<Name>.db     (client-local, NOT replicated)
	 * DediServer: Saved/Databases/<Name>.db     (server machine only)
	 * ListenServer: Saved/Databases/<Name>.db   (host machine only)
	 *
	 * Use for: save-game data in singleplayer, local caches, mod content.
	 */
	Local = 0,

	/**
	 * Authoritative shared database.
	 * Only the server (dedicated or listen-server host) may open it.
	 * Calls from a pure client return an authority error.
	 *
	 * Path: Saved/Databases/Server/<Name>.db
	 *
	 * Use for: world state, match logs, economy tables, NPC data
	 * that must be consistent across all connected players.
	 */
	Shared = 1,

	/**
	 * Shipped read-only content database.
	 * Opened read-only on any peer. Writes are always rejected.
	 *
	 * Path: resolved from cooked/project content, extracted to a writable cache
	 * when the payload is not already available as a normal OS file.
	 */
	Readonly = 3
};


// ── Database Persistence Mode ────────────────────────────────────────────────

/**
 * Controls HOW the database is stored and whether it survives
 * between game sessions.
 */
UENUM(BlueprintType)
enum class EESQLDatabasePersistence : uint8
{
	/**
	 * Database is a regular file on disk (.db).
	 * Persists between game sessions — data is always there.
	 * File is created on first open and kept forever (until manually deleted).
	 *
	 * Use for: permanent save data, server world state, analytics logs,
	 * mod content, any data that must survive a game restart.
	 */
	Persistent,

	/**
	 * Database lives purely in memory (SQLite :memory:).
	 * Created fresh on OpenDatabase(), destroyed on CloseDatabase()
	 * or when the game process exits — NO file is left behind.
	 *
	 * To preserve data, use the Snapshot API:
	 *   SaveSnapshot("MySlot")  → backs up the in-memory DB to a .db file on disk
	 *   LoadSnapshot("MySlot")  → replaces the in-memory DB contents from a .db file
	 *
	 * Use for: temporary match data, draft systems, undo buffers,
	 * or traditional "save slot" workflows where the player
	 * explicitly chooses when to save.
	 */
	Session
};


// ── Snapshot Info ─────────────────────────────────────────────────────────────

/** Metadata about a saved snapshot on disk. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLSnapshotInfo
{
	GENERATED_BODY()

	/** The logical slot name (e.g. "AutoSave", "Slot_01"). */
	UPROPERTY(BlueprintReadOnly, Category = "SQL|Snapshot")
	FString SlotName;

	/** User-facing display name (e.g. "Chapter 3 — Before Boss"). */
	UPROPERTY(BlueprintReadOnly, Category = "SQL|Snapshot")
	FString DisplayName;

	/** UTC timestamp when the snapshot was created. */
	UPROPERTY(BlueprintReadOnly, Category = "SQL|Snapshot")
	FDateTime Timestamp;

	/** File size of the snapshot .db in bytes. */
	UPROPERTY(BlueprintReadOnly, Category = "SQL|Snapshot")
	int64 FileSizeBytes = 0;

	/** Absolute path to the snapshot .db file. */
	UPROPERTY(BlueprintReadOnly, Category = "SQL|Snapshot")
	FString FilePath;
};


// ── Single Column ────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLColumn
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	EESQLColumnType Type = EESQLColumnType::Null;
};

USTRUCT(BlueprintType, meta = (DisableSplitPin, HasNativeMake = "/Script/UnrealExtendedSQL.ESQLBlueprintLibrary.MakeSQLBindingValue"))
struct UNREALEXTENDEDSQL_API FESQLBindingValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	EESQLColumnType ValueType = EESQLColumnType::Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	bool bIsNull = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	int64 IntegerValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	double FloatValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	FString TextValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	TArray<uint8> BlobValue;

	static FESQLBindingValue FromText(FString InTextValue)
	{
		FESQLBindingValue Value;
		Value.ValueType = EESQLColumnType::Text;
		Value.TextValue = MoveTemp(InTextValue);
		return Value;
	}

	static FESQLBindingValue FromInteger(int64 InIntegerValue)
	{
		FESQLBindingValue Value;
		Value.ValueType = EESQLColumnType::Integer;
		Value.IntegerValue = InIntegerValue;
		Value.TextValue = FString::Printf(TEXT("%lld"), InIntegerValue);
		return Value;
	}

	static FESQLBindingValue FromFloat(double InFloatValue)
	{
		FESQLBindingValue Value;
		Value.ValueType = EESQLColumnType::Float;
		Value.FloatValue = InFloatValue;
		Value.TextValue = FString::SanitizeFloat(InFloatValue);
		return Value;
	}

	static FESQLBindingValue FromBlob(TArray<uint8> InBlobValue)
	{
		FESQLBindingValue Value;
		Value.ValueType = EESQLColumnType::Blob;
		Value.BlobValue = MoveTemp(InBlobValue);
		return Value;
	}

	static FESQLBindingValue Null()
	{
		FESQLBindingValue Value;
		Value.ValueType = EESQLColumnType::Null;
		Value.bIsNull = true;
		return Value;
	}

	bool IsNull() const
	{
		return bIsNull || ValueType == EESQLColumnType::Null;
	}

	bool TryGetInt64(int64& OutValue) const
	{
		if (IsNull())
		{
			return false;
		}

		if (ValueType == EESQLColumnType::Integer)
		{
			OutValue = IntegerValue;
			return true;
		}

		if (ValueType == EESQLColumnType::Float)
		{
			OutValue = static_cast<int64>(FloatValue);
			return true;
		}

		return ESQLTypeParsing::TryParseInt64(TextValue, OutValue);
	}

	bool TryGetDouble(double& OutValue) const
	{
		if (IsNull())
		{
			return false;
		}

		if (ValueType == EESQLColumnType::Float)
		{
			OutValue = FloatValue;
			return true;
		}

		if (ValueType == EESQLColumnType::Integer)
		{
			OutValue = static_cast<double>(IntegerValue);
			return true;
		}

		return ESQLTypeParsing::TryParseDouble(TextValue, OutValue);
	}

	bool TryGetString(FString& OutValue) const
	{
		if (IsNull() || ValueType == EESQLColumnType::Blob)
		{
			return false;
		}

		OutValue = TextValue;
		return true;
	}

	bool TryGetBlob(TArray<uint8>& OutValue) const
	{
		if (IsNull() || ValueType != EESQLColumnType::Blob)
		{
			return false;
		}

		OutValue = BlobValue;
		return true;
	}
};


// ── Single Row ───────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLRow
{
	GENERATED_BODY()

	/** Column name → string value. Blobs are represented as empty strings. */
	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	TMap<FString, FString> Columns;

	/** Column names whose current row value is SQL NULL. */
	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	TSet<FString> NullColumns;

	bool HasColumn(const FString& ColumnName) const
	{
		return Columns.Contains(ColumnName) || NullColumns.Contains(ColumnName);
	}

	bool IsNull(const FString& ColumnName) const
	{
		return NullColumns.Contains(ColumnName);
	}

	const FString* FindValue(const FString& ColumnName) const
	{
		return Columns.Find(ColumnName);
	}

	bool TryGetString(const FString& ColumnName, FString& OutValue) const
	{
		if (IsNull(ColumnName))
		{
			return false;
		}

		if (const FString* FoundValue = Columns.Find(ColumnName))
		{
			OutValue = *FoundValue;
			return true;
		}

		return false;
	}

	bool TryGetInt64(const FString& ColumnName, int64& OutValue) const
	{
		FString StringValue;
		return TryGetString(ColumnName, StringValue) && ESQLTypeParsing::TryParseInt64(StringValue, OutValue);
	}

	bool TryGetDouble(const FString& ColumnName, double& OutValue) const
	{
		FString StringValue;
		return TryGetString(ColumnName, StringValue) && ESQLTypeParsing::TryParseDouble(StringValue, OutValue);
	}

	bool TryGetBool(const FString& ColumnName, bool& OutValue) const
	{
		FString StringValue;
		if (!TryGetString(ColumnName, StringValue))
		{
			return false;
		}

		if (StringValue == TEXT("1"))
		{
			OutValue = true;
			return true;
		}

		if (StringValue == TEXT("0"))
		{
			OutValue = false;
			return true;
		}

		if (StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
		{
			OutValue = true;
			return true;
		}

		if (StringValue.Equals(TEXT("false"), ESearchCase::IgnoreCase))
		{
			OutValue = false;
			return true;
		}

		return false;
	}

	bool TryGetText(const FString& ColumnName, FText& OutValue) const
	{
		FString StringValue;
		if (!TryGetString(ColumnName, StringValue))
		{
			return false;
		}

		OutValue = FText::FromString(StringValue);
		return true;
	}
};


// ── Database Open Parameters ─────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLDatabaseParams
{
	GENERATED_BODY()

	/** Logical name used to reference this database in all subsequent calls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	FString DatabaseName;

	/** File name on disk (without path). Defaults to <DatabaseName>.db.
	    Ignored for Session persistence (in-memory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	FString FileName;

	/** Where the database lives and who may access it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;

	/** How the database is stored — on disk (Persistent) or in memory (Session). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL")
	EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;
};


// ── Structured Query Spec (C++ Runtime) ────────────────────────────────────

UENUM(BlueprintType)
enum class EESQLFilterOperation : uint8
{
	Equal,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
	Like,
	In
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/UnrealExtendedSQL.ESQLBlueprintLibrary.MakeSQLFieldFilter"))
struct UNREALEXTENDEDSQL_API FESQLFieldFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	FString FieldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	EESQLFilterOperation Operation = EESQLFilterOperation::Equal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	TArray<FESQLBindingValue> Values;

	FESQLFieldFilter() = default;

	FESQLFieldFilter(FString InFieldName, EESQLFilterOperation InOperation, FESQLBindingValue InValue)
		: FieldName(MoveTemp(InFieldName))
		, Operation(InOperation)
	{
		Values.Add(MoveTemp(InValue));
	}

	FESQLFieldFilter(FString InFieldName, EESQLFilterOperation InOperation, TArray<FESQLBindingValue> InValues)
		: FieldName(MoveTemp(InFieldName))
		, Operation(InOperation)
		, Values(MoveTemp(InValues))
	{
	}
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/UnrealExtendedSQL.ESQLBlueprintLibrary.MakeSQLSortRule"))
struct UNREALEXTENDEDSQL_API FESQLSortRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	FString FieldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	bool bAscending = true;

	FESQLSortRule() = default;

	FESQLSortRule(FString InFieldName, bool bInAscending = true)
		: FieldName(MoveTemp(InFieldName))
		, bAscending(bInAscending)
	{
	}
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/UnrealExtendedSQL.ESQLBlueprintLibrary.MakeSQLQuerySpec"))
struct UNREALEXTENDEDSQL_API FESQLQuerySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	TArray<FESQLFieldFilter> Filters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	TArray<FESQLSortRule> SortRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	int32 Limit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Query")
	int32 Offset = 0;

	bool HasFilters() const
	{
		return Filters.Num() > 0;
	}

	bool HasSorting() const
	{
		return SortRules.Num() > 0;
	}

	bool HasPaging() const
	{
		return Limit > 0 || Offset > 0;
	}

	bool IsEmpty() const
	{
		return !HasFilters() && !HasSorting() && !HasPaging();
	}
};

struct UNREALEXTENDEDSQL_API FESQLTableSchemaColumn
{
	FString Name;
	FString SQLiteType;
	bool bPrimaryKey = false;
	bool bNullable = true;
	FString DefaultValue;
};

struct UNREALEXTENDEDSQL_API FESQLTableSchema
{
	FString DatabaseName;
	FString TableName;
	FString PrimaryKeyColumn;
	FString LabelColumn;
	EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;
	EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;
	const UScriptStruct* RowStruct = nullptr;
	TArray<FESQLTableSchemaColumn> Columns;

	bool IsValid() const
	{
		return RowStruct != nullptr
			&& !DatabaseName.TrimStartAndEnd().IsEmpty()
			&& !TableName.TrimStartAndEnd().IsEmpty()
			&& !PrimaryKeyColumn.TrimStartAndEnd().IsEmpty();
	}

	const FESQLTableSchemaColumn* FindColumn(const FString& ColumnName) const
	{
		return Columns.FindByPredicate([&ColumnName](const FESQLTableSchemaColumn& Column)
		{
			return Column.Name == ColumnName;
		});
	}

	FString GetEffectiveLabelColumn() const
	{
		return LabelColumn.TrimStartAndEnd().IsEmpty() ? PrimaryKeyColumn : LabelColumn;
	}

	bool HasColumn(const FString& ColumnName) const
	{
		return FindColumn(ColumnName) != nullptr;
	}
	};


// ── Structured Errors ───────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EESQLErrorCode : uint8
{
	None,
	Unknown,
	InvalidArgument,
	InvalidState,
	NotFound,
	OpenFailed,
	PrepareFailed,
	BindFailed,
	StepFailed,
	PragmaFailed,
	TransactionFailed,
	MigrationFailed,
	BackupFailed,
	RestoreFailed,
	ExportFailed,
	ImportFailed,
	IoFailure,
	AuthorityViolation,
	ReadonlyViolation,
	WrongThread,
	Cancelled
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Error")
	EESQLErrorCode Code = EESQLErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Error")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Error")
	FString SqlFragment;

	bool HasError() const
	{
		return Code != EESQLErrorCode::None || !Message.IsEmpty() || !SqlFragment.IsEmpty();
	}

	static FESQLError Make(EESQLErrorCode InCode, FString InMessage, FString InSqlFragment = FString())
	{
		FESQLError Error;
		Error.Code = InCode;
		Error.Message = MoveTemp(InMessage);
		Error.SqlFragment = MoveTemp(InSqlFragment);
		return Error;
	}
};

struct FESQLTransactionLifetime;

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLTicket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Async")
	FGuid Id;

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Async")
	FName DatabaseName;

	bool IsValid() const
	{
		return Id.IsValid() && !DatabaseName.IsNone();
	}

	void Reset()
	{
		Id.Invalidate();
		DatabaseName = NAME_None;
	}
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLMigrationStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Migration")
	int32 Sequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Migration")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Migration")
	FString Sql;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLMigrationSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Migration")
	TArray<FESQLMigrationStep> Steps;

	bool IsEmpty() const
	{
		return Steps.Num() == 0;
	}
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLTransactionHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Transaction")
	FName DatabaseName;

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Transaction")
	int64 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SQL|Transaction")
	int32 OwningThreadId = 0;

	bool IsValid() const
	{
		return !DatabaseName.IsNone() && Id != 0 && Lifetime.IsValid();
	}

	void Reset()
	{
		DatabaseName = NAME_None;
		Id = 0;
		OwningThreadId = 0;
		Lifetime.Reset();
	}

private:
	TSharedPtr<FESQLTransactionLifetime, ESPMode::ThreadSafe> Lifetime;

	friend class UESQLSubsystem;
};

struct UNREALEXTENDEDSQL_API FESQLErrorResult
{
	bool bSuccess = true;
	FESQLError Error;

	static FESQLErrorResult Success()
	{
		return FESQLErrorResult();
	}

	static FESQLErrorResult Failure(FESQLError InError)
	{
		FESQLErrorResult Result;
		Result.bSuccess = false;
		Result.Error = MoveTemp(InError);
		return Result;
	}

	bool HasError() const
	{
		return !bSuccess && Error.HasError();
	}

	const FString& GetErrorMessage() const
	{
		return Error.Message;
	}
};

template <typename TValue>
class TESQLValueOrError
{
public:
	static TESQLValueOrError MakeValue(TValue InValue)
	{
		TESQLValueOrError Result;
		Result.Value.Emplace(MoveTemp(InValue));
		return Result;
	}

	static TESQLValueOrError MakeError(FESQLError InError)
	{
		TESQLValueOrError Result;
		Result.Error.Emplace(MoveTemp(InError));
		return Result;
	}

	bool IsValue() const
	{
		return Value.IsSet();
	}

	bool IsError() const
	{
		return Error.IsSet();
	}

	explicit operator bool() const
	{
		return IsValue();
	}

	const TValue& GetValue() const
	{
		check(Value.IsSet());
		return Value.GetValue();
	}

	TValue& GetValue()
	{
		check(Value.IsSet());
		return Value.GetValue();
	}

	TValue ConsumeValue()
	{
		check(Value.IsSet());
		return MoveTemp(Value.GetValue());
	}

	const FESQLError& GetError() const
	{
		check(Error.IsSet());
		return Error.GetValue();
	}

	const FString& GetErrorMessage() const
	{
		return GetError().Message;
	}

private:
	TOptional<TValue> Value;
	TOptional<FESQLError> Error;
};


// ── Query Result ─────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct UNREALEXTENDEDSQL_API FESQLQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	FESQLError Error;

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	TArray<FESQLColumn> ColumnDefinitions;

	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	TArray<FESQLRow> Rows;

	/** Number of rows changed (INSERT/UPDATE/DELETE) */
	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	int32 RowsAffected = 0;

	/** Last inserted rowid (after INSERT) */
	UPROPERTY(BlueprintReadOnly, Category = "SQL")
	int64 LastInsertRowId = 0;

	/** Creates a success result with row data. */
	static FESQLQueryResult Success()
	{
		FESQLQueryResult Result;
		Result.bSuccess = true;
		Result.Error = FESQLError();
		Result.ErrorMessage.Reset();
		return Result;
	}

	/** Creates a success result with row data. */
	static FESQLQueryResult Success(const TArray<FESQLRow>& InRows, const TArray<FESQLColumn>& InColumns, int32 InRowsAffected = 0, int64 InLastInsertRowId = 0)
	{
		FESQLQueryResult Result;
		Result.bSuccess = true;
		Result.Rows = InRows;
		Result.ColumnDefinitions = InColumns;
		Result.RowsAffected = InRowsAffected;
		Result.LastInsertRowId = InLastInsertRowId;
		Result.Error = FESQLError();
		Result.ErrorMessage.Reset();
		return Result;
	}

	static FESQLQueryResult Failure(FESQLError InError)
	{
		FESQLQueryResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = InError.Message;
		Result.Error = MoveTemp(InError);
		return Result;
	}

	/** Creates a failure result with an error message. */
	static FESQLQueryResult Failure(const FString& InErrorMessage)
	{
		return Failure(FESQLError::Make(EESQLErrorCode::Unknown, InErrorMessage));
	}

	/** Explicit TCHAR* overload to disambiguate TEXT("…"). */
	static FESQLQueryResult Failure(const TCHAR* InErrorMessage)
	{
		return Failure(FString(InErrorMessage));
	}

	bool HasRows() const
	{
		return Rows.Num() > 0;
	}

	int32 NumRows() const
	{
		return Rows.Num();
	}

	const FESQLRow* GetFirstRow() const
	{
		return Rows.Num() > 0 ? &Rows[0] : nullptr;
	}

	bool TryGetSingleRow(FESQLRow& OutRow) const
	{
		if (Rows.Num() != 1)
		{
			return false;
		}

		OutRow = Rows[0];
		return true;
	}
};


// ── Delegates ────────────────────────────────────────────────────────────────

/** Fired when an async query completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnESQLQueryComplete, const FESQLQueryResult&, Result);

/** Single-cast callback for async queries. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnESQLQueryCompleteCallback, const FESQLQueryResult&, Result);

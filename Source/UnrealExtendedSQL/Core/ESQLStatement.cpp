// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLStatement.h"
#include "Private/ThirdParty/ESQLVendorSqlite.h"
#include "UnrealExtendedSQL.h"


// ── Construction / Destruction ───────────────────────────────────────────────

FESQLStatement::FESQLStatement()
{
}

FESQLStatement::~FESQLStatement()
{
	if (StmtHandle)
	{
		sqlite3_finalize(StmtHandle);
		StmtHandle = nullptr;
	}
}

FESQLStatement::FESQLStatement(FESQLStatement&& Other) noexcept
	: StmtHandle(Other.StmtHandle)
	, bDone(Other.bDone)
	, LastStepResult(Other.LastStepResult)
{
	Other.StmtHandle = nullptr;
	Other.bDone = false;
	Other.LastStepResult = 0;
}

FESQLStatement& FESQLStatement::operator=(FESQLStatement&& Other) noexcept
{
	if (this != &Other)
	{
		if (StmtHandle)
		{
			sqlite3_finalize(StmtHandle);
		}

		StmtHandle = Other.StmtHandle;
		bDone = Other.bDone;
		LastStepResult = Other.LastStepResult;

		Other.StmtHandle = nullptr;
		Other.bDone = false;
		Other.LastStepResult = 0;
	}
	return *this;
}


// ── Binding ──────────────────────────────────────────────────────────────────

bool FESQLStatement::BindBool(int32 Index, bool Value)
{
	return BindInt(Index, Value ? 1 : 0);
}

bool FESQLStatement::BindInt(int32 Index, int64 Value)
{
	if (!StmtHandle) return false;
	return sqlite3_bind_int64(StmtHandle, Index, Value) == SQLITE_OK;
}

bool FESQLStatement::BindFloat(int32 Index, double Value)
{
	if (!StmtHandle) return false;
	return sqlite3_bind_double(StmtHandle, Index, Value) == SQLITE_OK;
}

bool FESQLStatement::BindText(int32 Index, const FString& Value)
{
	if (!StmtHandle) return false;
	const FTCHARToUTF8 Utf8(*Value);
	return sqlite3_bind_text(StmtHandle, Index, Utf8.Get(), Utf8.Length(), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool FESQLStatement::BindBlob(int32 Index, const TArray<uint8>& Data)
{
	if (!StmtHandle) return false;
	return sqlite3_bind_blob(StmtHandle, Index, Data.GetData(), Data.Num(), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool FESQLStatement::BindNull(int32 Index)
{
	if (!StmtHandle) return false;
	return sqlite3_bind_null(StmtHandle, Index) == SQLITE_OK;
}

bool FESQLStatement::BindValue(int32 Index, const FESQLBindingValue& Value)
{
	if (Value.IsNull())
	{
		return BindNull(Index);
	}

	switch (Value.ValueType)
	{
	case EESQLColumnType::Null:
		return BindNull(Index);

	case EESQLColumnType::Integer:
		return BindInt(Index, Value.IntegerValue);

	case EESQLColumnType::Float:
		return BindFloat(Index, Value.FloatValue);

	case EESQLColumnType::Blob:
		return BindBlob(Index, Value.BlobValue);

	case EESQLColumnType::Text:
	default:
		return BindText(Index, Value.TextValue);
	}
}

bool FESQLStatement::BindValues(const TArray<FESQLBindingValue>& Bindings)
{
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		if (!BindValue(Index + 1, Bindings[Index]))
		{
			return false;
		}
	}

	return true;
}

bool FESQLStatement::BindStringArray(const TArray<FString>& Bindings)
{
	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		// SQLite bindings are 1-indexed
		if (!BindText(i + 1, Bindings[i]))
		{
			return false;
		}
	}
	return true;
}

bool FESQLStatement::ClearBindings()
{
	if (!StmtHandle) return false;
	return sqlite3_clear_bindings(StmtHandle) == SQLITE_OK;
}


// ── Execution ────────────────────────────────────────────────────────────────

bool FESQLStatement::Step()
{
	if (!StmtHandle)
	{
		LastStepResult = SQLITE_MISUSE;
		return false;
	}

	LastStepResult = sqlite3_step(StmtHandle);

	if (LastStepResult == SQLITE_ROW)
	{
		bDone = false;
		return true;
	}

	if (LastStepResult == SQLITE_DONE)
	{
		bDone = true;
		return false;
	}

	// Error — do NOT set bDone (it must stay false so IsDone() returns false,
	// causing ExecuteStatement to report Failure instead of Success)
	UE_LOG(LogExtendedSQL, Warning, TEXT("Statement::Step error: %hs"), sqlite3_errmsg(sqlite3_db_handle(StmtHandle)));
	return false;
}

bool FESQLStatement::EnumerateRows(TFunctionRef<bool(const FESQLRow&)> Visitor, FESQLError* OutError)
{
	if (!StmtHandle)
	{
		if (OutError)
		{
			*OutError = FESQLError::Make(EESQLErrorCode::InvalidState, TEXT("Statement handle is invalid"));
		}
		return false;
	}

	while (Step())
	{
		FESQLRow Row;
		const int32 ColumnCount = GetColumnCount();
		for (int32 ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex)
		{
			const FString ColumnName = GetColumnName(ColumnIndex);
			const bool bIsNull = GetColumnType(ColumnIndex) == EESQLColumnType::Null;
			Row.Columns.Add(ColumnName, bIsNull ? FString() : GetColumnText(ColumnIndex));
			if (bIsNull)
			{
				Row.NullColumns.Add(ColumnName);
			}
		}

		if (!Visitor(Row))
		{
			return true;
		}
	}

	if (!IsDone())
	{
		if (OutError)
		{
			*OutError = GetLastError(EESQLErrorCode::StepFailed, GetSQL());
		}
		return false;
	}

	if (OutError)
	{
		*OutError = FESQLError();
	}

	return true;
}

bool FESQLStatement::IsDone() const
{
	return bDone;
}

void FESQLStatement::Reset()
{
	if (StmtHandle)
	{
		sqlite3_reset(StmtHandle);
		bDone = false;
		LastStepResult = SQLITE_OK;
	}
}


// ── Column Readers ───────────────────────────────────────────────────────────

int64 FESQLStatement::GetColumnInt(int32 Index) const
{
	if (!StmtHandle) return 0;
	return sqlite3_column_int64(StmtHandle, Index);
}

double FESQLStatement::GetColumnFloat(int32 Index) const
{
	if (!StmtHandle) return 0.0;
	return sqlite3_column_double(StmtHandle, Index);
}

FString FESQLStatement::GetColumnText(int32 Index) const
{
	if (!StmtHandle) return FString();
	const unsigned char* Text = sqlite3_column_text(StmtHandle, Index);
	if (!Text) return FString();
	return UTF8_TO_TCHAR(reinterpret_cast<const char*>(Text));
}

TArray<uint8> FESQLStatement::GetColumnBlob(int32 Index) const
{
	TArray<uint8> Result;
	if (!StmtHandle) return Result;

	const void* Data = sqlite3_column_blob(StmtHandle, Index);
	const int32 Size = sqlite3_column_bytes(StmtHandle, Index);

	if (Data && Size > 0)
	{
		Result.SetNumUninitialized(Size);
		FMemory::Memcpy(Result.GetData(), Data, Size);
	}

	return Result;
}

EESQLColumnType FESQLStatement::GetColumnType(int32 Index) const
{
	if (!StmtHandle) return EESQLColumnType::Null;

	switch (sqlite3_column_type(StmtHandle, Index))
	{
	case SQLITE_INTEGER: return EESQLColumnType::Integer;
	case SQLITE_FLOAT:   return EESQLColumnType::Float;
	case SQLITE_TEXT:    return EESQLColumnType::Text;
	case SQLITE_BLOB:    return EESQLColumnType::Blob;
	case SQLITE_NULL:    return EESQLColumnType::Null;
	default:             return EESQLColumnType::Null;
	}
}

int32 FESQLStatement::GetColumnCount() const
{
	if (!StmtHandle) return 0;
	return sqlite3_column_count(StmtHandle);
}

FString FESQLStatement::GetColumnName(int32 Index) const
{
	if (!StmtHandle) return FString();
	const char* Name = sqlite3_column_name(StmtHandle, Index);
	if (!Name) return FString();
	return UTF8_TO_TCHAR(Name);
}


// ── State ────────────────────────────────────────────────────────────────────

bool FESQLStatement::IsValid() const
{
	return StmtHandle != nullptr;
}

FString FESQLStatement::GetSQL() const
{
	if (!StmtHandle) return FString();
	const char* SQL = sqlite3_sql(StmtHandle);
	if (!SQL) return FString();
	return UTF8_TO_TCHAR(SQL);
}

FESQLError FESQLStatement::GetLastError(EESQLErrorCode Code, const FString& SqlFragment) const
{
	if (!StmtHandle)
	{
		return FESQLError::Make(Code, TEXT("Statement handle is invalid"), SqlFragment);
	}

	return FESQLError::Make(
		Code,
		FString::Printf(TEXT("SQL execution error: %hs"), sqlite3_errmsg(sqlite3_db_handle(StmtHandle))),
		SqlFragment.IsEmpty() ? GetSQL() : SqlFragment);
}

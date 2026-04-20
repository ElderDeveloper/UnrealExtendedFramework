// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLDatabase.h"
#include "Private/Paths/ESQLPathResolver.h"
#include "Private/ThirdParty/ESQLVendorSqlite.h"
#include "Shared/ESQLSettings.h"
#include "UnrealExtendedSQL.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTLS.h"

namespace
{
FESQLError MakeSqlError(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return FESQLError::Make(Code, MoveTemp(Message), MoveTemp(SqlFragment));
}

template <typename TValue>
TESQLValueOrError<TValue> MakeValueError(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return TESQLValueOrError<TValue>::MakeError(MakeSqlError(Code, MoveTemp(Message), MoveTemp(SqlFragment)));
}

FESQLErrorResult MakeOperationError(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return FESQLErrorResult::Failure(MakeSqlError(Code, MoveTemp(Message), MoveTemp(SqlFragment)));
}

FESQLQueryResult MakeQueryError(EESQLErrorCode Code, FString Message, FString SqlFragment = FString())
{
	return FESQLQueryResult::Failure(MakeSqlError(Code, MoveTemp(Message), MoveTemp(SqlFragment)));
}

FESQLErrorResult ExecuteSqliteStatement(sqlite3* DbHandle, const char* Sql, EESQLErrorCode ErrorCode)
{
	sqlite3_stmt* Statement = nullptr;
	const int PrepareResult = sqlite3_prepare_v2(DbHandle, Sql, -1, &Statement, nullptr);
	if (PrepareResult != SQLITE_OK)
	{
		return MakeOperationError(
			ErrorCode,
			FString::Printf(TEXT("Failed to prepare SQL '%s': %hs"), UTF8_TO_TCHAR(Sql), sqlite3_errmsg(DbHandle)),
			UTF8_TO_TCHAR(Sql));
	}

	int StepResult = SQLITE_ROW;
	while ((StepResult = sqlite3_step(Statement)) == SQLITE_ROW)
	{
	}

	sqlite3_finalize(Statement);
	if (StepResult != SQLITE_DONE)
	{
		return MakeOperationError(
			ErrorCode,
			FString::Printf(TEXT("Failed to execute SQL '%s': %hs"), UTF8_TO_TCHAR(Sql), sqlite3_errmsg(DbHandle)),
			UTF8_TO_TCHAR(Sql));
	}

	return FESQLErrorResult::Success();
}
}


// ── Construction / Destruction ───────────────────────────────────────────────

FESQLDatabase::FESQLDatabase()
{
}

FESQLDatabase::~FESQLDatabase()
{
	Close();
}


// ── Open / Close ─────────────────────────────────────────────────────────────

FESQLDatabaseOpenResult FESQLDatabase::Open(const FString& FilePath)
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());

	const FString AbsPath = FESQLPathResolver::ResolveAbsolutePath(FilePath);
	Database->DatabasePath = AbsPath;
	Database->bInMemory = false;

	FESQLPathResolver::EnsureParentDirectoryExists(AbsPath);

	const FTCHARToUTF8 Utf8Path(*AbsPath);
	const int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&Database->DbHandle,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		const FESQLError Error = MakeSqlError(EESQLErrorCode::OpenFailed, FString::Printf(TEXT("Failed to open database '%s': %hs"),
			*FilePath,
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error"));
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *Error.Message);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return FESQLDatabaseOpenResult::MakeError(Error);
	}

	const FESQLErrorResult PragmaResult = Database->ApplyPragmas(false);
	if (!PragmaResult.bSuccess)
	{
		Database->Close();
		return FESQLDatabaseOpenResult::MakeError(PragmaResult.Error);
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Opened database: %s"), *FilePath);
	}

	return FESQLDatabaseOpenResult::MakeValue(Database);
}

FESQLDatabaseOpenResult FESQLDatabase::OpenReadOnly(const FString& FilePath)
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());

	const FString AbsPath = FESQLPathResolver::ResolveAbsolutePath(FilePath);
	Database->DatabasePath = AbsPath;
	Database->bInMemory = false;

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AbsPath))
	{
		return FESQLDatabaseOpenResult::MakeError(MakeSqlError(
			EESQLErrorCode::IoFailure,
			FString::Printf(TEXT("Database file does not exist: %s"), *FilePath)));
	}

	const FTCHARToUTF8 Utf8Path(*AbsPath);
	const int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&Database->DbHandle,
		SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		const FESQLError Error = MakeSqlError(EESQLErrorCode::OpenFailed, FString::Printf(TEXT("Failed to open database (read-only) '%s': %hs"),
			*FilePath,
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error"));
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *Error.Message);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return FESQLDatabaseOpenResult::MakeError(Error);
	}

	const FESQLErrorResult PragmaResult = Database->ApplyPragmas(true);
	if (!PragmaResult.bSuccess)
	{
		Database->Close();
		return FESQLDatabaseOpenResult::MakeError(PragmaResult.Error);
	}

	return FESQLDatabaseOpenResult::MakeValue(Database);
}

FESQLDatabaseOpenResult FESQLDatabase::OpenInMemory()
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());
	Database->bInMemory = true;

	const int Result = sqlite3_open_v2(
		":memory:",
		&Database->DbHandle,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY | SQLITE_OPEN_NOMUTEX,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		const FESQLError Error = MakeSqlError(EESQLErrorCode::OpenFailed, FString::Printf(TEXT("Failed to open in-memory database: %hs"),
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error"));
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *Error.Message);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return FESQLDatabaseOpenResult::MakeError(Error);
	}

	const FESQLErrorResult PragmaResult = Database->ApplyPragmas(false);
	if (!PragmaResult.bSuccess)
	{
		Database->Close();
		return FESQLDatabaseOpenResult::MakeError(PragmaResult.Error);
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Opened in-memory database"));
	}

	return FESQLDatabaseOpenResult::MakeValue(Database);
}

void FESQLDatabase::Close()
{
	FScopeLock Lock(&CriticalSection);
	if (DbHandle)
	{
		CheckThreadAffinity(TEXT("Close"));
	}

	// Clear statement cache first — all statements must be finalized before closing
	StatementCache.Empty();

	if (DbHandle)
	{
		const int Result = sqlite3_close(DbHandle);
		if (Result != SQLITE_OK)
		{
			UE_LOG(LogExtendedSQL, Warning, TEXT("Error closing database '%s': %hs"),
				*DatabasePath, sqlite3_errmsg(DbHandle));
		}
		DbHandle = nullptr;
	}

	DatabasePath.Empty();
}

bool FESQLDatabase::IsOpen() const
{
	return DbHandle != nullptr;
}

bool FESQLDatabase::IsInMemory() const
{
	return bInMemory;
}

FString FESQLDatabase::GetDatabasePath() const
{
	return DatabasePath;
}

void FESQLDatabase::CheckThreadAffinity(const TCHAR* Operation) const
{
#if DO_CHECK
	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
	if (!OwningThreadId.IsSet())
	{
		OwningThreadId = CurrentThreadId;
		return;
	}

	checkf(
		OwningThreadId.GetValue() == CurrentThreadId,
		TEXT("ESQLDatabase '%s' is bound to thread %u but thread %u attempted %s while the connection is opened with SQLITE_OPEN_NOMUTEX."),
		DatabasePath.IsEmpty() ? TEXT(":memory:") : *DatabasePath,
		OwningThreadId.GetValue(),
		CurrentThreadId,
		Operation);
#else
	(void)Operation;
#endif
}


// ── PRAGMAs ──────────────────────────────────────────────────────────────────

FESQLErrorResult FESQLDatabase::ApplyPragmas(bool bReadOnly)
{
	if (!DbHandle)
	{
		return MakeOperationError(EESQLErrorCode::InvalidState, TEXT("Cannot apply PRAGMAs — database not open"));
	}

	const UESQLSettings* Settings = UESQLSettings::Get();

	if (sqlite3_busy_timeout(DbHandle, 5000) != SQLITE_OK)
	{
		return MakeOperationError(EESQLErrorCode::PragmaFailed, FString::Printf(TEXT("Failed to set busy timeout: %hs"), sqlite3_errmsg(DbHandle)));
	}

	const FESQLErrorResult ForeignKeysResult = ExecuteSqliteStatement(DbHandle, "PRAGMA foreign_keys=ON;", EESQLErrorCode::PragmaFailed);
	if (!ForeignKeysResult.bSuccess)
	{
		return ForeignKeysResult;
	}

	// WAL mode only applies to writable file-backed databases.
	if (!bReadOnly && Settings && Settings->bUseWALMode && !bInMemory)
	{
		const FESQLErrorResult WalResult = ExecuteSqliteStatement(DbHandle, "PRAGMA journal_mode=WAL;", EESQLErrorCode::PragmaFailed);
		if (!WalResult.bSuccess)
		{
			return WalResult;
		}
	}

	if (!bReadOnly && !bInMemory)
	{
		const FESQLErrorResult SyncResult = ExecuteSqliteStatement(DbHandle, "PRAGMA synchronous=NORMAL;", EESQLErrorCode::PragmaFailed);
		if (!SyncResult.bSuccess)
		{
			return SyncResult;
		}
	}

	return FESQLErrorResult::Success();
}


// ── Execute ──────────────────────────────────────────────────────────────────

FESQLQueryResult FESQLDatabase::Execute(const FString& SQL)
{
	return Execute(SQL, TArray<FESQLBindingValue>());
}

FESQLQueryResult FESQLDatabase::Execute(const FString& SQL, const TArray<FString>& Bindings)
{
	TArray<FESQLBindingValue> NullableBindings;
	NullableBindings.Reserve(Bindings.Num());
	for (const FString& Binding : Bindings)
	{
		NullableBindings.Add(FESQLBindingValue::FromText(Binding));
	}

	return Execute(SQL, NullableBindings);
}

FESQLQueryResult FESQLDatabase::Execute(const FString& SQL, const TArray<FESQLBindingValue>& Bindings)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		return MakeQueryError(EESQLErrorCode::InvalidState, TEXT("Database not open"), SQL);
	}

	CheckThreadAffinity(TEXT("Execute"));

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("SQL: %s"), *SQL);
	}

	const FESQLStatementPrepareResult PrepareResult = GetOrPrepareLocked(SQL);
	if (!PrepareResult)
	{
		return FESQLQueryResult::Failure(PrepareResult.GetError());
	}
	TSharedPtr<FESQLStatement> Statement = PrepareResult.GetValue();

	// Bind parameters
	if (Bindings.Num() > 0 && !Statement->BindValues(Bindings))
	{
		return MakeQueryError(
			EESQLErrorCode::BindFailed,
			FString::Printf(TEXT("Failed to bind parameters: %hs"), sqlite3_errmsg(DbHandle)),
			SQL);
	}

	return ExecuteStatement(Statement);
}


// ── Execute Statement (internal) ─────────────────────────────────────────────

FESQLQueryResult FESQLDatabase::ExecuteStatement(TSharedPtr<FESQLStatement> Statement)
{
	if (!Statement || !Statement->IsValid())
	{
		return MakeQueryError(EESQLErrorCode::InvalidArgument, TEXT("Invalid statement"));
	}

	TArray<FESQLRow> Rows;
	TArray<FESQLColumn> Columns;
	const int32 ColumnCount = Statement->GetColumnCount();
	Columns.Reserve(ColumnCount);
	for (int32 ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex)
	{
		FESQLColumn Column;
		Column.Name = Statement->GetColumnName(ColumnIndex);
		Column.Type = EESQLColumnType::Null;
		Columns.Add(MoveTemp(Column));
	}

	FESQLError RowError;
	if (!Statement->EnumerateRows(
		[&Rows, &Columns, &Statement](const FESQLRow& Row)
		{
			if (Rows.IsEmpty())
			{
				for (int32 ColumnIndex = 0; ColumnIndex < Columns.Num(); ++ColumnIndex)
				{
					Columns[ColumnIndex].Type = Statement->GetColumnType(ColumnIndex);
				}
			}

			Rows.Add(Row);
			return true;
		},
		&RowError))
	{
		return FESQLQueryResult::Failure(RowError);
	}

	const int32 RowsAffected = DbHandle ? sqlite3_changes(DbHandle) : 0;
	const int64 LastRowId = DbHandle ? sqlite3_last_insert_rowid(DbHandle) : 0;

	return FESQLQueryResult::Success(Rows, Columns, RowsAffected, LastRowId);
}


// ── Prepared Statement API ───────────────────────────────────────────────────

FESQLStatementPrepareResult FESQLDatabase::Prepare(const FString& SQL)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		return MakeValueError<TSharedPtr<FESQLStatement>>(EESQLErrorCode::InvalidState, TEXT("Database not open"), SQL);
	}

	CheckThreadAffinity(TEXT("Prepare"));

	TSharedPtr<FESQLStatement> Statement = MakeShareable(new FESQLStatement());

	const FTCHARToUTF8 Utf8SQL(*SQL);
	const int Result = sqlite3_prepare_v2(DbHandle, Utf8SQL.Get(), Utf8SQL.Length(), &Statement->StmtHandle, nullptr);

	if (Result != SQLITE_OK)
	{
		return MakeValueError<TSharedPtr<FESQLStatement>>(
			EESQLErrorCode::PrepareFailed,
			FString::Printf(TEXT("Failed to prepare SQL: %hs"), sqlite3_errmsg(DbHandle)),
			SQL);
	}

	return FESQLStatementPrepareResult::MakeValue(Statement);
}

FESQLStatementPrepareResult FESQLDatabase::GetOrPrepare(const FString& SQL)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		return MakeValueError<TSharedPtr<FESQLStatement>>(EESQLErrorCode::InvalidState, TEXT("Database not open"), SQL);
	}

	CheckThreadAffinity(TEXT("GetOrPrepare"));
	return GetOrPrepareLocked(SQL);
}

FESQLStatementPrepareResult FESQLDatabase::GetOrPrepareLocked(const FString& SQL)
{
	// Check cache first
	if (TSharedPtr<FESQLStatement>* Found = StatementCache.Find(SQL))
	{
		TSharedPtr<FESQLStatement>& CachedStmt = *Found;
		if (CachedStmt && CachedStmt->IsValid())
		{
			// Reset + clear bindings before returning (prevent stale-binding bugs)
			CachedStmt->Reset();
			CachedStmt->ClearBindings();
			return FESQLStatementPrepareResult::MakeValue(CachedStmt);
		}
	}

	// Not cached — prepare new
	// Note: We do NOT hold the CriticalSection here because Execute() already holds it
	TSharedPtr<FESQLStatement> Statement = MakeShareable(new FESQLStatement());

	const FTCHARToUTF8 Utf8SQL(*SQL);
	const int Result = sqlite3_prepare_v2(DbHandle, Utf8SQL.Get(), Utf8SQL.Length(), &Statement->StmtHandle, nullptr);

	if (Result != SQLITE_OK)
	{
		return MakeValueError<TSharedPtr<FESQLStatement>>(
			EESQLErrorCode::PrepareFailed,
			FString::Printf(TEXT("Failed to prepare SQL: %hs"), sqlite3_errmsg(DbHandle)),
			SQL);
	}

	// Cache the statement
	StatementCache.Add(SQL, Statement);
	return FESQLStatementPrepareResult::MakeValue(Statement);
}


// ── Transactions ─────────────────────────────────────────────────────────────

FESQLErrorResult FESQLDatabase::BeginTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("BEGIN TRANSACTION"));
	return Result.bSuccess
		? FESQLErrorResult::Success()
		: FESQLErrorResult::Failure(Result.Error.HasError() ? Result.Error : MakeSqlError(EESQLErrorCode::TransactionFailed, Result.ErrorMessage, TEXT("BEGIN TRANSACTION")));
}

FESQLErrorResult FESQLDatabase::CommitTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("COMMIT"));
	return Result.bSuccess
		? FESQLErrorResult::Success()
		: FESQLErrorResult::Failure(Result.Error.HasError() ? Result.Error : MakeSqlError(EESQLErrorCode::TransactionFailed, Result.ErrorMessage, TEXT("COMMIT")));
}

FESQLErrorResult FESQLDatabase::RollbackTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("ROLLBACK"));
	return Result.bSuccess
		? FESQLErrorResult::Success()
		: FESQLErrorResult::Failure(Result.Error.HasError() ? Result.Error : MakeSqlError(EESQLErrorCode::TransactionFailed, Result.ErrorMessage, TEXT("ROLLBACK")));
}


// ── Backup / Snapshot ────────────────────────────────────────────────────────

FESQLErrorResult FESQLDatabase::BackupToFile(const FString& DestFilePath)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		return MakeOperationError(EESQLErrorCode::InvalidState, TEXT("Database not open"));
	}

	CheckThreadAffinity(TEXT("BackupToFile"));

	const FString AbsoluteDestPath = FESQLPathResolver::ResolveAbsolutePath(DestFilePath);
	FESQLPathResolver::EnsureParentDirectoryExists(AbsoluteDestPath);

	// Open destination database
	sqlite3* pDest = nullptr;
	const FTCHARToUTF8 Utf8Path(*AbsoluteDestPath);
	int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&pDest,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		const FESQLErrorResult ErrorResult = MakeOperationError(EESQLErrorCode::BackupFailed, FString::Printf(TEXT("Failed to open backup destination '%s': %hs"),
			*AbsoluteDestPath, pDest ? sqlite3_errmsg(pDest) : "unknown"));
		if (pDest) sqlite3_close(pDest);
		return ErrorResult;
	}

	// Perform backup
	sqlite3_backup* pBackup = sqlite3_backup_init(pDest, "main", DbHandle, "main");
	if (!pBackup)
	{
		const FESQLErrorResult ErrorResult = MakeOperationError(EESQLErrorCode::BackupFailed, FString::Printf(TEXT("Failed to init backup: %hs"), sqlite3_errmsg(pDest)));
		sqlite3_close(pDest);
		return ErrorResult;
	}

	Result = sqlite3_backup_step(pBackup, -1);  // -1 = copy all pages at once
	sqlite3_backup_finish(pBackup);
	sqlite3_close(pDest);

	if (Result != SQLITE_DONE)
	{
		return MakeOperationError(EESQLErrorCode::BackupFailed, FString::Printf(TEXT("Backup failed with error %d"), Result));
	}

	return FESQLErrorResult::Success();
}

FESQLErrorResult FESQLDatabase::RestoreFromFile(const FString& SourceFilePath)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		return MakeOperationError(EESQLErrorCode::InvalidState, TEXT("Database not open"));
	}

	CheckThreadAffinity(TEXT("RestoreFromFile"));

	const FString AbsoluteSourcePath = FESQLPathResolver::ResolveAbsolutePath(SourceFilePath);

	// Verify source exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*AbsoluteSourcePath))
	{
		return MakeOperationError(EESQLErrorCode::IoFailure, FString::Printf(TEXT("Source file does not exist: %s"), *AbsoluteSourcePath));
	}

	// Open source database
	sqlite3* pSource = nullptr;
	const FTCHARToUTF8 Utf8Path(*AbsoluteSourcePath);
	int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&pSource,
		SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		const FESQLErrorResult ErrorResult = MakeOperationError(EESQLErrorCode::RestoreFailed, FString::Printf(TEXT("Failed to open source '%s': %hs"),
			*AbsoluteSourcePath, pSource ? sqlite3_errmsg(pSource) : "unknown"));
		if (pSource) sqlite3_close(pSource);
		return ErrorResult;
	}

	// Clear statement cache — all existing statements are invalidated
	StatementCache.Empty();

	// Perform restore (source → this database)
	sqlite3_backup* pBackup = sqlite3_backup_init(DbHandle, "main", pSource, "main");
	if (!pBackup)
	{
		const FESQLErrorResult ErrorResult = MakeOperationError(EESQLErrorCode::RestoreFailed, FString::Printf(TEXT("Failed to init restore: %hs"), sqlite3_errmsg(DbHandle)));
		sqlite3_close(pSource);
		return ErrorResult;
	}

	Result = sqlite3_backup_step(pBackup, -1);
	sqlite3_backup_finish(pBackup);
	sqlite3_close(pSource);

	if (Result != SQLITE_DONE)
	{
		return MakeOperationError(EESQLErrorCode::RestoreFailed, FString::Printf(TEXT("Restore failed with error %d"), Result));
	}

	return FESQLErrorResult::Success();
}
int64 FESQLDatabase::GetLastInsertRowId() const
{
	FScopeLock Lock(&CriticalSection);
	if (!DbHandle) return 0;
	CheckThreadAffinity(TEXT("GetLastInsertRowId"));
	return sqlite3_last_insert_rowid(DbHandle);
}

int32 FESQLDatabase::GetChangesCount() const
{
	FScopeLock Lock(&CriticalSection);
	if (!DbHandle) return 0;
	CheckThreadAffinity(TEXT("GetChangesCount"));
	return sqlite3_changes(DbHandle);
}

FString FESQLDatabase::GetLastErrorMessage() const
{
	FScopeLock Lock(&CriticalSection);
	if (!DbHandle) return FString();
	CheckThreadAffinity(TEXT("GetLastErrorMessage"));
	return UTF8_TO_TCHAR(sqlite3_errmsg(DbHandle));
}

void FESQLDatabase::ResetThreadAffinity()
{
	FScopeLock Lock(&CriticalSection);
	OwningThreadId.Reset();
}

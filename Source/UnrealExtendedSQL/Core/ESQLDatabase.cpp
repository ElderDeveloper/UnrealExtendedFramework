// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLDatabase.h"
#include "ESQLUnrealVFS.h"
#include "Shared/ESQLSettings.h"
#include "UnrealExtendedSQL.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#include "sqlite3.h"
THIRD_PARTY_INCLUDES_END


// ── Construction / Destruction ───────────────────────────────────────────────

FESQLDatabase::FESQLDatabase()
{
}

FESQLDatabase::~FESQLDatabase()
{
	Close();
}


// ── Open / Close ─────────────────────────────────────────────────────────────

TSharedPtr<FESQLDatabase> FESQLDatabase::Open(const FString& FilePath, FString& OutError)
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());

	// CRITICAL: Convert to absolute path — UE's FPaths::ProjectSavedDir() returns
	// relative paths like "../../../../../../Users/..." which SQLite cannot open.
	const FString AbsPath = FPaths::ConvertRelativePathToFull(FilePath);
	Database->DatabasePath = AbsPath;
	Database->bInMemory = false;

	// Ensure parent directory exists
	const FString Directory = FPaths::GetPath(AbsPath);
	if (!Directory.IsEmpty())
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*Directory))
		{
			PlatformFile.CreateDirectoryTree(*Directory);
		}
	}

	const FTCHARToUTF8 Utf8Path(*AbsPath);
	const int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&Database->DbHandle,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
		FESQLUnrealVFS::GetVFSName()
	);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to open database '%s': %hs"),
			*FilePath,
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error");
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *OutError);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return nullptr;
	}

	if (!Database->ApplyPragmas(OutError))
	{
		Database->Close();
		return nullptr;
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Opened database: %s"), *FilePath);
	}

	return Database;
}

TSharedPtr<FESQLDatabase> FESQLDatabase::OpenReadOnly(const FString& FilePath, FString& OutError)
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());

	const FString AbsPath = FPaths::ConvertRelativePathToFull(FilePath);
	Database->DatabasePath = AbsPath;
	Database->bInMemory = false;

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AbsPath))
	{
		OutError = FString::Printf(TEXT("Database file does not exist: %s"), *FilePath);
		return nullptr;
	}

	const FTCHARToUTF8 Utf8Path(*AbsPath);
	// Use nullptr for VFS (default SQLite VFS) — our custom UE VFS uses
	// exclusive file handles via OpenWrite() which block concurrent access.
	// The default OS VFS supports proper shared locking for read-only connections.
	const int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&Database->DbHandle,
		SQLITE_OPEN_READONLY,
		nullptr
	);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to open database (read-only) '%s': %hs"),
			*FilePath,
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error");
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *OutError);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return nullptr;
	}

	return Database;
}

TSharedPtr<FESQLDatabase> FESQLDatabase::OpenInMemory(FString& OutError)
{
	TSharedPtr<FESQLDatabase> Database = MakeShareable(new FESQLDatabase());
	Database->bInMemory = true;

	const int Result = sqlite3_open(":memory:", &Database->DbHandle);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to open in-memory database: %hs"),
			Database->DbHandle ? sqlite3_errmsg(Database->DbHandle) : "unknown error");
		UE_LOG(LogExtendedSQL, Error, TEXT("%s"), *OutError);

		if (Database->DbHandle)
		{
			sqlite3_close(Database->DbHandle);
			Database->DbHandle = nullptr;
		}
		return nullptr;
	}

	if (!Database->ApplyPragmas(OutError))
	{
		Database->Close();
		return nullptr;
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Opened in-memory database"));
	}

	return Database;
}

void FESQLDatabase::Close()
{
	FScopeLock Lock(&CriticalSection);

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


// ── PRAGMAs ──────────────────────────────────────────────────────────────────

bool FESQLDatabase::ApplyPragmas(FString& OutError)
{
	if (!DbHandle)
	{
		OutError = TEXT("Cannot apply PRAGMAs — database not open");
		return false;
	}

	const UESQLSettings* Settings = UESQLSettings::Get();

	// WAL mode (if enabled in settings)
	if (Settings && Settings->bUseWALMode && !bInMemory)
	{
		char* ErrMsg = nullptr;
		int Result = sqlite3_exec(DbHandle, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &ErrMsg);
		if (Result != SQLITE_OK)
		{
			OutError = FString::Printf(TEXT("Failed to set WAL mode: %hs"), ErrMsg ? ErrMsg : "unknown");
			if (ErrMsg) sqlite3_free(ErrMsg);
			return false;
		}
	}

	// Foreign keys (always enabled)
	{
		char* ErrMsg = nullptr;
		int Result = sqlite3_exec(DbHandle, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &ErrMsg);
		if (Result != SQLITE_OK)
		{
			OutError = FString::Printf(TEXT("Failed to enable foreign keys: %hs"), ErrMsg ? ErrMsg : "unknown");
			if (ErrMsg) sqlite3_free(ErrMsg);
			return false;
		}
	}

	return true;
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
		return FESQLQueryResult::Failure(TEXT("Database not open"));
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("SQL: %s"), *SQL);
	}

	FString PrepError;
	TSharedPtr<FESQLStatement> Statement = GetOrPrepare(SQL, PrepError);
	if (!Statement)
	{
		return FESQLQueryResult::Failure(PrepError);
	}

	// Bind parameters
	if (Bindings.Num() > 0)
	{
		for (int32 Index = 0; Index < Bindings.Num(); ++Index)
		{
			const FESQLBindingValue& Binding = Bindings[Index];

			bool bBound = false;
			if (Binding.IsNull())
			{
				bBound = Statement->BindNull(Index + 1);
			}
			else
			{
				switch (Binding.ValueType)
				{
				case EESQLColumnType::Null:
					bBound = Statement->BindNull(Index + 1);
					break;

				case EESQLColumnType::Integer:
					bBound = Statement->BindInt(Index + 1, Binding.IntegerValue);
					break;

				case EESQLColumnType::Float:
					bBound = Statement->BindFloat(Index + 1, Binding.FloatValue);
					break;

				case EESQLColumnType::Blob:
					bBound = Statement->BindBlob(Index + 1, Binding.BlobValue);
					break;

				case EESQLColumnType::Text:
				default:
					bBound = Statement->BindText(Index + 1, Binding.TextValue);
					break;
				}
			}

			if (!bBound)
			{
				return FESQLQueryResult::Failure(FString::Printf(TEXT("Failed to bind parameters: %hs"),
					sqlite3_errmsg(DbHandle)));
			}
		}
	}

	return ExecuteStatement(Statement);
}


// ── Execute Statement (internal) ─────────────────────────────────────────────

FESQLQueryResult FESQLDatabase::ExecuteStatement(TSharedPtr<FESQLStatement> Statement)
{
	if (!Statement || !Statement->IsValid())
	{
		return FESQLQueryResult::Failure(TEXT("Invalid statement"));
	}

	TArray<FESQLRow> Rows;
	TArray<FESQLColumn> Columns;
	bool bColumnsPopulated = false;

	// Step through all result rows
	while (Statement->Step())
	{
		// Populate column definitions on first row
		if (!bColumnsPopulated)
		{
			const int32 ColCount = Statement->GetColumnCount();
			Columns.Reserve(ColCount);
			for (int32 i = 0; i < ColCount; ++i)
			{
				FESQLColumn Col;
				Col.Name = Statement->GetColumnName(i);
				Col.Type = Statement->GetColumnType(i);
				Columns.Add(Col);
			}
			bColumnsPopulated = true;
		}

		// Read row data
		FESQLRow Row;
		const int32 ColCount = Statement->GetColumnCount();
		for (int32 i = 0; i < ColCount; ++i)
		{
			const FString ColName = Statement->GetColumnName(i);
			const bool bIsNull = Statement->GetColumnType(i) == EESQLColumnType::Null;
			const FString Value = bIsNull ? FString() : Statement->GetColumnText(i);
			Row.Columns.Add(ColName, Value);
			if (bIsNull)
			{
				Row.NullColumns.Add(ColName);
			}
		}
		Rows.Add(MoveTemp(Row));
	}

	// Check for errors (Step returns false on both DONE and ERROR)
	if (!Statement->IsDone())
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("SQL execution error: %hs"),
			sqlite3_errmsg(DbHandle)));
	}

	const int32 RowsAffected = GetChangesCount();
	const int64 LastRowId = GetLastInsertRowId();

	return FESQLQueryResult::Success(Rows, Columns, RowsAffected, LastRowId);
}


// ── Prepared Statement API ───────────────────────────────────────────────────

TSharedPtr<FESQLStatement> FESQLDatabase::Prepare(const FString& SQL, FString& OutError)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		OutError = TEXT("Database not open");
		return nullptr;
	}

	TSharedPtr<FESQLStatement> Statement = MakeShareable(new FESQLStatement());

	const FTCHARToUTF8 Utf8SQL(*SQL);
	const int Result = sqlite3_prepare_v2(DbHandle, Utf8SQL.Get(), Utf8SQL.Length(), &Statement->StmtHandle, nullptr);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to prepare SQL: %hs\nSQL: %s"),
			sqlite3_errmsg(DbHandle), *SQL);
		return nullptr;
	}

	return Statement;
}

TSharedPtr<FESQLStatement> FESQLDatabase::GetOrPrepare(const FString& SQL, FString& OutError)
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
			return CachedStmt;
		}
	}

	// Not cached — prepare new
	// Note: We do NOT hold the CriticalSection here because Execute() already holds it
	TSharedPtr<FESQLStatement> Statement = MakeShareable(new FESQLStatement());

	const FTCHARToUTF8 Utf8SQL(*SQL);
	const int Result = sqlite3_prepare_v2(DbHandle, Utf8SQL.Get(), Utf8SQL.Length(), &Statement->StmtHandle, nullptr);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to prepare SQL: %hs\nSQL: %s"),
			sqlite3_errmsg(DbHandle), *SQL);
		return nullptr;
	}

	// Cache the statement
	StatementCache.Add(SQL, Statement);
	return Statement;
}


// ── Transactions ─────────────────────────────────────────────────────────────

bool FESQLDatabase::BeginTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("BEGIN TRANSACTION"));
	return Result.bSuccess;
}

bool FESQLDatabase::CommitTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("COMMIT"));
	return Result.bSuccess;
}

bool FESQLDatabase::RollbackTransaction()
{
	FESQLQueryResult Result = Execute(TEXT("ROLLBACK"));
	return Result.bSuccess;
}


// ── Backup / Snapshot ────────────────────────────────────────────────────────

bool FESQLDatabase::BackupToFile(const FString& DestFilePath, FString& OutError)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		OutError = TEXT("Database not open");
		return false;
	}

	// Ensure destination directory exists
	const FString Directory = FPaths::GetPath(DestFilePath);
	if (!Directory.IsEmpty())
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*Directory))
		{
			PlatformFile.CreateDirectoryTree(*Directory);
		}
	}

	// Open destination database
	sqlite3* pDest = nullptr;
	const FTCHARToUTF8 Utf8Path(*DestFilePath);
	int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&pDest,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
		FESQLUnrealVFS::GetVFSName()
	);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to open backup destination '%s': %hs"),
			*DestFilePath, pDest ? sqlite3_errmsg(pDest) : "unknown");
		if (pDest) sqlite3_close(pDest);
		return false;
	}

	// Perform backup
	sqlite3_backup* pBackup = sqlite3_backup_init(pDest, "main", DbHandle, "main");
	if (!pBackup)
	{
		OutError = FString::Printf(TEXT("Failed to init backup: %hs"), sqlite3_errmsg(pDest));
		sqlite3_close(pDest);
		return false;
	}

	Result = sqlite3_backup_step(pBackup, -1);  // -1 = copy all pages at once
	sqlite3_backup_finish(pBackup);
	sqlite3_close(pDest);

	if (Result != SQLITE_DONE)
	{
		OutError = FString::Printf(TEXT("Backup failed with error %d"), Result);
		return false;
	}

	return true;
}

bool FESQLDatabase::RestoreFromFile(const FString& SourceFilePath, FString& OutError)
{
	FScopeLock Lock(&CriticalSection);

	if (!DbHandle)
	{
		OutError = TEXT("Database not open");
		return false;
	}

	// Verify source exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*SourceFilePath))
	{
		OutError = FString::Printf(TEXT("Source file does not exist: %s"), *SourceFilePath);
		return false;
	}

	// Open source database
	sqlite3* pSource = nullptr;
	const FTCHARToUTF8 Utf8Path(*SourceFilePath);
	int Result = sqlite3_open_v2(
		Utf8Path.Get(),
		&pSource,
		SQLITE_OPEN_READONLY,
		FESQLUnrealVFS::GetVFSName()
	);

	if (Result != SQLITE_OK)
	{
		OutError = FString::Printf(TEXT("Failed to open source '%s': %hs"),
			*SourceFilePath, pSource ? sqlite3_errmsg(pSource) : "unknown");
		if (pSource) sqlite3_close(pSource);
		return false;
	}

	// Clear statement cache — all existing statements are invalidated
	StatementCache.Empty();

	// Perform restore (source → this database)
	sqlite3_backup* pBackup = sqlite3_backup_init(DbHandle, "main", pSource, "main");
	if (!pBackup)
	{
		OutError = FString::Printf(TEXT("Failed to init restore: %hs"), sqlite3_errmsg(DbHandle));
		sqlite3_close(pSource);
		return false;
	}

	Result = sqlite3_backup_step(pBackup, -1);
	sqlite3_backup_finish(pBackup);
	sqlite3_close(pSource);

	if (Result != SQLITE_DONE)
	{
		OutError = FString::Printf(TEXT("Restore failed with error %d"), Result);
		return false;
	}

	return true;
}


// ── SQL Dump (VCS Pipeline) ──────────────────────────────────────────────────

#include "VCSPipeline/ESQLDumpPipeline.h"

bool FESQLDatabase::ExportToSQLDump(const FString& DumpFilePath, FString& OutError)
{
	if (!IsOpen())
	{
		OutError = TEXT("Database not open");
		return false;
	}

	return FESQLDumpPipeline::ExportDatabaseToDump(SharedThis(this), DumpFilePath, OutError);
}

bool FESQLDatabase::ImportFromSQLDump(const FString& DumpFilePath, FString& OutError)
{
	if (!IsOpen())
	{
		OutError = TEXT("Database not open");
		return false;
	}

	return FESQLDumpPipeline::ImportDumpToDatabase(SharedThis(this), DumpFilePath, OutError);
}

int64 FESQLDatabase::GetLastInsertRowId() const
{
	if (!DbHandle) return 0;
	return sqlite3_last_insert_rowid(DbHandle);
}

int32 FESQLDatabase::GetChangesCount() const
{
	if (!DbHandle) return 0;
	return sqlite3_changes(DbHandle);
}

FString FESQLDatabase::GetLastErrorMessage() const
{
	if (!DbHandle) return FString();
	return UTF8_TO_TCHAR(sqlite3_errmsg(DbHandle));
}

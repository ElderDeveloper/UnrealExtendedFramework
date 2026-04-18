// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"
#include "Core/ESQLStatement.h"
#include "HAL/CriticalSection.h"

// Forward declaration — avoid exposing sqlite3.h in header
struct sqlite3;

/**
 * RAII wrapper around a sqlite3* connection handle.
 * NOT a UObject — used internally by UESQLSubsystem and can be used
 * from any C++ code without depending on the UObject ecosystem.
 *
 * Thread safety: All public methods acquire the CriticalSection.
 * Multiple threads may safely call Execute/Prepare concurrently.
 *
 * All Execute/Query functions use sqlite3_prepare_v2() internally,
 * NEVER sqlite3_exec(). Multi-statement SQL strings are rejected —
 * only the first statement is compiled. This is deliberate.
 */
class UNREALEXTENDEDSQL_API FESQLDatabase : public TSharedFromThis<FESQLDatabase>
{
public:

	~FESQLDatabase();

	// Non-copyable
	FESQLDatabase(const FESQLDatabase&) = delete;
	FESQLDatabase& operator=(const FESQLDatabase&) = delete;


	// ── Open / Close ─────────────────────────────────────────────────────

	/** Open a file-backed database (Persistent mode).
	    On open, automatically executes:
	      PRAGMA journal_mode=WAL;    (if bUseWALMode from settings, default: true)
	      PRAGMA foreign_keys=ON;     (SQLite FK enforcement is OFF by default)
	    Creates the file and parent directories if they don't exist. */
	static TSharedPtr<FESQLDatabase> Open(const FString& FilePath, FString& OutError);

	/** Open a file-backed database in READ-ONLY mode.
	    Use this for editor tools that only need to query data (e.g. FESQLId picker)
	    without conflicting with an existing read-write connection. */
	static TSharedPtr<FESQLDatabase> OpenReadOnly(const FString& FilePath, FString& OutError);

	/** Open a purely in-memory database (Session mode).
	    The database lives only in RAM — no file is created on disk.
	    Use BackupToFile() to persist it (snapshot), and
	    RestoreFromFile() to load a snapshot into memory. */
	static TSharedPtr<FESQLDatabase> OpenInMemory(FString& OutError);

	/** Close the database connection. Clears statement cache. */
	void Close();

	/** Returns true if the connection is open. */
	bool IsOpen() const;

	/** Returns true if this is an in-memory database (Session mode). */
	bool IsInMemory() const;

	/** Returns the file path this database was opened with. Empty for in-memory. */
	FString GetDatabasePath() const;


	// ── Execute (convenience) ────────────────────────────────────────────

	/** Execute a SQL statement with no bindings. Returns result with rows. */
	FESQLQueryResult Execute(const FString& SQL);

	/** Execute a SQL statement with string bindings (?1, ?2, etc.). */
	FESQLQueryResult Execute(const FString& SQL, const TArray<FString>& Bindings);

	/** Execute a SQL statement with nullable text bindings (?1, ?2, etc.). */
	FESQLQueryResult Execute(const FString& SQL, const TArray<FESQLBindingValue>& Bindings);


	// ── Prepared Statement API ───────────────────────────────────────────

	/** Prepare (compile) a SQL statement. Returns nullptr on error.
	    For repeated queries, prefer GetOrPrepare() which uses the cache. */
	TSharedPtr<FESQLStatement> Prepare(const FString& SQL, FString& OutError);


	// ── Transaction Helpers ──────────────────────────────────────────────

	bool BeginTransaction();
	bool CommitTransaction();
	bool RollbackTransaction();


	// ── Backup / Snapshot ────────────────────────────────────────────────

	/** Backup this database (memory or file) to a .db file on disk.
	    Uses sqlite3_backup API — safe to call while the DB is live.
	    This is the implementation behind UESQLSubsystem::SaveSnapshot(). */
	bool BackupToFile(const FString& DestFilePath, FString& OutError);

	/** Replace this database's contents with data from a .db file on disk.
	    Uses sqlite3_backup API — the current contents are fully overwritten.
	    This is the implementation behind UESQLSubsystem::LoadSnapshot(). */
	bool RestoreFromFile(const FString& SourceFilePath, FString& OutError);


	// ── SQL Dump (VCS Pipeline) ──────────────────────────────────────────

	/** Export this database to a .sqldump text file.
	    Writes CREATE TABLE + sorted INSERT OR REPLACE for every user table.
	    Convenience wrapper around FESQLDumpPipeline::ExportDatabaseToDump(). */
	bool ExportToSQLDump(const FString& DumpFilePath, FString& OutError);

	/** Import a .sqldump text file into this database.
	    Executes statements in a transaction.
	    Convenience wrapper around FESQLDumpPipeline::ImportDumpToDatabase(). */
	bool ImportFromSQLDump(const FString& DumpFilePath, FString& OutError);


	// ── Metadata ─────────────────────────────────────────────────────────

	/** Last rowid inserted via INSERT. */
	int64 GetLastInsertRowId() const;

	/** Number of rows modified by the last INSERT/UPDATE/DELETE. */
	int32 GetChangesCount() const;

	/** English-language error message from the last failed operation. */
	FString GetLastErrorMessage() const;

private:

	/** Private constructor — use Open() or OpenInMemory(). */
	FESQLDatabase();

	/** Internal: initialize PRAGMAs after open. */
	bool ApplyPragmas(FString& OutError);

	/** Internal: execute a prepared statement and collect results into FESQLQueryResult. */
	FESQLQueryResult ExecuteStatement(TSharedPtr<FESQLStatement> Statement);

	sqlite3* DbHandle = nullptr;
	FString DatabasePath;          // Empty for in-memory
	bool bInMemory = false;
	mutable FCriticalSection CriticalSection;

	// ── Statement Cache ──────────────────────────────────────────────────
	/** Caches compiled sqlite3_stmt* handles keyed by SQL string.
	    Avoids recompiling the same SQL on repeated InsertRow/SelectRows calls.
	    Cache is cleared on Close(). Thread-safe via CriticalSection.

	    IMPORTANT: GetOrPrepare() always calls sqlite3_reset() + sqlite3_clear_bindings()
	    on the cached statement before returning it. This ensures the caller receives
	    a clean, ready-to-bind statement — preventing stale-binding bugs. */
	TMap<FString, TSharedPtr<FESQLStatement>> StatementCache;

public:
	/** Get a cached statement or prepare a new one. Resets the statement before returning.
	    Prefer this over Prepare() for repeated queries (e.g. InsertRow in a loop). */
	TSharedPtr<FESQLStatement> GetOrPrepare(const FString& SQL, FString& OutError);
};

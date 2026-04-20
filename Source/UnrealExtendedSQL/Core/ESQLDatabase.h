// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"
#include "Core/ESQLStatement.h"
#include "HAL/CriticalSection.h"

// Forward declaration — avoid exposing sqlite3.h in header
struct sqlite3;

class FESQLDatabase;
using FESQLDatabaseOpenResult = TESQLValueOrError<TSharedPtr<FESQLDatabase>>;
using FESQLStatementPrepareResult = TESQLValueOrError<TSharedPtr<FESQLStatement>>;

/**
 * RAII wrapper around a sqlite3* connection handle.
 * NOT a UObject — used internally by UESQLSubsystem and can be used
 * from any C++ code without depending on the UObject ecosystem.
 *
	* Thread affinity: higher-level code should keep a single connection on one
	* owning thread. Debug builds assert if sqlite-handle operations hop threads.
	* Public methods that touch the sqlite handle serialize through CriticalSection.
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
	      PRAGMA synchronous=NORMAL;
	      PRAGMA foreign_keys=ON;     (SQLite FK enforcement is OFF by default)
	      sqlite3_busy_timeout(5000);
	    Creates the file and parent directories if they don't exist. */
	static FESQLDatabaseOpenResult Open(const FString& FilePath);

	/** Open a file-backed database in READ-ONLY mode.
	    Use this for editor tools that only need to query data (e.g. FESQLId picker)
	    without conflicting with an existing read-write connection. */
	static FESQLDatabaseOpenResult OpenReadOnly(const FString& FilePath);

	/** Open a purely in-memory database (Session mode).
	    The database lives only in RAM — no file is created on disk.
	    Use BackupToFile() to persist it (snapshot), and
	    RestoreFromFile() to load a snapshot into memory. */
	static FESQLDatabaseOpenResult OpenInMemory();

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

	/** Prepare (compile) a SQL statement. Returns structured error on failure.
	    For repeated queries, prefer GetOrPrepare() which uses the cache. */
	FESQLStatementPrepareResult Prepare(const FString& SQL);


	// ── Transaction Helpers ──────────────────────────────────────────────

	FESQLErrorResult BeginTransaction();
	FESQLErrorResult CommitTransaction();
	FESQLErrorResult RollbackTransaction();


	// ── Backup / Snapshot ────────────────────────────────────────────────

	/** Backup this database (memory or file) to a .db file on disk.
	    Uses sqlite3_backup API — safe to call while the DB is live.
	    This is the implementation behind UESQLSubsystem::SaveSnapshot(). */
	FESQLErrorResult BackupToFile(const FString& DestFilePath);

	/** Replace this database's contents with data from a .db file on disk.
	    Uses sqlite3_backup API — the current contents are fully overwritten.
	    This is the implementation behind UESQLSubsystem::LoadSnapshot(). */
	FESQLErrorResult RestoreFromFile(const FString& SourceFilePath);


	// ── Metadata ─────────────────────────────────────────────────────────

	/** Last rowid inserted via INSERT. */
	int64 GetLastInsertRowId() const;

	/** Number of rows modified by the last INSERT/UPDATE/DELETE. */
	int32 GetChangesCount() const;

	/** English-language error message from the last failed operation. */
	FString GetLastErrorMessage() const;

	/** Clear the debug owning-thread marker before an idle connection is returned
	    to a pool for later reuse on another worker thread. */
	void ResetThreadAffinity();

private:

	/** Private constructor — use Open() or OpenInMemory(). */
	FESQLDatabase();

	/** Internal: initialize PRAGMAs after open. */
	FESQLErrorResult ApplyPragmas(bool bReadOnly);

	/** Internal: lazily bind a connection to the first thread that uses it. */
	void CheckThreadAffinity(const TCHAR* Operation) const;

	/** Internal: execute a prepared statement and collect results into FESQLQueryResult. */
	FESQLQueryResult ExecuteStatement(TSharedPtr<FESQLStatement> Statement);

	/** Internal: implementation behind the public GetOrPrepare entrypoint.
	    Caller must already hold CriticalSection. */
	FESQLStatementPrepareResult GetOrPrepareLocked(const FString& SQL);

	sqlite3* DbHandle = nullptr;
	FString DatabasePath;          // Empty for in-memory
	bool bInMemory = false;
	mutable FCriticalSection CriticalSection;
	mutable TOptional<uint32> OwningThreadId;

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
	FESQLStatementPrepareResult GetOrPrepare(const FString& SQL);
};

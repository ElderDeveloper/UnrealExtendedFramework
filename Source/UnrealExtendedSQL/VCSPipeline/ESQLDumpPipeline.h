// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


// Forward declarations
class FESQLDatabase;
class UESQLTableAsset;

/**
 * Handles .sqldump text export/import for VCS-friendly SQL data storage.
 *
 * The .sqldump format is a plain-text SQL script containing:
 *   - CREATE TABLE IF NOT EXISTS statements
 *   - INSERT OR REPLACE statements for each row (sorted by PK for deterministic diffs)
 *
 * Workflow:
 *   .sqldump is the SOURCE OF TRUTH for version control.
 *   .db files are derived artifacts — rebuilt from .sqldump on editor open and during cook.
 *   .db, .db-wal, .db-shm are .gitignored.
 *
 * Bidirectional sync on editor open:
 *   1. .sqldump newer than .db → rebuild .db from .sqldump
 *   2. .db newer than .sqldump → re-export .sqldump from .db  
 *   3. Same timestamp or .db missing → rebuild .db from .sqldump
 *
 * Safe save order:
 *   1. Export .sqldump first (crash-safe)
 *   2. Then commit to .db
 */
class UNREALEXTENDEDSQL_API FESQLDumpPipeline
{
public:

	// ── Database-level dump ──────────────────────────────────────────────

	/** Export a database to a .sqldump text file.
	    Writes CREATE TABLE + sorted INSERT OR REPLACE for every table.
	    Skips internal SQLite tables (sqlite_*). */
	static bool ExportDatabaseToDump(
		TSharedPtr<FESQLDatabase> Database,
		const FString& DumpFilePath,
		FString& OutError
	);

	/** Import a .sqldump text file into a database.
	    Executes the SQL script line-by-line inside a transaction.
	    The database should be empty or the caller should handle conflicts
	    (the dump uses INSERT OR REPLACE). */
	static bool ImportDumpToDatabase(
		TSharedPtr<FESQLDatabase> Database,
		const FString& DumpFilePath,
		FString& OutError
	);


	// ── Bidirectional sync ───────────────────────────────────────────────

	/** Sync a .db file and its .sqldump counterpart.
	    Call this when opening an asset in the editor.
	    DumpPath = the .sqldump path alongside the .db file.
	    Returns true if the database is ready. */
	static bool SyncDatabaseAndDump(
		const FString& DatabasePath,
		const FString& DumpPath,
		FString& OutError
	);


	// ── Path helpers ─────────────────────────────────────────────────────

	/** Given a .db file path, returns the corresponding .sqldump path.
	    Example: "Saved/Databases/EditorData.db" → "Saved/Databases/EditorData.sqldump" */
	static FString GetDumpPathForDatabase(const FString& DatabasePath);

	/** Given a .sqldump path, returns the corresponding .db path. */
	static FString GetDatabasePathForDump(const FString& DumpPath);


	// ── Cook pipeline ────────────────────────────────────────────────────

	/** Rebuild a .db file from a .sqldump file.
	    Creates a fresh .db (deletes existing if present) and imports the dump.
	    Used during cook to guarantee clean database state. */
	static bool RebuildDatabaseFromDump(
		const FString& DumpPath,
		const FString& OutputDbPath,
		FString& OutError
	);

	/** Generate recommended .gitignore entries. */
	static FString GetGitIgnoreRecommendation();

private:

	/** Parse a .sqldump file into individual SQL statements.
	    Handles multi-line statements (statements terminated by semicolons). */
	static TArray<FString> ParseDumpStatements(const FString& DumpContent);
};

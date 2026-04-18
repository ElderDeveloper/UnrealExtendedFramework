// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Shared/ESQLTypes.h"
#include "Core/ESQLDatabase.h"
#include "ESQLSubsystem.generated.h"


/**
 * GameInstanceSubsystem that manages database lifecycle and shared SQL services.
 *
 * This layer owns connection management, net-mode-aware authority checks,
 * snapshots, player database routing, and raw SQL access.
 *
 * Typed schema-backed workflows should live on UESQLTableAsset instead of here.
 */
UCLASS()
class UNREALEXTENDEDSQL_API UESQLSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;


	// ── Database Lifecycle ───────────────────────────────────────────────

	/** Open (or create) a database.
	    DatabaseName is a logical handle used in all subsequent calls.
	    Scope controls path resolution and authority checks.
	    Persistence controls whether the DB is on disk or in memory. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	FESQLQueryResult OpenDatabase(
		const FString& DatabaseName,
		EESQLDatabaseScope Scope = EESQLDatabaseScope::Local,
		EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent,
		const FString& FileName = TEXT("")
	);

	/** Open a per-player database. Server-only.
	    Uses the player's UniqueNetId as the subfolder key. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database", meta = (DisplayName = "Open Player Database"))
	FESQLQueryResult OpenPlayerDatabase(
		const FString& DatabaseName,
		APlayerController* PlayerController,
		const FString& FileName = TEXT("")
	);

	/** Close a previously opened database. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void CloseDatabase(const FString& DatabaseName);

	/** Close a player-scoped database.
	    If bDeleteFile is true, also removes the .db file from disk. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void ClosePlayerDatabase(
		const FString& DatabaseName,
		APlayerController* PlayerController,
		bool bDeleteFile = false
	);

	/** Close all open databases. Called automatically on Deinitialize. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void CloseAllDatabases();

	/** Close all player-scoped databases. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	void CloseAllPlayerDatabases(bool bDeleteFiles = false);

	/** Check if a database is currently open. */
	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	bool IsDatabaseOpen(const FString& DatabaseName) const;

	/** Check if a player-scoped database is currently open for the given controller. */
	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	bool IsPlayerDatabaseOpen(const FString& DatabaseName, APlayerController* PlayerController) const;

	/** Returns the names of all currently open databases. */
	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	TArray<FString> GetOpenDatabaseNames() const;

	/** Delete a database and its associated files (.db, -wal, -shm).
	    The database must be closed first. Returns success if the file was deleted. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Database")
	FESQLQueryResult DeleteDatabase(
		const FString& DatabaseName,
		EESQLDatabaseScope Scope = EESQLDatabaseScope::Local,
		const FString& FileName = TEXT("")
	);

	/** Returns the absolute file path for a database.
	    Returns empty string for in-memory (Session) databases. */
	UFUNCTION(BlueprintPure, Category = "SQL|Database")
	FString GetDatabaseFilePath(const FString& DatabaseName) const;


	// ── Net-Mode Queries ─────────────────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool IsDedicatedServer() const;

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool IsListenServer() const;

	UFUNCTION(BlueprintPure, Category = "SQL|Network")
	bool HasServerAuthority() const;


	// ── Synchronous Queries ──────────────────────────────────────────────

	/** Execute raw SQL synchronously. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true"))
	FESQLQueryResult ExecuteSQL(const FString& DatabaseName, const FString& SQL);

	/** Execute SQL with text bindings (?1, ?2, …). */
	UFUNCTION(BlueprintCallable, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true"))
	FESQLQueryResult ExecuteSQLWithBindings(
		const FString& DatabaseName,
		const FString& SQL,
		const TArray<FString>& Bindings
	);

	/** Execute raw SQL against a player's database. Server-only. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Query", meta = (BlueprintInternalUseOnly = "true"))
	FESQLQueryResult ExecutePlayerSQL(
		const FString& DatabaseName,
		APlayerController* PlayerController,
		const FString& SQL
	);


	// ── Async Queries ────────────────────────────────────────────────────

	/** Execute SQL asynchronously; fires OnComplete on the game thread. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Async", meta = (BlueprintInternalUseOnly = "true"))
	void AsyncExecuteSQL(
		const FString& DatabaseName,
		const FString& SQL,
		const FOnESQLQueryCompleteCallback& OnComplete
	);


	// ── Semantic Helpers (no raw SQL) ────────────────────────────────────

	/** Create a table. Columns = Name:Type pairs (TEXT, INTEGER, REAL, BLOB). */
	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult CreateTable(
		const FString& DatabaseName,
		const FString& TableName,
		const TMap<FString, FString>& Columns,
		const FString& PrimaryKeyColumn = TEXT(""),
		bool bIfNotExists = true
	);

	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult DropTable(const FString& DatabaseName, const FString& TableName);

	UFUNCTION(BlueprintPure, Category = "SQL|Table")
	bool DoesTableExist(const FString& DatabaseName, const FString& TableName);

	/** Returns the names of all user tables in the database. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	TArray<FString> GetTableNames(const FString& DatabaseName);

	/** Returns column definitions for a table (name + type). */
	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	TArray<FESQLColumn> GetTableColumns(const FString& DatabaseName, const FString& TableName);

	/** Add a column to an existing table.
	    ColumnType: TEXT, INTEGER, REAL, BLOB.
	    DefaultValue: optional default (e.g. "0" or "'unknown'"). */
	UFUNCTION(BlueprintCallable, Category = "SQL|Table")
	FESQLQueryResult AlterTableAddColumn(
		const FString& DatabaseName,
		const FString& TableName,
		const FString& ColumnName,
		const FString& ColumnType = TEXT("TEXT"),
		const FString& DefaultValue = TEXT("")
	);

	/** Insert a single row. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row")
	FESQLQueryResult InsertRow(
		const FString& DatabaseName,
		const FString& TableName,
		const TMap<FString, FString>& ColumnValues
	);

	/** Insert multiple rows in a transaction. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row")
	FESQLQueryResult InsertRows(
		const FString& DatabaseName,
		const FString& TableName,
		const TArray<FESQLRow>& Rows
	);

	/** Insert or update based on conflict column (UPSERT). */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row")
	FESQLQueryResult UpsertRow(
		const FString& DatabaseName,
		const FString& TableName,
		const TMap<FString, FString>& ColumnValues,
		const FString& ConflictColumn
	);

	/** Select rows matching optional WHERE clause. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	FESQLQueryResult SelectRows(
		const FString& DatabaseName,
		const FString& TableName,
		const FString& WhereClause,
		const TArray<FString>& Bindings,
		int32 Limit = -1
	);

	/** Update rows matching WHERE clause. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row", meta = (AutoCreateRefTerm = "Bindings"))
	FESQLQueryResult UpdateRows(
		const FString& DatabaseName,
		const FString& TableName,
		const TMap<FString, FString>& SetValues,
		const FString& WhereClause,
		const TArray<FString>& Bindings
	);

	/** Delete rows matching WHERE clause. */
	UFUNCTION(BlueprintCallable, Category = "SQL|Row", meta = (AutoCreateRefTerm = "Bindings"))
	FESQLQueryResult DeleteRows(
		const FString& DatabaseName,
		const FString& TableName,
		const FString& WhereClause,
		const TArray<FString>& Bindings
	);

	/** Count rows (optionally filtered). */
	UFUNCTION(BlueprintPure, Category = "SQL|Row", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Bindings"))
	int32 CountRows(
		const FString& DatabaseName,
		const FString& TableName,
		const FString& WhereClause,
		const TArray<FString>& Bindings
	);


	// ── Transactions ─────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	bool BeginTransaction(const FString& DatabaseName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	bool CommitTransaction(const FString& DatabaseName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
	bool RollbackTransaction(const FString& DatabaseName);


	// ── Snapshots (for Session databases) ────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult SaveSnapshot(
		const FString& DatabaseName,
		const FString& SlotName,
		const FString& DisplayName = TEXT("")
	);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult LoadSnapshot(const FString& DatabaseName, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	FESQLQueryResult DeleteSnapshot(const FString& DatabaseName, const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
	TArray<FESQLSnapshotInfo> GetAllSnapshots(const FString& DatabaseName);

	UFUNCTION(BlueprintPure, Category = "SQL|Snapshot")
	bool DoesSnapshotExist(const FString& DatabaseName, const FString& SlotName);

	UFUNCTION(BlueprintPure, Category = "SQL|Snapshot")
	bool IsSessionDatabase(const FString& DatabaseName) const;


	// ── Delegates ────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "SQL")
	FOnESQLQueryComplete OnQueryComplete;

	UPROPERTY(BlueprintAssignable, Category = "SQL")
	FOnESQLQueryComplete OnQueryError;

	UPROPERTY(BlueprintAssignable, Category = "SQL|Multiplayer")
	FOnESQLPlayerDBEvent OnPlayerDatabaseOpened;

	UPROPERTY(BlueprintAssignable, Category = "SQL|Multiplayer")
	FOnESQLPlayerDBEvent OnPlayerDatabaseClosed;

private:

	/** Logical name → live database handle (Local + Server scoped) */
	TMap<FString, TSharedPtr<FESQLDatabase>> OpenDatabases;

	/** PlayerId → (DatabaseName → handle) for PlayerScoped databases */
	TMap<FString, TMap<FString, TSharedPtr<FESQLDatabase>>> PlayerDatabases;

	/** Scope metadata per logical database name */
	TMap<FString, EESQLDatabaseScope> DatabaseScopes;

	/** Persistence metadata per logical database name */
	TMap<FString, EESQLDatabasePersistence> DatabasePersistenceMap;

	/** Cached net mode */
	TEnumAsByte<ENetMode> CachedNetMode = NM_Standalone;

	// ── Internal Helpers ─────────────────────────────────────────────────

	FString ResolveDatabasePath(const FString& FileName, EESQLDatabaseScope Scope, const FString& PlayerId = TEXT("")) const;
	FString ResolveSnapshotPath(const FString& DatabaseName, const FString& SlotName) const;
	FString ResolveSnapshotMetaPath(const FString& DatabaseName, const FString& SlotName) const;

	TSharedPtr<FESQLDatabase> GetDatabase(const FString& DatabaseName) const;
	TSharedPtr<FESQLDatabase> GetPlayerDatabase(const FString& PlayerId, const FString& DatabaseName) const;
	FString GetPlayerIdFromController(APlayerController* PC) const;
	bool ValidateAuthority(EESQLDatabaseScope Scope, const FString& CallerName) const;

	// ── Auto Player DB Hooks ─────────────────────────────────────────────
	void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnLogout(AGameModeBase* GameMode, AController* Exiting);
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
};

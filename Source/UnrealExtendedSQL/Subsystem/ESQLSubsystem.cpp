// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLSubsystem.h"
#include "Shared/ESQLSettings.h"
#include "UnrealExtendedSQL.h"
#include "Async/Async.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


// ── Lifecycle ────────────────────────────────────────────────────────────────

void UESQLSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Cache net mode
	const UGameInstance* GI = GetGameInstance();
	if (GI && GI->GetWorld())
	{
		CachedNetMode = GI->GetWorld()->GetNetMode();
	}

	// Hook into GameMode PostLogin/Logout for auto player DB management
	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bAutoOpenPlayerDatabases && HasServerAuthority())
	{
		PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UESQLSubsystem::OnPostLogin);
		LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UESQLSubsystem::OnLogout);
	}

	UE_LOG(LogExtendedSQL, Log, TEXT("UESQLSubsystem initialized (NetMode: %d)"), (int32)CachedNetMode);
}

void UESQLSubsystem::Deinitialize()
{
	// Unhook GameMode delegates
	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
		PostLoginHandle.Reset();
	}
	if (LogoutHandle.IsValid())
	{
		FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
		LogoutHandle.Reset();
	}

	// Close all databases
	CloseAllPlayerDatabases(false);
	CloseAllDatabases();

	UE_LOG(LogExtendedSQL, Log, TEXT("UESQLSubsystem deinitialized"));
	Super::Deinitialize();
}


// ── Database Lifecycle ───────────────────────────────────────────────────────

FESQLQueryResult UESQLSubsystem::OpenDatabase(
	const FString& DatabaseName,
	EESQLDatabaseScope Scope,
	EESQLDatabasePersistence Persistence,
	const FString& FileName)
{
	if (DatabaseName.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("DatabaseName cannot be empty"));
	}

	// Authority check
	if (!ValidateAuthority(Scope, TEXT("OpenDatabase")))
	{
		return FESQLQueryResult::Failure(TEXT("Authority check failed — cannot open Server-scoped DB from a client"));
	}

	// Check if already open
	if (OpenDatabases.Contains(DatabaseName))
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is already open"), *DatabaseName));
	}

	// Check max open limit
	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->MaxOpenDatabases > 0 && OpenDatabases.Num() >= Settings->MaxOpenDatabases)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Max open databases limit reached (%d)"), Settings->MaxOpenDatabases));
	}

	TSharedPtr<FESQLDatabase> Database;
	FString Error;

	if (Persistence == EESQLDatabasePersistence::Session)
	{
		// In-memory database
		Database = FESQLDatabase::OpenInMemory(Error);
	}
	else
	{
		// File-backed database
		const FString ActualFileName = FileName.IsEmpty() ? (DatabaseName + TEXT(".db")) : FileName;
		const FString FullPath = ResolveDatabasePath(ActualFileName, Scope);
		Database = FESQLDatabase::Open(FullPath, Error);
	}

	if (!Database)
	{
		return FESQLQueryResult::Failure(Error);
	}

	// Store the connection and metadata
	OpenDatabases.Add(DatabaseName, Database);
	DatabaseScopes.Add(DatabaseName, Scope);
	DatabasePersistenceMap.Add(DatabaseName, Persistence);

	if (Settings && Settings->bEnableVerboseLogging)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Opened database '%s' (Scope: %d, Persistence: %d)"),
			*DatabaseName, (int32)Scope, (int32)Persistence);
	}

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::OpenPlayerDatabase(
	const FString& DatabaseName,
	APlayerController* PlayerController,
	const FString& FileName)
{
	if (!HasServerAuthority())
	{
		return FESQLQueryResult::Failure(TEXT("OpenPlayerDatabase is server-only"));
	}

	if (!PlayerController)
	{
		return FESQLQueryResult::Failure(TEXT("PlayerController is null"));
	}

	const FString PlayerId = GetPlayerIdFromController(PlayerController);
	if (PlayerId.IsEmpty())
	{
		return FESQLQueryResult::Failure(TEXT("Failed to resolve player identity"));
	}

	// Check if already open for this player
	if (const auto* PlayerMap = PlayerDatabases.Find(PlayerId))
	{
		if (PlayerMap->Contains(DatabaseName))
		{
			return FESQLQueryResult::Failure(FString::Printf(TEXT("Player database '%s' already open for player '%s'"),
				*DatabaseName, *PlayerId));
		}
	}

	const FString ActualFileName = FileName.IsEmpty() ? (DatabaseName + TEXT(".db")) : FileName;
	const FString FullPath = ResolveDatabasePath(ActualFileName, EESQLDatabaseScope::PlayerScoped, PlayerId);

	FString Error;
	TSharedPtr<FESQLDatabase> Database = FESQLDatabase::Open(FullPath, Error);
	if (!Database)
	{
		return FESQLQueryResult::Failure(Error);
	}

	PlayerDatabases.FindOrAdd(PlayerId).Add(DatabaseName, Database);

	OnPlayerDatabaseOpened.Broadcast(PlayerController, PlayerId);

	UE_LOG(LogExtendedSQL, Log, TEXT("Opened player database '%s' for player '%s'"), *DatabaseName, *PlayerId);
	return FESQLQueryResult::Success();
}

void UESQLSubsystem::CloseDatabase(const FString& DatabaseName)
{
	if (TSharedPtr<FESQLDatabase>* Found = OpenDatabases.Find(DatabaseName))
	{
		(*Found)->Close();
		OpenDatabases.Remove(DatabaseName);
		DatabaseScopes.Remove(DatabaseName);
		DatabasePersistenceMap.Remove(DatabaseName);

		UE_LOG(LogExtendedSQL, Log, TEXT("Closed database '%s'"), *DatabaseName);
	}
}

void UESQLSubsystem::ClosePlayerDatabase(
	const FString& DatabaseName,
	APlayerController* PlayerController,
	bool bDeleteFile)
{
	if (!PlayerController) return;

	const FString PlayerId = GetPlayerIdFromController(PlayerController);
	if (PlayerId.IsEmpty()) return;

	if (auto* PlayerMap = PlayerDatabases.Find(PlayerId))
	{
		if (TSharedPtr<FESQLDatabase>* Found = PlayerMap->Find(DatabaseName))
		{
			const FString DbPath = (*Found)->GetDatabasePath();
			(*Found)->Close();
			PlayerMap->Remove(DatabaseName);

			if (bDeleteFile && !DbPath.IsEmpty())
			{
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				PlatformFile.DeleteFile(*DbPath);
				// Also clean up WAL/SHM files
				PlatformFile.DeleteFile(*(DbPath + TEXT("-wal")));
				PlatformFile.DeleteFile(*(DbPath + TEXT("-shm")));
			}

			OnPlayerDatabaseClosed.Broadcast(PlayerController, PlayerId);

			// Remove player entry if no more databases
			if (PlayerMap->Num() == 0)
			{
				PlayerDatabases.Remove(PlayerId);
			}

			UE_LOG(LogExtendedSQL, Log, TEXT("Closed player database '%s' for '%s'%s"),
				*DatabaseName, *PlayerId, bDeleteFile ? TEXT(" (file deleted)") : TEXT(""));
		}
	}
}

void UESQLSubsystem::CloseAllDatabases()
{
	for (auto& Pair : OpenDatabases)
	{
		if (Pair.Value)
		{
			Pair.Value->Close();
		}
	}
	OpenDatabases.Empty();
	DatabaseScopes.Empty();
	DatabasePersistenceMap.Empty();
}

void UESQLSubsystem::CloseAllPlayerDatabases(bool bDeleteFiles)
{
	for (auto& PlayerPair : PlayerDatabases)
	{
		for (auto& DbPair : PlayerPair.Value)
		{
			if (DbPair.Value)
			{
				if (bDeleteFiles)
				{
					const FString DbPath = DbPair.Value->GetDatabasePath();
					DbPair.Value->Close();
					if (!DbPath.IsEmpty())
					{
						IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
						PlatformFile.DeleteFile(*DbPath);
						PlatformFile.DeleteFile(*(DbPath + TEXT("-wal")));
						PlatformFile.DeleteFile(*(DbPath + TEXT("-shm")));
					}
				}
				else
				{
					DbPair.Value->Close();
				}
			}
		}
	}
	PlayerDatabases.Empty();
}

bool UESQLSubsystem::IsDatabaseOpen(const FString& DatabaseName) const
{
	return OpenDatabases.Contains(DatabaseName);
}

bool UESQLSubsystem::IsPlayerDatabaseOpen(const FString& DatabaseName, APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return false;
	}

	const FString PlayerId = GetPlayerIdFromController(PlayerController);
	if (PlayerId.IsEmpty())
	{
		return false;
	}

	const TSharedPtr<FESQLDatabase> Database = GetPlayerDatabase(PlayerId, DatabaseName);
	return Database.IsValid() && Database->IsOpen();
}

TArray<FString> UESQLSubsystem::GetOpenDatabaseNames() const
{
	TArray<FString> Names;
	OpenDatabases.GetKeys(Names);
	return Names;
}


// ── Net-Mode Queries ─────────────────────────────────────────────────────────

bool UESQLSubsystem::IsDedicatedServer() const
{
	return CachedNetMode == NM_DedicatedServer;
}

bool UESQLSubsystem::IsListenServer() const
{
	return CachedNetMode == NM_ListenServer;
}

bool UESQLSubsystem::HasServerAuthority() const
{
	return CachedNetMode == NM_DedicatedServer
		|| CachedNetMode == NM_ListenServer
		|| CachedNetMode == NM_Standalone;
}


// ── Synchronous Queries ──────────────────────────────────────────────────────

FESQLQueryResult UESQLSubsystem::ExecuteSQL(const FString& DatabaseName, const FString& SQL)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	FESQLQueryResult Result = Database->Execute(SQL);

	// Fire delegates
	if (Result.bSuccess)
	{
		OnQueryComplete.Broadcast(Result);
	}
	else
	{
		OnQueryError.Broadcast(Result);
		UE_LOG(LogExtendedSQL, Warning, TEXT("ExecuteSQL error on '%s': %s"), *DatabaseName, *Result.ErrorMessage);
	}

	return Result;
}

FESQLQueryResult UESQLSubsystem::ExecuteSQLWithBindings(
	const FString& DatabaseName,
	const FString& SQL,
	const TArray<FString>& Bindings)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	FESQLQueryResult Result = Database->Execute(SQL, Bindings);

	if (Result.bSuccess)
	{
		OnQueryComplete.Broadcast(Result);
	}
	else
	{
		OnQueryError.Broadcast(Result);
		UE_LOG(LogExtendedSQL, Warning, TEXT("ExecuteSQLWithBindings error on '%s': %s"), *DatabaseName, *Result.ErrorMessage);
	}

	return Result;
}

FESQLQueryResult UESQLSubsystem::ExecutePlayerSQL(
	const FString& DatabaseName,
	APlayerController* PlayerController,
	const FString& SQL)
{
	if (!HasServerAuthority())
	{
		return FESQLQueryResult::Failure(TEXT("ExecutePlayerSQL is server-only"));
	}

	if (!PlayerController)
	{
		return FESQLQueryResult::Failure(TEXT("PlayerController is null"));
	}

	const FString PlayerId = GetPlayerIdFromController(PlayerController);
	TSharedPtr<FESQLDatabase> Database = GetPlayerDatabase(PlayerId, DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Player database '%s' not open for player '%s'"),
			*DatabaseName, *PlayerId));
	}

	return Database->Execute(SQL);
}


// ── Async Queries ────────────────────────────────────────────────────────────

void UESQLSubsystem::AsyncExecuteSQL(
	const FString& DatabaseName,
	const FString& SQL,
	const FOnESQLQueryCompleteCallback& OnComplete)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		FESQLQueryResult ErrorResult = FESQLQueryResult::Failure(
			FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
		OnComplete.ExecuteIfBound(ErrorResult);
		return;
	}

	// Capture by value for the background thread
	TWeakObjectPtr<UESQLSubsystem> WeakThis(this);
	FOnESQLQueryCompleteCallback Callback = OnComplete;
	FString SQLCopy = SQL;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Database, SQLCopy, Callback, WeakThis]()
	{
		FESQLQueryResult Result = Database->Execute(SQLCopy);

		// Return to game thread
		AsyncTask(ENamedThreads::GameThread, [Result, Callback, WeakThis]()
		{
			Callback.ExecuteIfBound(Result);

			if (WeakThis.IsValid())
			{
				if (Result.bSuccess)
				{
					WeakThis->OnQueryComplete.Broadcast(Result);
				}
				else
				{
					WeakThis->OnQueryError.Broadcast(Result);
				}
			}
		});
	});
}


// ── Semantic Helpers ─────────────────────────────────────────────────────────

FESQLQueryResult UESQLSubsystem::CreateTable(
	const FString& DatabaseName,
	const FString& TableName,
	const TMap<FString, FString>& Columns,
	const FString& PrimaryKeyColumn,
	bool bIfNotExists)
{
	if (Columns.Num() == 0)
	{
		return FESQLQueryResult::Failure(TEXT("Cannot create table with no columns"));
	}

	FString SQL = FString::Printf(TEXT("CREATE TABLE %s\"%s\" ("),
		bIfNotExists ? TEXT("IF NOT EXISTS ") : TEXT(""),
		*TableName);

	bool bFirst = true;
	for (const auto& Pair : Columns)
	{
		if (!bFirst) SQL += TEXT(", ");
		SQL += FString::Printf(TEXT("\"%s\" %s"), *Pair.Key, *Pair.Value);

		if (!PrimaryKeyColumn.IsEmpty() && Pair.Key == PrimaryKeyColumn)
		{
			SQL += TEXT(" PRIMARY KEY");
		}
		bFirst = false;
	}

	SQL += TEXT(")");

	return ExecuteSQL(DatabaseName, SQL);
}

FESQLQueryResult UESQLSubsystem::DropTable(const FString& DatabaseName, const FString& TableName)
{
	return ExecuteSQL(DatabaseName, FString::Printf(TEXT("DROP TABLE IF EXISTS \"%s\""), *TableName));
}

bool UESQLSubsystem::DoesTableExist(const FString& DatabaseName, const FString& TableName)
{
	FESQLQueryResult Result = ExecuteSQLWithBindings(
		DatabaseName,
		TEXT("SELECT name FROM sqlite_master WHERE type='table' AND name=?1"),
		{TableName}
	);
	return Result.bSuccess && Result.Rows.Num() > 0;
}

TArray<FString> UESQLSubsystem::GetTableNames(const FString& DatabaseName)
{
	TArray<FString> Names;

	FESQLQueryResult Result = ExecuteSQL(DatabaseName,
		TEXT("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"));

	if (Result.bSuccess)
	{
		for (const FESQLRow& Row : Result.Rows)
		{
			FString Name;
			if (Row.TryGetString(TEXT("name"), Name))
			{
				Names.Add(Name);
			}
		}
	}

	return Names;
}

TArray<FESQLColumn> UESQLSubsystem::GetTableColumns(const FString& DatabaseName, const FString& TableName)
{
	TArray<FESQLColumn> Columns;

	FESQLQueryResult Result = ExecuteSQL(DatabaseName,
		FString::Printf(TEXT("PRAGMA table_info(\"%s\")"), *TableName));

	if (Result.bSuccess)
	{
		for (const FESQLRow& Row : Result.Rows)
		{
			FESQLColumn Col;
			Row.TryGetString(TEXT("name"), Col.Name);

			FString TypeStr;
			Row.TryGetString(TEXT("type"), TypeStr);
			TypeStr = TypeStr.ToUpper();
			if (TypeStr.Contains(TEXT("INT")))
			{
				Col.Type = EESQLColumnType::Integer;
			}
			else if (TypeStr.Contains(TEXT("REAL")) || TypeStr.Contains(TEXT("FLOAT")) || TypeStr.Contains(TEXT("DOUBLE")))
			{
				Col.Type = EESQLColumnType::Float;
			}
			else if (TypeStr.Contains(TEXT("BLOB")))
			{
				Col.Type = EESQLColumnType::Blob;
			}
			else
			{
				Col.Type = EESQLColumnType::Text;
			}

			Columns.Add(Col);
		}
	}

	return Columns;
}

FESQLQueryResult UESQLSubsystem::AlterTableAddColumn(
	const FString& DatabaseName,
	const FString& TableName,
	const FString& ColumnName,
	const FString& ColumnType,
	const FString& DefaultValue)
{
	FString SQL = FString::Printf(TEXT("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s"),
		*TableName, *ColumnName, *ColumnType);

	if (!DefaultValue.IsEmpty())
	{
		SQL += FString::Printf(TEXT(" DEFAULT %s"), *DefaultValue);
	}

	return ExecuteSQL(DatabaseName, SQL);
}

FESQLQueryResult UESQLSubsystem::DeleteDatabase(
	const FString& DatabaseName,
	EESQLDatabaseScope Scope,
	const FString& FileName)
{
	// Refuse to delete if still open
	if (OpenDatabases.Contains(DatabaseName))
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("Cannot delete database '%s' — it is still open. Call CloseDatabase first."), *DatabaseName));
	}

	const FString ActualFileName = FileName.IsEmpty() ? (DatabaseName + TEXT(".db")) : FileName;
	const FString FullPath = ResolveDatabasePath(ActualFileName, Scope);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	bool bDeleted = false;

	if (PlatformFile.FileExists(*FullPath))
	{
		bDeleted = PlatformFile.DeleteFile(*FullPath);
	}

	// Also clean up WAL/SHM and .sqldump
	PlatformFile.DeleteFile(*(FullPath + TEXT("-wal")));
	PlatformFile.DeleteFile(*(FullPath + TEXT("-shm")));

	// Remove the .sqldump if it exists
	const FString DumpPath = FPaths::ChangeExtension(FullPath, TEXT(".sqldump"));
	PlatformFile.DeleteFile(*DumpPath);

	if (bDeleted)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Deleted database files for '%s' at %s"), *DatabaseName, *FullPath);
		return FESQLQueryResult::Success();
	}
	else
	{
		return FESQLQueryResult::Failure(FString::Printf(
			TEXT("Database file not found at: %s"), *FullPath));
	}
}

FString UESQLSubsystem::GetDatabaseFilePath(const FString& DatabaseName) const
{
	if (const TSharedPtr<FESQLDatabase>* Found = OpenDatabases.Find(DatabaseName))
	{
		if (Found->IsValid())
		{
			return (*Found)->GetDatabasePath();
		}
	}
	return FString();
}

FESQLQueryResult UESQLSubsystem::InsertRow(
	const FString& DatabaseName,
	const FString& TableName,
	const TMap<FString, FString>& ColumnValues)
{
	if (ColumnValues.Num() == 0)
	{
		return FESQLQueryResult::Failure(TEXT("No column values provided for InsertRow"));
	}

	FString ColumnList;
	FString PlaceholderList;
	TArray<FString> Bindings;
	int32 Index = 1;

	for (const auto& Pair : ColumnValues)
	{
		if (Index > 1) { ColumnList += TEXT(", "); PlaceholderList += TEXT(", "); }
		ColumnList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
		PlaceholderList += FString::Printf(TEXT("?%d"), Index);
		Bindings.Add(Pair.Value);
		++Index;
	}

	const FString SQL = FString::Printf(TEXT("INSERT INTO \"%s\" (%s) VALUES (%s)"),
		*TableName, *ColumnList, *PlaceholderList);

	return ExecuteSQLWithBindings(DatabaseName, SQL, Bindings);
}

FESQLQueryResult UESQLSubsystem::InsertRows(
	const FString& DatabaseName,
	const FString& TableName,
	const TArray<FESQLRow>& Rows)
{
	if (Rows.Num() == 0)
	{
		return FESQLQueryResult::Success();
	}

	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	Database->BeginTransaction();

	int32 TotalAffected = 0;
	for (const FESQLRow& Row : Rows)
	{
		FESQLQueryResult Result = InsertRow(DatabaseName, TableName, Row.Columns);
		if (!Result.bSuccess)
		{
			Database->RollbackTransaction();
			return Result;
		}
		TotalAffected += Result.RowsAffected;
	}

	Database->CommitTransaction();

	FESQLQueryResult Result = FESQLQueryResult::Success();
	Result.RowsAffected = TotalAffected;
	return Result;
}

FESQLQueryResult UESQLSubsystem::UpsertRow(
	const FString& DatabaseName,
	const FString& TableName,
	const TMap<FString, FString>& ColumnValues,
	const FString& ConflictColumn)
{
	if (ColumnValues.Num() == 0)
	{
		return FESQLQueryResult::Failure(TEXT("No column values provided for UpsertRow"));
	}

	FString ColumnList;
	FString PlaceholderList;
	FString UpdateList;
	TArray<FString> Bindings;
	int32 Index = 1;

	for (const auto& Pair : ColumnValues)
	{
		if (Index > 1) { ColumnList += TEXT(", "); PlaceholderList += TEXT(", "); }
		ColumnList += FString::Printf(TEXT("\"%s\""), *Pair.Key);
		PlaceholderList += FString::Printf(TEXT("?%d"), Index);

		// Build UPDATE SET (skip the conflict column)
		if (Pair.Key != ConflictColumn)
		{
			if (!UpdateList.IsEmpty()) UpdateList += TEXT(", ");
			UpdateList += FString::Printf(TEXT("\"%s\"=excluded.\"%s\""), *Pair.Key, *Pair.Key);
		}

		Bindings.Add(Pair.Value);
		++Index;
	}

	const FString SQL = FString::Printf(
		TEXT("INSERT INTO \"%s\" (%s) VALUES (%s) ON CONFLICT(\"%s\") DO UPDATE SET %s"),
		*TableName, *ColumnList, *PlaceholderList, *ConflictColumn, *UpdateList);

	return ExecuteSQLWithBindings(DatabaseName, SQL, Bindings);
}

FESQLQueryResult UESQLSubsystem::SelectRows(
	const FString& DatabaseName,
	const FString& TableName,
	const FString& WhereClause,
	const TArray<FString>& Bindings,
	int32 Limit)
{
	FString SQL = FString::Printf(TEXT("SELECT * FROM \"%s\""), *TableName);

	if (!WhereClause.IsEmpty())
	{
		SQL += FString::Printf(TEXT(" WHERE %s"), *WhereClause);
	}

	// Resolve limit
	int32 EffectiveLimit = Limit;
	if (EffectiveLimit < 0)
	{
		const UESQLSettings* Settings = UESQLSettings::Get();
		EffectiveLimit = Settings ? Settings->DefaultMaxResultLimit : 1000;
	}

	if (EffectiveLimit > 0)
	{
		SQL += FString::Printf(TEXT(" LIMIT %d"), EffectiveLimit);
	}

	if (Bindings.Num() > 0)
	{
		return ExecuteSQLWithBindings(DatabaseName, SQL, Bindings);
	}

	return ExecuteSQL(DatabaseName, SQL);
}

FESQLQueryResult UESQLSubsystem::UpdateRows(
	const FString& DatabaseName,
	const FString& TableName,
	const TMap<FString, FString>& SetValues,
	const FString& WhereClause,
	const TArray<FString>& Bindings)
{
	if (SetValues.Num() == 0)
	{
		return FESQLQueryResult::Failure(TEXT("No SET values provided for UpdateRows"));
	}

	FString SetClause;
	TArray<FString> AllBindings;
	int32 Index = 1;

	for (const auto& Pair : SetValues)
	{
		if (Index > 1) SetClause += TEXT(", ");
		SetClause += FString::Printf(TEXT("\"%s\"=?%d"), *Pair.Key, Index);
		AllBindings.Add(Pair.Value);
		++Index;
	}

	// Append WHERE bindings (re-numbered)
	FString AdjustedWhere = WhereClause;
	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		// Replace ?1, ?2, etc. in WhereClause with ?N, ?N+1, etc.
		const FString OldPlaceholder = FString::Printf(TEXT("?%d"), i + 1);
		const FString NewPlaceholder = FString::Printf(TEXT("?%d"), Index);
		AdjustedWhere = AdjustedWhere.Replace(*OldPlaceholder, *NewPlaceholder);
		AllBindings.Add(Bindings[i]);
		++Index;
	}

	const FString SQL = FString::Printf(TEXT("UPDATE \"%s\" SET %s WHERE %s"),
		*TableName, *SetClause, *AdjustedWhere);

	return ExecuteSQLWithBindings(DatabaseName, SQL, AllBindings);
}

FESQLQueryResult UESQLSubsystem::DeleteRows(
	const FString& DatabaseName,
	const FString& TableName,
	const FString& WhereClause,
	const TArray<FString>& Bindings)
{
	const FString SQL = FString::Printf(TEXT("DELETE FROM \"%s\" WHERE %s"), *TableName, *WhereClause);

	if (Bindings.Num() > 0)
	{
		return ExecuteSQLWithBindings(DatabaseName, SQL, Bindings);
	}

	return ExecuteSQL(DatabaseName, SQL);
}

int32 UESQLSubsystem::CountRows(
	const FString& DatabaseName,
	const FString& TableName,
	const FString& WhereClause,
	const TArray<FString>& Bindings)
{
	FString SQL = FString::Printf(TEXT("SELECT COUNT(*) FROM \"%s\""), *TableName);
	if (!WhereClause.IsEmpty())
	{
		SQL += FString::Printf(TEXT(" WHERE %s"), *WhereClause);
	}
	SQL = SQL.Replace(TEXT("SELECT COUNT(*)"), TEXT("SELECT COUNT(*) AS RowCount"));

	FESQLQueryResult Result = Bindings.Num() > 0
		? ExecuteSQLWithBindings(DatabaseName, SQL, Bindings)
		: ExecuteSQL(DatabaseName, SQL);

	if (const FESQLRow* Row = Result.GetFirstRow())
	{
		int64 Count = 0;
		if (Row->TryGetInt64(TEXT("RowCount"), Count))
		{
			return static_cast<int32>(Count);
		}
	}

	return 0;
}


// ── Transactions ─────────────────────────────────────────────────────────────

bool UESQLSubsystem::BeginTransaction(const FString& DatabaseName)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database) return false;
	return Database->BeginTransaction();
}

bool UESQLSubsystem::CommitTransaction(const FString& DatabaseName)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database) return false;
	return Database->CommitTransaction();
}

bool UESQLSubsystem::RollbackTransaction(const FString& DatabaseName)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database) return false;
	return Database->RollbackTransaction();
}


// ── Snapshots ────────────────────────────────────────────────────────────────

FESQLQueryResult UESQLSubsystem::SaveSnapshot(
	const FString& DatabaseName,
	const FString& SlotName,
	const FString& DisplayName)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);
	FString Error;

	if (!Database->BackupToFile(SnapshotPath, Error))
	{
		return FESQLQueryResult::Failure(Error);
	}

	// Write snapshot metadata (.meta.json)
	const FString MetaPath = ResolveSnapshotMetaPath(DatabaseName, SlotName);
	TSharedPtr<FJsonObject> MetaJson = MakeShareable(new FJsonObject());
	MetaJson->SetStringField(TEXT("SlotName"), SlotName);
	MetaJson->SetStringField(TEXT("DisplayName"), DisplayName.IsEmpty() ? SlotName : DisplayName);
	MetaJson->SetStringField(TEXT("Timestamp"), FDateTime::UtcNow().ToIso8601());
	MetaJson->SetStringField(TEXT("DatabaseName"), DatabaseName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const int64 FileSize = PlatformFile.FileSize(*SnapshotPath);
	MetaJson->SetNumberField(TEXT("FileSizeBytes"), FileSize);

	FString MetaString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaString);
	FJsonSerializer::Serialize(MetaJson.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(MetaString, *MetaPath);

	UE_LOG(LogExtendedSQL, Log, TEXT("Saved snapshot '%s' for database '%s' (%lld bytes)"),
		*SlotName, *DatabaseName, FileSize);

	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::LoadSnapshot(const FString& DatabaseName, const FString& SlotName)
{
	TSharedPtr<FESQLDatabase> Database = GetDatabase(DatabaseName);
	if (!Database)
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Database '%s' is not open"), *DatabaseName));
	}

	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*SnapshotPath))
	{
		return FESQLQueryResult::Failure(FString::Printf(TEXT("Snapshot '%s' does not exist"), *SlotName));
	}

	FString Error;
	if (!Database->RestoreFromFile(SnapshotPath, Error))
	{
		return FESQLQueryResult::Failure(Error);
	}

	UE_LOG(LogExtendedSQL, Log, TEXT("Loaded snapshot '%s' into database '%s'"), *SlotName, *DatabaseName);
	return FESQLQueryResult::Success();
}

FESQLQueryResult UESQLSubsystem::DeleteSnapshot(const FString& DatabaseName, const FString& SlotName)
{
	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);
	const FString MetaPath = ResolveSnapshotMetaPath(DatabaseName, SlotName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*SnapshotPath))
	{
		PlatformFile.DeleteFile(*SnapshotPath);
	}

	if (PlatformFile.FileExists(*MetaPath))
	{
		PlatformFile.DeleteFile(*MetaPath);
	}

	return FESQLQueryResult::Success();
}

TArray<FESQLSnapshotInfo> UESQLSubsystem::GetAllSnapshots(const FString& DatabaseName)
{
	TArray<FESQLSnapshotInfo> Snapshots;

	const UESQLSettings* Settings = UESQLSettings::Get();
	const FString SnapshotDir = FPaths::Combine(
		UESQLSettings::ResolveDatabaseDirectoryPath(),
		TEXT("Snapshots"),
		DatabaseName
	);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*SnapshotDir))
	{
		return Snapshots;
	}

	// Find all .meta.json files
	TArray<FString> MetaFiles;
	PlatformFile.FindFilesRecursively(MetaFiles, *SnapshotDir, TEXT(".meta.json"));

	for (const FString& MetaPath : MetaFiles)
	{
		FString MetaString;
		if (!FFileHelper::LoadFileToString(MetaString, *MetaPath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> MetaJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MetaString);
		if (!FJsonSerializer::Deserialize(Reader, MetaJson) || !MetaJson.IsValid())
		{
			continue;
		}

		FESQLSnapshotInfo Info;
		Info.SlotName = MetaJson->GetStringField(TEXT("SlotName"));
		Info.DisplayName = MetaJson->GetStringField(TEXT("DisplayName"));
		Info.FileSizeBytes = static_cast<int64>(MetaJson->GetNumberField(TEXT("FileSizeBytes")));

		FString TimestampStr = MetaJson->GetStringField(TEXT("Timestamp"));
		FDateTime::ParseIso8601(*TimestampStr, Info.Timestamp);

		Info.FilePath = ResolveSnapshotPath(DatabaseName, Info.SlotName);
		Snapshots.Add(Info);
	}

	return Snapshots;
}

bool UESQLSubsystem::DoesSnapshotExist(const FString& DatabaseName, const FString& SlotName)
{
	const FString SnapshotPath = ResolveSnapshotPath(DatabaseName, SlotName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	return PlatformFile.FileExists(*SnapshotPath);
}

bool UESQLSubsystem::IsSessionDatabase(const FString& DatabaseName) const
{
	if (const EESQLDatabasePersistence* Found = DatabasePersistenceMap.Find(DatabaseName))
	{
		return *Found == EESQLDatabasePersistence::Session;
	}
	return false;
}
// ── Internal Helpers ─────────────────────────────────────────────────────────

FString UESQLSubsystem::ResolveDatabasePath(
	const FString& FileName,
	EESQLDatabaseScope Scope,
	const FString& PlayerId) const
{
	const UESQLSettings* Settings = UESQLSettings::Get();
	const FString BaseDir = UESQLSettings::ResolveDatabaseDirectoryPath();

	switch (Scope)
	{
	case EESQLDatabaseScope::Local:
		return FPaths::Combine(BaseDir, FileName);

	case EESQLDatabaseScope::Server:
		return FPaths::Combine(BaseDir,
			Settings ? Settings->ServerSubdirectory : TEXT("Server"),
			FileName);

	case EESQLDatabaseScope::PlayerScoped:
		return FPaths::Combine(BaseDir,
			Settings ? Settings->PlayerSubdirectory : TEXT("Players"),
			PlayerId,
			FileName);

	default:
		return FPaths::Combine(BaseDir, FileName);
	}
}

FString UESQLSubsystem::ResolveSnapshotPath(const FString& DatabaseName, const FString& SlotName) const
{
	return FPaths::Combine(
		UESQLSettings::ResolveDatabaseDirectoryPath(),
		TEXT("Snapshots"),
		DatabaseName,
		SlotName + TEXT(".db")
	);
}

FString UESQLSubsystem::ResolveSnapshotMetaPath(const FString& DatabaseName, const FString& SlotName) const
{
	return FPaths::Combine(
		UESQLSettings::ResolveDatabaseDirectoryPath(),
		TEXT("Snapshots"),
		DatabaseName,
		SlotName + TEXT(".meta.json")
	);
}

TSharedPtr<FESQLDatabase> UESQLSubsystem::GetDatabase(const FString& DatabaseName) const
{
	if (const TSharedPtr<FESQLDatabase>* Found = OpenDatabases.Find(DatabaseName))
	{
		return *Found;
	}
	return nullptr;
}

TSharedPtr<FESQLDatabase> UESQLSubsystem::GetPlayerDatabase(const FString& PlayerId, const FString& DatabaseName) const
{
	if (const auto* PlayerMap = PlayerDatabases.Find(PlayerId))
	{
		if (const TSharedPtr<FESQLDatabase>* Found = PlayerMap->Find(DatabaseName))
		{
			return *Found;
		}
	}
	return nullptr;
}

FString UESQLSubsystem::GetPlayerIdFromController(APlayerController* PC) const
{
	if (!PC) return FString();

	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	if (!PS) return FString();

	// Try to get UniqueNetId
	FUniqueNetIdRepl UniqueNetIdRepl = PS->GetUniqueId();
	if (UniqueNetIdRepl.IsValid())
	{
		return UniqueNetIdRepl->ToString();
	}

	// PIE fallback — use player index to avoid collisions
#if WITH_EDITOR
	const int32 PIEIndex = UE::GetPlayInEditorID();
	return FString::Printf(TEXT("LOCAL_%d"), PIEIndex);
#else
	// Non-editor fallback
	return FString::Printf(TEXT("LOCAL_%d"), PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetControllerId() : 0);
#endif
}

bool UESQLSubsystem::ValidateAuthority(EESQLDatabaseScope Scope, const FString& CallerName) const
{
	if (Scope != EESQLDatabaseScope::Server && Scope != EESQLDatabaseScope::PlayerScoped)
	{
		return true;  // Local scope has no restriction
	}

	const UESQLSettings* Settings = UESQLSettings::Get();
	if (Settings && Settings->bEnforceServerAuthority && !HasServerAuthority())
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("%s: Authority check failed — scope %d requires server authority"),
			*CallerName, (int32)Scope);
		return false;
	}

	return true;
}


// ── Auto Player DB Hooks ─────────────────────────────────────────────────────

void UESQLSubsystem::OnPostLogin(AGameModeBase* /*GameMode*/, APlayerController* NewPlayer)
{
	if (!NewPlayer) return;

	const FString PlayerId = GetPlayerIdFromController(NewPlayer);
	if (PlayerId.IsEmpty()) return;

	// Use a default database name for auto-opened player databases
	const FString DefaultDbName = TEXT("PlayerData");
	FString Error;

	FESQLQueryResult Result = OpenPlayerDatabase(DefaultDbName, NewPlayer);
	if (Result.bSuccess)
	{
		UE_LOG(LogExtendedSQL, Log, TEXT("Auto-opened player database for '%s'"), *PlayerId);
	}
	else
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("Failed to auto-open player database for '%s': %s"),
			*PlayerId, *Result.ErrorMessage);
	}
}

void UESQLSubsystem::OnLogout(AGameModeBase* /*GameMode*/, AController* Exiting)
{
	APlayerController* PC = Cast<APlayerController>(Exiting);
	if (!PC) return;

	const FString PlayerId = GetPlayerIdFromController(PC);
	if (PlayerId.IsEmpty()) return;

	const UESQLSettings* Settings = UESQLSettings::Get();
	const bool bDeleteFile = Settings ? Settings->bDeletePlayerDBOnDisconnect : false;

	// Close all databases for this player
	if (auto* PlayerMap = PlayerDatabases.Find(PlayerId))
	{
		TArray<FString> DbNames;
		PlayerMap->GetKeys(DbNames);

		for (const FString& DbName : DbNames)
		{
			ClosePlayerDatabase(DbName, PC, bDeleteFile);
		}
	}

	UE_LOG(LogExtendedSQL, Log, TEXT("Auto-closed player databases for '%s'%s"),
		*PlayerId, bDeleteFile ? TEXT(" (files deleted)") : TEXT(""));
}

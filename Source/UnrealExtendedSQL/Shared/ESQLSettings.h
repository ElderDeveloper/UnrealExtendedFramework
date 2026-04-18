// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ESQLSettings.generated.h"


/**
 * Developer settings for SQLite configuration.
 * These settings appear in Project Settings → Extended Framework → SQL.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Extended SQL"))
class UNREALEXTENDEDSQL_API UESQLSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UESQLSettings();


	// ── Defaults ──────────────────────────────────────────────────────────────

	/** Default directory for database files (relative to Saved/) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults",
		meta = (DisplayName = "Database Directory"))
	FString DefaultDatabaseDirectory = TEXT("Databases");

	/** Resolve the configured database root directory to an absolute path. */
	static FString ResolveDatabaseDirectoryPath();

	/** Resolve a logical database name to an absolute .db file path. */
	static FString ResolveDatabaseFilePath(const FString& DatabaseName);

	/** Resolve a player-scoped logical database name to an absolute .db file path. */
	static FString ResolvePlayerDatabaseFilePath(const FString& DatabaseName, const FString& PlayerId);

	/** Subdirectory under DefaultDatabaseDirectory for Server-scoped databases. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults",
		meta = (DisplayName = "Server Subdirectory"))
	FString ServerSubdirectory = TEXT("Server");

	/** Subdirectory under DefaultDatabaseDirectory for PlayerScoped databases.
	    Each player gets a subfolder keyed by their UniqueNetId. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults",
		meta = (DisplayName = "Player Subdirectory"))
	FString PlayerSubdirectory = TEXT("Players");

	/** If true, databases are opened with WAL journal mode for better
	    concurrent read performance. Recommended for most use cases. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults",
		meta = (DisplayName = "Use WAL Journal Mode"))
	bool bUseWALMode = true;


	// ── Debug ────────────────────────────────────────────────────────────────

	/** If true, enables verbose SQLite logging to the output log.
	    Logs every SQL statement executed, bind values, and timing. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug",
		meta = (DisplayName = "Enable Verbose Logging"))
	bool bEnableVerboseLogging = false;


	// ── Limits ────────────────────────────────────────────────────────────────

	/** Maximum number of simultaneous open database handles. 0 = unlimited. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Limits",
		meta = (DisplayName = "Max Open Databases", ClampMin = "0", ClampMax = "64"))
	int32 MaxOpenDatabases = 16;

	/** Default maximum number of rows returned by SelectRows when no
	    explicit Limit is provided (-1). Set to 0 for unlimited (not recommended).
	    Individual queries can override this with their own Limit parameter. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Limits",
		meta = (DisplayName = "Default Max Result Limit", ClampMin = "0"))
	int32 DefaultMaxResultLimit = 1000;


	// ── Multiplayer ──────────────────────────────────────────────────────────

	/**
	 * If true, the subsystem will automatically create and open a
	 * PlayerScoped database for every player that logs in to a
	 * dedicated or listen server session.
	 * The database is closed and flushed when the player disconnects.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Multiplayer",
		meta = (DisplayName = "Auto-Open Player Databases"))
	bool bAutoOpenPlayerDatabases = false;

	/**
	 * If true, server-scoped databases opened with Scope::Server
	 * will refuse to execute on a pure client (NM_Client).
	 * This prevents accidental authority violations from Blueprints.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Multiplayer",
		meta = (DisplayName = "Enforce Server Authority"))
	bool bEnforceServerAuthority = true;

	/**
	 * If true, player database files are deleted from disk when the
	 * player disconnects. If false, they persist between sessions
	 * (useful for persistent worlds or leagues).
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Multiplayer",
		meta = (DisplayName = "Delete Player DB on Disconnect"))
	bool bDeletePlayerDBOnDisconnect = false;


	// ── Helpers ──────────────────────────────────────────────────────────────

	/** Get the singleton settings instance */
	static const UESQLSettings* Get()
	{
		return GetDefault<UESQLSettings>();
	}

	/** Category path for Project Settings UI */
	virtual FName GetCategoryName() const override { return FName(TEXT("Extended Framework")); }
};

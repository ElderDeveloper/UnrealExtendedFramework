# ExtendedSQL — SQLite for Unreal Engine (Architecture Design)

> **Module name:** `UnrealExtendedSQL`
> **Author:** Kemal Erdem YILMAZ
> **Plugin parent:** UnrealExtendedFramework
> **Engine target:** UE 5.x+

---

## 1. Design Goals

| Goal | Rationale |
|---|---|
| **Modular** | `UnrealExtendedSQL` stays isolated inside the plugin. Other modules may depend on it, but SQL should not depend on PlayFab, EOS, Gameplay, or project code. |
| **Layered and C++-first** | The real product surface should exist in C++ first. Blueprint and future K2 nodes should expose the same runtime behavior, not define separate logic. |
| **Typed table workflows** | Normal schema-backed usage should revolve around `UESQLTableAsset`, `FESQLId`, and typed row structs instead of raw SQL strings and `TMap<FString, FString>` payloads. |
| **Raw power preserved** | `UESQLSubsystem` and `FESQLDatabase` still exist for raw SQL, dynamic schemas, migrations, analytics/logging, and advanced runtime systems. |
| **Schema ownership in assets** | `UESQLTableAsset` should own schema, table identity, primary-key rules, label behavior, import/export, and the typed CRUD surface used by both C++ and Blueprint tooling. |
| **K2 as UX layer only** | Future custom K2 nodes should sit above the typed C++ layer and only improve graph ergonomics, pin typing, and discoverability. |
| **Thread-safe and cross-platform** | SQLite access should continue to use the custom UE VFS, connection-level locking, prepared statements, and background execution where appropriate. |
| **Multiplayer-native** | Database scope, player-scoped databases, server authority, and seamless-travel-safe player context remain first-class concerns. |
| **VCS-friendly authoring** | `.sqldump` remains the version-controlled source of truth for authored SQL data, with `.db` files treated as derived artifacts. |

---

## 2. Current File Structure (April 2026)

```
UnrealExtendedFramework/
├── Thirdparty/
│   └── SQLite/
│       ├── sqlite3.c
│       └── sqlite3.h
│
├── Source/
│   ├── UnrealExtendedSQL/
│   │   ├── UnrealExtendedSQL.Build.cs
│   │   ├── UnrealExtendedSQL.h
│   │   ├── UnrealExtendedSQL.cpp
│   │   ├── Core/
│   │   │   ├── ESQLDatabase.h
│   │   │   ├── ESQLDatabase.cpp
│   │   │   ├── ESQLStatement.h
│   │   │   ├── ESQLStatement.cpp
│   │   │   ├── ESQLUnrealVFS.h
│   │   │   └── ESQLUnrealVFS.cpp
│   │   ├── PlayerData/
│   │   │   ├── ESQLPlayerDBComponent.h
│   │   │   └── ESQLPlayerDBComponent.cpp
│   │   ├── Shared/
│   │   │   ├── ESQLId.h
│   │   │   ├── ESQLPropertySerializer.h
│   │   │   ├── ESQLPropertySerializer.cpp
│   │   │   ├── ESQLSettings.h
│   │   │   ├── ESQLSettings.cpp
│   │   │   └── ESQLTypes.h
│   │   ├── Subsystem/
│   │   │   ├── ESQLSubsystem.h
│   │   │   └── ESQLSubsystem.cpp
│   │   ├── TableAsset/
│   │   │   ├── ESQLStructValidator.h
│   │   │   ├── ESQLStructValidator.cpp
│   │   │   ├── ESQLTableAsset.h
│   │   │   └── ESQLTableAsset.cpp
│   │   └── VCSPipeline/
│   │       ├── ESQLDumpPipeline.h
│   │       └── ESQLDumpPipeline.cpp
│   │
│   └── UnrealExtendedSQLEditor/
│       ├── UnrealExtendedSQLEditor.Build.cs
│       ├── UnrealExtendedSQLEditor.h
│       ├── UnrealExtendedSQLEditor.cpp
│       ├── AssetFactory/
│       │   ├── ESQLTableAssetActions.h
│       │   ├── ESQLTableAssetActions.cpp
│       │   ├── ESQLTableAssetFactory.h
│       │   └── ESQLTableAssetFactory.cpp
│       ├── SqlId/
│       │   ├── ESQLIdCustomization.h
│       │   └── ESQLIdCustomization.cpp
│       ├── StructContextMenu/
│       │   ├── ESQLStructMenuExtension.h
│       │   └── ESQLStructMenuExtension.cpp
│       ├── TableAssetEditor/
│       │   ├── FESQLTableEditorToolkit.h
│       │   ├── FESQLTableEditorToolkit.cpp
│       │   ├── SESQLRowEditor.h
│       │   ├── SESQLRowEditor.cpp
│       │   ├── SESQLTableListViewRow.h
│       │   └── SESQLTableListViewRow.cpp
│       └── Validation/
│           ├── SESQLValidationDialog.h
│           └── SESQLValidationDialog.cpp
│
└── ExtendedSQL.md
```

Current note:

- the runtime module no longer contains the old generic async action files
- the runtime module no longer contains the old generic Blueprint helper library
- the current public direction is a stronger typed C++ layer first, then a thin Blueprint/K2 layer above it

### 2.1 Runtime Module Responsibilities

| Area | Files | Responsibility |
|---|---|---|
| Module bootstrap | `UnrealExtendedSQL.Build.cs`, `UnrealExtendedSQL.h`, `UnrealExtendedSQL.cpp` | Build rules, module startup/shutdown, runtime registration. |
| Shared public types | `Shared/ESQLTypes.h` | Public result types, row/column types, database scope enums, snapshot metadata, delegates, binding values. |
| SQL row handle | `Shared/ESQLId.h` | `FESQLId` reference type used for row ids, foreign-key-like references, picker metadata, and asset-linked row resolution. |
| Struct serialization | `Shared/ESQLPropertySerializer.h/.cpp` | Converts reflected Unreal property values to SQL bindings/strings and back again for typed row save/load. |
| Project settings | `Shared/ESQLSettings.h/.cpp` | Global defaults for database directories, multiplayer behavior, limits, and debug toggles. |
| SQLite connection layer | `Core/ESQLDatabase.h/.cpp` | RAII database handle, execute/prepare helpers, statement cache, transactions, backup/snapshot, dump helpers. |
| Prepared statement layer | `Core/ESQLStatement.h/.cpp` | Typed bind/step/read wrapper around `sqlite3_stmt*`. |
| Platform file bridge | `Core/ESQLUnrealVFS.h/.cpp` | Unreal-backed SQLite VFS so file access goes through UE platform abstractions. |
| Subsystem service layer | `Subsystem/ESQLSubsystem.h/.cpp` | Database lifecycle, raw SQL/query helpers, snapshot API, net-mode/authority rules, player-database management, and shared runtime services. |
| Player database context | `PlayerData/ESQLPlayerDBComponent.h/.cpp` | Player-scoped database ownership/state object for server-side multiplayer context, not the long-term query surface. |
| Typed table API | `TableAsset/ESQLTableAsset.h/.cpp` | Schema-owning asset, typed row load/save surface, schema sync, import/export, label/primary-key behavior, and runtime/editor table identity. |
| Struct compatibility rules | `TableAsset/ESQLStructValidator.h/.cpp` | Validates `UScriptStruct` compatibility with SQLite storage rules and column generation. |
| VCS pipeline | `VCSPipeline/ESQLDumpPipeline.h/.cpp` | `.sqldump` export/import, `.db` <-> `.sqldump` sync, deterministic authored-data pipeline, cook-time rebuild path. |

### 2.2 Editor Module Responsibilities

| Area | Files | Responsibility |
|---|---|---|
| Editor bootstrap | `UnrealExtendedSQLEditor.Build.cs`, `UnrealExtendedSQLEditor.h`, `UnrealExtendedSQLEditor.cpp` | Editor module startup, registration of asset actions, customizations, and menu extensions. |
| Asset creation | `AssetFactory/ESQLTableAssetFactory.h/.cpp` | Content Browser asset creation flow for new SQL table assets with struct-driven configuration. |
| Asset actions | `AssetFactory/ESQLTableAssetActions.h/.cpp` | Asset type actions, Content Browser integration, editor opening behavior. |
| SQL id property UI | `SqlId/ESQLIdCustomization.h/.cpp` | `FESQLId` detail customization with searchable picker, meta-tag support, live DB lookup, and label-column configuration. |
| Struct context menu | `StructContextMenu/ESQLStructMenuExtension.h/.cpp` | Right-click struct shortcut for creating SQL table assets from structs. |
| Table asset editor host | `TableAssetEditor/FESQLTableEditorToolkit.h/.cpp` | Main asset editor toolkit and tab orchestration for SQL table editing. |
| Row editor widgets | `TableAssetEditor/SESQLRowEditor.h/.cpp`, `TableAssetEditor/SESQLTableListViewRow.h/.cpp` | Slate widgets for row-level editing and list-view row presentation inside the table editor. |
| Validation dialog | `Validation/SESQLValidationDialog.h/.cpp` | Struct validation feedback UI for unsupported property types or schema issues. |

---

## 3. Current Layer Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                          EDITOR LAYER                                │
│  UnrealExtendedSQLEditor                                             │
│  - asset creation/editing                                            │
│  - struct validation                                                 │
│  - FESQLId picker customization                                      │
├──────────────────────────────────────────────────────────────────────┤
│                     BLUEPRINT / K2 UX LAYER                          │
│  thin reflection and future K2 nodes                                 │
│  - should call typed C++ operations                                  │
│  - should not own core SQL behavior                                  │
├──────────────────────────────────────────────────────────────────────┤
│                      TYPED TABLE API LAYER                           │
│  UESQLTableAsset + FESQLId + FESQLPropertySerializer                 │
│  - schema ownership                                                  │
│  - row identity + labels                                             │
│  - typed row load/save                                               │
│  - import/export + schema sync                                       │
├──────────────────────────────────────────────────────────────────────┤
│                    SUBSYSTEM / CONTEXT LAYER                         │
│  UESQLSubsystem + UESQLPlayerDBComponent + FESQLDumpPipeline         │
│  - database lifecycle and authority                                  │
│  - raw SQL and utility queries                                       │
│  - player-scoped database context                                    │
│  - snapshot and VCS pipeline services                                │
├──────────────────────────────────────────────────────────────────────┤
│                           CORE LAYER                                 │
│  FESQLDatabase + FESQLStatement + ESQLUnrealVFS                      │
│  - sqlite connection/statement primitives                            │
│  - transactions, backup, statement caching                           │
│  - platform-safe file access                                         │
├──────────────────────────────────────────────────────────────────────┤
│                        THIRDPARTY LAYER                              │
│  sqlite3.c / sqlite3.h                                               │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.1 Clear Responsibility Separation

| Surface | Must own | Must not own |
|---|---|---|
| `FESQLDatabase` / `FESQLStatement` | sqlite connection and statement primitives, transactions, backup, prepared execution, value binding, statement cache | gameplay schema rules, player identity, asset metadata, Blueprint or editor UX |
| `FESQLPropertySerializer` | struct/property value conversion between Unreal reflection and SQL value representation | query execution, database lifetime, table ownership, editor state |
| `FESQLId` | row id payload plus optional asset/picker metadata | row loading, query execution, schema creation |
| `UESQLSubsystem` | database registry, path resolution, authority rules, open/close/delete, snapshots, raw SQL, player database services | typed table CRUD as the main public workflow, editor-specific UI, asset-authored schema ownership |
| `UESQLPlayerDBComponent` | player database context, player identity, lifecycle hooks, convenience access to player-scoped db ownership | string query APIs, typed row CRUD, schema logic |
| `UESQLTableAsset` | schema ownership, typed row CRUD, primary-key policy, label policy, schema sync, asset-driven import/export, table identity | global database registry, server authority policy, generic raw SQL service layer |
| `FESQLDumpPipeline` | `.sqldump` export/import/sync and authored-data VCS workflow | runtime query API, gameplay-facing CRUD, player context |
| `UnrealExtendedSQLEditor` | asset creation, validation UI, row editor widgets, `FESQLId` picker customization | runtime SQL semantics, gameplay data rules, database service logic |
| Future Blueprint/K2 layer | graph UX, pin typing, discoverability, async designer-friendly wrappers | core runtime behavior, schema rules, duplicated CRUD implementations |

### 3.2 Validation Against Current Code

| Surface | Validation result | Notes |
|---|---|---|
| `FESQLDatabase` / `FESQLStatement` | Good | Current core headers are already mostly clean low-level wrappers. |
| `FESQLPropertySerializer` | Good | Current serializer is correctly focused on reflection/value conversion. |
| `FESQLId` | Good | Current id type is only a reference container with picker metadata. |
| `UESQLPlayerDBComponent` | Good | It has already been reduced to ownership/state context instead of query helpers. |
| `FESQLDumpPipeline` | Good | It is already isolated as a VCS/dump service. |
| `UnrealExtendedSQLEditor` | Mostly good | Current editor files are editor-only and centered on authoring/customization. |
| `UESQLTableAsset` | Mostly good | It already owns typed load/save, schema sync, import/export, and label behavior, which is the right public typed contract. |
| `UESQLSubsystem` | Good after cleanup | It should own lifecycle, authority, raw SQL, snapshots, and player-db services only. |

### 3.3 Recommended Better Responsibility Division

The cleaner long-term split is:

1. Keep `FESQLDatabase` and `FESQLStatement` as the only low-level SQL primitives.
2. Keep `UESQLSubsystem` focused on database lifecycle, authority, raw SQL, snapshots, and player database services.
3. Keep `UESQLPlayerDBComponent` as context only.
4. Keep `UESQLTableAsset` as the main typed public contract for both C++ and future Blueprint/K2 workflows.
5. Keep `FESQLPropertySerializer` as the single struct/value mapping layer reused by both subsystem helpers and table helpers.
6. Keep `FESQLDumpPipeline` separate as the authored-data/VCS pipeline.
7. Keep editor code strictly editor-only.

The one boundary that should be improved next is inside the runtime layer:

- keep typed struct helper behavior out of `UESQLSubsystem`
- let `UESQLTableAsset` or one shared typed table helper own struct-based row operations
- make Blueprint bridge functions and future K2 nodes call that same typed path

Minimal recommended re-division:

- `UESQLSubsystem` should keep: open/close/delete db, snapshots, raw execute, raw select/update/delete helpers, authority, player-db services
- `UESQLSubsystem` should not reintroduce typed struct CRUD helpers such as `InsertStructRow`, `UpsertStructRow`, or `StructToColumnBindings`
- `UESQLTableAsset` should keep: typed row load/save, row-id lookups, schema sync, label logic, import/export
- `FESQLPropertySerializer` should remain the shared mapper used by both layers

If the typed surface grows further, add only one additional non-UObject helper, for example `FESQLTypedTableRuntime`, to avoid bloating `UESQLTableAsset` while still keeping the implementation consolidated.

### 3.4 Should C++ Use `UESQLTableAsset`?

Yes, but not for every C++ use case.

The right split is:

| Use case | Recommended surface | Why |
|---|---|---|
| Typed gameplay/content data with a stable schema | `UESQLTableAsset` | The asset already owns schema, database/table identity, primary key rules, labels, import/export, and the typed row API we want K2 nodes to share. |
| Shared typed workflows across C++ and future K2 nodes | `UESQLTableAsset` | This prevents Blueprint/K2 from inventing a second implementation path. |
| Row handles, foreign-key-like references, label resolution | `FESQLId` + `UESQLTableAsset` | `FESQLId` stores the row id, while the asset provides the schema and picker/runtime lookup context. |
| Dynamic schemas, joins, analytics, migrations, logging, utility SQL | `UESQLSubsystem` | These are service/power-user scenarios where asset ownership is not the right abstraction. |
| Internal low-level database control | `FESQLDatabase` / `FESQLStatement` | This is the core portability layer, not the main gameplay-facing API. |

So the answer is not "all C++ should use the asset" and it is also not "the asset is only for Blueprint".

The better rule is:

- `UESQLTableAsset` is the typed table contract for both C++ and Blueprint-facing workflows
- `UESQLSubsystem` is the raw lifecycle/service layer
- `FESQLDatabase` is the low-level primitive layer

That keeps the public product coherent.

### 3.5 Current Recommended API Stack

1. `FESQLDatabase` / `FESQLStatement` for low-level core operations.
2. `UESQLSubsystem` for database lifecycle, raw SQL, snapshots, multiplayer authority, and shared services.
3. `UESQLTableAsset` for typed row CRUD, schema ownership, label behavior, and asset-driven workflows.
4. Future Blueprint bridge functions and K2 nodes on top of the typed table layer.

Current precedence note:

- sections 1 through 3 define the current intended architecture
- section 13 defines the current file checklist
- deeper component details below still include older implementation discussion and should be reconciled against the current layering before treating them as final

---

## 4. Component Details

// ── Column Value ─────────────────────────────────────────────
UENUM(BlueprintType)
enum class EESQLColumnType : uint8
{
    Null,
    Integer,
    Float,
    Text,
    Blob
};


// ── Database Scope (multiplayer) ─────────────────────────────

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
     * DediServer:  Saved/Databases/<Name>.db     (server machine only)
     * ListenServer: Saved/Databases/<Name>.db    (host machine only)
     *
     * Use for: save-game data in singleplayer, local caches, mod content.
     */
    Local,

    /**
     * Server-authoritative shared database.
     * Only the server (dedicated or listen-server host) may open it.
     * Calls from a pure client return an error.
     *
     * Path: Saved/Databases/Server/<Name>.db
     *
     * Use for: world state, match logs, economy tables, NPC data
     * that must be consistent across all connected players.
     */
    Server,

    /**
     * Per-player database managed by the server.
     * Server creates one .db per connected player, keyed by UniqueNetId.
     * Opened when the player joins, closed when they leave.
     *
     * Path: Saved/Databases/Players/<UniqueNetId>/<Name>.db
     *
     * Use for: per-player inventory, quest progress, stats
     * in a server-authoritative multiplayer session.
     */
    PlayerScoped
};


// ── Database Persistence Mode ───────────────────────────────

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


// ── Snapshot Info ────────────────────────────────────────────

/** Metadata about a saved snapshot on disk. */
USTRUCT(BlueprintType)
struct FESQLSnapshotInfo
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


// ── Single Column ────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FESQLColumn
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "SQL")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "SQL")
    EESQLColumnType Type = EESQLColumnType::Null;
};

// ── Single Row ───────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FESQLRow
{
    GENERATED_BODY()

    /** Column name → string value.  Blobs and NULLs are represented as
        empty strings; use ColumnTypes for disambiguation. */
    UPROPERTY(BlueprintReadOnly, Category = "SQL")
    TMap<FString, FString> Columns;
};

// ── Database Open Parameters ─────────────────────────────────
USTRUCT(BlueprintType)
struct FESQLDatabaseParams
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

// ── Query Result ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FESQLQueryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "SQL")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "SQL")
    FString ErrorMessage;

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

    static FESQLQueryResult Success(/* ... */);
    static FESQLQueryResult Failure(const FString& Msg);
};

// ── Delegates ────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnESQLQueryComplete, const FESQLQueryResult&, Result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnESQLQueryCompleteCallback, const FESQLQueryResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnESQLPlayerDBEvent, APlayerController*, PlayerController, const FString&, PlayerId);
```

#### `ESQLSettings.h`

Following the `UEPFSettings` pattern — a `UDeveloperSettings` surfaced in **Project Settings → Extended Framework → SQL**.

```cpp
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Extended SQL"))
class UESQLSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:

    /** Default directory for database files (relative to Saved/) */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
    FString DefaultDatabaseDirectory = TEXT("Databases");

    /** Subdirectory under DefaultDatabaseDirectory for Server-scoped databases. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
    FString ServerSubdirectory = TEXT("Server");

    /** Subdirectory under DefaultDatabaseDirectory for PlayerScoped databases.
        Each player gets a subfolder keyed by their UniqueNetId. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
    FString PlayerSubdirectory = TEXT("Players");

    /** If true, databases are opened with WAL journal mode for better
        concurrent read performance. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
    bool bUseWALMode = true;

    /** If true, enables verbose SQLite logging to the output log. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
    bool bEnableVerboseLogging = false;

    /** Maximum number of simultaneous open database handles. 0 = unlimited. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Limits",
        meta = (ClampMin = "0", ClampMax = "64"))
    int32 MaxOpenDatabases = 16;

    /** Default maximum number of rows returned by SelectRows when no
        explicit Limit is provided. Set to 0 for unlimited (not recommended).
        Individual queries can override this with their own Limit parameter. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Limits",
        meta = (ClampMin = "0"))
    int32 DefaultMaxResultLimit = 1000;


    // ── Multiplayer ──────────────────────────────────────────────────────

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


    static const UESQLSettings* Get() { return GetDefault<UESQLSettings>(); }
    virtual FName GetCategoryName() const override { return FName(TEXT("Extended Framework")); }
};
```

---

### 4.3 Core — `FESQLDatabase` / `FESQLStatement`

Pure C++ — no `UCLASS`, no `GENERATED_BODY()`. This is **the portability layer**.

> **Cross-platform I/O:** To support all platforms (PC, consoles, mobile),
> `FESQLDatabase` uses a **custom SQLite VFS** backed by `FPlatformFileManager`
> instead of raw C `fopen()`. This ensures file I/O goes through Unreal’s
> platform abstraction layer, respecting sandboxing and platform-specific
> file system restrictions (e.g. PS5, Switch, Xbox GDK).

#### `ESQLDatabase.h` (excerpt)

```cpp
/**
 * RAII wrapper around a sqlite3* connection handle.
 * NOT a UObject — used internally by UESQLSubsystem and can be used
 * from any C++ code without depending on the UObject ecosystem.
 */
class UNREALEXTENDEDSQL_API FESQLDatabase : public TSharedFromThis<FESQLDatabase>
{
public:
    ~FESQLDatabase();

    /** Open a file-backed database (Persistent mode).
        On open, automatically executes:
          PRAGMA journal_mode=WAL;    (if bUseWALMode, default: true)
          PRAGMA foreign_keys=ON;     (SQLite FK enforcement is OFF by default) */
    static TSharedPtr<FESQLDatabase> Open(const FString& FilePath, FString& OutError);

    /** Open a purely in-memory database (Session mode).
        The database lives only in RAM — no file is created on disk.
        Use BackupToFile() to persist it (snapshot), and
        RestoreFromFile() to load a snapshot into memory. */
    static TSharedPtr<FESQLDatabase> OpenInMemory(FString& OutError);

    void Close();
    bool IsOpen() const;

    /** Returns true if this connection is an in-memory database (Session mode). */
    bool IsInMemory() const;

    // ── Quick helpers ────────────────────────────────────
    /** All Execute/Query functions use sqlite3_prepare_v2() internally,
        NEVER sqlite3_exec(). This means multi-statement SQL strings
        (e.g. "DROP TABLE x; DROP TABLE y") are rejected — only the first
        statement is compiled. This is a deliberate safety measure. */
    FESQLQueryResult Execute(const FString& SQL);
    FESQLQueryResult Execute(const FString& SQL, const TArray<FString>& Bindings);

    // ── Prepared-statement API ───────────────────────────
    TSharedPtr<FESQLStatement> Prepare(const FString& SQL, FString& OutError);

    // ── Transaction helpers ──────────────────────────────
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

    // ── Backup / Snapshot ────────────────────────────────

    /** Backup this database (memory or file) to a .db file on disk.
        Uses sqlite3_backup API — safe to call while the DB is live.
        This is the implementation behind UESQLSubsystem::SaveSnapshot(). */
    bool BackupToFile(const FString& DestFilePath, FString& OutError);

    /** Replace this database's contents with data from a .db file on disk.
        Uses sqlite3_backup API — the current contents are fully overwritten.
        This is the implementation behind UESQLSubsystem::LoadSnapshot(). */
    bool RestoreFromFile(const FString& SourceFilePath, FString& OutError);

    // ── Metadata ─────────────────────────────────────────
    int64 GetLastInsertRowId() const;
    int32 GetChangesCount() const;
    FString GetLastErrorMessage() const;

private:
    struct sqlite3* DbHandle = nullptr;
    FString DatabasePath;          // Empty string for in-memory databases
    bool bInMemory = false;
    FCriticalSection CriticalSection;  // Per-connection lock

    // ── Statement Cache ──────────────────────────────────
    /** Caches compiled sqlite3_stmt* handles keyed by SQL string.
        Avoids recompiling the same SQL on repeated InsertRow/SelectRows calls.
        Cache is cleared on Close(). Thread-safe via CriticalSection.

        IMPORTANT: GetOrPrepare() always calls sqlite3_reset() + sqlite3_clear_bindings()
        on the cached statement before returning it. This ensures the caller receives
        a clean, ready-to-bind statement — preventing stale-binding bugs when the
        same INSERT is called from multiple locations or in a loop. */
    TMap<FString, TSharedPtr<FESQLStatement>> StatementCache;
    TSharedPtr<FESQLStatement> GetOrPrepare(const FString& SQL, FString& OutError);
};
```

#### `ESQLStatement.h` (excerpt)

```cpp
/**
 * Wraps a compiled sqlite3_stmt* — bind parameters, step through rows, read columns.
 */
class UNREALEXTENDEDSQL_API FESQLStatement
{
public:
    ~FESQLStatement();

    // ── Binding ──────────────────────────────────────────
    bool BindInt(int32 Index, int64 Value);
    bool BindFloat(int32 Index, double Value);
    bool BindText(int32 Index, const FString& Value);
    bool BindBlob(int32 Index, const TArray<uint8>& Data);
    bool BindNull(int32 Index);

    // ── Execution ────────────────────────────────────────
    /** Step one row. Returns true while there are rows to read. */
    bool Step();
    void Reset();

    // ── Column readers ───────────────────────────────────
    int64   GetColumnInt(int32 Index) const;
    double  GetColumnFloat(int32 Index) const;
    FString GetColumnText(int32 Index) const;
    TArray<uint8> GetColumnBlob(int32 Index) const;
    EESQLColumnType GetColumnType(int32 Index) const;
    int32   GetColumnCount() const;
    FString GetColumnName(int32 Index) const;

private:
    friend class FESQLDatabase;
    struct sqlite3_stmt* StmtHandle = nullptr;
};
```

---

### 4.4 Subsystem — `UESQLSubsystem`

Follows the same pattern as `UEPFSubsystem` / `UEPFAnalyticsSubsystem`: a
`UGameInstanceSubsystem` that manages database lifecycle and shared runtime SQL services.

The subsystem is **net-mode–aware**: it detects whether it's running as a
dedicated server, listen server, or standalone/client and adjusts path
resolution and authority enforcement accordingly.

```cpp
UCLASS()
class UNREALEXTENDEDSQL_API UESQLSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Database Lifecycle ───────────────────────────────

    /** Open (or create) a database.
        DatabaseName is a logical handle used in all subsequent calls.
        Scope controls path resolution and authority checks.
        Persistence controls whether the DB is on disk (Persistent) or in memory (Session). */
    UFUNCTION(BlueprintCallable, Category = "SQL|Database")
    FESQLQueryResult OpenDatabase(
        const FString& DatabaseName,
        EESQLDatabaseScope Scope = EESQLDatabaseScope::Local,
        EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent,
        const FString& FileName = TEXT("")   // defaults to <DatabaseName>.db (ignored for Session)
    );

    /** Open a per-player database for a specific connected player.
        Only valid on the server. Uses the player's UniqueNetId as the subfolder key.
        Equivalent to OpenDatabase with Scope=PlayerScoped but targeted at a specific controller. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Database", meta = (DisplayName = "Open Player Database"))
    FESQLQueryResult OpenPlayerDatabase(
        const FString& DatabaseName,
        APlayerController* PlayerController,
        const FString& FileName = TEXT("")
    );

    /** Close a previously opened database. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Database")
    void CloseDatabase(const FString& DatabaseName);

    /** Close a player-scoped database for a specific player.
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

    /** Close all player-scoped databases (e.g. on match end).
        If bDeleteFiles is true, also removes the .db files from disk. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Database")
    void CloseAllPlayerDatabases(bool bDeleteFiles = false);

    /** Check if a logical database name is currently open. */
    UFUNCTION(BlueprintPure, Category = "SQL|Database")
    bool IsDatabaseOpen(const FString& DatabaseName) const;

    /** Returns the names of all currently open databases. */
    UFUNCTION(BlueprintPure, Category = "SQL|Database")
    TArray<FString> GetOpenDatabaseNames() const;

    // ── Net-Mode Queries ─────────────────────────────────

    /** Returns true if this instance is running as a dedicated server. */
    UFUNCTION(BlueprintPure, Category = "SQL|Network")
    bool IsDedicatedServer() const;

    /** Returns true if this instance is a listen server (host + player). */
    UFUNCTION(BlueprintPure, Category = "SQL|Network")
    bool IsListenServer() const;

    /** Returns true if this instance has server authority
        (dedicated server OR listen server). */
    UFUNCTION(BlueprintPure, Category = "SQL|Network")
    bool HasServerAuthority() const;

    // ── Synchronous Queries (game-thread) ────────────────

    /** Execute raw SQL synchronously (CREATE TABLE, pragma, etc.). */
    UFUNCTION(BlueprintCallable, Category = "SQL|Query")
    FESQLQueryResult ExecuteSQL(const FString& DatabaseName, const FString& SQL);

    /** Execute SQL with text bindings (positional ?1, ?2, …). */
    UFUNCTION(BlueprintCallable, Category = "SQL|Query")
    FESQLQueryResult ExecuteSQLWithBindings(
        const FString& DatabaseName,
        const FString& SQL,
        const TArray<FString>& Bindings
    );

    /** Execute raw SQL against a specific player's database. Server-only.
        Accepts APlayerController* for convenience (extracts UniqueNetId internally).
        The actual database key is the UniqueNetId, not the PC pointer. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Query")
    FESQLQueryResult ExecutePlayerSQL(
        const FString& DatabaseName,
        APlayerController* PlayerController,
        const FString& SQL
    );

    // ── Async Queries (background thread → game thread) ──

    /** Execute SQL asynchronously; fires OnComplete on the game thread. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Async")
    void AsyncExecuteSQL(
        const FString& DatabaseName,
        const FString& SQL,
        const FOnESQLQueryCompleteCallback& OnComplete
    );

    // ── Semantic Helpers (no raw SQL) ────────────────────

    /** Create a table if it doesn't exist. Columns specified as Name:Type pairs.
        Type can be: TEXT, INTEGER, REAL, BLOB.
        PrimaryKeyColumn: if specified, that column becomes PRIMARY KEY. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Table")
    FESQLQueryResult CreateTable(
        const FString& DatabaseName,
        const FString& TableName,
        const TMap<FString, FString>& Columns,
        const FString& PrimaryKeyColumn = TEXT(""),
        bool bIfNotExists = true
    );

    /** Drop a table. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Table")
    FESQLQueryResult DropTable(
        const FString& DatabaseName,
        const FString& TableName
    );

    /** Check if a table exists in the database. */
    UFUNCTION(BlueprintPure, Category = "SQL|Table")
    bool DoesTableExist(const FString& DatabaseName, const FString& TableName);

    /** Insert a single row. Keys = column names, Values = data. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult InsertRow(
        const FString& DatabaseName,
        const FString& TableName,
        const TMap<FString, FString>& ColumnValues
    );

    /** Insert multiple rows inside a single transaction. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult InsertRows(
        const FString& DatabaseName,
        const FString& TableName,
        const TArray<FESQLRow>& Rows
    );

    /** Insert or update a row based on a conflict column (UPSERT).
        If a row with the same ConflictColumn value exists, it is updated.
        Otherwise, a new row is inserted.
        Uses: INSERT ... ON CONFLICT(ConflictColumn) DO UPDATE SET ... */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult UpsertRow(
        const FString& DatabaseName,
        const FString& TableName,
        const TMap<FString, FString>& ColumnValues,
        const FString& ConflictColumn
    );

    /** Select rows matching optional WHERE clause. Empty Where = select all. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult SelectRows(
        const FString& DatabaseName,
        const FString& TableName,
        const FString& WhereClause = TEXT(""),
        const TArray<FString>& Bindings = TArray<FString>(),
        int32 Limit = -1  // -1 = use DefaultMaxResultLimit from settings, 0 = no limit
    );

    /** Update rows matching WHERE clause. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult UpdateRows(
        const FString& DatabaseName,
        const FString& TableName,
        const TMap<FString, FString>& SetValues,
        const FString& WhereClause,
        const TArray<FString>& Bindings = TArray<FString>()
    );

    /** Delete rows matching WHERE clause. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Row")
    FESQLQueryResult DeleteRows(
        const FString& DatabaseName,
        const FString& TableName,
        const FString& WhereClause,
        const TArray<FString>& Bindings = TArray<FString>()
    );

    /** Count rows (optionally filtered). */
    UFUNCTION(BlueprintPure, Category = "SQL|Row")
    int32 CountRows(
        const FString& DatabaseName,
        const FString& TableName,
        const FString& WhereClause = TEXT(""),
        const TArray<FString>& Bindings = TArray<FString>()
    );

    // ── Transaction Control ──────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
    bool BeginTransaction(const FString& DatabaseName);

    UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
    bool CommitTransaction(const FString& DatabaseName);

    UFUNCTION(BlueprintCallable, Category = "SQL|Transaction")
    bool RollbackTransaction(const FString& DatabaseName);

    // ── Snapshot API (for Session databases) ───────────

    /** Save the current state of a Session database to a named slot on disk.
        Uses SQLite's online backup API — safe to call while the DB is live.
        The snapshot file is written to Saved/Databases/Snapshots/<DatabaseName>/<SlotName>.db */
    UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
    FESQLQueryResult SaveSnapshot(
        const FString& DatabaseName,
        const FString& SlotName,
        const FString& DisplayName = TEXT("")   // optional user-facing label
    );

    /** Load a previously saved snapshot into a Session database, replacing
        all current in-memory data. The snapshot file is not deleted. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
    FESQLQueryResult LoadSnapshot(
        const FString& DatabaseName,
        const FString& SlotName
    );

    /** Delete a snapshot file from disk. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
    FESQLQueryResult DeleteSnapshot(
        const FString& DatabaseName,
        const FString& SlotName
    );

    /** Returns metadata for all snapshots that exist for a given database name. */
    UFUNCTION(BlueprintCallable, Category = "SQL|Snapshot")
    TArray<FESQLSnapshotInfo> GetAllSnapshots(const FString& DatabaseName);

    /** Check if a specific snapshot slot exists on disk. */
    UFUNCTION(BlueprintPure, Category = "SQL|Snapshot")
    bool DoesSnapshotExist(const FString& DatabaseName, const FString& SlotName);

    /** Returns true if the named database is a Session (in-memory) database. */
    UFUNCTION(BlueprintPure, Category = "SQL|Snapshot")
    bool IsSessionDatabase(const FString& DatabaseName) const;

    // ── Delegates ────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "SQL")
    FOnESQLQueryComplete OnQueryComplete;

    UPROPERTY(BlueprintAssignable, Category = "SQL")
    FOnESQLQueryComplete OnQueryError;

    /** Fired (server-side only) when a player-scoped database is opened */
    UPROPERTY(BlueprintAssignable, Category = "SQL|Multiplayer")
    FOnESQLPlayerDBEvent OnPlayerDatabaseOpened;

    /** Fired (server-side only) when a player-scoped database is closed */
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
    TMap<FString, EESQLDatabasePersistence> DatabasePersistence;

    FString ResolveSnapshotPath(const FString& DatabaseName, const FString& SlotName) const;

    TSharedPtr<FESQLDatabase> GetDatabase(const FString& DatabaseName) const;
    TSharedPtr<FESQLDatabase> GetPlayerDatabase(const FString& PlayerId, const FString& DatabaseName) const;
    FString ResolveDatabasePath(const FString& FileName, EESQLDatabaseScope Scope, const FString& PlayerId = TEXT("")) const;
    /** Extracts UniqueNetId from a PC (via its PlayerState). This is the
        canonical way to resolve a player identity — decoupled from component ownership.
        In PIE without online subsystem, falls back to "LOCAL_<PlayerIndex>"
        using GPlayInEditorID to avoid collisions between simulated players. */
    FString GetPlayerIdFromController(APlayerController* PC) const;
    bool ValidateAuthority(EESQLDatabaseScope Scope, const FString& CallerName) const;

    // ── Auto Player DB Hooks ─────────────────────────────
    void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
    void OnLogout(AGameModeBase* GameMode, AController* Exiting);
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;
};
```

---

### 4.5 Player Data — `UESQLPlayerDBComponent`

An `UActorComponent` that lives on **`APlayerState`** to give each connected
player their own database handle. This is the **per-player Blueprint-side
entry point** in a multiplayer session.

> **Why PlayerState and not PlayerController?**
>
> This is a deliberate framework-level decision:
>
> | Concern | PlayerController | PlayerState |
> |---|---|---|
> | Seamless travel | ❌ Destroyed & recreated | ✅ Persists across travel |
> | GameMode hooks | `PostLogin(PC*)` — direct | One hop via `PC->PlayerState` |
> | Server presence | Server + owning client | Replicated to all (but component is not) |
>
> Since this is a **reusable plugin**, we must handle seamless travel out of
> the box. The component lives on PlayerState so the database handle survives
> map transitions without the consuming project needing any special handling.
>
> The **subsystem API** still accepts `APlayerController*` for convenience
> (because that's what `PostLogin` / `Logout` hand you). Internally it just
> extracts the `UniqueNetId` — the PC is never stored as a key.

> The component is **server-side only** — `bReplicates` is set to `false`.
> On clients it is inert (no database file is created).

Current responsibility:

- player database ownership
- player identity
- lifecycle/state
- acting as context for player-scoped typed table workflows

It should not be a query-builder API.

```cpp
/**
 * Attach to a PlayerState (server-side) to give that player
 * their own SQLite database scoped by UniqueNetId.
 *
 * Lives on PlayerState (not PlayerController) so the database
 * handle survives seamless travel without any special teardown/reopen logic.
 *
 * Typical setup:
 *   1. Override AGameModeBase::GetPlayerStateClass() to return your
 *      custom APlayerState subclass that has this component, OR
 *   2. In your GameMode Blueprint PostLogin, add the component to
 *      NewPlayer->PlayerState dynamically.
 *      — or configure bAutoOpenPlayerDatabases in Project Settings and
 *        the subsystem does it for you via the subsystem API (no component needed).
 *   3. Use the component as player-db ownership/state metadata while
 *      higher-level player workflows are handled through table/id APIs.
 *
 * The subsystem API (OpenPlayerDatabase, ExecutePlayerSQL, etc.) takes
 * APlayerController* for convenience — both pathways resolve to the
 * same UniqueNetId-keyed database file.
 */
UCLASS(ClassGroup=(SQL), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDSQL_API UESQLPlayerDBComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UESQLPlayerDBComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── State ────────────────────────────────────────────

    /** The UniqueNetId string for this player (extracted from owning PlayerState) */
    UFUNCTION(BlueprintPure, Category = "SQL|Player")
    FString GetPlayerId() const;

    /** True if this component's database is currently open. */
    UFUNCTION(BlueprintPure, Category = "SQL|Player")
    bool IsPlayerDatabaseOpen() const;

    /** Returns the owning PlayerState (convenience cast). */
    UFUNCTION(BlueprintPure, Category = "SQL|Player")
    APlayerState* GetOwningPlayerState() const;

    // ── Config ───────────────────────────────────────────

    /** The logical database name to create for this player.
        Defaults to "PlayerData". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL|Player")
    FString PlayerDatabaseName = TEXT("PlayerData");

private:
    FString CachedPlayerId;
    TWeakObjectPtr<UESQLSubsystem> SQLSubsystem;

    /** Resolve the UniqueNetId from the owning PlayerState. */
    FString ResolvePlayerId() const;

    /** Get the internal DB key used for the player databases map. */
    FString GetPlayerDBKey() const;
};
```

---

### 4.6 Current Blueprint Bridge Direction

The old generic async action layer and the old generic Blueprint helper library
were intentionally removed.

Current direction:

- keep the Blueprint surface narrow while the typed C++ layer is being built
- let `UESQLTableAsset` define the main schema-backed runtime contract
- let `FESQLId` remain the common row reference type
- add future async or K2 surfaces only as wrappers over the typed table layer

This prevents the Blueprint layer from drifting away from the actual C++ API.

### 4.7 Current Blueprint Utility Surface

The generic `UESQLBlueprintLibrary` direction is no longer the intended public story.

Current utility guidance is:

- small id-centric helpers are acceptable
- generic row-map parsing helpers should not become the main user surface
- typed row workflows should point back to `UESQLTableAsset`
- future K2 nodes should consume the typed table layer rather than reintroduce generic SQL wrappers

---

### 4.8 SQL Table Asset — `UESQLTableAsset`

A **custom asset type** that looks and feels like a DataTable in the editor but is
backed by a SQLite database file instead of UE's serialization system. This is
NOT a subclass of `UDataTable` — it's a completely separate `UObject` that
mirrors the DataTable editor UX.

This is also the main typed runtime contract for both C++ and future Blueprint/K2 workflows.

> **Design philosophy:** A DataTable lets you define rows via a `UScriptStruct`.
> A SQL Table Asset does the same thing — but the rows live in a `.db` file
> instead of a binary `.uasset`, making them queryable, mutable at runtime,
> and compatible with the entire SQLite ecosystem.

#### Struct Validation

Not every `UScriptStruct` maps cleanly to SQLite. Before creating a SQL Table
Asset, the struct is validated:

| UE Property Type | SQLite Column Type | Allowed? |
|---|---|---|
| `bool` | `INTEGER` (0/1) | ✅ |
| `int32`, `int64` | `INTEGER` | ✅ |
| `uint8` (including `UENUM`) | `INTEGER` | ✅ |
| `float`, `double` | `REAL` | ✅ |
| `FString` | `TEXT` | ✅ |
| `FName` | `TEXT` | ✅ |
| `FText` | `TEXT` (JSON: `{"key":"","ns":"","src":""}`) | ✅ — stored as JSON, displayed as FText in editor |
| `FDateTime` | `TEXT` (ISO 8601) | ✅ |
| `FGuid` | `TEXT` | ✅ |
| `TArray<T>` | — | ❌ Not supported |
| `TMap<K,V>` | — | ❌ Not supported |
| `TSet<T>` | — | ❌ Not supported |
| Nested `USTRUCT` | — | ❌ Not supported |
| `UObject*` / `TSoftObjectPtr` | — | ❌ Not supported |
| `FVector`, `FRotator`, etc. | — | ❌ Not supported (use separate X/Y/Z fields) |
| Delegates | — | ❌ Not supported |

If any field is unsupported, the **Validation Dialog** opens with a clear report:

```
┌──────────────────────────────────────────────────────────────┐
│    ⚠  SQL Table Validation — FMyItemStruct                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ✅  ItemId          FName         → TEXT                    │
│  ✅  DisplayName     FString       → TEXT                    │
│  ✅  Price           int32         → INTEGER                 │
│  ✅  Weight          float         → REAL                    │
│  ❌  Tags            TArray<FName> → NOT SUPPORTED           │
│       Reason: Array types cannot be mapped to a single       │
│       SQLite column. Split into a separate table or use      │
│       a comma-separated TEXT field.                           │
│  ❌  SpawnOffset     FVector       → NOT SUPPORTED           │
│       Reason: Compound struct. Use separate float fields     │
│       (SpawnOffsetX, SpawnOffsetY, SpawnOffsetZ).            │
│                                                              │
│  2 of 6 fields are not compatible.                           │
│  Fix the struct and try again.                               │
│                                                              │
│                               [ OK ]                         │
└──────────────────────────────────────────────────────────────┘
```

#### `ESQLStructValidator.h` (excerpt)

```cpp
/**
 * Validates a UScriptStruct for SQLite compatibility.
 * Used by the editor module before creating a UESQLTableAsset,
 * and available at runtime for programmatic checks.
 */
class UNREALEXTENDEDSQL_API FESQLStructValidator
{
public:

    /** Result for a single field. */
    struct FFieldResult
    {
        FString FieldName;
        FString UETypeName;          // "int32", "TArray<FName>", etc.
        FString SQLiteType;          // "INTEGER", "TEXT", or "" if invalid
        bool bIsValid;
        FString ErrorReason;         // Empty if valid
    };

    /** Validate all fields of a struct. Returns true if ALL fields are valid. */
    static bool Validate(
        const UScriptStruct* Struct,
        TArray<FFieldResult>& OutResults
    );

    /** Map a single FProperty to a SQLite type string. Returns empty string if unsupported. */
    static FString MapPropertyToSQLiteType(const FProperty* Property);

    /** Returns the SQL column definition string for a validated struct.
        e.g. "ItemId TEXT, DisplayName TEXT, Price INTEGER, Weight REAL" */
    static FString BuildColumnDefinition(const UScriptStruct* Struct);
};
```

#### `ESQLTableAsset.h` (excerpt)

```cpp
/**
 * Schema-owning typed table contract for ExtendedSQL.
 *
 * This asset owns table identity, schema sync, primary-key behavior,
 * label behavior, and the typed row load/save surface used by both C++
 * and higher-level Blueprint/K2 workflows.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDSQL_API UESQLTableAsset : public UObject
{
    GENERATED_BODY()

public:

    // ── Asset Metadata ──────────────────────────────────

    /** The struct that defines the row schema.
        Set once during asset creation. The editor auto-migrates the
        DB schema if the struct changes (add/remove columns). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SQL Table")
    const UScriptStruct* RowStruct = nullptr;

    /** The database this table lives in (logical name, same as OpenDatabase).
        Defaults to "EditorData". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
    FString DatabaseName = TEXT("EditorData");

    /** The SQL table name inside the database.
        Defaults to the struct name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
    FString TableName;

    /** Which column serves as the PRIMARY KEY in SQLite.
        This column is managed by the system — it does NOT need to exist
        in the RowStruct. It is auto-added as the first column.
        Defaults to "RowName" (TEXT, auto-generated if left empty on insert).
        Query results always include this column alongside the struct fields. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
    FString PrimaryKeyColumn = TEXT("RowName");

    /** Database scope — controls file path and authority. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
    EESQLDatabaseScope Scope = EESQLDatabaseScope::Local;

    /** Persistence mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SQL Table")
    EESQLDatabasePersistence Persistence = EESQLDatabasePersistence::Persistent;

    // ── Runtime API ─────────────────────────────────────

    /** Open the underlying database, ensure the table exists,
        and auto-migrate schema if the struct has changed.
        Calls SyncSchema() internally. */
    UFUNCTION(BlueprintCallable, Category = "SQL Table")
    FESQLQueryResult Initialize(UObject* WorldContextObject);

    /** Insert a row from a struct instance. Uses reflection to extract values.
        Blueprint-accessible via CustomThunk — accepts any UStruct as input pin.
        Internally uses DECLARE_FUNCTION(execInsertRowFromStruct) with
        Stack.MostRecentPropertyAddress to read the struct data at runtime. */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "SQL Table",
        meta = (CustomStructureParam = "RowData"))
    FESQLQueryResult InsertRowFromStruct(const int32& RowData);
    DECLARE_FUNCTION(execInsertRowFromStruct);

    /** Query all rows, returned as FESQLQueryResult.
        The PrimaryKeyColumn is always included as the first column in results.
        MaxRows: 0 = unlimited (developer's responsibility to manage memory). */
    UFUNCTION(BlueprintCallable, Category = "SQL Table")
    FESQLQueryResult GetAllRows(int32 MaxRows = 0);

    /** Query a single row by this table's primary key value. */
    UFUNCTION(BlueprintCallable, Category = "SQL Table")
    FESQLQueryResult GetRowById(UObject* WorldContextObject, const FString& RowId);

    // ── Typed C++ Runtime API ───────────────────────────

    FESQLQueryResult LoadRowIntoStruct(UObject* WorldContextObject, const FString& RowId, void* OutStructData, const UScriptStruct* StructType);
    FESQLQueryResult LoadRowIntoStruct(UObject* WorldContextObject, const FESQLId& SqlId, void* OutStructData, const UScriptStruct* StructType);
    bool DoesRowExist(UObject* WorldContextObject, const FString& RowId, FString* OutError = nullptr);
    bool DoesRowExist(UObject* WorldContextObject, const FESQLId& SqlId, FString* OutError = nullptr);
    FESQLQueryResult DeleteRowById(UObject* WorldContextObject, const FString& RowId);
    FESQLQueryResult DeleteRowById(UObject* WorldContextObject, const FESQLId& SqlId);
    FESQLQueryResult SaveRowFromStruct(UObject* WorldContextObject, const void* StructData, const UScriptStruct* StructType, FString* OutResolvedRowId = nullptr, const FString& RowIdOverride = TEXT(""));

    /** Get row count. */
    UFUNCTION(BlueprintPure, Category = "SQL Table")
    int32 GetRowCount();

    // ── Schema Migration ──────────────────────────────────

    /** Compare the current RowStruct fields against the DB table columns.
        Automatically applies migrations:
        - New fields in struct   →  ALTER TABLE ADD COLUMN (with default value)
        - Removed fields         →  ALTER TABLE DROP COLUMN (SQLite 3.35.0+)
          If DROP COLUMN is unavailable (older SQLite), falls back to the
          recreate-table approach (CREATE new → copy data → DROP old → RENAME).
        - PrimaryKeyColumn       →  always preserved, never dropped
        Called automatically by Initialize() and when the editor opens the asset.
        No user confirmation needed — schema is always synced silently.

        The plugin ships its own SQLite amalgamation (version pinned in
        Thirdparty/SQLite/). The version is checked at module startup via
        sqlite3_libversion_number() and logged to LogExtendedSQL. */
    FESQLQueryResult SyncSchema(TSharedPtr<FESQLDatabase> Database);

    // ── Editor Support ──────────────────────────────────

    /** Returns the column definitions derived from the row struct.
        Includes the PrimaryKeyColumn as the first entry. */
    TArray<FESQLColumn> GetColumnDefinitions() const;

    /** Validate the row struct for SQLite compatibility. */
    bool ValidateStruct(TArray<FESQLStructValidator::FFieldResult>& OutResults) const;

    /** Export all rows to a CSV file. Used by the editor toolbar. */
    bool ExportToCSV(const FString& OutputFilePath, FString& OutError) const;

    /** Import rows from a CSV file. Existing rows with matching PrimaryKey are updated,
        new rows are inserted (UPSERT logic). The CSV header row must match column names. */
    bool ImportFromCSV(const FString& InputFilePath, FString& OutError);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
```

---

### 4.9 Editor Module — `UnrealExtendedSQLEditor`

An **editor-only** module (`Type: "Editor"`) that provides:

1. **DataTable-like editor** for `UESQLTableAsset` (row grid, add/delete/edit rows)
2. **Two creation pathways:** right-click Blueprint struct shortcut AND `Add New` with struct picker (for C++ structs)
3. **Struct validation dialog** with clear error reporting
4. **Asset factory & type actions** for Content Browser integration

#### Two Ways to Create a SQL Table Asset

C++ structs don't appear as assets in the Content Browser — you can't right-click
them. So we need **two entry points**:

| Entry Point | Works for | How |
|---|---|---|
| **Right-click on UScriptStruct asset** → "Create SQL Table from Struct" | Blueprint structs only (they're visible in CB) | `FESQLStructMenuExtension` (context menu extender) |
| **Content Browser → Add New → SQL → SQL Table** | ALL structs (C++ AND Blueprint) | `UESQLTableAssetFactory` with struct picker dialog |

> This mirrors exactly how UE's `UDataTable` works: you can either right-click
> a struct asset, or go to `Add New → Miscellaneous → Data Table` and pick
> any struct from a dropdown — including C++ ones like `FMyGameStruct`.

#### UX Flow A: Right-Click Blueprint Struct (shortcut)

```
  Content Browser: Right-click on a Blueprint UScriptStruct asset
       │
       ├─ Context menu shows: "Create SQL Table from Struct"
       │   (added by ESQLStructMenuExtension)
       │
       ├─ FESQLStructValidator::Validate(SelectedStruct)
       │
       ├─ [All fields valid?]
       │   │
       │   ├─ YES → Open "New SQL Table" dialog (struct is pre-filled):
       │   │         - Row Struct:    FMyItemStruct (locked — came from right-click)
       │   │         - Database Name: [EditorData___________]
       │   │         - Table Name:    [MyItems________________]
       │   │         - Scope:         [Local ▼]
       │   │         - Persistence:   [Persistent ▼]
       │   │         - Save path:     [/Game/Data/____________]
       │   │                                    [ Create ]  [ Cancel ]
       │   │
       │   └─ Creates UESQLTableAsset in Content Browser
       │      (internally runs CREATE TABLE in the target .db file)
       │
       └─ NO → Opens SESQLValidationDialog
               (shows field-by-field compatibility report)
               User must fix the struct first
```

#### UX Flow B: Add New → SQL Table (for C++ structs and all)

```
  Content Browser: Right-click empty space → Add New → SQL → SQL Table
       │
       ├─ UESQLTableAssetFactory::ConfigureProperties() fires
       │
       ├─ Opens struct picker dialog (lists ALL registered UScriptStructs):
       │   ┌──────────────────────────────────────────────────────────────┐
       │   │  Pick Row Struct for SQL Table                              │
       │   ├──────────────────────────────────────────────────────────────┤
       │   │  🔍 [Search structs...___________________________]          │
       │   │                                                              │
       │   │  ▸ /Script/MyGame                                            │
       │   │      FMyItemStruct                     ← C++ struct          │
       │   │      FMyQuestData                      ← C++ struct          │
       │   │  ▸ /Script/Engine                                            │
       │   │      FTableRowBase                                           │
       │   │  ▸ /Game/Blueprints/Structs                                  │
       │   │      S_InventorySlot                   ← Blueprint struct    │
       │   │      S_WeaponStats                     ← Blueprint struct    │
       │   │                                                              │
       │   │                               [ Select ]  [ Cancel ]        │
       │   └──────────────────────────────────────────────────────────────┘
       │
       ├─ User selects a struct (e.g. FMyItemStruct — defined in C++)
       │
       ├─ FESQLStructValidator::Validate(FMyItemStruct)
       │
       ├─ [All fields valid?]
       │   │
       │   ├─ YES → Open "New SQL Table" config dialog (same as Flow A)
       │   │
       │   └─ NO → Opens SESQLValidationDialog
       │
       └─ Creates UESQLTableAsset .uasset + CREATE TABLE in .db
```

#### UX Flow: Editing Rows (DataTable-like Editor)

```
  Double-click a UESQLTableAsset in Content Browser
       │
       └─ Opens FESQLTableEditorToolkit
          ┌──────────────────────────────────────────────────────────────┐
          │  SQL Table Editor — MyItems (FMyItemStruct)        [×]     │
          ├──────────────────────────────────────────────────────────────┤
          Database: EditorData  │  Table: MyItems  │  Rows: 47      │
          │           │  [+ Add Row]  [- Delete]  [↻ Refresh]  [⚡ Query]  [⤒ Import CSV]  [⤓ Export CSV] │
          ├──────────────────────────────────────────────────────────────┤
          │  Row#  │ ItemId    │ DisplayName      │ Price │ Weight     │
          │────────┼───────────┼──────────────────┼───────┼────────────│
          │  1     │ sword_01  │ Iron Sword       │ 150   │ 3.5        │
          │  2     │ shield_02 │ Wooden Shield    │ 80    │ 5.0        │
          │  3     │ potion_hp │ Health Potion    │ 25    │ 0.3        │
          │  ...   │           │                  │       │            │
          ├──────────────────────────────────────────────────────────────┤
          │  Click any cell to edit inline. Changes write to .db       │
          │  immediately via parameterized UPDATE.                      │
          └──────────────────────────────────────────────────────────────┘
```

> **Key difference from DataTable:** When you edit a cell, it runs a
> `UPDATE MyItems SET DisplayName = ?1 WHERE rowid = ?2` — the change is
> written to the `.db` file immediately. There's no "save asset" step for row
> data. The `.uasset` itself only stores metadata (struct type + DB path).

#### `FESQLTableEditorToolkit` — Editor Toolkit

```cpp
/**
 * Asset editor toolkit for UESQLTableAsset.
 * Mirrors UE's DataTable editor but reads/writes via SQLite.
 */
class FESQLTableEditorToolkit : public FAssetEditorToolkit
{
public:
    void InitEditor(
        const EToolkitMode::Type Mode,
        const TSharedPtr<IToolkitHost>& InitToolkitHost,
        UESQLTableAsset* InTableAsset
    );

    // FAssetEditorToolkit interface
    virtual FName GetToolkitFName() const override { return FName("ESQLTableEditor"); }
    virtual FText GetBaseToolkitName() const override { return INVTEXT("SQL Table Editor"); }
    virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
    UESQLTableAsset* TableAsset = nullptr;
    TSharedPtr<SESQLTableEditor> TableEditorWidget;

    // Database connection used by the editor (opened on init, closed on destroy)
    TSharedPtr<FESQLDatabase> EditorDatabase;

    void RefreshRows();
    void OnAddRowClicked();
    void OnDeleteRowClicked();
    void OnCellValueCommitted(int32 RowId, const FString& ColumnName, const FString& NewValue);
};
```

#### `SESQLTableEditor` — Row Grid Widget

```cpp
/**
 * Slate widget: a spreadsheet-like row grid for SQL Table editing.
 * Modeled after SDataTableListViewRow but backed by SQLite queries.
 */
class SESQLTableEditor : public SCompoundWidget
{
    SLATE_BEGIN_ARGS(SESQLTableEditor) {}
        SLATE_ARGUMENT(UESQLTableAsset*, TableAsset)
        SLATE_ARGUMENT(TSharedPtr<FESQLDatabase>, Database)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Reload all rows from the database and refresh the list view. */
    void RefreshData();

    /** Delegate: fired when a cell value is committed by the user. */
    FOnCellValueCommitted OnCellValueCommitted;

private:
    TSharedPtr<SHeaderRow> HeaderRow;
    TSharedPtr<SListView<TSharedPtr<FESQLRow>>> ListView;
    TArray<TSharedPtr<FESQLRow>> Rows;

    // Column definitions derived from the RowStruct
    TArray<FESQLColumn> Columns;

    TSharedRef<ITableRow> OnGenerateRow(
        TSharedPtr<FESQLRow> Item,
        const TSharedRef<STableViewBase>& OwnerTable
    );

    void OnCellEdited(int32 RowIndex, const FString& ColumnName, const FString& NewValue);
};
```

#### `ESQLTableAssetFactory` — Asset Factory with Struct Picker

```cpp
/**
 * UFactory for creating UESQLTableAsset from the Content Browser.
 *
 * Appears under: Add New → SQL → SQL Table
 *
 * On creation, shows a struct picker dialog (just like DataTable creation)
 * that lists ALL registered UScriptStructs — including C++ ones.
 * After picking, validates the struct and opens the config dialog.
 */
UCLASS()
class UESQLTableAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UESQLTableAssetFactory();

    // UFactory interface
    virtual UObject* FactoryCreateNew(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn
    ) override;

    virtual bool ShouldShowInNewMenu() const override { return true; }

    /** Called before the "name your asset" step.
        Opens the struct picker + validation. Returns false to cancel. */
    virtual bool ConfigureProperties() override;

    virtual FText GetDisplayName() const override { return INVTEXT("SQL Table"); }
    virtual uint32 GetMenuCategories() const override;

private:
    /** The struct the user picked in the struct picker dialog.
        Set by ConfigureProperties(), used by FactoryCreateNew(). */
    UPROPERTY()
    const UScriptStruct* SelectedStruct = nullptr;

    /** Opens the UE struct picker (same widget DataTable uses).
        Filters to show all UScriptStructs and validates selection. */
    bool ShowStructPicker();

    /** Opens the SQL Table config dialog (DB name, table name, scope, etc.).
        Returns false if user cancels. */
    bool ShowConfigDialog();
};
```

#### `ESQLTableAssetActions` — Content Browser Integration

```cpp
/**
 * Asset type actions for UESQLTableAsset.
 * Controls: thumbnail color, category, right-click actions, double-click behavior.
 */
class FESQLTableAssetActions : public FAssetTypeActions_Base
{
public:
    virtual UClass* GetSupportedClass() const override { return UESQLTableAsset::StaticClass(); }
    virtual FText GetName() const override { return INVTEXT("SQL Table"); }
    virtual FColor GetTypeColor() const override { return FColor(56, 178, 172); }  // Teal
    virtual uint32 GetCategories() override;  // Returns the custom "SQL" category

    virtual void OpenAssetEditor(
        const TArray<UObject*>& InObjects,
        TSharedPtr<IToolkitHost> EditWithinLevelEditor
    ) override;
};
```

#### `ESQLStructMenuExtension` — Right-Click Shortcut (Blueprint Structs)

```cpp
/**
 * Extends the Content Browser context menu for UScriptStruct ASSETS.
 * Adds: "Create SQL Table from Struct"
 *
 * This is a CONVENIENCE SHORTCUT for Blueprint structs that are visible
 * in the Content Browser. For C++ structs (which are not CB assets),
 * use the UESQLTableAssetFactory pathway: Add New → SQL → SQL Table.
 *
 * Registered during UnrealExtendedSQLEditor module startup via
 * FContentBrowserModule::GetAllAssetContextMenuExtenders().
 */
class FESQLStructMenuExtension
{
public:
    static void Register();
    static void Unregister();

private:
    static TSharedRef<FExtender> OnExtendContentBrowserAssetMenu(
        const TArray<FAssetData>& SelectedAssets
    );

    /** The actual menu entry callback. */
    static void ExecuteCreateSQLTable(const FAssetData& StructAsset);

    static FDelegateHandle ContentBrowserExtenderHandle;
};
```

#### `UnrealExtendedSQLEditor.Build.cs`

```csharp
public class UnrealExtendedSQLEditor : ModuleRules
{
    public UnrealExtendedSQLEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Engine",
            "UnrealExtendedSQL"    // Runtime module (UESQLTableAsset, FESQLDatabase, etc.)
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "Slate",
            "SlateCore",
            "UnrealEd",             // FAssetEditorToolkit, FAssetTypeActions
            "ContentBrowser",       // Context menu extenders
            "InputCore",
            "EditorFramework",       // FAppStyle (UE 5.1+ replacement for EditorStyle)
            "ToolMenus",
            "PropertyEditor",       // Details panel integration
            "AssetTools"            // IAssetTools, RegisterAssetTypeActions
        });
    }
}
```

---

## 5. Build Configuration

### `UnrealExtendedSQL.Build.cs`

```csharp
public class UnrealExtendedSQL : ModuleRules
{
    public UnrealExtendedSQL(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Compile the SQLite amalgamation directly as a private source file
        string SQLitePath = Path.Combine(PluginDirectory, "Thirdparty", "SQLite");
        PrivateIncludePaths.Add(SQLitePath);

        // Add sqlite3.c as an additional source file
        // (compiled with the module — avoids a separate External module)
        string SQLiteSource = Path.Combine(SQLitePath, "sqlite3.c");
        PrivateDefinitions.Add("SQLITE_CORE");
        // sqlite3.c is placed in ThirdParty/SQLite/ and compiled as part of this module.
        // UBT will pick it up automatically since it's under a directory added to PrivateIncludePaths.

        PublicIncludePaths.AddRange(new string[] { ModuleDirectory });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Engine",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CoreUObject",
            "UnrealExtendedFramework"   // Optional: only if sharing utilities
        });

        // SQLite compile definitions
        PublicDefinitions.Add("SQLITE_THREADSAFE=1");
        PublicDefinitions.Add("SQLITE_ENABLE_FTS5=1");
        PublicDefinitions.Add("SQLITE_ENABLE_JSON1=1");
        PublicDefinitions.Add("SQLITE_ENABLE_COLUMN_METADATA=1");
    }
}
```

### `.uplugin` additions

```json
{
    "Name": "UnrealExtendedSQL",
    "Type": "Runtime",
    "LoadingPhase": "Default"
},
{
    "Name": "UnrealExtendedSQLEditor",
    "Type": "Editor",
    "LoadingPhase": "Default"
}
```

---

## 6. Threading Model

```
  Blueprint / Game Thread                    Background Thread
  ────────────────────────                   ─────────────────
  UESQLSubsystem::AsyncExecuteSQL()
       │
       ├─ Capture: DatabaseName, SQL, Bindings
       │
       └─ Async(ENamedThreads::AnyBackgroundThreadNormalTask, [=]()
              {
                  FESQLQueryResult Result = Database->Execute(SQL, Bindings);
                  // ↓ marshal back to game thread
                  AsyncTask(ENamedThreads::GameThread, [Result, Callback]()
                  {
                      Callback.ExecuteIfBound(Result);
                  });
              });
```

- **Read operations** (SELECT) → always safe to run on background thread.
- **Write operations** (INSERT/UPDATE/DELETE) → the `FCriticalSection` inside `FESQLDatabase` serializes writes.
- **Synchronous API** also provided for trivial operations (schema creation, small lookups) that don't warrant async overhead.

---

## 7. Multiplayer Architecture

This is the core addition that makes `UnrealExtendedSQL` viable for dedicated
and listen server game sessions.

### 7.1 Net-Mode Detection

The subsystem queries `UGameInstance::GetWorld()->GetNetMode()` during
`Initialize()` and caches the result:

| Net Mode | `HasServerAuthority()` | Can open `Server` scope | Can open `PlayerScoped` | Notes |
|---|---|---|---|---|
| `NM_Standalone` | ✅ | ✅ | ✅ | Singleplayer — everything is local |
| `NM_DedicatedServer` | ✅ | ✅ | ✅ | Headless server — the canonical multiplayer case |
| `NM_ListenServer` | ✅ | ✅ | ✅ | Host is also a player — authority lives here |
| `NM_Client` | ❌ | ❌ (returns error) | ❌ (returns error) | Pure client — may only use `Local` scope |

When `bEnforceServerAuthority` is true (default), any attempt to open a
`Server` or `PlayerScoped` database from `NM_Client` returns
`FESQLQueryResult::Failure("No server authority")` without touching the filesystem.

### 7.2 Database File Layout

```
Saved/
└── Databases/                           ← DefaultDatabaseDirectory
    ├── local_cache.db                   ← Scope::Local + Persistent
    │
    ├── Server/                          ← ServerSubdirectory
    │   ├── WorldState.db                ← Scope::Server + Persistent
    │   └── MatchLog.db                  ← Scope::Server + Persistent
    │
    ├── Players/                         ← PlayerSubdirectory
    │   ├── 76561198012345678/           ← UniqueNetId (Steam example)
    │   │   └── PlayerData.db            ← Scope::PlayerScoped
    │   │
    │   ├── 00000000-ABCD-1234-.../      ← UniqueNetId (EOS example)
    │   │   └── PlayerData.db
    │   │
    │   └── LOCAL_0/                     ← PIE fallback: Player index 0 (no UniqueNetId)
    │   │   └── PlayerData.db
    │   └── LOCAL_1/                     ← PIE fallback: Player index 1
    │       └── PlayerData.db
    │
    └── Snapshots/                       ← Snapshot saves (from Session databases)
        ├── GameProgress/                ← DatabaseName that was snapshotted
        │   ├── AutoSave.db              ← SlotName = "AutoSave"
        │   ├── AutoSave.meta.json       ← { displayName, timestamp, fileSizeBytes }
        │   ├── Slot_01.db               ← SlotName = "Slot_01"
        │   ├── Slot_01.meta.json
        │   ├── Slot_02.db
        │   └── Slot_02.meta.json
        │
        └── MatchDraft/                  ← Another database's snapshots
            ├── Round1.db
            └── Round1.meta.json
```

> **Note:** Session databases (in-memory) have NO file in the directory tree above.
> They exist only in RAM. The `Snapshots/` folder is the only disk footprint for
> Session databases, and only when the user explicitly calls `SaveSnapshot()`.

### 7.3 Player Lifecycle Flow

```
  AGameModeBase::PostLogin(NewPlayer)
       │
       ├─ Subsystem listens via FGameModeEvents::GameModePostLoginEvent
       │
       ├─ Extract UniqueNetId from NewPlayer->GetPlayerState()->GetUniqueId()
       │
       ├─ if (bAutoOpenPlayerDatabases)
       │   ├─ Resolve path:  Saved/Databases/Players/<UniqueNetId>/PlayerData.db
       │   ├─ FESQLDatabase::Open(...)
       │   ├─ Store in PlayerDatabases[UniqueNetId]["PlayerData"]
       │   └─ Broadcast OnPlayerDatabaseOpened
       │
       └─ (or) Blueprint calls OpenPlayerDatabase() manually in PostLogin


  AGameModeBase::Logout(Exiting)
       │
       ├─ Subsystem listens via FGameModeEvents::GameModeLogoutEvent
       │
       ├─ Find all databases for this player in PlayerDatabases
       │
       ├─ For each:  CommitTransaction → Close → (optionally delete file)
       │
       ├─ Remove from PlayerDatabases map
       │
       └─ Broadcast OnPlayerDatabaseClosed
```

### 7.4 Server-Authoritative Design Principles

| Principle | Implementation |
|---|---|
| **Server owns the data** | All `Server` and `PlayerScoped` database files physically exist on the server machine only. Clients never read or write directly. |
| **No replication of DB handles** | `FESQLDatabase` is not a `UObject` and is not replicated. The server queries the database and sends results to clients via normal RPC / replication. |
| **Blueprint authority guard** | `ValidateAuthority()` is called before every operation on `Server` or `PlayerScoped` databases. If the caller lacks authority, the result is an immediate error — no file I/O occurs. |
| **Player isolation** | Each player's database lives in its own subdirectory. There is no cross-player query API — the server must explicitly query two different player databases and join the results in C++/Blueprint if needed. |
| **Graceful disconnect** | On `Logout`, player databases are flushed and closed. If `bDeletePlayerDBOnDisconnect` is true, the file is removed (ideal for ephemeral match data). If false, it persists (ideal for persistent worlds). |
| **Listen server dual-role** | On a listen server, the host player is treated identically to a remote player — they get their own `PlayerScoped` database *and* can access `Server` databases. The subsystem uses the local player's UniqueNetId (not a hardcoded "host" key). |

### 7.5 Typical Multiplayer Patterns

#### Pattern A — Shared World State (Dedicated Server)
```
  GameMode BeginPlay  (RUNS ON SERVER ONLY)
    │
    ├─ SQL: Open Database
    │    DatabaseName = "WorldState"
    │    Scope        = Server
    │
    ├─ SQL: Create Table
    │    TableName = "Territories"
    │    Columns   = { "Name": "TEXT", "OwnerId": "TEXT", "CapturedAt": "TEXT" }
    │
    └─ SQL: Insert Row
         TableName    = "Territories"
         ColumnValues = { "Name": "Castle", "OwnerId": "", "CapturedAt": "" }

  When player captures territory  (SERVER RPC):
    │
    └─ SQL: Update Rows
         DatabaseName = "WorldState"
         TableName    = "Territories"
         SetValues    = { "OwnerId": PlayerNetId, "CapturedAt": Now() }
         WhereClause  = "Name = ?1"
         Bindings     = [ "Castle" ]
```

#### Pattern B — Per-Player Inventory (Dedicated / Listen Server)
```
  GameMode PostLogin  (RUNS ON SERVER ONLY)
    │
    ├─ SQL: Open Player Database
    │    DatabaseName    = "Inventory"
    │    PlayerController = NewPlayer
    │
    └─ SQL: Create Table (on player's DB)
         TableName = "Items"
         Columns   = { "ItemId": "TEXT", "Quantity": "INTEGER", "SlotIndex": "INTEGER" }

  Player picks up item  (SERVER):
    │
    └─ SQL: Execute Player SQL
         DatabaseName    = "Inventory"
         PlayerController = PickingUpPlayer
         SQL = "INSERT INTO Items (ItemId, Quantity, SlotIndex) VALUES (?1, ?2, ?3)"

  Player disconnects → Logout:
    │
    └─ Subsystem auto-closes Inventory.db
       (file persists on disk for next login if bDeletePlayerDBOnDisconnect = false)
```

#### Pattern C — Listen Server Local + Server Hybrid
```
  ListenServer host:
    ├─ Opens "MatchLog" with Scope::Server    → server-side match history
    ├─ Opens "Settings" with Scope::Local      → host's personal settings (client-side)
    └─ Auto-opens PlayerData for all players   → per-player progress

  Remote client:
    └─ Opens "Settings" with Scope::Local      → that client's personal settings
       (cannot touch Server or PlayerScoped — authority check fails)
```

---

## 8. Persistence & Snapshot Model

Databases support two persistence modes, controlled by `EESQLDatabasePersistence`:

### 8.1 Persistent Mode (default)

```
  OpenDatabase("WorldState", Server, Persistent)
       │
       └─ Creates/opens: Saved/Databases/Server/WorldState.db
          │
          ├─ File exists on disk at all times
          ├─ Survives game restarts, crashes, reboots
          ├─ All writes go directly to the .db file (via WAL journal)
          └─ CloseDatabase() just closes the handle — file stays
```

**Use for:** Server world state, analytics, mod databases, permanent player data, anything that should "just be there" on next launch.

### 8.2 Session Mode

```
  OpenDatabase("GameProgress", Local, Session)
       │
       └─ Creates:  SQLite :memory: database (no file on disk)
          │
          ├─ All queries read/write RAM — extremely fast
          ├─ Game closes → data is GONE (by design)
          ├─ Game crashes → data is GONE (by design)
          │
          ├─ SaveSnapshot("GameProgress", "AutoSave")
          │    └─ sqlite3_backup: memory → Saved/Databases/Snapshots/GameProgress/AutoSave.db
          │       + writes AutoSave.meta.json with { displayName, timestamp, fileSize }
          │
          ├─ SaveSnapshot("GameProgress", "Slot_01", "Chapter 3 — Before Boss")
          │    └─ sqlite3_backup: memory → Saved/Databases/Snapshots/GameProgress/Slot_01.db
          │
          └─ LoadSnapshot("GameProgress", "Slot_01")
               └─ sqlite3_backup: file → memory (current data replaced entirely)
```

**Use for:** Save-slot workflows, temporary match data, draft/undo systems, any scenario where the player explicitly decides when to save.

### 8.3 How `sqlite3_backup` Works Under the Hood

SQLite provides a built-in [Online Backup API](https://www.sqlite.org/backup.html) that copies a database page-by-page:

| Operation | Source | Destination | Safe while live? |
|---|---|---|---|
| `SaveSnapshot` | `:memory:` (session DB) | file on disk (`.db`) | ✅ Yes — reads are consistent |
| `LoadSnapshot` | file on disk (`.db`) | `:memory:` (session DB) | ✅ Yes — overwrites atomically |

- No manual serialisation needed — the backup API handles locking internally.
- The current implementation copies all pages in one call via `sqlite3_backup_step(..., -1)` inside `FESQLDatabase::SaveToFile()` / `LoadFromFile()`.
- Incremental page-stepped snapshot copying is still future work if large session databases ever need hitch reduction.

### 8.4 Snapshot Metadata (`.meta.json`)

Each snapshot has a sidecar JSON file storing Blueprint-friendly metadata:

```json
{
    "slotName": "Slot_01",
    "displayName": "Chapter 3 — Before Boss",
    "timestamp": "2026-04-09T22:30:00Z",
    "fileSizeBytes": 524288
}
```

This is what `GetAllSnapshots()` returns as `TArray<FESQLSnapshotInfo>` — so Blueprints can display a save-slot UI (name, date, file size) without opening any database.

### 8.5 Blueprint Example — Session + Save Slots

```
Event BeginPlay
  │
  ├─ SQL: Open Database
  │    DatabaseName = "GameProgress"
  │    Scope        = Local
  │    Persistence  = Session          ← in-memory, no file
  │
  ├─ SQL: Does Snapshot Exist ("GameProgress", "AutoSave")
  │    │
  │    └─ [Yes] → SQL: Load Snapshot ("GameProgress", "AutoSave")
  │                  (restores all tables/data from last auto-save)
  │
  └─ SQL: Create Table (if not exists)
       DatabaseName = "GameProgress"
       TableName    = "QuestLog"
       Columns      = { "QuestId": "TEXT", "State": "TEXT", "Progress": "INTEGER" }


Event: Player reaches checkpoint
  │
  └─ SQL: Save Snapshot
       DatabaseName = "GameProgress"
       SlotName     = "AutoSave"
       DisplayName  = "Checkpoint — Village Square"


Event: Player opens Save menu and picks "Save to Slot 1"
  │
  └─ SQL: Save Snapshot
       DatabaseName = "GameProgress"
       SlotName     = "Slot_01"
       DisplayName  = "Manual Save — Before Final Boss"


Event: Player opens Load menu
  │
  ├─ SQL: Get All Snapshots ("GameProgress")
  │    → Returns array: [ {Slot_01, "Manual Save...", 2026-04-09, 512KB}, {AutoSave, ...} ]
  │    → Populate UI list
  │
  └─ Player selects "Slot_01" → SQL: Load Snapshot ("GameProgress", "Slot_01")
       (in-memory database is fully replaced with Slot_01's data)


Event EndPlay / Game Closing
  │
  └─ (optional) SQL: Save Snapshot ("GameProgress", "AutoSave")
     Session database is then discarded — no leftover file
```

### 8.6 Persistent vs Session — Quick Reference

| | **Persistent** | **Session** |
|---|---|---|
| Storage | File on disk (`.db`) | SQLite `:memory:` |
| Survives game close | ✅ Yes | ❌ No |
| Survives crash | ✅ Yes (WAL journal) | ❌ No |
| Performance | Fast (memory-mapped I/O) | Fastest (pure RAM) |
| Save/Load slots | N/A — data is always live | ✅ via `SaveSnapshot` / `LoadSnapshot` |
| Disk footprint | Always present | Only when snapshots are saved |
| Multiplayer | Works with all scopes | Typically `Local` scope (client-side) |
| `OpenDatabase` param | `Persistence::Persistent` | `Persistence::Session` |

### 8.7 Version Control & Database Granularity

#### The Problem

`.db` files are **binary**. Git/Perforce can't diff or 3-way merge them.
If two developers edit the same `.db`, it's an unresolvable binary conflict.

#### The Solution: `.sqldump` as VCS Source of Truth

The `.db` file is a **build artifact** — like a `.obj` file. It is never
committed to version control. The canonical source of truth is a `.sqldump`
text file that Git can diff, blame, and 3-way merge.

```
  Version-controlled (Git/Perforce):      NOT version-controlled (.gitignore):
  ─────────────────────────────────       ──────────────────────────────────
  DT_Items.uasset                         Items.db          ← generated
  DT_Items.sqldump                        Items.db-wal      ← SQLite WAL
  DT_Quests.uasset                        Quests.db         ← generated
  DT_Quests.sqldump                       Quests.db-wal
```

#### `.sqldump` Format

A valid, self-contained SQL script. Sorted by PrimaryKey for deterministic diffs:

```sql
-- UnrealExtendedSQL v1.0
-- Asset: DT_Items | Struct: FMyItemStruct | PrimaryKey: RowName
-- Generated: 2026-04-10T00:30:00Z — DO NOT EDIT MANUALLY

CREATE TABLE IF NOT EXISTS Items (
  RowName TEXT PRIMARY KEY,
  ItemId TEXT NOT NULL,
  DisplayName TEXT,
  Price INTEGER DEFAULT 0,
  Weight REAL DEFAULT 0.0
);

INSERT INTO Items VALUES ('potion_hp', 'potion_hp', '{"key":"","ns":"","src":"Health Potion"}', 25, 0.3);
INSERT INTO Items VALUES ('shield_02', 'shield_02', '{"key":"","ns":"","src":"Wooden Shield"}', 80, 5.0);
INSERT INTO Items VALUES ('sword_01', 'sword_01', '{"key":"","ns":"","src":"Iron Sword"}', 150, 3.5);
```

#### How It Works

```
    Editor saves a SQL Table Asset row change
       │
       ├─ Export full .sqldump FIRST (crash-safe: text truth is updated before binary)
       └─ Write change to .db (for immediate use)

  Editor opens a UESQLTableAsset
       │
       ├─ Compare timestamps: .db vs .sqldump
       │
       ├─ .sqldump is newer (VCS pull / merge happened)
       │   └─ Rebuild .db from .sqldump
       │
       ├─ .db is newer (external edit: DB Browser, Python, etc.)
       │   └─ Re-export .sqldump from .db
       │      (external changes are captured into VCS-tracked text)
       │
       └─ Same timestamp or .db missing → Rebuild .db from .sqldump

  Git pull / merge
       │
       ├─ .sqldump files are merged (standard text 3-way merge)
       │   Dev A changed price on line 12, Dev B added row on line 15 → both apply
       └─ .db files are not in the repo — nothing to conflict

  Packaging / cook integration
       │
      ├─ `FESQLDumpPipeline::RebuildDatabaseFromDump()` can rebuild a clean `.db`
      │   from a `.sqldump`
      ├─ automatic cook-time discovery and staging is not wired globally yet
      └─ projects that need packaged SQL table databases should invoke that
         helper from their cook or prebuild pipeline
```

#### Merge Example

```
  Dev A: changes Iron Sword price from 150 → 200
  Dev B: adds a new "Steel Axe" row

  .sqldump diff A:
    - INSERT INTO Items VALUES ('sword_01', 'sword_01', ..., 150, 3.5);
    + INSERT INTO Items VALUES ('sword_01', 'sword_01', ..., 200, 3.5);

  .sqldump diff B:
    + INSERT INTO Items VALUES ('axe_01', 'axe_01', ..., 120, 4.0);

  Git 3-way merge: ✅ clean — different lines, no conflict
```

#### Database Granularity

The `DatabaseName` field on `UESQLTableAsset` controls how many `.db` files
are created. Each unique `DatabaseName` = one `.db` + one `.sqldump`:

| Strategy | DatabaseName | Result | VCS conflict surface |
|---|---|---|---|
| **Per-asset** (recommended) | Each asset has a unique name | `Items.db`, `Quests.db`, `NPCs.db` | Minimal — different tables = different files |
| **Shared** | All assets use `"GameData"` | Single `GameData.db` | Larger — any table edit touches same `.sqldump` |

> **Recommendation:** Use one database per SQL Table Asset for editor content.
> This gives maximum merge safety. That isolation is not automatic today: new
> assets keep the class default `DatabaseName = "EditorData"`, so per-asset
> layouts require authors or the creation flow to assign unique names.
>
> Automatic `.sqldump` sync is currently wired for SQL Table Assets through the
> table-asset editor flow. Live runtime databases (player saves, match logs,
> server analytics) do NOT auto-export `.sqldump` files — the I/O cost of
> regenerating a full text dump on every write would be too high for normal
> runtime use. If you need a text dump there, use the export helpers manually.

#### What About External `.db` Edits?

If someone opens a `.db` in DB Browser for SQLite or a Python script,
the editor detects the change on next open (`.db` has a newer timestamp
than `.sqldump`) and **re-exports the `.sqldump` from the `.db`**.

This means external edits flow into the `.sqldump` automatically —
the next `git commit` will include those changes as readable text diffs.

| What happened | `.db` vs `.sqldump` | Editor action |
|---|---|---|
| Git pull / merge | `.sqldump` is newer | Rebuild `.db` from `.sqldump` |
| External tool edited `.db` | `.db` is newer | Re-export `.sqldump` from `.db` |
| Normal editor save | Same timestamp | Both already in sync |
| `.db` missing (first clone) | Only `.sqldump` exists | Rebuild `.db` from `.sqldump` |

### 8.8 Cook Pipeline (Packaging SQL Table Assets)

The plugin already has the **rebuild primitive** needed for packaging authored
table databases, but the final cook/staging integration is not fully wired yet.

| Piece | Current status | Notes |
|---|---|---|
| Rebuild `.db` from `.sqldump` | ✅ Implemented | `FESQLDumpPipeline::RebuildDatabaseFromDump()` creates a fresh database from the text source of truth. |
| Editor open-time sync | ✅ Implemented | `UESQLTableAsset::SyncDatabaseAndDump()` is called by the table-asset editor so authored data stays in sync while editing. |
| Automatic cook discovery/staging | ⚠️ Not wired globally yet | There is no module-level hook today that enumerates every SQL table asset and stages rebuilt `.db` files automatically during packaging. |

Recommended project-level packaging flow today:

1. Enumerate the authored `UESQLTableAsset` instances you want to ship.
2. Rebuild each target `.db` from its `.sqldump` using `FESQLDumpPipeline::RebuildDatabaseFromDump()`.
3. Stage those rebuilt `.db` files with your packaged content.
4. At runtime, open them read-only directly or copy them to a writable location before the first mutation.

This keeps the contract consistent with the rest of the plugin: `.sqldump` is
the authored source of truth, the rebuild helper already exists, and packaging
automation is a project integration task rather than a finished runtime feature.

### 8.9 Editor ↔ Runtime Database Safety (WAL Mode)

When editing a SQL Table Asset in the editor while PIE is running, both the
editor and the runtime can have open connections to the same `.db` file.

SQLite's **WAL (Write-Ahead Logging)** mode ensures this is safe:

| Operation | Result |
|---|---|
| Editor writes + PIE reads simultaneously | ✅ Safe — WAL allows concurrent readers and a single writer |
| Both write simultaneously | ✅ Safe — SQLite serializes writes via its locking protocol |
| Editor open + PIE opens same file | ✅ Safe — multiple connections to the same `.db` are normal |

> The `FCriticalSection` in `FESQLDatabase` guards against concurrent access from
> multiple UE threads within the same process. SQLite's file-level locking handles
> cross-process safety (editor vs packaged game). WAL mode is enabled by default
> via `PRAGMA journal_mode=WAL` in `FESQLDatabase::Open()`.

---

## 9. Blueprint Usage Examples

### SQL Table Asset Workflow (Current Authored-Data Path)

This is the current recommended Blueprint-facing path for authored,
schema-backed table data. The asset owns the schema and row creation surface,
but Blueprint results still come back through `FESQLQueryResult` today. Fully
typed Blueprint row outputs are future K2 work on top of this same asset path.

```
Event BeginPlay
    │
    ├─ MyItemsTable -> Initialize(self)
    │
    ├─ Make FMyItemStruct
    │    ItemId      = "sword_01"
    │    DisplayName = "Iron Sword"
    │    Price       = 150
    │
    ├─ MyItemsTable -> Insert Row From Struct(RowData)
    │
    └─ MyItemsTable -> Get Row By Id(self, "sword_01")
             └─ Result.Rows[0] -> read columns from the returned row map
```

### Raw Subsystem Workflow (Low-Level / Utility Path)

Use this when you genuinely want raw SQL or ad hoc utility tables. For normal
schema-backed gameplay/content data, prefer the SQL Table Asset path above.

```
Event BeginPlay
    │
    ├─ SQL: Open Database
    │    DatabaseName = "SaveData"
    │    Scope        = Local
    │    Persistence  = Persistent
    │    FileName     = "savegame.db"
    │
    ├─ SQL: Create Table
    │    DatabaseName = "SaveData"
    │    TableName    = "PlayerProgress"
    │    Columns      = { "Level": "INTEGER", "Score": "INTEGER", "Timestamp": "TEXT" }
    │
    └─ SQL: Insert Row
             DatabaseName = "SaveData"
             TableName    = "PlayerProgress"
             ColumnValues = { "Level": "5", "Score": "12340", "Timestamp": "2026-04-09T22:00:00Z" }
```

### Async Raw Query + Read Results

The current async entry point is `UESQLSubsystem::AsyncExecuteSQL()`.

```
SQL: Async Execute SQL
    DatabaseName = "SaveData"
    SQL          = "SELECT * FROM PlayerProgress WHERE Level > ?1 ORDER BY Score DESC"
    │
    ├─ OnComplete(Result)
    │    ├─ if Result.bSuccess -> ForEachLoop(Result.Rows)
    │    │                        └─ Get Column Value (Row, "Score") -> Print String
    │    └─ else -> Print String(Result.ErrorMessage)
```

---

## 10. Dependency Graph

```
                                 Game / plugin modules (Gameplay, EOS, PlayFab, etc.)
                                                                             │
                                                                             │ depend on runtime APIs only
                                                                             ▼
┌────────────────────────────────────────────────────────────────────────┐
│ UnrealExtendedSQL (runtime)                                           │
│                                                                        │
│ Shared layer:  ESQLTypes, ESQLId, ESQLSettings                         │
│ Core layer:    FESQLDatabase, FESQLStatement, ESQLUnrealVFS            │
│ Service layer: UESQLSubsystem, UESQLPlayerDBComponent                  │
│ Typed layer:   UESQLTableAsset, ESQLStructValidator,                   │
│                ESQLPropertySerializer, FESQLDumpPipeline               │
└───────────────────────────────┬────────────────────────────────────────┘
                                                                │
                                                                ▼
                                 Thirdparty/SQLite/sqlite3.c + sqlite3.h

┌────────────────────────────────────────────────────────────────────────┐
│ UnrealExtendedSQLEditor (editor-only, depends on UnrealExtendedSQL)    │
│                                                                        │
│ AssetFactory, SqlId customization, StructContextMenu,                  │
│ TableAssetEditor, Validation                                           │
└────────────────────────────────────────────────────────────────────────┘

Blueprints and future K2 nodes should call into:
- `UESQLSubsystem` for raw SQL, lifecycle, snapshots, and player-db services
- `UESQLTableAsset` for typed schema-backed table workflows

The editor module depends on the runtime module. The runtime module does not
depend on gameplay modules or editor-only code.
```

---

## 11. Security & Safety

| Concern | Mitigation |
|---|---|
| **SQL Injection** | `UESQLTableAsset` typed save/load paths and subsystem semantic helpers (`InsertRow`, `SelectRows`, etc.) parameterize values internally. Raw `ExecuteSQL` / `ExecutePlayerSQL` are intentionally low-level and should be reserved for trusted SQL or paired with `ExecuteSQLWithBindings` when values come from data. |
| **File access** | Databases default to `FPaths::ProjectSavedDir() / Settings.DefaultDatabaseDirectory`. Absolute paths require explicit opt-in. |
| **Concurrent writes** | `FCriticalSection` per database handle + SQLite's built-in serialised threading mode. |
| **Large results** | `SelectRows` exposes `Limit`; `GetAllRows` and typed load-many are caller-controlled. Use targeted row lookups or `AsyncExecuteSQL` when result sets may be large. |
| **Crash safety** | WAL journal mode (default) survives process crashes without data loss. |
| **Authority enforcement** | `Server` and `PlayerScoped` databases reject operations from `NM_Client`. Prevents Blueprints from accidentally bypassing server authority. |
| **Player isolation** | Each player's data is in a separate `.db` file in a UniqueNetId-keyed subfolder. No player can accidentally (or intentionally) read another player's database through the public API. |
| **Disconnect cleanup** | Player databases are flushed (pending transactions committed) and closed on `Logout`. Configurable file deletion prevents stale file accumulation on long-running dedicated servers. |
| **Editor ↔ Runtime** | WAL journal mode allows the editor and PIE to access the same `.db` file concurrently without corruption. Multiple readers + one writer is safe. The editor opens its own `FESQLDatabase` connection; PIE opens another through the subsystem. Both go through the same `FCriticalSection` + SQLite internal locking. |
| **PIE multi-player** | In PIE with multiple players, each simulated player gets a unique fallback ID (`LOCAL_0`, `LOCAL_1`, etc.) derived from the PIE player index. This prevents player database collisions in development. |

---

## 12. Future Expansion Hooks

These are **not** in the initial scope but the architecture supports them cleanly:

- **Typed async wrappers / K2 nodes** — Designer-friendly async nodes that wrap `UESQLTableAsset` rather than inventing a second typed implementation path.
- **Cook/staging automation** — A project or plugin cook hook that enumerates SQL table assets, calls `FESQLDumpPipeline::RebuildDatabaseFromDump()`, and stages rebuilt `.db` files automatically.
- **Schema migrations** — Versioned migration scripts or asset-aware schema versions layered on top of table assets and `SyncSchema()`.
- **Player-scoped typed helpers** — Asset-owned helpers that combine `UESQLPlayerDBComponent` context with table CRUD without widening `UESQLSubsystem` again.
- **Encryption** — Swap the amalgamation for SQLCipher (API-compatible) behind the same core database abstraction.
- **Database browser panel** — An editor tab with a tree view of SQL databases, table viewers, and a query console.
- **Cross-player/server analytics utilities** — Server-only helpers for joining or aggregating player databases for leaderboards, trading, analytics, or moderation review.
- **Session archival** — On match end, merge session or player databases into a single archive `.db` with match metadata for replays, anti-cheat analysis, or audit trails.

---

## 13. File Checklist

### Runtime Module — `UnrealExtendedSQL`

| # | File | Purpose |
|---|---|---|
| 1 | `Thirdparty/SQLite/sqlite3.c` | SQLite amalgamation source |
| 2 | `Thirdparty/SQLite/sqlite3.h` | SQLite amalgamation header |
| 3 | `Source/UnrealExtendedSQL/UnrealExtendedSQL.Build.cs` | UBT module definition |
| 4 | `Source/UnrealExtendedSQL/UnrealExtendedSQL.h/.cpp` | Module startup/shutdown |
| 5 | `Source/UnrealExtendedSQL/Shared/ESQLTypes.h` | Shared SQL result/value/snapshot/public type definitions |
| 6 | `Source/UnrealExtendedSQL/Shared/ESQLId.h` | Typed SQL row id/reference type with asset-linked picker metadata |
| 7 | `Source/UnrealExtendedSQL/Shared/ESQLSettings.h/.cpp` | Developer settings for paths, limits, multiplayer, and debugging |
| 8 | `Source/UnrealExtendedSQL/Shared/ESQLPropertySerializer.h/.cpp` | Reflected property serialization/deserialization between structs and SQL values |
| 9 | `Source/UnrealExtendedSQL/Core/ESQLDatabase.h/.cpp` | Core database connection wrapper and execution/transaction helpers |
| 10 | `Source/UnrealExtendedSQL/Core/ESQLStatement.h/.cpp` | Prepared statement wrapper and typed bind/read helpers |
| 11 | `Source/UnrealExtendedSQL/Core/ESQLUnrealVFS.h/.cpp` | Unreal-backed SQLite VFS for cross-platform file access |
| 12 | `Source/UnrealExtendedSQL/Subsystem/ESQLSubsystem.h/.cpp` | Database lifecycle, raw SQL, snapshots, authority, and player DB service layer |
| 13 | `Source/UnrealExtendedSQL/PlayerData/ESQLPlayerDBComponent.h/.cpp` | Player database context/state component |
| 14 | `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.h/.cpp` | Schema-owning asset and typed row API surface |
| 15 | `Source/UnrealExtendedSQL/TableAsset/ESQLStructValidator.h/.cpp` | Struct compatibility and SQL column generation rules |
| 16 | `Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.h/.cpp` | `.sqldump` export/import/sync pipeline |
| 17 | `UnrealExtendedFramework.uplugin` | Plugin/module registration |

### Editor Module — `UnrealExtendedSQLEditor`

| # | File | Purpose |
|---|---|---|
| 18 | `Source/UnrealExtendedSQLEditor/UnrealExtendedSQLEditor.Build.cs` | Editor module build definition |
| 19 | `Source/UnrealExtendedSQLEditor/UnrealExtendedSQLEditor.h/.cpp` | Module startup — registers asset actions, menu extenders |
| 20 | `Source/UnrealExtendedSQLEditor/AssetFactory/ESQLTableAssetFactory.h/.cpp` | UFactory for creating new SQL Table assets |
| 21 | `Source/UnrealExtendedSQLEditor/AssetFactory/ESQLTableAssetActions.h/.cpp` | Asset type actions and Content Browser integration |
| 22 | `Source/UnrealExtendedSQLEditor/SqlId/ESQLIdCustomization.h/.cpp` | Details customization and picker UI for `FESQLId` |
| 23 | `Source/UnrealExtendedSQLEditor/StructContextMenu/ESQLStructMenuExtension.h/.cpp` | Struct context-menu shortcut for creating SQL table assets |
| 24 | `Source/UnrealExtendedSQLEditor/TableAssetEditor/FESQLTableEditorToolkit.h/.cpp` | Main SQL table asset editor toolkit |
| 25 | `Source/UnrealExtendedSQLEditor/TableAssetEditor/SESQLRowEditor.h/.cpp` | Row editing widget for SQL table editor UI |
| 26 | `Source/UnrealExtendedSQLEditor/TableAssetEditor/SESQLTableListViewRow.h/.cpp` | Table/list row presentation widget |
| 27 | `Source/UnrealExtendedSQLEditor/Validation/SESQLValidationDialog.h/.cpp` | Validation dialog for incompatible structs or schema issues |

---

## 14. Naming Conventions

Following the existing plugin patterns:

| Pattern | Example | Follows |
|---|---|---|
| Module prefix | `ESQL` | `EPF` (PlayFab), `EB` (Backend) |
| Types | `FESQLRow`, `FESQLQueryResult` | `FEPFError`, `FEPFResult` |
| Enums | `EESQLDatabaseScope`, `EESQLColumnType` | `EEPFAuthMode` |
| Settings class | `UESQLSettings` | `UEPFSettings` |
| Subsystem class | `UESQLSubsystem` | `UEPFAnalyticsSubsystem` |
| Component class | `UESQLPlayerDBComponent` | UE `UActorComponent` convention |
| Blueprint category | `"SQL|..."`, `"SQL|Player"`, `"SQL|Multiplayer"` | `"PlayFab|..."` |
| Log category | `LogExtendedSQL` | `LogExtendedPlayFab` |
| Player DB key | `UniqueNetId` string | Platform-agnostic (Steam, EOS, etc.) |

# ExtendedSQL Implementation Plan

> Concrete engineering plan derived from [ExtendedSQLiteFeasibility.md](ExtendedSQLiteFeasibility.md).
>
> This document is prescriptive: it specifies the files to add, change, and delete, the order of work, and the acceptance criteria for each phase. Every decision here must match the feasibility doc; if the two disagree, the feasibility doc wins and this plan is updated.

---

## 0. Current State Snapshot

Modules in scope:

- `Source/UnrealExtendedSQL/`
  - `Blueprint/` — `ESQLAsyncTableActions`, `ESQLBlueprintBridge` (both to be hidden/removed)
  - `Core/` — `ESQLDatabase`, `ESQLStatement`, `ESQLUnrealVFS` (VFS to be deleted)
  - `PlayerData/` — `ESQLPlayerDBComponent` (to be deleted)
  - `Shared/`, `Subsystem/ESQLSubsystem`, `TableAsset/ESQLTableAsset`, `ESQLStructValidator`
  - `VCSPipeline/ESQLDumpPipeline` (to be reduced to legacy one-way import)
- `Source/UnrealExtendedSQLEditor/` — `AssetFactory/`, `K2Node/`, `SqlId/`, `StructContextMenu/`, `TableAssetEditor/`, `Validation/`
- `Thirdparty/SQLite/` — already contains a vendored `sqlite3.c` / `sqlite3.h`

Target modules after this plan:

- `UnrealExtendedSQL` — runtime, public surface is `UESQLSubsystem` + `UESQLTableAsset` + `FESQLId` + result/error types.
- `UnrealExtendedSQLEditor` — editor tooling for `.usqlite/` projects, K2 nodes, validators, migration commandlet.

---

## 1. Guiding Constraints

These constraints are reasserted from the feasibility doc and must not be violated by any phase:

1. One public runtime surface: `UESQLSubsystem`. No other class is `BlueprintCallable` at the database level.
2. One authored format: `.usqlite/` directory. No `.sqlmigration`. No committed `.db`. `.sqldump` is one-way legacy import only.
3. One SQLite build: vendored amalgamation under `Thirdparty/SQLite/`. No `SQLiteCore`. No engine SQLite headers.
4. No custom `sqlite3_vfs`. Stock OS VFS only. Unreal-awareness lives in the path resolver and a one-time extraction step.
5. Three database scopes: `Shared`, `Local`, `Readonly`. Clients never open `Shared`.
6. Writer-thread-per-database + reader pool + WAL. Transactions are writer-thread-affine.
7. Bridge is private, non-`BlueprintCallable`, never named in public docs.

---

## 2. Phased Delivery

Each phase ends with a buildable tree and a defined acceptance gate. Phases are ordered so earlier work does not depend on later work.

### Phase A — Foundation: vendored SQLite build

**Goal:** compile a single, owned SQLite amalgamation with a fixed define set, and remove any path that touches `SQLiteCore`.

**Work:**

1. Add `Source/UnrealExtendedSQL/Private/ThirdParty/SQLiteAmalgamation.cpp` that `#include "sqlite3.c"` with the defines below. This is the only TU that compiles the amalgamation.
2. Fixed defines (compile-time, set in `UnrealExtendedSQL.Build.cs` via `PublicDefinitions`/`PrivateDefinitions`, applied only to the amalgamation TU where needed):
   - `SQLITE_THREADSAFE=2`
   - `SQLITE_ENABLE_FTS5`
   - `SQLITE_ENABLE_JSON1`
   - `SQLITE_ENABLE_RTREE`
   - `SQLITE_ENABLE_DBSTAT_VTAB`
   - `SQLITE_DEFAULT_FOREIGN_KEYS=1`
   - `SQLITE_DEFAULT_WAL_SYNCHRONOUS=1`
   - `SQLITE_OMIT_DEPRECATED`
   - `SQLITE_OMIT_LOAD_EXTENSION`
   - `SQLITE_API=` set to a hidden-visibility decorator so symbols do not leak across modules.
3. Update `UnrealExtendedSQL.Build.cs`:
   - Add `PrivateIncludePaths` pointing at `Thirdparty/SQLite/`.
   - Remove any `PublicDependencyModuleNames` / `PrivateDependencyModuleNames` entry for `SQLiteCore` (verify none exist).
   - Ensure no header re-exports `sqlite3.h`; it is `#include`d only from `Private/` sources.
4. Introduce an internal include wrapper `Source/UnrealExtendedSQL/Private/ThirdParty/ESQLSqlite.h` that is the only header other runtime sources use to reach `sqlite3.h`. Public headers never include it.

**Acceptance:**

- `grep -R "SQLiteCore" Source/` returns no results.
- `grep -R "#include \"sqlite3.h\"" Source/` only matches files under `Private/`.
- Plugin compiles on Win64 editor target.

---

### Phase B — Delete the custom VFS

**Goal:** remove `ESQLUnrealVFS` and make every `sqlite3_open_v2` call go through the stock OS VFS.

**Work:**

1. Delete `Source/UnrealExtendedSQL/Core/ESQLUnrealVFS.{h,cpp}`.
2. Remove all registration calls (`sqlite3_vfs_register`) and all references to the custom VFS name.
3. In `ESQLDatabase::Open`, pass `nullptr` as the `zVfs` argument to `sqlite3_open_v2` so the default VFS is used.
4. Add `Source/UnrealExtendedSQL/Private/Paths/ESQLPathResolver.{h,cpp}` with:
   - `enum class EESQLScope : uint8 { Shared, Local, Readonly };`
   - `struct FESQLResolvedPath { FString AbsolutePath; EESQLScope Scope; bool bReadOnly; };`
   - `static FESQLResolvedPath Resolve(FName LogicalName, EESQLScope Scope, const FString& LocalUserId = TEXT(""));`
   - Rules:
     - `Shared` → `FPaths::ProjectSavedDir() / TEXT("ExtendedSQL") / LogicalName.ToString() + TEXT(".db")`
     - `Local` → `FPaths::ProjectPersistentDownloadDir() / TEXT("ExtendedSQL/Local") / LocalUserId / LogicalName.ToString() + TEXT(".db")` (falls back to `ProjectSavedDir` on desktop/editor when `PersistentDownloadDir` is not meaningful).
     - `Readonly` → locate packaged payload via `IPlatformFile`; if not a real OS file, extract once (see Phase B.5).
   - `FPaths::NormalizeFilename` and `IFileManager::Get().MakeDirectory(..., /*Tree=*/true)` before returning.
5. Add `Source/UnrealExtendedSQL/Private/Paths/ESQLContentExtractor.{h,cpp}`:
   - `static FString EnsureExtracted(const FString& PackagedPath);`
   - Computes a content hash (SHA1) of the packaged payload, extracts to `FPaths::ProjectSavedDir() / TEXT("ExtendedSQL/ContentCache") / <hash> / <filename>` if not already present, returns the absolute path.
   - Idempotent; concurrent-safe using `FFileHelper::SaveArrayToFile` with a `.tmp` + atomic rename.

**Acceptance:**

- No file under `Source/` references `sqlite3_vfs`, `sqlite3_vfs_register`, or the old custom VFS name.
- A unit test opens a `Local` database, writes a row, closes, reopens, reads the row back.
- A unit test resolves a `Readonly` logical name whose payload lives in a pak-like location (simulated) and observes extraction into the content cache.

---

### Phase C — Core connection + statement hardening

**Goal:** make `ESQLDatabase` / `ESQLStatement` usable as the building blocks for the subsystem's writer + reader connection model. No public API changes here.

**Work:**

1. `ESQLDatabase`:
   - Open with `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX` for writer; `SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX` for readers; `Readonly` scope always uses readonly flags.
   - On open, execute: `PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON; PRAGMA busy_timeout=5000;`
   - Add `FESQLThreadAffinity` guard (`TOptional<uint32> OwningThreadId`) so debug builds `check()` that a connection is only used from its owning thread.
2. `ESQLStatement`:
   - Add typed bind helpers for all `FESQLValue` kinds used today.
   - Add `EnumerateRows(TFunctionRef<bool(const FESQLRow&)>)` so callers do not manage step loops.
3. Add `FESQLError` (code, message, SQL fragment) and make all Core APIs return `TValueOrError<T, FESQLError>` or equivalent; no exceptions.

**Acceptance:**

- Unit tests cover: open/close, WAL mode assertion via `PRAGMA journal_mode`, bind+step+finalize, error propagation for malformed SQL.

---

### Phase D — Subsystem: authority + routing + concurrency

**Goal:** implement the single public runtime surface per §9 and §10 of the feasibility doc.

**Work:**

1. Rewrite `Source/UnrealExtendedSQL/Subsystem/ESQLSubsystem.{h,cpp}`:
   - Base class: `UGameInstanceSubsystem` (runtime lifetime tied to `UGameInstance`; matches PIE/shipping semantics).
   - Public API categories (all `BlueprintCallable` unless noted):
     - `OpenDatabase(FName LogicalName, EESQLScope Scope)` / `CloseDatabase(FName)` — idempotent.
     - Typed CRUD taking a `UESQLTableAsset*`: `InsertRow`, `UpdateRow`, `DeleteRow`, `SelectRow`, `SelectRows`.
     - Raw SQL: `Execute(FName DB, const FString& Sql, const TArray<FESQLValue>& Params)` and `Query(...)`.
     - Transactions: `BeginTransaction(FName DB, FESQLTransactionHandle& Out)`, `Commit(Handle)`, `Rollback(Handle)`.
     - Async variants: `*_Async(..., FOnESQLComplete OnDone)` returning a `FESQLTicket` for cancellation. `BlueprintCallable` with `BlueprintInternalUseOnly=false` but paired with K2 latent nodes for ergonomics.
     - Snapshot helpers: `BackupTo(FName DB, const FString& DestAbsolutePath)` using `sqlite3_backup_*`.
2. Authority/routing gate (§9):
   - `FESQLScopeGate::Check(UWorld* World, EESQLScope, EESQLOp Op) -> TOptional<FESQLError>`.
   - Rules:
     - `Shared` write on client (`World->GetNetMode() == NM_Client`) → reject with `EESQLErrorCode::AuthorityViolation`.
     - `Shared` read on client → reject (clients use replication).
     - `Readonly` write (any peer) → reject.
     - `Local` any op → allow.
   - Every public entry point calls the gate before the resolver.
3. Concurrency model (§10):
   - Per open database, subsystem owns one `FESQLDatabaseContext`:
     - `TUniquePtr<FRunnableThread> WriterThread;`
     - `TQueue<FESQLWorkItem, EQueueMode::Mpsc> WriterQueue;`
     - `FEvent* WriterWake;`
     - `TArray<TSharedRef<FESQLDatabase>> ReaderPool; // size = FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads()/2, 2, 4)`
     - `FCriticalSection ReaderPoolCS;`
   - Writer loop: drain queue, run item (sync call or transaction step), dispatch completion to game thread via `AsyncTask(ENamedThreads::GameThread, ...)`.
   - Reader acquire/release: `TSharedRef<FESQLDatabase> AcquireReader()` / `ReleaseReader(...)`. Readers may run on any task-graph thread.
   - Sync API from non-writer-thread returns `EESQLErrorCode::WrongThread` except when the caller is the writer thread itself.
4. Transactions:
   - `FESQLTransactionHandle` stores `FName DB`, `uint64 Id`, `uint32 OwningThreadId` (must be writer thread).
   - Nested `Begin` issues `SAVEPOINT sp_<n>`; `Commit` issues `RELEASE sp_<n>`; `Rollback` issues `ROLLBACK TO sp_<n>`.
   - Abandoned handles (detected via weak handle -> context) roll back on next writer idle tick.
5. Migration application:
   - `ApplyMigrations(FName DB, const FESQLMigrationSet& Set)` runs migrations in order inside a single writer transaction. A `_esql_schema_version` table tracks the last applied id.

**Acceptance:**

- `UESQLSubsystem` is the only `BlueprintCallable` class in the runtime module (verified by grep for `UFUNCTION(BlueprintCallable` across `Source/UnrealExtendedSQL/`).
- Scope-violation tests: opening `Shared` on a simulated client net mode fails; writing to `Readonly` fails.
- Concurrency tests: 1000 mixed write/read operations across 8 threads complete without corruption; writer-queue order is FIFO.
- Transaction nesting test: `Begin; Begin; Rollback (inner); Commit (outer)` yields the outer-only state.

---

### Phase E — Table asset: metadata-first

**Goal:** reduce `UESQLTableAsset` to schema/identity metadata, per §2.2 / §4.3.

**Work:**

1. Keep on `UESQLTableAsset`:
   - `FName LogicalDatabaseName` (resolved via §2.4 path rules)
   - `EESQLScope Scope` (default `Shared`)
   - `FName TableName`
   - `TObjectPtr<UScriptStruct> RowStruct`
   - `FName PrimaryKey`
   - `FText Label`, editor tags
   - Editor-side validation/import/export metadata
2. Remove from `UESQLTableAsset`:
   - Any `BlueprintCallable` CRUD / query methods.
   - Any async helpers.
   - Any direct reference to `ESQLDatabase` or statements.
3. Add `GetSchemaDescriptor()` returning a `FESQLTableSchema` consumed by the subsystem's typed CRUD.
4. `ESQLStructValidator` stays but moves its validation call sites so they run at editor time and at `UESQLSubsystem::OpenDatabase` (fail-fast if the row struct drifted).

**Acceptance:**

- `UESQLTableAsset.h` contains zero `UFUNCTION(BlueprintCallable)` members that perform database I/O.
- Existing typed CRUD tests pass when rewired through the subsystem.

---

### Phase F — Delete player-scoped surfaces

**Goal:** remove `UESQLPlayerDBComponent` and all player-only public APIs per §2.3 / §3.

**Work:**

1. Delete `Source/UnrealExtendedSQL/PlayerData/ESQLPlayerDBComponent.{h,cpp}`.
2. Delete any `SQL|Player` K2 nodes under `Source/UnrealExtendedSQLEditor/K2Node/`.
3. Local caches are expressed by setting `UESQLTableAsset::Scope = Local` (or by opening a logical name with `Scope=Local` directly). No new API is introduced.
4. Add a single redirector/log: if a project references the deleted component at load time, log once with the migration instruction (point at `.usqlite/` migration commandlet).

**Acceptance:**

- Grep for `ESQLPlayerDBComponent` returns no results outside of historical markdown.
- Sample project that used to use the component compiles after switching to `Scope=Local` + subsystem calls.

---

### Phase G — Bridge: move to Private and strip public surface

**Goal:** per §2.7, bridge remains as a K2-expansion implementation detail, never a public surface.

**Work:**

1. Move `Source/UnrealExtendedSQL/Blueprint/ESQLBlueprintBridge.{h,cpp}` → `Source/UnrealExtendedSQL/Private/Blueprint/ESQLBlueprintBridge.{h,cpp}`.
2. Remove `UNREALEXTENDEDSQL_API` from the type and all members.
3. Replace every `UFUNCTION(BlueprintCallable, ...)` on the bridge with `UFUNCTION(BlueprintInternalUseOnly="true")` (or remove `UFUNCTION` entirely where only C++ K2-expansion callers need it).
4. Delete `ESQLAsyncTableActions` if it is redundant with the subsystem's async API; otherwise move it alongside the bridge under `Private/Blueprint/` with the same hiding rules.
5. Update K2 nodes in `UnrealExtendedSQLEditor` to expand to subsystem calls directly; bridge is only used where reflection-only expansion is unavoidable.

**Acceptance:**

- No public header in `UnrealExtendedSQL/Public/` references `UESQLBlueprintBridge`.
- No `BlueprintCallable` functions exist on the bridge class.
- K2 nodes still compile and produce working graphs.

---

### Phase H — `.usqlite/` format, emitter, and builder

**Goal:** per §8, ship the authored format end-to-end.

**Work:**

1. Define types in `Source/UnrealExtendedSQLEditor/Private/USqlite/`:
   - `FUSqliteProject` — manifest (`project.json`).
   - `FUSqliteTableSchema` — `schema/<table>.json`.
   - `FUSqliteRow` — one line in `data/<table>.ndjson`.
   - `FUSqliteMigration` — `migrations/NNNN_<slug>.sql`.
   - `FUSqliteLock` — `.usqlite-lock.json`.
2. Serializer (`FUSqliteSerializer`):
   - Deterministic JSON emission: sorted keys, `\n` line endings, UTF-8 no BOM, 2-space indent.
   - NDJSON rows use stable column order matching the schema file and are sorted by primary key before write.
   - Rewrite `.usqlite-lock.json` on every save with SHA256 per file; in-memory check that re-saving an unchanged in-memory project produces a byte-identical tree.
3. Builder (`FUSqliteBuilder::Build(const FUSqliteProject&, const FString& OutDbPath)`):
   - Create empty DB → apply migrations in numeric order (one transaction) → load NDJSON rows → run `PRAGMA integrity_check`.
   - Deterministic: normalizes SQLite change counters / timestamps where possible; the builder documents the exceptions.
4. Validator:
   - Append-only migrations: compare committed migration hashes to `.usqlite-lock.json`; any mismatch on a previously committed migration is an error.
   - Schema drift: `schema/*.json` changes must correspond to a new migration with a matching `_esql_schema_version` bump.
5. Wire `UESQLSubsystem::OpenDatabase` to accept either a prebuilt `.db` (cooked) or a `.usqlite/` root (editor): the editor-only path runs the builder into an intermediate `.db` once per project hash.

**Acceptance:**

- Round-trip test: create project → save → reload → re-save produces byte-identical tree.
- Builder determinism test: identical `.usqlite/` input yields identical `.db` bytes (modulo documented exceptions).
- Validator rejects: hand-edited old migration, schema change without matching migration, duplicate migration numbers.

---

### Phase I — Legacy `.sqldump` import commandlet

**Goal:** per §3.1, provide a one-shot migration from the old world into `.usqlite/`.

**Work:**

1. Reduce `Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.{h,cpp}` to an **import-only** parser. Delete all export/round-trip code.
2. Add `UEditorSqlMigrateCommandlet` in `UnrealExtendedSQLEditor`:
   - Inputs: a folder containing legacy `.db` and/or `.sqldump` files.
   - Output: `.usqlite/` project(s) emitted via Phase H serializer.
   - Creates an initial migration `0001_initial.sql` from the legacy schema.
3. After import, runtime opens only the generated `.db` built from the new `.usqlite/`. Legacy files are untouched on disk but unused.

**Acceptance:**

- Commandlet runs on a fixture containing one `.db` + one `.sqldump` and produces a valid `.usqlite/` that builds back to a `.db` whose schema+data match the source (verified by comparing normalized SQL dumps).

---

### Phase J — Editor UX: asset, K2 nodes, validation

**Goal:** align editor surfaces with the reduced public runtime.

**Work:**

1. `AssetFactory/` — continue creating `UESQLTableAsset` but present only metadata-first fields; remove UI for fields that no longer exist.
2. `TableAssetEditor/` — schema + row preview; no runtime CRUD widgets other than "build test `.db`" and "run query against test DB" that both route through `UESQLSubsystem`.
3. `K2Node/` — keep typed row/query nodes; expand into `UESQLSubsystem` calls. Remove all player-only node families.
4. `Validation/` — plug `FUSqliteValidator` into the asset's `GetValidationErrors` / `IsDataValid` so authoring errors surface inline.
5. `StructContextMenu/` and `SqlId/` — unchanged in intent; audit that they do not depend on removed APIs.

**Acceptance:**

- Editor opens, creates a table asset, runs a typed query against a test DB via the subsystem, all without touching removed classes.

---

### Phase K — Documentation and samples

**Goal:** public-facing docs match the reset.

**Work:**

1. Rewrite `ExtendedSQL.md`, `ExtendedSQLBlueprint.md`, `ExtendedSQLImplementation.md` to teach only:
   - Get `UESQLSubsystem`
   - Reference a `UESQLTableAsset`
   - Typed CRUD / raw SQL / transactions / async
   - Scopes (`Shared` / `Local` / `Readonly`) and the routing rule
   - `.usqlite/` authoring
2. Remove all mentions of: bridge class, player component, custom VFS, `.sqldump` as authored truth, `SQLiteCore`.
3. Mark `OldFeasibility*.md` files as historical (add a one-line banner at the top pointing at `ExtendedSQLiteFeasibility.md`).

**Acceptance:**

- Public docs grep clean for the removed class names and formats (except in explicit "migration from legacy" sections).

---

## 3. File-Level Change Summary

### 3.1 Delete

- `Source/UnrealExtendedSQL/Core/ESQLUnrealVFS.{h,cpp}`
- `Source/UnrealExtendedSQL/PlayerData/ESQLPlayerDBComponent.{h,cpp}`
- Any player-only K2 node files under `Source/UnrealExtendedSQLEditor/K2Node/` (audited in Phase F/J).
- Any `SQLiteCore` references in `.Build.cs` files (verified zero by Phase A).

### 3.2 Move (and strip public surface)

- `Source/UnrealExtendedSQL/Blueprint/ESQLBlueprintBridge.{h,cpp}` → `Source/UnrealExtendedSQL/Private/Blueprint/`
- `Source/UnrealExtendedSQL/Blueprint/ESQLAsyncTableActions.{h,cpp}` → `Source/UnrealExtendedSQL/Private/Blueprint/` (or delete if subsumed).

### 3.3 Add

- `Source/UnrealExtendedSQL/Private/ThirdParty/SQLiteAmalgamation.cpp`
- `Source/UnrealExtendedSQL/Private/ThirdParty/ESQLSqlite.h`
- `Source/UnrealExtendedSQL/Private/Paths/ESQLPathResolver.{h,cpp}`
- `Source/UnrealExtendedSQL/Private/Paths/ESQLContentExtractor.{h,cpp}`
- `Source/UnrealExtendedSQL/Private/Concurrency/ESQLDatabaseContext.{h,cpp}`
- `Source/UnrealExtendedSQL/Private/Concurrency/ESQLWorkItem.{h,cpp}`
- `Source/UnrealExtendedSQL/Public/Types/ESQLScope.h` (enum + small value types only)
- `Source/UnrealExtendedSQL/Public/Types/ESQLError.h`
- `Source/UnrealExtendedSQL/Public/Types/ESQLTransactionHandle.h`
- `Source/UnrealExtendedSQLEditor/Private/USqlite/FUSqliteProject.{h,cpp}`
- `Source/UnrealExtendedSQLEditor/Private/USqlite/FUSqliteSerializer.{h,cpp}`
- `Source/UnrealExtendedSQLEditor/Private/USqlite/FUSqliteBuilder.{h,cpp}`
- `Source/UnrealExtendedSQLEditor/Private/USqlite/FUSqliteValidator.{h,cpp}`
- `Source/UnrealExtendedSQLEditor/Private/Commandlets/UEditorSqlMigrateCommandlet.{h,cpp}`

### 3.4 Rewrite

- `Source/UnrealExtendedSQL/Subsystem/ESQLSubsystem.{h,cpp}` — full public API per Phase D.
- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.{h,cpp}` — metadata-first per Phase E.
- `Source/UnrealExtendedSQL/Core/ESQLDatabase.{h,cpp}` — open flags + pragmas + thread affinity per Phase C.
- `Source/UnrealExtendedSQL/Core/ESQLStatement.{h,cpp}` — bind helpers + enumerator per Phase C.
- `Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.{h,cpp}` — reduce to import-only per Phase I.

---

## 4. Public API Sketch

Illustrative (not final) header sketch for Phase D:

```cpp
UENUM(BlueprintType)
enum class EESQLScope : uint8 { Shared, Local, Readonly };

USTRUCT(BlueprintType)
struct FESQLError { /* code, message, sql */ };

USTRUCT(BlueprintType)
struct FESQLTransactionHandle { /* opaque */ };

UCLASS()
class UNREALEXTENDEDSQL_API UESQLSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="SQL|Database")
    bool OpenDatabase(FName LogicalName, EESQLScope Scope, FESQLError& OutError);

    UFUNCTION(BlueprintCallable, Category="SQL|Database")
    void CloseDatabase(FName LogicalName);

    UFUNCTION(BlueprintCallable, Category="SQL|CRUD", meta=(AutoCreateRefTerm="Row"))
    bool InsertRow(const UESQLTableAsset* Table, const FInstancedStruct& Row, FESQLId& OutId, FESQLError& OutError);

    // ... UpdateRow, DeleteRow, SelectRow, SelectRows

    UFUNCTION(BlueprintCallable, Category="SQL|Raw")
    bool Execute(FName DB, const FString& Sql, const TArray<FESQLValue>& Params, FESQLError& OutError);

    UFUNCTION(BlueprintCallable, Category="SQL|Transaction")
    bool BeginTransaction(FName DB, FESQLTransactionHandle& OutHandle, FESQLError& OutError);
    UFUNCTION(BlueprintCallable, Category="SQL|Transaction")
    bool Commit(const FESQLTransactionHandle& Handle, FESQLError& OutError);
    UFUNCTION(BlueprintCallable, Category="SQL|Transaction")
    bool Rollback(const FESQLTransactionHandle& Handle, FESQLError& OutError);

    // Async variants pair with K2 latent nodes in the editor module.
};
```

---

## 5. Concurrency Implementation Notes

- Writer thread is an `FRunnable` that blocks on `WriterWake->Wait(IdleMs)` and drains `WriterQueue` until empty; after drain it dispatches completions and waits again.
- `FESQLWorkItem` holds a `TUniqueFunction<void(FESQLDatabase&)>` plus a `TUniqueFunction<void(FESQLError)>` completion, captured by value to avoid game-thread lifetime issues.
- Transactions serialize behind the writer queue naturally: while a transaction is open, every queued item for that DB runs on the same connection and therefore inside the transaction until `Commit`/`Rollback` closes it. Non-transaction items submitted during an open transaction execute after the transaction closes.
- Reader pool uses a simple LIFO stack under `FCriticalSection`. If all readers are busy, `AcquireReader` opens a new connection up to a cap (default 4) and tears down spares after an idle timeout.
- All completions dispatch via `AsyncTask(ENamedThreads::GameThread, ...)`. Callers that do not need game-thread affinity can opt out via a flag on the ticket.
- Shutdown path:
  1. Stop accepting new work (`bStopping = true`).
  2. Signal writer wake; writer drains remaining items with `EESQLErrorCode::Cancelled` for any item that had not started.
  3. Roll back any open transactions.
  4. Join writer thread; close reader pool; close writer connection.

---

## 6. Authority Implementation Notes

- `UESQLSubsystem` caches `GetWorld()->GetNetMode()` at `OpenDatabase` time for `Shared` databases and re-checks on every write (cheap). For `Local` and `Readonly`, no net-mode check is performed.
- A Blueprint caller on a client that attempts a `Shared` write receives `FESQLError{Code=AuthorityViolation, ...}` and the call is a no-op. No RPC is issued.
- Server-side code that *wants* to expose data to clients does so through normal replication or through explicit RPCs at the gameplay layer — not through the SQL surface. This is a policy boundary enforced by absence of an API, not by a runtime check.

---

## 7. Testing Strategy

Each phase lands with tests under `Source/UnrealExtendedSQL/Private/Tests/` using the Automation framework:

- **Phase A:** build sanity test (loads module, runs `sqlite3_libversion`, asserts version and compile options via `PRAGMA compile_options`).
- **Phase B:** path resolution + content extraction tests.
- **Phase C:** core connection/statement tests.
- **Phase D:** subsystem tests covering authority, routing, concurrency, transactions, async completion thread affinity.
- **Phase E:** table asset metadata validation.
- **Phase F:** absence tests — expect compile errors if any code references deleted types (covered implicitly by the plugin building).
- **Phase H:** serializer determinism, builder determinism, validator rejections.
- **Phase I:** migration commandlet round-trip.

A full-suite target `Automation RunTests ExtendedSQL.` is documented in the README.

---

## 8. Order Of Work (Critical Path)

1. Phase A — vendored SQLite (unblocks everything else)
2. Phase B — delete custom VFS + path resolver
3. Phase C — core hardening
4. Phase D — subsystem (largest single phase; land incrementally: open/close → CRUD → raw SQL → transactions → async)
5. Phase E — table asset slimming (can land in parallel with late D)
6. Phase F — delete player surfaces (after D+E so call sites have a target)
7. Phase G — bridge hiding (after F so K2 nodes have a stable target)
8. Phase H — `.usqlite/` format (can start in parallel with D once types in Phase C are stable)
9. Phase I — legacy import commandlet (after H)
10. Phase J — editor UX polish (after G + H)
11. Phase K — documentation (last, reflects shipped reality)

---

## 9. Definition Of Done

The plan is complete when all of the following hold simultaneously:

- Plugin compiles for Win64 editor and Win64 shipping with no references to `SQLiteCore`, `ESQLUnrealVFS`, `ESQLPlayerDBComponent`, or `.sqlmigration`.
- `UESQLSubsystem` is the only `BlueprintCallable` database class in the runtime module.
- Every runtime `sqlite3_open_v2` call passes `nullptr` for `zVfs`.
- A fresh project can: create a `UESQLTableAsset` → save a `.usqlite/` project → build a `.db` → open it via the subsystem → run typed CRUD and raw SQL → commit a transaction → receive async completion on the game thread.
- A simulated client is rejected with `AuthorityViolation` when issuing any `Shared` write.
- The automation suite `ExtendedSQL.*` passes on Win64.
- Public docs (`ExtendedSQL.md` and peers) describe only the surfaces this plan keeps.

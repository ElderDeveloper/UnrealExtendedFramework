# ExtendedSQL Feasibility Reset

> This document is the post-review feasibility verdict for UnrealExtendedSQL and UnrealExtendedSQLEditor.
>
> It intentionally replaces the earlier feasibility text that tried to evolve the current architecture instead of choosing a stronger one.

---

## 1. Executive Verdict

ExtendedSQL is still worth building, but only if the module stops investing in the wrong abstractions.

The earlier direction tried to evolve the current architecture in place. That produced a set of surfaces that pull in different directions and cannot be reconciled by more features. This reset removes them.

**Cut by this reset:**

- `UESQLPlayerDBComponent` and all public player-scoped database APIs
- `UESQLBlueprintBridge` as a public surface
- async action classes as a second public API
- `UESQLTableAsset` as a runtime service object (it becomes metadata-only)
- the custom Unreal SQLite VFS
- `.sqldump`-centric sync as the authored-data model
- engine `SQLiteCore` and any mixing with local SQLite headers

**Kept and promoted:**

- `UESQLSubsystem` as the single public runtime surface
- `UESQLTableAsset` as metadata-first authoring
- one vendored SQLite amalgamation
- `.usqlite/` as the sole authored format

The previous path can keep producing features, but it will keep producing the wrong kind of growth: duplicated APIs, misleading multiplayer stories, and brittle runtime/storage assumptions. The module becomes much more feasible if it commits to this harder reset.

---

## 2. Feasibility Depends On Seven Decisions

### 2.1 One Public Runtime Front Door

`UESQLSubsystem` must become the single supported runtime service surface.

Why this matters:

- runtime ownership becomes obvious
- Blueprint and C++ can teach the same story
- async, raw SQL, typed CRUD, and transactions can share one route and validation layer
- documentation stops splitting itself across five surfaces

This means the subsystem will be large. That is acceptable and preferable to a scattered design.

### 2.2 `UESQLTableAsset` Becomes Metadata-First

The asset should remain valuable, but for the right reasons:

- schema identity
- row struct identity
- logical database name
- logical table name
- label and primary-key metadata
- editor validation/import/export metadata

The asset should stop growing as the main runtime CRUD/query/async object.

The feasibility win here is major: typed authoring still exists, but the runtime becomes smaller and more coherent.

### 2.3 Delete The Public Player-Scoped Database Story

Per-player database files and `UESQLPlayerDBComponent` are not the right long-term public model.

They create too much API duplication and teach the wrong mental model for authoritative multiplayer data.

Feasible target model:

- server-authoritative tables remain normal shared tables
- player-owned data is keyed by `PlayerId`
- helper APIs may make that filtering convenient
- no separate public `SQL|Player` taxonomy is needed
- local per-client caches, when needed, are just another logical database name opened through `UESQLSubsystem` — not a separate component, not a separate public API

This is a bold cut, but it reduces complexity everywhere at once.

### 2.4 One Storage Path, No Custom SQLite VFS

The earlier design carried a custom Unreal SQLite VFS as a first-class runtime concept. That is removed.

Feasible storage model:

- **All** runtime database files — writable or readonly — are opened through the stock SQLite OS VFS against an absolute OS filesystem path.
- The module owns a single path resolver that converts a logical database name (from a `UESQLTableAsset` or subsystem call) into an absolute OS path using `IPlatformFile` / `FPlatformFileManager`.
- Writable databases resolve under a platform-correct writable root (`FPaths::ProjectSavedDir()` on desktop/editor, `FPaths::ProjectPersistentDownloadDir()` or platform equivalent on sandboxed targets). Resolution is platform-aware; the SQLite layer is not.
- Readonly databases shipped inside packaged content are not opened from inside a pak. On first use, the path resolver extracts the shipped `.db` payload to the writable root (once, idempotent, checksum-keyed) and opens the extracted file. If the content DB needs mutation, it is copied to a writable slot first.
- There is no custom `sqlite3_vfs`. There is no Unreal-aware locking layer. Shared-memory / WAL semantics are whatever the stock platform VFS provides.

This collapses §2.4, §7, and §11 into one consistent rule: **SQLite always sees a normal OS path; Unreal awareness lives only in the path resolver and the one-time extraction step.**

### 2.5 `.usqlite` Is The Only Authored Format

A module intended for team authoring and source control should not center its future on `.db` versus `.sqldump` timestamp arbitration, and should not split authored truth across two text formats.

Feasible target:

- `.usqlite/` is the **sole** committed authored form.
- `.db` files are derived runtime/cooked artifacts, never committed as source of truth.
- `.sqldump` becomes legacy one-way import only, to migrate existing projects into `.usqlite/`. It is not a round-trip format and is not emitted by new tooling.
- No separate `.sqlmigration` format exists. Migrations are first-class entries inside `.usqlite/` (see §8 for the format definition).

This directly improves mergeability and removes the ambiguity of "which text file is the truth."

### 2.6 Own The SQLite Amalgamation, Drop `SQLiteCore`

ExtendedSQL ships and links its own SQLite amalgamation (`sqlite3.c` / `sqlite3.h`) vendored under `Thirdparty/SQLite/`. The engine's `SQLiteCore` module is not a dependency of any ExtendedSQL module.

Feasible target:

- One translation unit compiles the amalgamation with a fixed, documented set of `SQLITE_*` defines.
- The committed baseline feature set is: `SQLITE_THREADSAFE=2` (multi-thread, one connection per thread), `SQLITE_ENABLE_FTS5`, `SQLITE_ENABLE_JSON1`, `SQLITE_ENABLE_RTREE`, `SQLITE_ENABLE_DBSTAT_VTAB`, `SQLITE_DEFAULT_FOREIGN_KEYS=1`, `SQLITE_DEFAULT_WAL_SYNCHRONOUS=1`, `SQLITE_OMIT_DEPRECATED`, `SQLITE_OMIT_LOAD_EXTENSION`.
- Symbols are namespaced or hidden so the vendored build cannot ODR-collide with any other plugin that links `SQLiteCore` in the same project. The amalgamation is compiled with `SQLITE_API=` set to a hidden-visibility decorator and linked only into ExtendedSQL modules; public headers do not re-export `sqlite3.h`.
- No code path references `SQLiteCore`, engine-provided SQLite headers, or `#if WITH_SQLITE_*` engine guards.

### 2.7 Hide The Bridge

A bridge layer can still exist for reflection and K2 expansion, but it is no longer a public product surface.

Concrete hiding rules:

- Bridge headers live under `UnrealExtendedSQL/Private/` and are not added to any module's public include path.
- Bridge `UFUNCTION`s are not `BlueprintCallable`. Where K2 nodes need them, they are `BlueprintInternalUseOnly=true` and marked `meta=(BlueprintInternalUseOnly="true")`.
- The bridge class is not exported (`MinimalAPI` is intentional; no `UNREALEXTENDEDSQL_API` on its type or methods).
- Public documentation, samples, and K2 node tooltips never name the bridge class. The visible user story is: `UESQLSubsystem` + `UESQLTableAsset` + editor K2 nodes.

---

## 3. What We Should Stop Preserving

The following directions are removal targets, not modernization targets. Existing users migrate once; there is no parallel deprecation period for most of them.

- `UESQLPlayerDBComponent`
- public player-scoped database APIs
- player-only K2 node families
- asset-owned async/runtime method growth
- large public bridge helper inventories
- the custom Unreal SQLite VFS (removed entirely, not narrowed)
- timestamp-based `.db` versus `.sqldump` source-of-truth arbitration
- `.sqlmigration` as a separate authored artifact
- round-trip `.sqldump` tooling (kept as one-way legacy import only)
- any reference to engine `SQLiteCore` or engine-provided SQLite headers

Trying to preserve these while also simplifying the module is what kept the earlier documentation inconsistent.

### 3.1 Migration-In For Existing Projects

Projects already using the current module keep working through one transition path:

- an editor commandlet / button reads existing `.db` and/or `.sqldump` inputs and emits a `.usqlite/` project.
- after migration, the legacy files are no longer read at runtime; the `.usqlite/` project is the source of truth.
- the migration tool is the only place `.sqldump` is still parsed.

---

## 4. Revised Product Shape

The revised module shape is straightforward:

### 4.1 Editor Layer

`UnrealExtendedSQLEditor` owns:

- asset creation
- schema validation
- authoring UX
- K2 node UX
- data import/export tooling

### 4.2 Public Runtime Layer

`UESQLSubsystem` owns:

- typed row operations
- raw SQL execution
- async dispatch (see §10 for the concurrency contract)
- transactions (see §10 for scope and thread affinity)
- migration application from `.usqlite/` migration entries
- snapshot helpers
- authority and routing checks (see §9)
- the logical-name → OS-path resolver, including one-time extraction of readonly shipped `.db` payloads
- local client-cache databases, opened under a reserved `local:` logical namespace by the same subsystem

### 4.3 Metadata Layer

`UESQLTableAsset` owns:

- row struct identity
- table identity
- logical database identity
- label metadata
- authoring metadata

### 4.4 Core Layer

Core SQL types own:

- connection lifetime
- statement lifetime
- typed value binding
- row/result representation
- serialization

### 4.5 Source-Of-Truth Layer

Source control owns:

- `.usqlite/` projects (schema, authored rows, and migrations — see §8)

Cooked/runtime artifacts own:

- derived `.db`
- caches
- snapshots

---

## 5. Revised Multiplayer Position

The earlier feasibility direction gave too much legitimacy to player-scoped database files as a first-class multiplayer abstraction.

That is not the best product decision.

Revised multiplayer stance:

- authoritative game state lives in server-owned runtime databases
- player-owned state is modeled inside shared authoritative tables, keyed by `PlayerId`
- replication and snapshot systems move rows or change sets, not database topology
- optional client-local caches are served by the **same** `UESQLSubsystem` under a reserved `local:` logical namespace; they are not a separate component, subsystem, or public API surface. They are plain databases that happen to resolve to the local client's writable root and are never routed to the server.

This change sharply reduces the amount of SQL-specific multiplayer surface area the module has to explain.

---

## 6. Revised Blueprint Position

Blueprint viability remains strong, but the Blueprint story must become smaller.

Blueprint should teach:

- get `UESQLSubsystem`
- pass a `UESQLTableAsset`
- use typed row/query nodes
- use `FESQLId` for row identity

Blueprint should stop teaching:

- public bridge-centric usage
- player database components
- mirrored player-only node families
- asset methods as the real runtime behavior owner

This makes the Blueprint surface more learnable and far easier to document.

---

## 7. Revised VFS And Filesystem Position

ExtendedSQL does not ship a custom `sqlite3_vfs`. The stock platform VFS is used for every open, on every platform.

Unreal awareness lives in exactly one place: the **path resolver** inside `UESQLSubsystem`. Its job is narrow and mechanical:

1. Take a logical database name (e.g. from a `UESQLTableAsset` or a direct subsystem call).
2. Classify it as writable game data, writable local cache (`local:` prefix), or readonly shipped content.
3. Produce an absolute OS path under the correct platform root:
   - writable game data → `FPaths::ProjectSavedDir() / "ExtendedSQL" / <name>.db` on desktop/editor, or the platform's persistent writable equivalent (`FPaths::ProjectPersistentDownloadDir()` on sandboxed targets).
   - writable local caches → same rule, but segregated under a `Local/` subfolder keyed by local user id.
   - readonly shipped content → locate the packaged payload through `IPlatformFile`; if it is not already a real OS file (e.g. it lives inside a pak), extract it once into a content cache folder under the writable root, keyed by content hash, then return the extracted path.
4. Hand the resolved absolute path to stock `sqlite3_open_v2`.

What this explicitly removes:

- no custom VFS registration
- no Unreal-aware locking or WAL shim
- no attempt to make SQLite read from inside a pak
- no "attachment" API that pretends a cooked content DB can be opened in-place

The only Unreal-aware I/O step remaining is the one-time extraction of readonly content DBs, and that step uses `IFileManager` / `IPlatformFile` to produce a normal file that SQLite then opens normally.

---

## 8. Revised Authored-Data Position: `.usqlite` Defined

The old design spent too much energy keeping a live relationship between binary DB files and text dumps. That is operationally weak for teams. The stronger plan is a single authored format.

### 8.1 What `.usqlite/` Is

A `.usqlite/` project is a **directory on disk**, not a single blob. Everything inside it is UTF-8 text, designed to merge cleanly in Git. It is the sole authored artifact; `.db` files are generated from it.

Directory layout:

```
<Name>.usqlite/
  project.json           # project manifest: name, schema version, settings
  schema/
    <table>.json         # one file per table: columns, types, PK/FK, indexes, label metadata
  data/
    <table>.ndjson       # one line per authored row, stable column order, deterministic ordering by PK
  migrations/
    0001_<slug>.sql      # forward SQL, numbered, append-only
    0002_<slug>.sql
    ...
  .usqlite-lock.json     # generated: content hash of each file above, used to detect merge conflicts and to key derived .db caches
```

Rules:

- **Text-first.** Every file is human-readable and diff-friendly. No binary blobs are committed under `.usqlite/`.
- **Deterministic emission.** The editor always writes rows, columns, and keys in a stable order so that re-saving an unchanged project produces a byte-identical tree.
- **One format, no siblings.** There is no separate `.sqlmigration` file type; migrations live under `migrations/` inside the project. There is no committed `.db`.
- **Append-only migrations.** Once a migration file is committed, its contents are frozen. Schema changes always produce a new numbered migration; hand-editing `schema/*.json` without a matching migration is a validator error.
- **Lockfile is generated.** `.usqlite-lock.json` is rewritten on every save and is the only file whose job is to be regenerated; merge conflicts in it signal a real authoring conflict elsewhere.

### 8.2 How `.db` Is Produced

- The editor and cook pipeline both build `.db` the same way: start from an empty database, apply `migrations/*.sql` in order, then load `data/*.ndjson` into the corresponding tables.
- The build is deterministic: identical `.usqlite/` input produces identical `.db` output (modulo SQLite page timestamps, which are normalized).
- Runtime never reads `.usqlite/`. Runtime only opens `.db`.

### 8.3 Legacy `.sqldump`

`.sqldump` is not an authored format anymore. It exists only as a one-way import into `.usqlite/` (see §3.1) and is never emitted by new tooling.

---

## 9. Authority And Routing Model

All database access goes through `UESQLSubsystem`, which enforces a single authority rule before touching a connection.

### 9.1 Database Scope Classes

Every logical database resolves to exactly one of three scopes, declared on the `UESQLTableAsset` or at open time:

- **`Shared`** — the authoritative game database. Exists only on the server / listen-server host. Writes and reads execute locally on the authority.
- **`Local`** — a client-local cache or preferences DB. Exists on whichever peer opened it. Never routed; each peer owns its own file.
- **`Readonly`** — shipped content DB. Opened read-only on any peer. No writes, no routing.

Scope is a compile-time property of the logical database name, not per-call.

### 9.2 The Routing Rule

For any subsystem call `Op(DB, ...)`:

1. Resolve `Scope(DB)`.
2. If `Scope == Shared`:
   - On the authority (`GetNetMode() != NM_Client`): execute locally.
   - On a client: the call is rejected at the subsystem boundary. Clients never open `Shared` connections. Clients obtain data through gameplay replication, not through SQL. There is no automatic client→server SQL RPC; that path is intentionally absent because it would re-introduce the "SQL as network protocol" antipattern.
3. If `Scope == Local`: execute locally on the caller. No authority check.
4. If `Scope == Readonly`: execute locally on the caller. Writes are rejected.

### 9.3 Enforcement

- Every public subsystem entry point begins with a scope check. Violations return a typed error result and log once; they do not crash.
- `Shared` writes additionally assert `HasAuthority()` on whatever context they were called from (subsystem pulls net mode from the owning `UWorld`).
- The routing rule is stated once, on the subsystem, and is not duplicated on table assets, components, or K2 nodes.

### 9.4 What Routing Does Not Do

- It does not RPC SQL statements.
- It does not attempt partial replication of query results.
- It does not attempt to keep a client-side mirror of `Shared` in sync.
- It does not own gameplay replication policy; that remains the job of actors, replicated structs, and snapshot systems.

---

## 10. Concurrency Model

The subsystem exposes a synchronous API and an async API, both backed by the same concurrency contract.

### 10.1 Connections And Threads

- One **writer connection** per logical database, owned by the subsystem, pinned to a dedicated **writer thread** (one worker thread per open database). All writes and all transactions execute on that thread.
- A small pool of **reader connections** per database, opened read-only against the same file. Readers may execute on any thread in the subsystem's task pool.
- Databases are opened in WAL mode (`journal_mode=WAL`, `synchronous=NORMAL`) so readers do not block the writer and vice versa.
- `SQLITE_THREADSAFE=2` is used: each connection is confined to a single thread at a time; the subsystem guarantees no connection crosses threads without serialization.

### 10.2 Synchronous vs Async API

- **Sync calls** on the writer thread execute inline. Sync calls from any other thread are a programming error and return an error result; the game thread must use the async API for writes.
- **Async calls** enqueue a work item on the owning database's writer queue (for writes/transactions) or the reader pool (for reads). Completion delegates are dispatched back to the game thread by default.
- The game thread never blocks on SQLite I/O through the public API. The only blocking wait is opt-in (`bWaitForCompletion=true` on shutdown helpers).

### 10.3 Transactions

- Transactions are **writer-thread-affine**. A transaction handle is bound to the writer thread of its database and cannot be used from another thread.
- Transactions are **not reentrant across async boundaries**. An open transaction must be committed or rolled back by the same logical operation that opened it; async operations issued while a transaction is open are serialized behind it on the writer queue.
- Nested `BeginTransaction` calls use `SAVEPOINT` internally so that gameplay code can compose transactional helpers without deadlock.
- If a transaction scope is abandoned (owning object destroyed, future dropped), the writer thread rolls it back on the next idle tick.

### 10.4 Ordering Guarantees

- Writes against a single database are serialized and execute in enqueue order (FIFO on the writer queue).
- Reads see a consistent snapshot at statement start (WAL semantics) and may freely interleave with writes.
- Cross-database ordering is **not** guaranteed; callers that need it must coordinate explicitly.

### 10.5 Shutdown

- On subsystem deinitialize (PIE end, game exit), the writer queue is drained with a bounded timeout. Remaining work items are cancelled with an error result; open transactions are rolled back.
- Reader connections are closed after writers. The vendored SQLite build is compiled with `SQLITE_OMIT_LOAD_EXTENSION` so there is no extension-unload ordering to worry about.

---

## 11. Revised Runtime API Position

A large `UESQLSubsystem` is feasible and desirable.

The fear that one subsystem becomes "too big" is less important than the cost of the current split.

The split is already expensive:

- duplicated docs
- duplicated K2 patterns
- duplicated async paths
- duplicated player-only variants
- unclear ownership of routing and transactions

A larger subsystem is a smaller product. That is the correct trade.

---

## 12. Revised Risk Assessment

### 12.1 Risks We Are Accepting

- the subsystem will become a substantial class
- removing the public player-surface is a hard break for existing users (mitigated by the migration commandlet in §3.1)
- owning a SQLite amalgamation increases maintenance responsibility
- `.usqlite/` requires new tooling, a deterministic emitter, and a validator
- `Shared`-scope clients cannot issue SQL; gameplay code must use replication instead

### 12.2 Why Those Risks Are Worth It

They buy:

- one public runtime story
- one authored format
- one storage path (stock SQLite OS VFS)
- honest feature guarantees (owned amalgamation, documented defines)
- a defined concurrency contract and a defined authority rule
- smaller Blueprint and K2 taxonomy
- cleaner multiplayer guidance
- far better documentation coherence

Those are higher-value wins than preserving the old surfaces.

---

## 13. Implementation Consequences

The feasibility reset directly implies the following code and tooling priorities:

1. move all typed runtime behavior onto `UESQLSubsystem`
2. stop expanding runtime behavior on `UESQLTableAsset`
3. delete player-scoped public surfaces; provide a one-time migration commandlet
4. route every open through the stock SQLite OS VFS via the subsystem path resolver
5. delete the custom `sqlite3_vfs` code path entirely; keep only the one-time extraction step for readonly shipped content
6. vendor one SQLite amalgamation with a fixed `SQLITE_*` define set; drop all references to `SQLiteCore` and engine SQLite headers
7. replace `.sqldump` as the design center with `.usqlite/` as the sole authored format (schema + data + migrations in one directory)
8. move the bridge into `Private/`, strip `BlueprintCallable` from its `UFUNCTION`s, and stop naming it in public docs
9. implement the authority/routing rule (§9) as a single subsystem-level gate
10. implement the writer-thread-per-database concurrency model (§10), with WAL, a reader pool, and game-thread completion dispatch

If those things do not happen, the module will keep carrying the same contradictions under a different set of docs.

---

## 14. What Still Looks Strong

Even after the reset, several parts of the original ambition remain good and worth keeping:

- typed table assets as an authoring concept
- Blueprint-first SQL workflows
- explicit transaction support
- snapshot and backup tooling
- server-authoritative multiplayer support
- optional readonly shipped content databases
- Unreal-aware import/export tooling (now targeting `.usqlite/` only)

The reset is not a rejection of the module. It is a rejection of the wrong shape for the module.

---

## 15. Final Feasibility Statement

ExtendedSQL is feasible, but the feasible version is more opinionated than the earlier plan.

The feasible version says:

- one runtime front door (`UESQLSubsystem`)
- metadata-first table assets
- no public player-scoped database model; local caches use the same subsystem under a `local:` namespace
- stock SQLite OS VFS for every open, on every platform
- one Unreal-aware step: logical-name path resolution plus one-time extraction of readonly shipped content
- `.usqlite/` as the sole authored format (schema + data + migrations in one directory)
- hidden, private bridge layer
- one owned SQLite amalgamation with a documented define set; no `SQLiteCore`
- a stated authority/routing rule and a stated concurrency contract

That is the design worth implementing.

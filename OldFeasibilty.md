# Historical Note

This file is historical. Prefer `ExtendedSQLiteFeasibility.md` for the current feasibility baseline and the `ExtendedSQL*.md` docs for the current public product story.

# ExtendedSQL — Harsh Feasibility Review for a Public FAB Release

> **Scope:** A brutally honest audit of `UnrealExtendedSQL` and `UnrealExtendedSQLEditor` as a *standalone* Unreal Engine SQLite plugin for the FAB marketplace. Goal: general-public usability in both Blueprint and C++, first-class multiplayer, and real version-control workflows (diff/merge on GitHub).
>
> Verdict up front: **The bones are good. The packaging, boundaries, naming, VCS story, and Blueprint UX are not marketplace-ready.** Today this is a private internal module wearing a plugin's clothes. Shipping it as-is would generate support tickets from day one.

---

## 0. TL;DR — What a Buyer Will Actually Experience

A buyer downloads this on Day 1 and runs into:

1. **They can't install just "SQL".** The `.uplugin` bundles EOS, PlayFab, Gameplay, AI, Animation, and 6+ other runtime modules. A SQLite buyer does not want GameplayAbilities, Niagara, OnlineSubsystemEOS, and DLSS force-enabled. See [UnrealExtendedFramework.uplugin](UnrealExtendedFramework.uplugin).
2. **Class prefix leaks the framework.** Every public type is `UESQL…`/`FESQL…`/`EESQL…`. "ESQL" is not a recognized product name; users expect `USQLite…` or `UExtSQL…`. Discoverability in the content browser and autocomplete is poor.
3. **Duplicate SQLite.** The build compiles against UE's `SQLiteCore`, but `Thirdparty/SQLite/sqlite3.c` is still shipped (see [Thirdparty/SQLite/](Thirdparty/SQLite/)). Either it's dead weight in the `.zip` or — worse — a cook-time timebomb if somebody re-enables it. Comment in [UnrealExtendedSQL.Build.cs](Source/UnrealExtendedSQL/UnrealExtendedSQL.Build.cs#L13) admits this.
4. **`StructUtils` dependency is UE 5.6-broken.** [UnrealExtendedSQLEditor.Build.cs](Source/UnrealExtendedSQLEditor/UnrealExtendedSQLEditor.Build.cs#L41) still lists `"StructUtils"` as a module, and [FESQLTableEditorToolkit.cpp](Source/UnrealExtendedSQLEditor/TableAssetEditor/FESQLTableEditorToolkit.cpp#L17) `#include "StructUtils/UserDefinedStruct.h"`. UE 5.6 deprecates the StructUtils plugin; this will fail the marketplace build bot.
5. **Zero sample content.** `Content/` has a font, particle, and settings tag — nothing SQL-related. No sample table asset, no sample struct, no demo map. Buyers need a 90-second "does it work?" test and they don't have one.
6. **No docs for buyers.** `DocsURL` is `"Will be added"`. `SupportURL` is empty. Marketplace submission will bounce on this alone.
7. **"Merge conflict resolution with git" is a slogan, not a feature.** `.sqldump` exports raw `INSERT OR REPLACE` lines with no row keys in diff-friendly form, a naive string-split parser, timestamp-wins sync, and no three-way merge tool. Two devs editing the same table on two branches = silent data loss on merge.

The sections below expand each of these and propose concrete fixes, ordered by severity.

---

## 1. Marketplace-Blocking Issues (fix before *any* submission)

### 1.1 The plugin is not a plugin — it's a framework bundle

`UnrealExtendedFramework.uplugin` forces 8 modules on every consumer, half of which are unrelated to SQL ([UnrealExtendedFramework.uplugin](UnrealExtendedFramework.uplugin)):

```
UnrealExtendedEditor, UnrealExtendedFramework, UnrealExtendedGameplay,
UnrealExtendedBackend, UnrealExtendedEOS, UnrealExtendedPlayFab,
UnrealExtendedSQL, UnrealExtendedSQLEditor
```

Plus forced plugin deps: `GameplayAbilities`, `Niagara`, `OnlineSubsystem*` (Null, EOS, Utils), `EOSShared`, `EOSVoiceChat`, `DLSS`, `EnhancedInput`.

A user who wants SQLite storage gets DLSS as a side effect. That is disqualifying for a FAB "database/storage" listing.

**Required fix — Option A (clean, recommended):**

- Ship `UnrealExtendedSQL` as its *own* plugin repo: `ExtendedSQLite` or `USQLite` (pick a short brand and own it).
- Keep the SQL code exactly where it is internally, but produce a second `.uplugin` at release time that includes only:
  - `UnrealExtendedSQL` + `UnrealExtendedSQLEditor`
  - The minimal `Content/` needed for SQL samples
  - Plugin deps: only `DeveloperSettings`
- Use a subtree/monorepo split script so internal dev stays single-source.

**Required fix — Option B (acceptable):**

- Keep one repo but split the `.uplugin`: ship `UnrealExtendedFramework.uplugin` as the bundle AND `UnrealExtendedSQL.uplugin` containing only the two SQL modules, with `"Category": "Database"` and a separate `FriendlyName` / `Description` / icon.

### 1.2 Metadata is empty

```
"DocsURL": "Will be added"
"MarketplaceURL": "Will not be added"
"SupportURL": ""
"Category": "Other"
```

FAB requires real values. Before submission:

- `DocsURL` → a real docs site (GitHub Pages, ReadTheDocs, or a single README on GitHub).
- `SupportURL` → issues page or email.
- `Category` → `"Database"` / `"Utilities"` (NOT "Other").
- A proper `Icon128.png` in `Resources/`.
- `Version` and `VersionName` incremented on every submission.

### 1.3 Duplicate SQLite shipping surface

- `UnrealExtendedSQL.Build.cs` adds `PublicDependencyModuleNames.Add("SQLiteCore")` AND `PrivateIncludePaths.Add("Thirdparty/SQLite")`.
- `Thirdparty/SQLite/sqlite3.c` + `sqlite3.h` are still in the repo but *not* compiled.
- You also set `PublicDefinitions` for `SQLITE_THREADSAFE`, `SQLITE_ENABLE_FTS5`, `SQLITE_ENABLE_JSON1`, `SQLITE_ENABLE_COLUMN_METADATA` — but these only affect code compiled *in this module*. `SQLiteCore` was already compiled with its own defines upstream, so your `FTS5`/`JSON1` defines are a lie unless you link your own amalgamation.

**Action:**

- Decide once: **use `SQLiteCore` OR ship our own amalgamation, not both.**
- If `SQLiteCore`: delete `Thirdparty/SQLite/` entirely, drop the `PublicDefinitions` (they're misleading), and document which SQLite version UE ships (and therefore what features the plugin actually supports — no FTS5, no JSON1 by default on UE's build).
- If own amalgamation: remove the `SQLiteCore` dependency, compile `sqlite3.c` inside this module, rename the third-party folder `sqlite-amalgamation-<version>/`, include a `LICENSE-sqlite.txt` (public domain notice), and gate `THIRD_PARTY_INCLUDES_START/END` around every include.
- **Recommendation: ship own amalgamation.** FTS5 is the single biggest reason people pick SQLite for game telemetry/search, and `SQLiteCore` doesn't give it. Also isolates you from UE upgrades that silently change the embedded version.

### 1.4 `StructUtils` module will fail UE 5.6 compile

See [UnrealExtendedSQLEditor.Build.cs](Source/UnrealExtendedSQLEditor/UnrealExtendedSQLEditor.Build.cs#L41):

```csharp
"StructUtils"           // UUserDefinedStruct
```

And [FESQLTableEditorToolkit.cpp](Source/UnrealExtendedSQLEditor/TableAssetEditor/FESQLTableEditorToolkit.cpp#L17):

```cpp
#include "StructUtils/UserDefinedStruct.h"
```

`UUserDefinedStruct` lives in `Engine` (not the StructUtils plugin). `FInstancedStruct` moved into `CoreUObject` in 5.6. This dependency is wrong on both counts.

**Action:**

- Drop `"StructUtils"` from `PrivateDependencyModuleNames`.
- Change include to `#include "Engine/UserDefinedStruct.h"`.
- Verify on UE 5.3 / 5.4 / 5.5 / 5.6 in CI.

### 1.5 No engine version matrix declared

`.uplugin` has no `"EngineVersion"` field. FAB listings target a specific engine version. Without it, a buyer on UE 5.3 installs it, it breaks against 5.6-only APIs, and leaves a 1-star review.

**Action:**

- Declare a supported range in the listing and add `"EngineVersion": "5.4.0"` (or whichever you baseline on).
- Stand up a GitHub Actions matrix building against the N most recent LTS engine versions.
- Add a `CHANGELOG.md` tracking engine bumps.

### 1.6 Zero starter content

`Content/` contains unrelated assets. A buyer will ask "ok, now what?" and close the editor.

**Action — minimum viable content:**

- `Content/ExtendedSQL/Samples/DT_ItemStruct.uasset` — sample `USTRUCT` row shape.
- `Content/ExtendedSQL/Samples/ST_Items.uasset` — sample `UESQLTableAsset` pre-wired.
- `Content/ExtendedSQL/Samples/BP_SQLDemo.uasset` — Blueprint actor demonstrating load/save/query.
- `Content/ExtendedSQL/Samples/L_SQLDemo.umap` — one-room demo map that spawns the Blueprint and prints results.
- `Content/ExtendedSQL/Samples/DA_QuickStart.uasset` — data asset with clickable "Run quick start" button (custom editor).

---

## 2. Architectural Criticisms

### 2.1 `UESQLSubsystem` is a God-object

Today it owns: connection registry, path resolution, scope/authority policy, sync SQL, async SQL dispatch, transactions, snapshots, player DB auto-lifecycle, PostLogin/Logout hooks, schema helpers (`CreateTable`, `AlterTableAddColumn`, `DoesTableExist`), row helpers (`InsertRow`, `UpdateRows`, `UpsertRow`, `SelectRows`, `DeleteRows`, `CountRows`), player-scoped variants of all of the above.

That's 4 responsibilities in one UObject. Problems:

- The `FCriticalSection` in `FESQLDatabase` serializes *everything* — see [ESQLDatabase.cpp](Source/UnrealExtendedSQL/Core/ESQLDatabase.cpp). A read on DB A holds a lock that blocks… nothing useful, but Blueprint devs will still write code paths that call it from ticks.
- Authority policy is enforced at `OpenDatabase` time only — once a client holds a `Server`-scope handle, there's no runtime re-check.
- `CachedNetMode` is captured at `Initialize()` time and never refreshed when PIE net mode changes.

**Action — split into composition:**

```
UESQLSubsystem           → lifecycle only (open/close, registry)
FESQLConnection          → the per-DB handle (rename of FESQLDatabase)
FESQLAuthorityPolicy     → static functions, unit-testable, no UObject
FESQLPathResolver        → static functions, unit-testable
UESQLQueryService        → optional BlueprintFunctionLibrary for raw SQL
UESQLSnapshotService     → separate subsystem for save/load slot workflows
UESQLPlayerService       → PostLogin/Logout hooks, per-player DB
```

Each piece is independently testable. The public Blueprint API reduces from "1 subsystem with 60 functions" to "5 clear surfaces with 10 functions each".

### 2.2 `UESQLTableAsset` is *also* becoming a God-object

Look at the API surface (from the subagent read): `InsertRowFromStruct`, `GetAllRows`, `GetRowById`, `LoadRowIntoStruct`, `LoadRowsIntoStructArray`, `LoadPageIntoStructArray`, `SaveRowFromStruct`, `DeleteRowById`, `DoesRowExist`, `ResolveRowDisplayText`, plus player-scoped variants of each, plus async variants of each (background thread dispatch + game-thread callback).

That's ~30 methods on one UCLASS. The typed row API duplicates what `UESQLBlueprintBridge` exposes via CustomThunks. **Pick one.**

**Action:**

- Make `UESQLTableAsset` contain *data only* — schema, PK column, label column, scope, persistence, source struct.
- Move every runtime operation to a separate `UESQLTableService` BlueprintFunctionLibrary that takes a `UESQLTableAsset*` as its first argument.
- Delete per-method async twins; have *one* async action `USQLAsyncRun` that accepts any table service call as a delegate. Fewer K2 nodes, better discoverability.

### 2.3 `FESQLRow` as `TMap<FString, FString>` is a long-term trap

[ESQLTypes.h](Source/UnrealExtendedSQL/Shared/ESQLTypes.h) stores *every* column value as an `FString`. Pros: Blueprint-friendly. Cons:

- Integer columns get round-tripped through `FString` twice per query.
- BLOB columns are impossible to store without base64 or lossy text conversion.
- Memory cost of a 100k-row result is ~3–5× what it should be.
- You lose type information — callers have to re-parse from string in downstream code.

**Action — keep the string view for Blueprint, add a typed view for C++:**

```cpp
USTRUCT(BlueprintType)
struct FESQLValue
{
    EESQLColumnType Type;
    int64   AsInt = 0;
    double  AsDouble = 0.0;
    FString AsText;
    TArray<uint8> AsBlob;
    bool    bIsNull = true;
};

USTRUCT(BlueprintType)
struct FESQLRow
{
    TArray<FESQLValue> Columns;  // dense, positional
    TMap<FName, int32> NameToIndex;  // optional, built once per result
    // Keep TryGetString / TryGetInt / TryGetBlob helpers for back-compat.
};
```

This is the single biggest internal change, but it unlocks blob storage (save thumbnails, binary save slots, replay blobs) which is a *huge* selling point over DataTable.

### 2.4 Raw `sqlite3_exec` for PRAGMAs breaks the "no multi-statement" rule

[ESQLDatabase.cpp:220,232](Source/UnrealExtendedSQL/Core/ESQLDatabase.cpp#L220) uses `sqlite3_exec` for `PRAGMA journal_mode=WAL` / `PRAGMA foreign_keys=ON`. The header [ESQLDatabase.h:22](Source/UnrealExtendedSQL/Core/ESQLDatabase.h#L22) explicitly says "NEVER sqlite3_exec". This is self-inconsistent.

**Action:** Prepare/step the PRAGMAs like any other query. No `sqlite3_exec` anywhere.

### 2.5 VFS is incomplete for shipping builds

[ESQLUnrealVFS.cpp](Source/UnrealExtendedSQL/Core/ESQLUnrealVFS.cpp) wraps `IFileHandle` — fine on desktop. But:

- No `.pak` / `PakFile` read path. Read-only DBs inside cooked content (the obvious shipping case) will try to use `IFileHandle::OpenRead` on a virtual pak path and fail on consoles.
- No xFullPathname / xCurrentTime / xGetLastError implementations shown (need to verify).
- No sector-size probing; SQLite assumes 4096 when VFS returns default.
- `ReadOnly` path intentionally bypasses the Unreal VFS and uses the OS default — see [ESQLDatabase.cpp](Source/UnrealExtendedSQL/Core/ESQLDatabase.cpp). On consoles and mobile, there *is no* OS VFS mapped to cooked pak data. This design will work in editor and break on Switch/iOS.

**Action:**

- Implement an `FPakFileReadVFS` specialization for read-only open from `.pak`.
- When `Persistence == Persistent` AND packaged AND path resolves inside `Content/`, extract the DB to `FPaths::ProjectPersistentDownloadDir()` on first open, then use Unreal VFS from there (or keep it read-only if the game never writes).
- Document the three concrete deployment modes: (a) editor-authored .db shipped in pak, (b) runtime-authored .db in Saved/, (c) downloadable content .db unpacked at runtime.

### 2.6 Statement cache has no eviction

`FESQLDatabase::GetOrPrepare` caches every unique SQL string forever (per the subagent summary). Analytics/logging systems that build SQL with embedded values (bad practice, but common) will leak `sqlite3_stmt*` handles until the connection closes.

**Action:**

- LRU cap at `UESQLSettings::MaxCachedStatements` (default 128).
- Log + warn once when eviction fires, and again if hit rate drops below 50%.
- Document "always parameterize" in the user guide.

---

## 3. Multiplayer: Good Intent, Weak Execution

The `EESQLDatabaseScope` enum (Local / Server / PlayerScoped) is the right abstraction. The implementation has real holes.

### 3.1 `PlayerScoped` uses `UniqueNetId` as a folder name

Path: `Saved/Databases/Players/<UniqueNetId>/<Name>.db` (per [ESQLSettings.h](Source/UnrealExtendedSQL/Shared/ESQLSettings.h)).

Problems:

- Steam/EOS IDs are fine, but OSS plugins can emit IDs containing `|`, `:`, `/`, `\` (e.g., `EOS|abc:local`).
- Filesystem-unsafe characters on Windows → silent open failure.
- PII concerns on some platforms (EOS IDs can be loggable, sometimes not).
- No sharding — 10,000 players → 10,000 sibling folders in one directory → NTFS enumeration stalls.

**Action:**

- `ResolvePlayerDatabaseFilePath` should hash the raw net ID to a hex digest, then shard `ab/cd/abcd1234…`.
- Keep a sidecar `.json` mapping `displayName -> hashedId` for debugging.
- Add a project setting `bHashPlayerIds` (default true) with an explicit opt-out for devs who want human-readable folders.

### 3.2 No concurrency policy between clients

The docs say "Server-authoritative", but nothing prevents the client from:

- Opening a `Local` DB with the same file path the server is using in listen-server PIE.
- Calling `ExecuteSQL` on a `Server`-scope DB *from the client* after `bEnforceServerAuthority=false` (no runtime guard — it's a static check at open time).
- Two clients trying to open `PlayerScoped` databases concurrently on a shared dev machine.

**Action:**

- Add an `FESQLAuthorityGuard` RAII that re-checks `GetNetMode()` on *every* write.
- Add a `OpenFlags` bitfield: `ReadOnly | WriteAhead | SharedCache | MultiProcess`.
- Surface `SQLITE_BUSY` handling — today, if two processes touch the same WAL file, the second one just silently fails a query. Expose a `BusyTimeoutMs` setting and set it via `sqlite3_busy_timeout`.

### 3.3 No replication hooks

A marketplace plugin marketed as "multiplayer-native" has to answer "how do I replicate a row change from server to client?". Today the answer is "you don't — write your own RPC that calls `ExecuteSQL` on the server".

That's fine for dedicated-server-only designs but it leaves:

- Listen server clients reading from nothing.
- Clients needing a read cache of server data.
- No "SQL-backed replicated struct" primitive.

**Option — small, high-impact addition:**

- `UESQLReplicatedTableComponent` — replicates a `TArray<FESQLRow>` snapshot via a single RPC; on-client writes are rejected unless the component is flagged `ClientAuthoritative`.
- `FESQLChangeSet` — diff of `{inserted, updated, deleted}` row IDs broadcast via multicast.
- This is a major feature but it turns "SQLite for Unreal" into "replicated SQLite for Unreal", which is genuinely differentiated on FAB.

### 3.4 Seamless travel is not covered

`UESQLSubsystem` is a `GameInstanceSubsystem` — survives seamless travel — but the `PostLogin/Logout` hooks are bound to the *current* GameMode. After a seamless travel the new GameMode won't have the hooks, and connected player DBs silently stop auto-opening.

**Action:**

- Bind to `FGameModeEvents::GameModePostLoginEvent` (global) instead of the per-GameMode delegates.
- Or expose an explicit `UESQLPlayerService::RebindLifecycle(AGameModeBase*)` for users to call from `OnPostSeamlessTravel`.

---

## 4. Source Control & Merge Conflict Reality Check

This is the feature the user asked for. Today it does not deliver.

### 4.1 `.sqldump` format is not merge-friendly

The current exporter (see [ESQLDumpPipeline.cpp:1-120](Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.cpp#L1)) outputs:

```sql
-- ExtendedSQL Database Dump
-- Timestamp: 2026-04-18T…
CREATE TABLE IF NOT EXISTS "Items" (...);
INSERT OR REPLACE INTO "Items" ("Id","Name","Price") VALUES ('sword','Iron Sword','50');
INSERT OR REPLACE INTO "Items" ("Id","Name","Price") VALUES ('potion','Health','10');
```

Why this merges badly in git:

1. **Timestamp in the header** → every export produces a different first line. Every developer gets a line-1 conflict on every push.
2. **All columns on one line** → a one-column edit becomes a whole-row diff. Git sees "the line changed" but a human can't tell *which column*.
3. **`INSERT OR REPLACE`** masks real conflicts. If dev A deletes a row and dev B edits it, the merged dump just re-inserts B's version. Data loss on merge, no warning.
4. **`ORDER BY <first column>`** assumes the first column is the PK. [ESQLDumpPipeline.cpp:79](Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.cpp#L79) hard-codes that assumption. If the table's real PK is the 3rd column, the dump is unstable across runs.
5. **No per-row PK marker** — a git merge that re-orders two adjacent rows is indistinguishable from a row edit.
6. **Naive parser** — the importer splits on `;`. Any column value containing `;` inside a string literal corrupts on import (the subagent already flagged this; `'hello; world'` becomes two broken statements).

**Action — redesign the dump format:**

A line-oriented, canonical, diffable format. Each row gets *multiple* lines keyed by PK:

```
@@table Items
@@schema CREATE TABLE "Items" ("Id" TEXT PRIMARY KEY, "Name" TEXT, "Price" INTEGER)
@@row Id=sword
  Name   = "Iron Sword"
  Price  = 50
@@row Id=potion
  Name   = "Health"
  Price  = 10
```

- One row = one `@@row <PK>` block.
- One column = one line.
- Deterministic order: rows sorted by PK byte-wise; columns by schema order.
- Comment block with git hash + schema version at top, no timestamp.
- Parser is line-oriented, immune to semicolons in values.
- **Merge semantics actually work:** git sees per-column line diffs; two devs editing different columns of the same row produce a clean 3-way merge.

Call it `.esqldump` or `.db.txt` — anything but `.sqldump`, which is an existing sqlite tool name and will confuse users.

### 4.2 Add a real three-way merge driver

Once the format is line-oriented, ship a git merge driver:

```
# .gitattributes
*.esqldump merge=esqldump diff=esqldump
```

- `esqldump` merge driver = a small command-line tool in the plugin's `Programs/` directory that does a schema-aware 3-way merge: schema conflicts abort, row conflicts inside the same cell abort, row conflicts on different cells merge, insert/delete conflicts are reported with a conflict marker.
- Optional diff driver for better `git diff` output.
- Can be written as a Python script, or — better — as a C++ Commandlet (`ExtendedSQLMergeCommandlet`) so it runs with `UnrealEditor-Cmd.exe`.

That turns "merge-friendly VCS" from marketing copy into a concrete feature.

### 4.3 Schema versioning / migrations

Today, [UESQLTableAsset::SyncSchema](Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.cpp) infers columns from the struct and issues `ALTER TABLE ADD COLUMN` silently. Problems:

- Renames are detected as "drop old + add new" — data loss.
- Type changes are detected as… undefined (`TEXT` ↔ `INTEGER` coercions happen silently).
- No version stamp on the DB, so after a struct change, a dev who pulls main without realizing the schema moved will silently corrupt their local `.db`.

**Action:**

- Store `PRAGMA user_version = N` in the DB.
- Every `UESQLTableAsset` carries `SchemaVersion` (editor-bumped on any struct change).
- Expose a `UESQLMigration` data asset: each entry is `(FromVersion, ToVersion, TArray<FString> SqlSteps)`.
- On open, if `user_version` < asset version → run migrations in a transaction.
- Fail loudly if no migration path exists.
- Provide a Blueprint/Python-callable "Dump before migrate" option by default.

### 4.4 `.db`, `.db-wal`, `.db-shm` must be gitignored

Documented in [ESQLDumpPipeline.h](Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.h) (`GetGitIgnoreRecommendation`) but not enforced. Buyers will forget, check in a `.db-wal`, and corrupt their teammate's state.

**Action:**

- On first editor launch after install, detect the `.gitignore` at project root; if it exists and is missing the recommended lines, pop a one-time modal offering to append them. Decline is remembered.
- Same check for `.gitattributes` (for the `esqldump` merge driver).

### 4.5 Bidirectional timestamp sync is a race condition

[FESQLDumpPipeline::SyncDatabaseAndDump](Source/UnrealExtendedSQL/VCSPipeline/ESQLDumpPipeline.cpp) compares file mtimes: newer wins. This is wrong when:

- Dev pulls main (dump has new mtime because git touched it), local .db is ahead → dump overwrites local work.
- Clock skew across dev machines → younger file has older mtime.
- Editor auto-save writes .db while git is mid-checkout.

**Action:**

- Stop using mtimes. Store a content hash of the dump inside the DB (`PRAGMA user_version` is too small — use a `_esql_meta` table with `{dump_sha1, schema_version, last_write_utc}`).
- On open: compare `_esql_meta.dump_sha1` vs. current dump file SHA1. If they differ AND the DB has local uncommitted changes (track via a dirty bit), *prompt the user* — don't silently pick a winner.
- "Newest wins" is never acceptable for dev data.

---

## 5. API / DX Criticisms (Blueprint and C++ both)

### 5.1 Raw SQL exposed to Blueprint is a trap

`UESQLSubsystem::ExecuteSQL(DbName, SQL)` takes an `FString` SQL and is `BlueprintCallable`. This lets any junior Blueprinter write:

```
Execute SQL: "DELETE FROM Players WHERE Name = '" + PlayerName + "'"
```

And you have a SQL injection in a game. Not hypothetical — this is the single most common mistake in SQLite tutorials.

**Action:**

- Keep `ExecuteSQL` but mark it `AdvancedDisplay` and `meta=(BlueprintInternalUseOnly="true", AllowPrivateAccess="false")` so it doesn't appear in normal autocomplete.
- Force Blueprint users through `ExecuteSQLWithBindings(SQL, TMap<FString, FESQLBindingValue>)` or the typed table API.
- Add a lint rule in the editor: if a Blueprint node's SQL text contains a `+` concat or a `Format` call at compile time, emit a compile warning "SQL appears to be string-concatenated — use parameter bindings".

### 5.2 `FESQLQueryResult.bSuccess` check is forgotten 80% of the time

Every sync call returns `FESQLQueryResult` (bSuccess + rows + error). Blueprint users consume `.Rows` and ignore `.bSuccess`. On failure they silently get an empty array and wonder why their inventory is empty.

**Action:**

- Use UBlueprintAsyncActionBase `OnSuccess` / `OnFailure` pins as the primary Blueprint surface. This is the only pattern Blueprint users reliably handle correctly.
- For sync calls, return `EESQLResult { Success, SchemaError, SqlError, NotFound, AuthorityDenied, DatabaseClosed }` directly and force the caller to branch on it (Blueprint `Switch on Enum`).

### 5.3 K2 nodes reconstruct the world on every pin change

The typed K2 nodes (`UK2Node_ESQLTypedRowBridgeBase` family) call `ReconstructNode` on `PinDefaultValueChanged` / `PostReconstructNode`. That flashes the node, breaks wires, and re-runs graph validation. In a large blueprint with 20 SQL nodes this is noticeable lag when opening the graph.

**Action:**

- Cache the resolved struct per `TableAsset*` across all nodes in the graph.
- Only reconstruct if the struct identity *actually* changed — `GetFName()` compare, not pointer compare.
- Defer reconstruction to next tick via `GEditor->GetTimerManager()` so multi-pin edits in quick succession collapse into one.

### 5.4 `MakeQuerySpec` node is the wrong abstraction

The visual query builder node ([ESQLQueryBuilderK2Nodes.h](Source/UnrealExtendedSQLEditor/K2Node/ESQLQueryBuilderK2Nodes.h)) adds filter/sort clauses inline on one node. Problems:

- 8-clause hard limit.
- Nested `AND`/`OR` groups are impossible.
- No "contains"/"starts with"/"in list" operators visible from the graph.
- Once you have 4+ filter clauses the node is 800px wide and hides connections.

**Action — two-tier UX:**

- **Tier 1 (simple queries):** Keep the inline node but cap at 4 clauses. "More than 4 filters → use Tier 2".
- **Tier 2 (power users):** A proper `UESQLQuery` Blueprint object (factory in content browser) with a dedicated editor tab. Authored once, reused by reference. Same editor shows the generated SQL preview, live "run against editor DB" result count, and EXPLAIN QUERY PLAN output.

### 5.5 Blueprint naming mismatches C++ conventions

Blueprint pins say "Sql Id", "Sql Row", "Sql Bindings". C++ says `FESQLId`, `FESQLRow`, `FESQLBindingValue`. The `E` prefix ("Extended") is invisible in Blueprint which means users can't map BP nodes back to C++ classes when they try to extend behavior.

**Action (breaking, one-time rename):**

- Settle on a single user-facing brand. If the plugin is `ExtendedSQLite`, types become `FSQLId`, `FSQLRow`, `USQLTable` (drop the `E`).
- If the brand is `USQLite`, types become `FUSQLId`, `UUSQLTable` (ugly — don't pick this).
- Recommendation: **`ExtendedSQLite`**, types **`FSQLId`, `FSQLRow`, `USQLTable`, `USQLSubsystem`, `USQLConnection`**.

### 5.6 No reactive / observer pattern

Games need "inventory changed → refresh UI". Today users have to poll or wrap every write in a manual delegate broadcast.

**Action:**

- `FOnSQLRowChanged` multicast on `USQLTable` — fired after `SaveRow`/`DeleteRow`.
- `FOnSQLTableChanged` for bulk operations.
- UMG-friendly `USQLTableBindingComponent` that spawns `OnRowsLoaded` + `OnRowChanged` events and auto-refreshes bound List Views.

---

## 6. Performance Concerns (will bite at scale)

### 6.1 Single mutex per connection = serialization

`FESQLDatabase::CriticalSection` wraps every public method. In games with a main-thread UI reading a DB while a background thread writes telemetry, this is straight contention.

**Action:**

- Use SQLite's shared-cache + `WAL` + `SQLITE_OPEN_NOMUTEX` and do per-query reader/writer separation in C++.
- Provide an `FESQLReadOnlySnapshot` handle that holds a read transaction and never blocks writers (WAL readers are lock-free).
- Document the concurrency model in the README with a diagram.

### 6.2 Async on thread pool, not the dedicated SQL thread

[UESQLBlueprintBridge](Source/UnrealExtendedSQL/Blueprint/ESQLBlueprintBridge.h) async calls use `Async::RunOnBackgroundThreadPool`. If a user fires 200 async queries in one frame, they spawn 200 pool tasks, each fighting the same mutex.

**Action:**

- Dedicated FRunnable per DB connection + `TQueue<FESQLJob>`. Queries execute serially on that one thread, results marshaled back to game thread.
- Exposes a consistent concurrency model: "one DB = one SQL thread".

### 6.3 Reflection-based serialization is slow per row

[FESQLPropertySerializer](Source/UnrealExtendedSQL/Shared/ESQLPropertySerializer.h) reflects properties for *every* row save/load. For 10k-row loads this hurts.

**Action:**

- Cache the `(UScriptStruct*, FProperty*) -> ColumnName/Bind/Read` mapping per struct type, keyed by `GetFName()`. First save/load builds the cache; subsequent operations are pointer dereferences.
- Optionally code-gen the mapping via UHT `UFUNCTION(SpecifierSQLColumn="...")` on struct members, but that's a big lift.

### 6.4 JSON for arrays/maps is unqueryable

TArray/TMap fields are stored as JSON TEXT. You cannot `SELECT … WHERE Tags CONTAINS 'rare'`. That's fine as a baseline but it should be documented as a limitation, and the plugin should offer at least one alternative:

**Options:**

- Junction tables: `UESQLTableAsset` auto-creates `<Table>_<ArrayField>` junction tables for array members. Queryable.
- JSON1 extension: if the plugin ships its own SQLite with `SQLITE_ENABLE_JSON1`, expose `JSON_EXTRACT` helpers. Users get `WHERE Tags LIKE '%rare%'` or `WHERE json_extract(Tags, '$[0]') = 'rare'` out of the box.

---

## 7. Editor UX Criticisms

### 7.1 DataTable-adapted grid is missing DataTable features

Per the subagent, [FESQLTableEditorToolkit](Source/UnrealExtendedSQLEditor/TableAssetEditor/FESQLTableEditorToolkit.h) adapted Epic's `FDataTableEditor`. Missing vs DataTable:

- Column sort (click header).
- Column resize persistence.
- CSV import (the Build.cs pulls `DesktopPlatform` but I don't see an import path).
- "Find in table" / row find-navigator.
- Copy/paste rows between tables.
- Diff against checked-in `.esqldump`.

**Action:** a two-week pass making the editor feature-parity with DataTable, then one week adding SQL-specific features (column add/drop/rename UI, EXPLAIN QUERY PLAN panel, raw SQL scratchpad tab).

### 7.2 Row editor writes on every property change

`SESQLRowEditor` (per the subagent) writes back to DB on every property change. Problems:

- No batching — typing "Sword Of Undying" in a name field issues 16 `UPDATE` statements.
- No undo integration beyond `FEditorUndoClient` — intra-cell text undo is broken.
- No validation — editing an integer column as text stores garbage until next schema sync.

**Action:**

- Debounce writes (250ms).
- Buffer edits into an `FTransaction` for proper undo.
- Validate against column type before writing.
- "Dirty" indicator on the row; explicit "commit row" button for users who want manual control.

### 7.3 Validation dialog is one-shot, not continuous

`SESQLValidationDialog` is shown once on asset create. If a user adds a `TArray<UObject*>` field to the struct six months later, the next `Initialize()` will silently fail.

**Action:**

- Validate on every struct change via `INotifyOnStructChanged` (already registered in the toolkit).
- Show persistent error badges on the asset icon in the content browser when validation fails.
- `UESQLTableAsset::GetAssetRegistryTags` surfaces the validation state so asset filters can "show broken tables".

### 7.4 `SGraphPinESQLId` picker is stale-prone

The FESQLId picker caches queries by DB file mtime. If a teammate edits the DB from another editor instance, your picker is stale until you click a different node and come back.

**Action:**

- Watch the DB file with `FFileManager` change notifications.
- Invalidate cache on `WM_FILECHANGE` or inotify equivalent.
- Show a subtle "refresh" icon in the picker header.

### 7.5 No "connect to external SQLite" mode

A lot of buyers will have an existing `.db` from Django/Rails/whatever and want to browse it in the Unreal editor. Today they can't — they must create a `UESQLTableAsset` pointing at a struct they don't have.

**Action:**

- Add a "SQL Database Browser" standalone editor window (like Stats or Output Log).
- Takes any `.db` path, lets the user run queries, browse tables, and optionally generate a `UScriptStruct` + `UESQLTableAsset` from a selected table schema.
- This alone triples the plugin's addressable audience.

---

## 8. Cross-Platform & Shipping Blockers

### 8.1 Platform support not declared

`.uplugin` has no `"PlatformAllowList"`. Implicitly supports everything. Realistically:

- **Works:** Win64, Linux, Mac (native filesystem).
- **Untested:** Android, iOS (need to verify `IFileHandle` paths inside app sandboxes).
- **Broken:** Console platforms without special pak integration (see §2.5).
- **HTML5/WebGL builds:** SQLite persists nowhere useful; must use IndexedDB VFS (doesn't ship).

**Action:**

- Declare platforms explicitly.
- Add a "supported platforms" matrix to the README.
- Mark HTML5 as "not supported" (or ship an IndexedDB VFS, but that's a separate 2-month project).

### 8.2 Cooking behavior is unspecified

Does `UESQLTableAsset.Scope = Local` and a pre-populated editor `.db` end up in the cooked pak? Where? The plugin currently has no `UAssetManager` integration, no `GetAssetRegistryTags` surface for the `.db` path, and no `UObject` reference to the `.db` file from the asset. Meaning: on cook, the `.db` is *not* copied into the pak unless the user manually adds it to `AdditionalNonAssetDirectoriesToCopy` in `DefaultGame.ini`.

**Action:**

- Implement `UESQLTableAsset::GetAssetDependencies` returning a virtual dependency on the `.db` (or `.esqldump`).
- Use `FContentDirectoryMonitor` or `ICookedPackageWriter` hook to stage the `.db` into `<Game>/Content/Databases/` at cook time.
- For `.esqldump`-based workflows: rebuild the `.db` at cook time from the dump (already intended per `FESQLDumpPipeline::RebuildDatabaseFromDump`) and stage *only* the `.db` — never ship the dump.
- Document this on a dedicated "Packaging & Cooking" page.

### 8.3 Save game integration missing

Unreal users *expect* SQLite-as-savegame. Today the plugin has snapshot save/load but no `USaveGame` bridge.

**Action:**

- `USQLSaveGame` subclass: wraps any number of `USQLTable` assets, serializes them as one `.sav` file (zip of embedded `.db`).
- One-liner: `USQLSaveGame::SaveToSlot("Slot01")` + `USQLSaveGame::LoadFromSlot("Slot01")`.
- This is the most-requested feature for SQL-in-Unreal based on every forum thread ever.

---

## 9. Security

### 9.1 SQL injection surface

Already covered in §5.1. The subagent also noted `ESQLK2FieldFilterUtils::IsSafeIdentifier` exists but isn't audited. Audit it against OWASP's SQL injection cheat sheet and document it.

### 9.2 Path traversal

`UESQLSettings::ResolveDatabaseFilePath(Name)` — if `Name` comes from user input (modding, user save slot name) and contains `..`, does it escape the Saved/ directory? Needs review.

**Action:**

- `FPaths::ValidatePath` or a hardcoded regex: `[A-Za-z0-9_\-]{1,64}` for DB names.
- Reject absolute paths. Always re-anchor under `FPaths::ProjectSavedDir()`.
- Unit test with `../../../../etc/passwd` type inputs.

### 9.3 PRAGMA hardening for untrusted DBs

If a user loads a `.db` from untrusted sources (UGC, downloaded saves), a crafted DB can exploit SQLite bugs (CVE-2022-35737, CVE-2020-13434, etc.). Before opening untrusted DBs:

```sql
PRAGMA trusted_schema = OFF;
PRAGMA cell_size_check = ON;
PRAGMA integrity_check;
```

Expose an `OpenUntrusted()` overload that runs these automatically.

### 9.4 No encryption story

SQLite doesn't ship with SEE (SQLCipher is a fork). Marketplace buyers targeting mobile ask "can I encrypt save data?" every time.

**Action:**

- At minimum, document "SQLCipher integration is possible but not shipped; see docs/encryption.md".
- Better: ship an optional `FESQLEncryptionProvider` interface with a reference `FAESEncryptionProvider` that XORs pages with an `FAES` key (weak but trivially deployable), plus a `LICENSE.md` mention that production-grade encryption requires SQLCipher.

---

## 10. Documentation & Onboarding Gap

### 10.1 There is no user-facing doc

Three `.md` files exist ([ExtendedSQL.md](ExtendedSQL.md), [ExtendedSQLBlueprint.md](ExtendedSQLBlueprint.md), [ExtendedSQLImplementation.md](ExtendedSQLImplementation.md)) — all author-facing design docs, not user guides. [README.md](README.md) talks about the framework, not SQL.

**Required docs before ship:**

| Doc                        | Audience                | Length                |
| -------------------------- | ----------------------- | --------------------- |
| `docs/QuickStart.md`     | First-5-minutes user    | 1 page                |
| `docs/Blueprints.md`     | Blueprint-only dev      | 5 pages               |
| `docs/CPlusPlus.md`      | C++ dev                 | 3 pages               |
| `docs/Multiplayer.md`    | MP dev                  | 3 pages               |
| `docs/VersionControl.md` | Team lead               | 2 pages               |
| `docs/Migrations.md`     | Anyone shipping updates | 2 pages               |
| `docs/Packaging.md`      | Release engineer        | 1 page                |
| `docs/FAQ.md`            | Everyone                | 2 pages               |
| `docs/APIReference.md`   | Advanced users          | auto-gen from Doxygen |

Plus at least *three* YouTube videos: (1) create first table, (2) player save in 10 minutes, (3) resolve your first merge conflict.

### 10.2 No inline tooltips

`UPROPERTY` and `UFUNCTION` declarations need `meta=(Tooltip="…")` on every public member. Today the settings class and FESQLId have exactly one `ToolTip` each. Blueprint users see "DatabaseName" with no explanation of what it is or where the file ends up.

**Action:** lint pass adding a one-sentence tooltip to every `BlueprintReadWrite`, `BlueprintCallable`, `EditAnywhere`.

---

## 11. The Full "Proper SQLite for Unreal" Plugin — What It Should Contain

If we take a blank sheet of paper and ask "what would the definitive SQLite plugin for UE look like?", here is the feature list. Items marked `[NOW]` exist today, `[MISSING]` do not.

### Core runtime

- `[NOW]` SQLite amalgamation or SQLiteCore
- `[NOW]` Custom VFS via `IFileHandle`
- `[MISSING]` PakFile read-only VFS
- `[NOW]` RAII connection + prepared statement wrappers
- `[MISSING]` Dedicated SQL thread per connection (FRunnable + TQueue)
- `[NOW]` Statement cache (needs LRU cap)
- `[NOW]` Transaction API
- `[NOW]` Backup/restore (snapshots)
- `[MISSING]` Explicit read transactions / shared snapshots
- `[MISSING]` Busy timeout config

### Typed API

- `[NOW]` `USQLTable` data asset with struct-derived schema
- `[NOW]` Typed row load/save via reflection
- `[PARTIAL]` Async variants (exist, but per-operation; should be unified)
- `[MISSING]` Observer delegates (OnRowChanged, OnRowInserted, OnRowDeleted)
- `[MISSING]` Cached typed struct accessor (perf)
- `[MISSING]` Relationship/foreign key helpers (JOIN via `FSQLId` chain)
- `[MISSING]` Array field junction tables
- `[MISSING]` Computed columns / virtual tables

### Blueprint UX

- `[NOW]` K2 node for typed rows
- `[NOW]` K2 query builder (needs more filter operators)
- `[MISSING]` UBlueprintAsyncAction unified "SQL Call" node
- `[MISSING]` FESQLId picker in shipping (runtime picker UI for modding)
- `[MISSING]` UMG binding components
- `[MISSING]` Replicated table component
- `[MISSING]` Save game bridge

### Editor

- `[NOW]` Grid-style table editor (needs DataTable parity)
- `[MISSING]` "SQL Database Browser" standalone window (open any `.db`)
- `[MISSING]` Query runner tab (EXPLAIN QUERY PLAN)
- `[MISSING]` Schema diff viewer (this branch vs. main)
- `[MISSING]` Migration authoring tool
- `[MISSING]` CSV import/export
- `[MISSING]` Asset registry tags for broken tables
- `[MISSING]` Content-browser "Create from existing .db" factory

### Multiplayer

- `[NOW]` Database scope enum (Local/Server/Player)
- `[NOW]` PostLogin/Logout auto-managed player DBs
- `[MISSING]` Seamless travel safety
- `[MISSING]` `UESQLReplicatedTableComponent`
- `[MISSING]` Server-authoritative write guard (runtime)
- `[MISSING]` Hashed PlayerId folders

### VCS

- `[NOW]` `.sqldump` export/import
- `[MISSING]` Line-oriented merge-friendly format
- `[MISSING]` Git merge driver binary/commandlet
- `[MISSING]` `.gitattributes` auto-setup
- `[MISSING]` Schema versioning + migration runner
- `[MISSING]` Content-hash dirty detection (replace mtime sync)
- `[MISSING]` Pre-commit hook for validation

### Packaging

- `[MISSING]` Cook-time staging of `.db` files
- `[MISSING]` `IAssetRegistryInterface` hooks
- `[MISSING]` Packaged-game rebuild from `.esqldump`
- `[MISSING]` Mobile/console paths tested

### Security

- `[PARTIAL]` Parameter bindings (exist, not enforced in BP)
- `[MISSING]` Path traversal hardening
- `[MISSING]` Trusted/untrusted DB open modes
- `[MISSING]` Optional encryption provider interface
- `[MISSING]` SQL injection lint rule

### Observability

- `[PARTIAL]` One log category (`LogExtendedSQL`)
- `[MISSING]` `stat SQL` dev command
- `[MISSING]` Tracy / Unreal Insights integration for SQL queries
- `[MISSING]` `csv2` stats for ops-per-frame / query time
- `[MISSING]` In-editor "slow query log"

### Docs & samples

- `[PARTIAL]` Design docs (author-facing)
- `[MISSING]` Quickstart, BP guide, C++ guide, MP guide, VCS guide, FAQ
- `[MISSING]` Sample map + sample assets
- `[MISSING]` YouTube walkthroughs

---

## 12. Prioritized Roadmap (what to do first)

### Phase 1 — Marketplace hygiene (unblocks submission)

1. Split `.uplugin` so SQL ships standalone.
2. Remove `Thirdparty/SQLite/` OR remove `SQLiteCore` (decide, don't dual-track).
3. Drop `StructUtils` module dependency; fix include.
4. Rename types to drop the `E` prefix; settle on `ExtendedSQLite` brand.
5. Add real `DocsURL`, `SupportURL`, `Category`, `Icon128`, `EngineVersion`.
6. Ship sample content + demo map.
7. Write the 9 required docs (Phase 1 can stub APIReference).

### Phase 2 — VCS story that actually works

1. Redesign dump format (line-oriented, per-column).
2. Implement merge commandlet + `.gitattributes` installer.
3. Replace mtime sync with content-hash + dirty flag.
4. Schema versioning + migration runner.

### Phase 3 — DX + performance

1. Split `UESQLSubsystem` God-object.
2. Dedicated SQL thread per connection.
3. LRU statement cache.
4. Cached struct reflection mapper.
5. Unified BP async node; deprecate per-op twins.
6. Reactive delegates (OnRowChanged).

### Phase 4 — Shipping features

1. PakFile VFS, cook-time staging.
2. USQLSaveGame.
3. USQLReplicatedTableComponent.
4. UMG binding components.
5. Standalone database browser window.
6. EXPLAIN QUERY PLAN tab.

### Phase 5 — Nice-to-have

1. FTS5 shipping (requires own amalgamation).
2. JSON1 helpers.
3. Encryption provider interface.
4. Tracy integration.

---

## 13. Bold / Radical Proposals (take the plugin from "good" to "category-defining")

Explicit user request: "Freely suggest huge changes." Here are the big swings.

### 13.1 Abandon `.sqldump`, adopt `.usqlite` folder format

Instead of one text dump per DB, treat a database as a **folder on disk** with:

```
MyGame/Content/Databases/Items.usqlite/
  schema.sql                   # full CREATE TABLE statements, committed
  schema.version               # integer, single line
  rows/
    Items/
      sword.row                # one file per row, keyed by PK
      potion.row
    Recipes/
      iron_sword.row
  migrations/
    001_add_price_column.sql
  _meta.json                   # content hash, last-known-good DB hash
```

- **Merges become trivial:** git sees independent files per row. Two devs editing different rows have zero conflicts.
- `.db` binary is still built at cook/runtime from the folder.
- Looks weird in git but actually works — same idea as Unreal's own "text-based asset" work on UAssetAPI.

### 13.2 Make the query editor the "Material Editor of data"

A full node-graph editor for queries:

- Visual tables, visual JOINs via pin-dragging.
- Live result preview.
- EXPLAIN QUERY PLAN overlay on the graph.
- Nodes compile to SQL at runtime; users never see SQL text unless they click "source".
- This alone would be the #1 differentiator vs. DataTable and vs. every other Unreal SQLite plugin on FAB.

### 13.3 Replicated tables as a native UE reflection feature

Extend the schema so tables marked `ReplicationMode=Mirror` automatically generate:

- A server-side writer API (`USQLTable::ServerSetRow`)
- A replicated compact binary changeset (via `FFastArraySerializer`)
- A client-side read-only mirror DB
- A `FSQLBindableArray<TStruct>` you can bind UMG list views to
  No RPC plumbing by the user.

### 13.4 First-class modding support

Marketplace buyers making moddable games will pay extra for:

- `USQLTable::OverlayFromFile(ModDbPath)` — apply a mod DB's rows on top of base
- Signed mod DB headers (hash-check on load)
- Runtime `FSQLId` picker widget (UMG, shippable) so mod tools run outside the editor
- Hot-reload: file-watcher re-applies DB changes while PIE is running

### 13.5 Analytics pipeline out of the box

Every game needs telemetry. Ship a pre-canned:

- `USQLAnalyticsProvider` — implements `IAnalyticsProvider`, writes events to a local `.db`.
- Automatic batched upload hook (user provides upload URL; plugin handles batching + retry).
- Example "Top 10 deaths by location" query + heatmap render as sample.

### 13.6 Rename the module, actually commit to the brand

If this ships, the brand is `ExtendedSQLite`. Type prefix is `F`/`U`/`E` followed by `SQL` — no second letter. Module names become `ExtendedSQLite` and `ExtendedSQLiteEditor`. The `UnrealExtendedFramework` bundle continues to depend on them transitively for internal dev, but the marketplace listing is `ExtendedSQLite — The SQLite Toolkit for Unreal Engine`.

Everything else — the internal dev experience, the single-source codebase, the author's other modules — stays exactly the same. This is a packaging + namespace move, not an architecture rewrite.

---

## 14. Honest Timeline Estimate

Phases 1 + 2 are **the minimum** for a FAB listing that does what this plugin claims to do. Without them the plugin is a private utility, not a product. Phases 3–4 are what makes it beat the existing SQLite plugins on the marketplace. Phase 5 is what makes it a category-killer.

If the plugin ships without Phase 2, the "VCS-friendly, merge-resolvable" promise is a bug report generator.

---

## 15. What's Already Excellent (Do Not Change)

Credit where due:

- **Custom VFS over `IFileHandle`** — the right foundation for cross-platform.
- **`FESQLId` as a distinct row-handle type** — better than Epic's DataTable `FName` row keys.
- **Multiplayer-aware scope enum** — the right abstraction even if the implementation has gaps.
- **Statement cache with mandatory reset** — better than every naive SQLite wrapper on the internet.
- **Typed table asset as the "blessed" contract** — gives K2 nodes something to build on.
- **`.sqldump`-as-source-of-truth philosophy** — the *idea* is right; only the *format* and *sync logic* need to change.
- **Struct validation dialog at asset-create time** — better UX than silent truncation.

The core engineering is sound. The product wrapping, boundary discipline, merge-safety, and Blueprint ergonomics are where the next month of work has to go.

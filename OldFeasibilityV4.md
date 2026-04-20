# Historical Note

This file is historical. Prefer `ExtendedSQLiteFeasibility.md` for the current feasibility baseline and the `ExtendedSQL*.md` docs for the current public product story.

# ExtendedSQL Feasibility Reset

> This document is the post-review feasibility verdict for UnrealExtendedSQL and UnrealExtendedSQLEditor.
>
> It intentionally replaces the earlier feasibility text that tried to evolve the current architecture instead of choosing a stronger one.

---

## 1. Executive Verdict

ExtendedSQL is still worth building, but only if the module stops investing in the wrong abstractions.

The earlier direction kept trying to preserve too many surfaces at once:

- `UESQLSubsystem`
- `UESQLTableAsset` as a runtime service object
- `UESQLBlueprintBridge`
- async action classes as a second public API
- player-scoped databases and `UESQLPlayerDBComponent`
- a custom Unreal VFS for writable runtime use
- `.sqldump`-centric sync as the authored-data model

That path can keep producing features, but it will keep producing the wrong kind of growth: duplicated APIs, misleading multiplayer stories, and brittle runtime/storage assumptions.

The module becomes much more feasible if it commits to a harder reset.

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

This is a bold cut, but it reduces complexity everywhere at once.

### 2.4 Split Writable And Readonly Storage Paths

The current Unreal VFS concept is still useful, but not as the default writable runtime path.

Feasible storage split:

- writable runtime DBs use native SQLite file access on real filesystem paths
- readonly shipped content DBs can use an Unreal/Pak-aware attachment or extraction path
- if shipped content needs mutation, copy or rebuild it into a writable runtime DB

This is more realistic than trying to make one custom VFS carry every persistence mode.

### 2.5 Replace `.sqldump` As The Design Center

A module intended for team authoring and source control should not center its future on `.db` versus `.sqldump` timestamp arbitration.

Feasible target:

- `.usqlite/` is the committed authored form
- `.sqlmigration` is the canonical migration artifact
- `.db` files are derived runtime/cooked artifacts
- `.sqldump` becomes legacy import/export compatibility only

This directly improves mergeability and reduces ambiguity.

### 2.6 Own The SQLite Capability Story

ExtendedSQL should not keep pretending it controls SQLite feature flags if it does not.

Feasible target:

- ship one owned SQLite amalgamation for the module
- document exactly which features are enabled
- stop mixing `SQLiteCore`, local headers, and optimistic definitions

This increases maintenance responsibility, but it makes the module materially more trustworthy.

### 2.7 Hide The Bridge

A bridge layer can still exist for reflection and K2 expansion, but it should stop being a second public product surface.

Feasible target:

- visible user story: subsystem plus table assets plus editor nodes
- hidden bridge story: small implementation detail used by K2 and reflection-only paths

---

## 3. What We Should Stop Preserving

The following directions should be treated as removal targets rather than modernization targets:

- `UESQLPlayerDBComponent`
- public player-scoped database APIs
- player-only K2 node families
- asset-owned async/runtime method growth
- large public bridge helper inventories
- current custom Unreal VFS as the default writable runtime path
- timestamp-based `.db` versus `.sqldump` source-of-truth arbitration
- hybrid `SQLiteCore` plus local feature claims

Trying to preserve these while also simplifying the module is what kept the earlier documentation inconsistent.

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
- async dispatch
- transactions
- migration application
- snapshot helpers
- authority and route checks
- readonly content attachments

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

- `.usqlite/`
- `.sqlmigration`
- text-first authored data

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
- player-owned state is modeled inside shared authoritative tables
- replication and snapshot systems move rows or change sets, not database topology
- optional client-local caches remain valid, but they are local runtime concerns, not the dominant multiplayer API story

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

A custom Unreal-aware file path layer is still useful, but only when it solves a real problem.

Real problems:

- readonly data living in Unreal-managed packaged content
- asset-like shipping of curated database content
- controlled extraction/attachment flows for cooked builds

Not a good target for the custom VFS:

- default writable runtime database access
- general WAL-heavy concurrency
- pretending shared memory and locking can be ignored safely

The feasibility reset therefore narrows the custom VFS rather than deleting the concept entirely.

---

## 8. Revised Authored-Data Position

The old design spent too much energy keeping a live relationship between binary DB files and text dumps.

That is operationally weak for teams.

The stronger plan is:

- author text-first `.usqlite` projects
- emit binary `.db` artifacts for runtime and cook
- store migrations as canonical `.sqlmigration` text files
- keep `.sqldump` only as a compatibility bridge while the module migrates

This is the right trade for a plugin that wants to be collaborative, inspectable, and version-control friendly.

---

## 9. Revised Runtime API Position

A large `UESQLSubsystem` is feasible and desirable.

The fear that one subsystem becomes "too big" is less important than the cost of the current split.

The split is already expensive:

- duplicated docs
- duplicated K2 patterns
- duplicated async paths
- duplicated player-only variants
- unclear ownership of routing and transactions

A larger subsystem is a smaller product.

That is the correct trade.

---

## 10. Revised Risk Assessment

### 10.1 Risks We Are Accepting

- the subsystem will become a substantial class
- moving off the current public player-surface will require deprecation work
- owning a SQLite amalgamation increases maintenance responsibility
- `.usqlite` requires new tooling and serialization rules

### 10.2 Why Those Risks Are Worth It

They buy:

- one public runtime story
- honest storage guarantees
- honest feature guarantees
- smaller Blueprint and K2 taxonomy
- cleaner multiplayer guidance
- far better documentation coherence

Those are higher-value wins than preserving the old surfaces.

---

## 11. Implementation Consequences

The feasibility reset directly implies the following code and tooling priorities:

1. move representative typed runtime behavior onto `UESQLSubsystem`
2. stop expanding runtime behavior on `UESQLTableAsset`
3. deprecate and remove player-scoped public surfaces
4. move writable DB access to the native SQLite OS VFS
5. narrow the Unreal-aware VFS to readonly packaged-content scenarios
6. formalize one owned SQLite build
7. replace `.sqldump` as the design center with `.usqlite` and `.sqlmigration`
8. shrink the bridge and async layers into hidden K2 expansion details

If those things do not happen, the module will keep carrying the same contradictions under a different set of docs.

---

## 12. What Still Looks Strong

Even after the reset, several parts of the original ambition remain good and worth keeping:

- typed table assets as an authoring concept
- Blueprint-first SQL workflows
- explicit transaction support
- snapshot and backup tooling
- merge-friendly text representations for source control
- server-authoritative multiplayer support
- optional readonly shipped content databases
- Unreal-aware import/export tooling

The reset is not a rejection of the module. It is a rejection of the wrong shape for the module.

---

## 13. Final Feasibility Statement

ExtendedSQL is feasible, but the feasible version is more opinionated than the earlier plan.

The feasible version says:

- one runtime front door
- metadata-first table assets
- no public player-scoped database model
- native writable SQLite paths
- narrowed readonly Unreal-aware VFS usage
- `.usqlite` plus `.sqlmigration` as authored truth
- hidden bridge layers
- one owned SQLite capability set

That is the design worth implementing.

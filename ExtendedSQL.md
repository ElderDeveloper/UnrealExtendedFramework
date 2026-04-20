# ExtendedSQL

This is the public guide for UnrealExtendedSQL after the 2026 architecture reset.

The product story is intentionally small:

1. `UESQLSubsystem` is the runtime front door.
2. `UESQLTableAsset` is schema and authoring metadata.
3. `.usqlite/` is the authored source of truth.

If another SQL document teaches a different runtime model, prefer this file.

---

## 1. Mental Model

ExtendedSQL has three layers that matter to game teams.

### `UESQLSubsystem`

The subsystem owns runtime behavior:

- database open/close/delete
- typed row load/save/find/count/page operations
- raw SQL execution for advanced cases
- async dispatch and ticket cancellation
- transactions
- snapshots and backups
- migration application

Runtime code should start here.

### `UESQLTableAsset`

The table asset describes one typed table:

- row struct identity
- logical database name
- SQL table name
- primary key column
- default label column
- scope and persistence hints
- editor validation and authoring helpers

The asset is not the main runtime service surface. Pass it into subsystem calls.

### `.usqlite/`

`.usqlite/` is the authored format for source control.

- commit `.usqlite/`
- treat `.db` as a built artifact, test artifact, or cooked artifact
- author schema and data through the editor and serializer flow
- use legacy import only to migrate older projects in

---

## 2. Quick Start

### Blueprint

1. Get the game instance subsystem for `UESQLSubsystem`.
2. Reference a `UESQLTableAsset`.
3. Use the typed row/query nodes such as `Get SQL Row`, `Save SQL Row`, `Find SQL Rows`, or `Load SQL Page`.
4. Inspect the returned `FESQLQueryResult` when you need success state, error text, or raw rows.

### C++

```cpp
UGameInstance* GameInstance = GetGameInstance();
UESQLSubsystem* SQL = GameInstance ? GameInstance->GetSubsystem<UESQLSubsystem>() : nullptr;

if (!SQL || !ItemsTableAsset)
{
    return;
}

FInventoryItemRow Row;
const FESQLQueryResult Result = SQL->LoadRowIntoStruct(
    this,
    ItemsTableAsset,
    TEXT("item_001"),
    &Row,
    FInventoryItemRow::StaticStruct());

if (!Result.bSuccess)
{
    UE_LOG(LogTemp, Warning, TEXT("SQL load failed: %s"), *Result.ErrorMessage);
}
```

For typed Blueprint nodes, the editor expands the wildcard row pins to the same subsystem-owned runtime behavior.

---

## 3. Core Concepts

### `FESQLId`

`FESQLId` is the picker-friendly identity for typed rows.

Use it when gameplay or UI needs to point at one row without hand-typing raw row ids. The associated table asset provides the database, table, primary key, and default label metadata.

### `FESQLQuerySpec`

`FESQLQuerySpec` is the structured query input for typed searches.

It combines:

- filters
- sort rules
- limit
- offset

Use it for reusable typed search logic instead of concatenating SQL strings for normal game workflows.

### Scope and Persistence

Every table asset and explicit database open uses scope and persistence settings.

Scope:

- `Local` for client-local or standalone databases
- `Shared` for authoritative/shared game data
- `Readonly` for shipped readonly content data

Persistence:

- `Persistent` for disk-backed databases
- `Session` for in-memory databases

The subsystem owns the final path and runtime routing rules.

---

## 4. Authoring With `UESQLTableAsset`

Create one row struct, then create one `UESQLTableAsset` that points at it.

The asset should answer these questions:

- which row struct defines the schema?
- which logical database owns the table?
- what is the SQL table name?
- which column is the primary key?
- which column should UI use as the default label?

The editor validates these fields inline. Invalid assets should fail validation before they become runtime problems.

The table editor is metadata-first and preview-first:

- schema comes from the row struct
- row display is a preview, not a live CRUD grid
- test actions run through `UESQLSubsystem`

---

## 5. `.usqlite/` Workflow

Use `.usqlite/` as the team-facing source of truth.

Recommended workflow:

1. Create or update a row struct.
2. Create or update a `UESQLTableAsset`.
3. Save authoring data into the sibling `.usqlite/` project.
4. Let the editor/runtime build a test `.db` when needed.
5. Commit the `.usqlite/` project and the asset metadata.

At runtime, the subsystem works with SQLite databases. In editor workflows, the authoring project can rebuild the derived `.db` used for preview and testing.

If you are migrating an older project, import the legacy data once, emit a `.usqlite/` project, and continue authoring there.

---

## 6. Runtime Features

### Typed Table Operations

The normal typed surface is table-centric:

- load one row
- save one row
- load many rows
- find rows by query
- find first row by field
- count rows
- load one page
- delete one row

All of these run through `UESQLSubsystem` with a `UESQLTableAsset` input.

### Raw SQL

Raw SQL remains available for advanced cases:

- analytics
- debugging
- migrations
- admin tooling
- one-off operational queries

Use raw SQL when you truly need SQL text. For normal gameplay and content data, prefer typed table calls.

### Async

Async SQL is part of the same runtime model, not a separate product story.

Use async when you need non-blocking reads or writes. The subsystem and editor nodes still own the user-facing behavior even when hidden helper classes are used internally.

### Transactions and Snapshots

The subsystem also owns:

- begin/commit/rollback transaction flow
- snapshot save/load/delete
- backup to another path
- migration application

These are subsystem concerns, not table-asset concerns.

---

## 7. Recommended Usage Rules

Use these rules to keep projects consistent.

1. Start runtime work from `UESQLSubsystem`.
2. Treat `UESQLTableAsset` as metadata and typed schema input.
3. Commit `.usqlite/` rather than treating `.db` as authored truth.
4. Prefer typed queries over handwritten SQL for normal gameplay data.
5. Use `FESQLId` when a designer or UI needs a row reference.
6. Use raw SQL only when the typed surface is not the right tool.

---

## 8. Summary

ExtendedSQL is designed to be easy to explain:

- `UESQLSubsystem` runs SQL
- `UESQLTableAsset` describes typed tables
- `.usqlite/` is what teams author and commit

If your game code and docs follow that rule, the rest of the module stays coherent.

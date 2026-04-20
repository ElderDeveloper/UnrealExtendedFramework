# ExtendedSQL Implementation Guide

This file explains the current implementation model for UnrealExtendedSQL.

It is not a migration checklist. It is the practical reference for how the module is expected to be used and extended now.

---

## 1. Public Contract

The public contract is intentionally narrow.

### Runtime owner

`UESQLSubsystem` is the single runtime front door.

It owns:

- database open/close/delete
- typed table queries and row mutation
- raw SQL execution
- async query dispatch
- transaction flow
- snapshots and backups
- migration application

### Metadata owner

`UESQLTableAsset` owns schema and authoring metadata.

It owns:

- row struct identity
- database name
- table name
- primary key column
- default label column
- scope and persistence hints
- editor validation and authoring helpers

### Authored source of truth

`.usqlite/` is the format intended for authoring and source control.

The implementation treats `.db` files as build outputs, preview outputs, or cooked outputs rather than the long-term authored truth.

---

## 2. Runtime Flow

### Opening databases

`UESQLSubsystem::OpenDatabase` accepts:

- logical database name
- scope
- persistence
- optional file name override

The subsystem resolves those inputs to the actual runtime database instance and file path rules.

Use explicit open calls when you need direct database lifecycle control. Typed table calls can still rely on the table asset's logical database metadata.

### Typed table calls

The main typed operations are exposed through subsystem UFUNCTIONs and typed custom thunks.

Representative operations include:

- query rows for one table
- count rows
- load one row by id
- load one row from `FESQLId`
- save one typed row
- load many typed rows
- find rows by field or query spec
- load a page of rows

These calls take `UESQLTableAsset*` so that runtime code can stay table-centric without pushing behavior ownership back onto the asset.

### Raw SQL

Raw SQL stays available on the subsystem for advanced use cases.

Prefer typed calls when possible. Use raw SQL when you need:

- ad hoc maintenance queries
- specialized analytics
- migration helpers
- debugging and inspection

---

## 3. `UESQLTableAsset` Responsibilities

`UESQLTableAsset` is the schema contract between editor authoring and runtime execution.

Its responsibilities are:

- expose the row struct used for typed hydration
- describe the database and SQL table names
- define the primary key column
- define the default label column for picker UX
- expose scope and persistence defaults
- validate schema compatibility in editor builds

The asset also exposes authoring helpers such as:

- CSV import/export
- `.usqlite/` export
- `.db` and `.usqlite/` synchronization helpers
- legacy import from `.sqldump`

These helpers exist for authoring and migration. Runtime gameplay code should still go through the subsystem.

---

## 4. `.usqlite/` Authoring Model

`.usqlite/` is the intended committed format.

Implementation expectations:

1. a table asset points at a logical database
2. that database can have a sibling `.usqlite/` project
3. editor authoring updates the `.usqlite/` project
4. preview and runtime flows build or open the derived `.db`

The implementation already exposes table-asset helpers such as `ExportToUSqlite`, `SyncDatabaseAndProject`, and `GetUSqliteProjectPath` to support that workflow.

Practical rule:

- commit `.usqlite/`
- do not treat the working `.db` as the authoritative authoring artifact

---

## 5. Validation And Editor Behavior

Editor validation should catch authoring problems before runtime.

Current validation responsibilities include:

- verifying the row struct exists
- validating schema compatibility
- checking required metadata such as database name, table name, and primary key
- surfacing `.usqlite/` project mismatches as asset validation errors when relevant

The editor table tooling is preview-oriented:

- schema comes from the row struct
- row inspection is preview-first
- test database and test query actions route through `UESQLSubsystem`

This keeps editor behavior aligned with the runtime ownership model.

---

## 6. Blueprint Implementation Notes

Blueprint-visible typed nodes expand into subsystem calls for row and query execution.

Blueprint query-builder helpers and typed hydration helpers are implemented through public helper functions, but they are still supporting pieces around the same public model:

- `UESQLSubsystem` is the runtime owner
- `UESQLTableAsset` provides typed schema metadata
- `FESQLId` and `FESQLQuerySpec` are the main data inputs

Design goal:

Blueprint authors should not need to understand internal expansion details to use the module correctly.

---

## 7. Transactions, Snapshots, And Async

These features are subsystem-level concerns.

### Transactions

Use the subsystem transaction API when a group of changes must commit or roll back together.

### Snapshots and backups

Use snapshot and backup helpers for save-slot-style flows, recovery, and tooling.

### Async

Use async subsystem or async node flows when reads or writes should not block gameplay.

Async is not a second architecture. It is the same SQL runtime exposed through non-blocking execution paths.

---

## 8. C++ Example

```cpp
if (UGameInstance* GameInstance = GetGameInstance())
{
    if (UESQLSubsystem* SQL = GameInstance->GetSubsystem<UESQLSubsystem>())
    {
        FESQLQuerySpec QuerySpec;
        QuerySpec.Limit = 25;

        int64 MatchCount = 0;
        const FESQLQueryResult CountResult = SQL->CountTableRows(ItemsTableAsset, QuerySpec, MatchCount);

        if (CountResult.bSuccess)
        {
            UE_LOG(LogTemp, Log, TEXT("Found %lld rows"), MatchCount);
        }
    }
}
```

The main extension rule is simple: add runtime behavior to the subsystem, not to the table asset.

---

## 9. Legacy Migration

Older projects may still have legacy database dumps or older database files.

The intended migration path is one-way:

1. import the legacy data
2. emit a `.usqlite/` project
3. continue authoring from `.usqlite/`

Do not build new workflows around legacy import formats.

---

## 10. Summary

When extending the module, keep these rules in mind:

1. runtime behavior belongs on `UESQLSubsystem`
2. typed schema metadata belongs on `UESQLTableAsset`
3. authored truth belongs in `.usqlite/`

That separation is what keeps the module understandable in both C++ and Blueprint.

# ExtendedSQL Blueprint Guide

This guide explains the supported Blueprint workflow for UnrealExtendedSQL.

The intended Blueprint story is simple:

1. get `UESQLSubsystem`
2. pass a `UESQLTableAsset`
3. work with typed rows, `FESQLId`, and `FESQLQuerySpec`

If a workflow requires you to understand hidden helper classes, it is not the intended public path.

---

## 1. Getting The Subsystem

In Blueprint, start by getting the game instance subsystem for `UESQLSubsystem`.

That subsystem owns:

- database lifecycle
- typed row queries
- raw SQL
- async work
- transactions
- snapshots

Do not build your main gameplay workflow around calling methods on the table asset directly.

---

## 2. Using A Table Asset

`UESQLTableAsset` is the typed schema input for Blueprint nodes.

It provides:

- the row struct type
- the logical database name
- the SQL table name
- the primary key column
- the default label column for pickers
- scope and persistence hints

In practice, the table asset is what lets the editor give you typed row pins and better picker UX.

---

## 3. Common Typed Nodes

The visible typed Blueprint workflow centers on row and query nodes such as:

- `Get SQL Row`
- `Get SQL Rows`
- `Save SQL Row`
- `Find SQL Rows`
- `Find First SQL Row By Field`
- `Count SQL Rows`
- `Load SQL Page`
- `Async Load SQL Row`
- `Async Load SQL Rows`
- `Async Find SQL Rows`
- `Async Save SQL Row`

Common node inputs:

- `World Context` when needed by the node
- `Table Asset`
- `SQL Id` for single-row lookups
- typed `Row` or `Rows`

All of these nodes are table-centric. The asset describes the table; the subsystem owns runtime behavior.

---

## 4. `FESQLId`

`FESQLId` is the normal Blueprint identity type for selecting one row.

Use it when you want:

- dropdown-based row selection in details panels
- UI widgets that carry a row reference
- gameplay data that refers to one row in a typed table

Use raw string row ids only for explicit override or low-level cases.

---

## 5. Building Structured Queries

Use `FESQLQuerySpec` for reusable typed queries.

The query builder flow is:

1. create `FESQLBindingValue`
2. create one or more field filters
3. create optional sort rules
4. create a query spec
5. pass that query spec into typed query nodes

This is the normal way to express filter, sort, limit, and offset behavior in Blueprint without dropping to handwritten SQL.

Use raw SQL nodes only when you need advanced SQL text that the typed query model does not cover.

---

## 6. Async Blueprint Usage

Async nodes are part of the same public SQL model.

Use them when you need non-blocking SQL work. The public mental model does not change:

- the subsystem still owns runtime behavior
- the table asset still provides typed schema metadata
- the success and failure pins still return typed rows or query results

From the user's perspective, async is just another way to call the same SQL system.

---

## 7. Editor Authoring Flow

The authoring workflow is metadata-first and `.usqlite/`-first.

Typical flow:

1. define a row struct
2. create a `UESQLTableAsset`
3. set database name, table name, primary key, and label column
4. save authoring data into the `.usqlite/` project
5. use the editor's test database and test query actions for preview

The table editor is intended for schema and preview, not as a full live runtime CRUD panel.

---

## 8. Validation

If a table asset is misconfigured, validation should fail in the editor before runtime.

Examples of validation problems:

- missing row struct
- missing database or table name
- invalid primary key settings
- struct fields that cannot be represented cleanly
- mismatches between the asset and the sibling `.usqlite/` project

Treat validation failures as authoring problems to fix before gameplay code or Blueprint logic depends on the asset.

---

## 9. Blueprint Best Practices

1. Treat `UESQLSubsystem` as the runtime owner.
2. Treat `UESQLTableAsset` as typed metadata.
3. Prefer `FESQLId` over raw string row ids.
4. Prefer `FESQLQuerySpec` over hand-built SQL strings for normal content queries.
5. Commit `.usqlite/` and use derived `.db` files for test or cooked output.
6. Use async nodes for non-blocking work, not as a separate product surface.

---

## 10. Blueprint Summary

Blueprint usage should fit in one sentence:

Get `UESQLSubsystem`, pass a `UESQLTableAsset`, use typed nodes, and let `.usqlite/` remain the authored source of truth.

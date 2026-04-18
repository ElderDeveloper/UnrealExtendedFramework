# ExtendedSQL Blueprint API

This document lists every Blueprint-visible function and custom node currently exposed by the `UnrealExtendedSQL` plugin.

Functions marked `BlueprintInternalUseOnly` are intentionally omitted because they do not appear in normal Blueprint menus. That hidden layer still exists under the hood for legacy graphs and internal K2 expansion paths, but it is not part of the public Blueprint authoring surface.

## At a Glance

- Use `UESQLSubsystem` for database lifecycle, transactions, snapshots, and low-level table helpers.
- Use `UESQLTableAsset` and the custom `SQL` or `SQL|Player` nodes for typed row workflows.
- Structured query builder nodes, paging/query bridge nodes, and raw SQL execution helpers are hidden from normal Blueprint menus and retained only for legacy graphs.
- Public `FESQLId` pins are the picker-friendly row-id surface. The legacy public `SQL|Id` helper category is gone, and public row lookup/delete nodes no longer expose raw string `RowId` inputs.

## Recommended Typed SQL Nodes

These nodes are registered by the editor module and are the intended Blueprint surface for typed row reads, writes, paging, and async operations.

### Palette: SQL

| Node | Key Inputs | Outputs | Notes |
| --- | --- | --- | --- |
| `Get SQL Row` | `World Context`, `Table Asset`, `SQL Id` | `Row`, `Result` | Standard typed single-row load. `SQL Id` uses the picker-driven `FESQLId` surface. |
| `Resolve SQL Id Row` | `World Context`, `Table Asset`, `SQL Id` | `Row`, `Result` | Generic typed fallback when you already have an `FESQLId` value flowing through UI or data graphs. |
| `Save SQL Row` | `World Context`, `Table Asset`, typed `Row`, optional `Row Id Override` | `Resolved Row Id`, `Result` | Saves one typed row and returns the final row id used by the table. |
| `Get SQL Rows` | `World Context`, `Table Asset`, optional `Max Rows` | `Rows`, `Result` | Loads all typed rows, or up to `Max Rows` if provided. |
| `Async Load SQL Row` | `World Context`, `Table Asset`, `SQL Id` | `OnSuccess`, `OnFailure`, `Result`, `Row` | Async typed single-row load. `Row` is hydrated before `OnSuccess` fires. |
| `Async Load SQL Rows` | `World Context`, `Table Asset`, optional `Max Rows` | `OnSuccess`, `OnFailure`, `Result`, `Rows` | Async typed bulk load. |
| `Async Save SQL Row` | `World Context`, `Table Asset`, typed `Row Data`, optional `Row Id Override` | `OnSuccess(Result, Resolved Row Id)`, `OnFailure(Result)` | Async typed save. The typed row payload is captured before activation. |

### Palette: SQL|Player

| Node | Key Inputs | Outputs | Notes |
| --- | --- | --- | --- |
| `Get Player SQL Row` | `Player DB`, `Table Asset`, `SQL Id` | `Row`, `Result` | Typed single-row load against a player-scoped database. |
| `Save Player SQL Row` | `Player DB`, `Table Asset`, typed `Row`, optional `Row Id Override` | `Resolved Row Id`, `Result` | Typed save against a player-scoped table. |
| `Get Player SQL Rows` | `Player DB`, `Table Asset`, optional `Max Rows` | `Rows`, `Result` | Loads typed rows from a player-scoped table. |
| `Delete Player SQL Row` | `Player DB`, `Table Asset`, `SQL Id` | `Result` | Deletes a player-scoped row through the `FESQLId` picker surface. |

### Typed Node Behavior

- If `Table Asset` is a literal asset, the node can infer the struct type from `RowStruct` and automatically type the `Row` or `Rows` pins.
- If `Table Asset` is dynamic, connect a typed struct or typed struct array so the node can infer the row type from the connected pin.
- The public `SQL Id` pins on these nodes use `FESQLId` and the custom picker instead of manual string entry.
- `Row Id Override` on save nodes intentionally stays a string because it is a manual override path, not a row-selection input.
- Structured query, field-filter, paging, and row-count helper nodes are hidden from normal Blueprint menus and remain available only for legacy graphs that already reference them.

## `UESQLSubsystem` Blueprint API

Access this subsystem from Blueprint through `Get Game Instance Subsystem` and selecting `ESQLSubsystem`.

### Category: SQL|Database

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Open Database` (`OpenDatabase`) | `FESQLQueryResult` | Opens or creates a logical database handle. Inputs: `DatabaseName`, `Scope`, `Persistence`, optional `FileName`. |
| `Open Player Database` (`OpenPlayerDatabase`) | `FESQLQueryResult` | Opens a per-player database keyed from the player's `UniqueNetId`. Server-only. |
| `Close Database` (`CloseDatabase`) | `void` | Closes one logical database. |
| `Close Player Database` (`ClosePlayerDatabase`) | `void` | Closes one player-scoped database and can optionally delete the file. |
| `Close All Databases` (`CloseAllDatabases`) | `void` | Closes all open non-player databases. |
| `Close All Player Databases` (`CloseAllPlayerDatabases`) | `void` | Closes all player-scoped databases and can optionally delete their files. |
| `Is Database Open` (`IsDatabaseOpen`) | `bool` | Returns whether a named database handle is currently open. |
| `Is Player Database Open` (`IsPlayerDatabaseOpen`) | `bool` | Returns whether a player's database is currently open for the given controller. |
| `Get Open Database Names` (`GetOpenDatabaseNames`) | `TArray<FString>` | Returns all currently open logical database names. |
| `Delete Database` (`DeleteDatabase`) | `FESQLQueryResult` | Deletes a closed database and its sidecar files (`.db`, `-wal`, `-shm`). |
| `Get Database File Path` (`GetDatabaseFilePath`) | `FString` | Returns the resolved absolute path for a database, or an empty string for session-only in-memory databases. |

### Category: SQL|Network

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Is Dedicated Server` (`IsDedicatedServer`) | `bool` | Net mode helper. |
| `Is Listen Server` (`IsListenServer`) | `bool` | Net mode helper. |
| `Has Server Authority` (`HasServerAuthority`) | `bool` | Convenience authority check for SQL operations that require server ownership. |

### Category: SQL|Table

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Create Table` (`CreateTable`) | `FESQLQueryResult` | Creates a table from a `Columns` name-to-type map with optional `PrimaryKeyColumn` and `If Not Exists`. |
| `Drop Table` (`DropTable`) | `FESQLQueryResult` | Drops a table by name. |
| `Does Table Exist` (`DoesTableExist`) | `bool` | Returns whether a table exists in the target database. |
| `Get Table Names` (`GetTableNames`) | `TArray<FString>` | Returns all user table names in the database. |
| `Get Table Columns` (`GetTableColumns`) | `TArray<FESQLColumn>` | Returns the column definitions for a table. |
| `Alter Table Add Column` (`AlterTableAddColumn`) | `FESQLQueryResult` | Adds one column to an existing table, with optional type and default value. |

### Category: SQL|Row

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Insert Row` (`InsertRow`) | `FESQLQueryResult` | Inserts one row from a `ColumnValues` map. |
| `Insert Rows` (`InsertRows`) | `FESQLQueryResult` | Inserts multiple rows inside one transaction. |
| `Upsert Row` (`UpsertRow`) | `FESQLQueryResult` | Inserts or updates based on `ConflictColumn`. |
| `Update Rows` (`UpdateRows`) | `FESQLQueryResult` | Updates rows matching a `WhereClause` and `Bindings`. |
| `Delete Rows` (`DeleteRows`) | `FESQLQueryResult` | Deletes rows matching a `WhereClause` and `Bindings`. |

### Category: SQL|Transaction

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Begin Transaction` (`BeginTransaction`) | `bool` | Begins an explicit transaction on a named database. |
| `Commit Transaction` (`CommitTransaction`) | `bool` | Commits an explicit transaction. |
| `Rollback Transaction` (`RollbackTransaction`) | `bool` | Rolls back an explicit transaction. |

### Category: SQL|Snapshot

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Save Snapshot` (`SaveSnapshot`) | `FESQLQueryResult` | Saves a snapshot of a session database into a named slot. |
| `Load Snapshot` (`LoadSnapshot`) | `FESQLQueryResult` | Loads a previously saved snapshot slot into a session database. |
| `Delete Snapshot` (`DeleteSnapshot`) | `FESQLQueryResult` | Deletes one saved snapshot slot. |
| `Get All Snapshots` (`GetAllSnapshots`) | `TArray<FESQLSnapshotInfo>` | Returns all snapshot metadata for a session database. |
| `Does Snapshot Exist` (`DoesSnapshotExist`) | `bool` | Returns whether a named snapshot slot exists. |
| `Is Session Database` (`IsSessionDatabase`) | `bool` | Returns whether a database is a session/in-memory database. |

## `UESQLTableAsset` Blueprint API

`UESQLTableAsset` owns schema metadata, primary key behavior, and picker label behavior. The custom SQL nodes above are built on top of this asset type.

### Category: SQL Table

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Initialize` | `FESQLQueryResult` | Opens the backing database, ensures the table exists, and synchronizes schema from the configured row struct. |
| `Insert Row From Struct` (`InsertRowFromStruct`) | `FESQLQueryResult` | Inserts one row from an arbitrary struct input pin through the custom thunk path. |
| `Get All Rows` (`GetAllRows`) | `FESQLQueryResult` | Returns all rows as a raw query result. `MaxRows = 0` means unlimited. |
| `Get Row By Id` (`GetRowById`) | `FESQLQueryResult` | Loads one row by a picker-friendly `SQL Id` (`FESQLId`) as a raw query result. |
| `Get Row Count` (`GetRowCount`) | `int32` | Returns the current row count. |
| `Is Initialized` (`IsInitialized`) | `bool` | Returns whether the asset's database connection is live. |

## `UESQLPlayerDBComponent` Blueprint API

Attach this component to a `PlayerState` to represent a player-owned database context. It is the explicit `Player DB` input for the player-scoped typed SQL nodes.

### Category: SQL|Player

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Get Player Id` (`GetPlayerId`) | `FString` | Returns the cached `UniqueNetId` string resolved from the owning `PlayerState`. |
| `Is Player Database Open` (`IsPlayerDatabaseOpen`) | `bool` | Returns whether this component's player database is open. |
| `Get Owning Player State` (`GetOwningPlayerState`) | `APlayerState*` | Convenience access to the owner as a player state. |

## `UESQLBlueprintBridge` Public Blueprint API

This function library exposes the small non-query runtime bridge surface that still needs direct Blueprint access. Structured-query builders and query bridge helpers are hidden from normal Blueprint menus and retained only for legacy graphs or editor-node expansion. Typed row loading and saving helpers are hidden and surfaced instead through the custom K2 nodes documented earlier. The legacy string `RowId` overloads for existence/delete are now hidden from normal Blueprint menus.

### Category: SQL|Bridge

| Blueprint Function | Returns | Notes |
| --- | --- | --- |
| `Is SQL Query Result Successful` (`IsSQLQueryResultSuccessful`) | `bool` | Returns the success state of an `FESQLQueryResult`. |
| `Does SQL Row Exist` (`DoesSQLRowExistById`) | `bool` | Visible existence-check node. Inputs: `World Context`, `Table Asset`, picker-friendly `SQL Id`, advanced `OutError`. |
| `Delete SQL Row` (`DeleteSQLRowById`) | `FESQLQueryResult` | Visible delete node. Inputs: `World Context`, `Table Asset`, picker-friendly `SQL Id`. |

## Public Surface Summary

- The recommended typed authoring surface is the custom `SQL` and `SQL|Player` node set for direct row load/save/delete workflows.
- Structured query builders, filter helpers, paging/count wrappers, player query helpers, and raw SQL execution nodes are hidden from normal Blueprint menus and retained only for legacy graphs.
- `UESQLSubsystem` remains the public lifecycle, transaction, snapshot, and semantic table API.
- `UESQLTableAsset` remains the public schema-owning asset API.
- `UESQLBlueprintBridge` remains public only for the small non-query bridge helpers that still need direct Blueprint access.
- Public row lookup/delete entry points now use `FESQLId`; the remaining public string row-id input is the explicit `Row Id Override` save path.
- Hidden raw bridge thunks, structured query builders, paging/count wrappers, raw SQL execution helpers, and the async factory functions are intentionally not part of the public Blueprint inventory.


# ExtendedSQL Implementation Plan

This file tracks the remaining implementation work after the runtime ownership
model was locked down.

Authoritative architecture still lives in `ExtendedSQL.md` and
`ExtendedSQLBlueprint.md`. This document is the execution roadmap for the work
that is still missing.


## 1. Scope

The remaining work splits into two implementation tracks:

1. complete the remaining typed C++ runtime and reflection bridge
2. build the editor-only K2 node layer on top of that runtime

The important constraint is unchanged:

- `UESQLSubsystem` stays the lifecycle/raw-SQL/player-db service layer
- `UESQLTableAsset` stays the schema-owning typed table contract
- `UESQLPlayerDBComponent` stays context/state only
- K2 nodes only improve graph UX and pin typing; they must not own runtime behavior

Implementation style guideline for the remaining work:

- prefer extending the existing SQL files instead of exploding the system into many tiny files
- add new files only where a new abstraction boundary is actually justified


## 2. Current Baseline (April 2026)

Already implemented:

- `UESQLSubsystem` has been cleaned back to lifecycle, authority, snapshots, raw SQL, and player-db services
- `UESQLTableAsset` already owns the current typed runtime surface:
	- `Initialize()`
	- `GetAllRows()`
	- `GetRowById()`
	- `GetRowCount()`
	- `LoadRowIntoStruct()`
	- `ResolveRowDisplayText()`
	- `SaveRowFromStruct()`
	- `LoadTypedRow<T>()`
	- `LoadTypedRows<T>()`
	- `SaveTypedRow<T>()`
	- basic `TryLoadTypedRow<T>()`, `TryLoadTypedRows<T>()`, and `TrySaveTypedRow<T>()` convenience helpers
	- the first structured query-spec runtime surface:
		- `QueryRows()`
		- `FindRows<T>()`
		- `FindFirstRow<T>()`
		- `CountRows(const FESQLQuerySpec&, ...)`
		- `LoadPage<T>()`
	- the first native typed async C++ runtime surface:
		- `AsyncLoadTypedRow<T>()`
		- `AsyncLoadTypedRows<T>()`
		- `AsyncSaveTypedRow<T>()`
		- `AsyncFindRows<T>()`
		- `AsyncFindFirstRow<T>()`
		- `AsyncLoadPage<T>()`
	- the first player-scoped typed table surface:
		- `LoadPlayerTypedRow<T>()`
		- `LoadPlayerTypedRows<T>()`
		- `SavePlayerTypedRow<T>()`
		- `DeletePlayerRowById(...)`
		- `FindPlayerRows<T>()`
		- optional already-resolved `PlayerId` overloads for server-side systems
	- `DoesRowExist()`
	- `DeleteRowById()`
	- dump import/export/sync helpers
- `FESQLBindingValue` now has explicit null / integer / float / text / blob kinds with typed SQLite bind dispatch
- `FESQLRow` and `FESQLQueryResult` now expose the first C++ convenience accessors (`HasColumn`, `IsNull`, typed getters, `HasRows`, `NumRows`, `TryGetSingleRow`)
- `InsertRowFromStruct` already exists as a runtime `CustomThunk`
- `FESQLId` already exists as the row-handle/reference type with editor picker metadata
- the SQL table asset editor, row editor, validation dialog, and `FESQLId` details customization already exist

Still missing:

- no reflection-friendly typed load/save/query bridge beyond `InsertRowFromStruct`
- the typed query-spec layer is now started, but is still C++-only and not yet surfaced through reflection-friendly runtime wrappers or K2
- the player-scoped typed table layer is now started, but optional already-resolved player-id overloads are still deferred
- the native typed async runtime is now started, but only the core C++ callback surface exists so far
- no runtime Blueprint function library or async action layer for typed table workflows
- the reflection-friendly runtime bridge now covers id helpers, typed load/save/delete/exists/query helpers, and shared-table count/page wrappers
- the first SQL K2 nodes now exist in `UnrealExtendedSQLEditor`, but only the initial typed row-array nodes are implemented so far
- Phase `C++-2` is started, but generic row values are still stored row-side as strings plus null markers; richer row-side typed storage can remain deferred unless query/spec work proves it necessary


## 3. Hard Rules For The Remaining Work

1. Do not add new typed struct CRUD helpers back onto `UESQLSubsystem`.
2. Do not reintroduce a generic SQL-first Blueprint library as the main public story.
3. Do not make K2 nodes call raw SQL directly when a typed runtime path should exist.
4. Do not make designers provide database names, table names, or column strings for normal workflows.
5. Do not split the remaining runtime into many parallel helper layers unless one layer is clearly reusable and not table-asset-specific.
6. Typed mode should be the primary UX. Dynamic fallback mode can exist, but it is not the main product path.


## 4. Remaining C++ Runtime Phases

### Phase C++-1: Finish The Typed Table Runtime Contract

Goal:

- turn `UESQLTableAsset` from a partially complete typed helper surface into the full supported typed runtime contract

Primary files:

- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.h`
- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.cpp`

Required additions:

- add explicit runtime label-resolution helpers on `UESQLTableAsset`
- add `FESQLId`-centric convenience overloads everywhere the typed path should support them
- add clearer typed failure helpers for common cases:
	- not found
	- struct mismatch
	- schema mismatch
	- invalid table asset configuration
- add small `Try...` style convenience helpers where they materially improve gameplay C++ call sites

Recommended concrete additions:

- `FText ResolveRowDisplayText(UObject* WorldContextObject, const FString& RowId, const FString& LabelColumnOverride = TEXT(""), FString* OutError = nullptr)`
- `FText ResolveRowDisplayText(UObject* WorldContextObject, const FESQLId& SqlId, FString* OutError = nullptr)`
- `bool TryLoadTypedRow<T>(UObject* WorldContextObject, const FESQLId& SqlId, T& OutRow, FString* OutError = nullptr)`
- `bool TryLoadTypedRows<T>(UObject* WorldContextObject, TArray<T>& OutRows, int32 MaxRows, FString* OutError = nullptr)`
- `bool TrySaveTypedRow<T>(UObject* WorldContextObject, const T& RowData, FString* OutResolvedRowId = nullptr, FString* OutError = nullptr)`

Exit criteria:

- gameplay C++ can do load/save/delete/exists/display-label operations through `UESQLTableAsset` without dropping to string SQL for common typed cases


### Phase C++-2: Upgrade The Generic Value And Result Model

Goal:

- make the generic SQL types usable enough that the typed layer, bridge layer, and advanced callers are not forced into raw map/string handling everywhere

Primary files:

- `Source/UnrealExtendedSQL/Shared/ESQLTypes.h`
- `Source/UnrealExtendedSQL/Shared/ESQLTypes.cpp` only if implementation volume justifies adding it later

Required additions:

- upgrade `FESQLBindingValue` from null-or-text into an explicit typed value model
- add convenience accessors on `FESQLRow`
- add convenience accessors on `FESQLQueryResult`

Recommended shape:

- `FESQLBindingValue` should support at minimum:
	- null
	- integer
	- float/double
	- text
	- blob
- `FESQLRow` should support helpers like:
	- `HasColumn()`
	- `IsNull()`
	- `TryGetString()`
	- `TryGetInt64()`
	- `TryGetDouble()`
	- `TryGetBool()`
	- `TryGetText()`
- `FESQLQueryResult` should support helpers like:
	- `HasRows()`
	- `NumRows()`
	- `TryGetSingleRow()`

Exit criteria:

- generic SQL results are no longer painful to consume in transitional C++ code
- typed query/filter work in later phases no longer has to pretend everything is text


### Phase C++-3: Add Structured Typed Query Specs

Status:

- completed

Goal:

- provide a non-SQL-string path for common typed queries so both C++ and K2 can share one structured query contract

Primary files:

- preferably extend `Source/UnrealExtendedSQL/Shared/ESQLTypes.h`
- extend `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.h`
- extend `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.cpp`

Recommended new types:

- `EESQLFilterOperation`
- `FESQLFieldFilter`
- `FESQLSortRule`
- `FESQLQuerySpec`

Required runtime operations on `UESQLTableAsset`:

- `FindRows<T>(...)`
- `FindFirstRow<T>(...)`
- `CountRows(const FESQLQuerySpec&)`
- `LoadPage<T>(...)` or equivalent page-oriented helper if paging is needed immediately

Implemented so far:

- `EESQLFilterOperation`, `FESQLFieldFilter`, `FESQLSortRule`, and `FESQLQuerySpec` now exist
- the structured query types are now reflection-friendly and Blueprint-safe, so later bridge and K2 work can target the same contract directly
- SQL generation for query-spec execution now lives inside `UESQLTableAsset`
- `QueryRows()` exists for raw structured-query execution
- `FindRows<T>()`, `FindFirstRow<T>()`, `CountRows(...)`, and `LoadPage<T>()` now exist as the first filtered typed read core
- paging currently builds directly on `FESQLQuerySpec::Limit` and `FESQLQuerySpec::Offset`

Still to finish in this phase:

- no remaining runtime blockers are identified in the structured typed query-spec layer
- tighten validation only if real typed-query call sites show that more operator/type policing is necessary

Implementation notes:

- field names should be authored field names first, then resolved through `ResolveColumnName()`
- SQL generation should stay internal to `UESQLTableAsset`
- keep the initial operator set small and predictable:
	- equals
	- not-equals
	- less/greater comparisons for numeric-compatible fields
	- `IN`
	- `LIKE` / contains for text-compatible fields

Exit criteria:

- normal filtered typed reads no longer require callers to write SQL text
- the future field-aware query K2 nodes have a runtime contract to target


### Phase C++-4: Add Player-Scoped Typed Table Helpers

Status:

- started

Goal:

- let callers use typed table workflows against player-scoped databases without widening `UESQLSubsystem`

Primary files:

- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.h`
- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.cpp`
- only add one small shared helper if player-context resolution becomes too noisy

Required additions:

- table-asset overloads that accept `UESQLPlayerDBComponent*`
- optional overloads for already-resolved player ids if needed by server systems

Recommended operations:

- `LoadPlayerTypedRow<T>(...)`
- `LoadPlayerTypedRows<T>(...)`
- `SavePlayerTypedRow<T>(...)`
- `DeletePlayerRowById(...)`
- `FindPlayerRows<T>(...)`

Implemented so far:

- `UESQLTableAsset` now exposes the first player-scoped typed row helpers using `UESQLPlayerDBComponent*` as context
- player-scoped typed load, load-many, find, save, and delete now stay table-centric instead of pushing typed behavior back onto `UESQLSubsystem`
- player database file resolution now mirrors the subsystem's player database layout
- server systems can now use the same player-scoped typed helpers with already-resolved `PlayerId` strings instead of requiring a `UESQLPlayerDBComponent`
- player-scoped filtered count and page helpers now exist for both `UESQLPlayerDBComponent*` and already-resolved `PlayerId` workflows, keeping query-spec parity with the shared-table typed API

Still to finish in this phase:

- decide whether any additional player-scoped convenience wrappers beyond the current CRUD/find/count/page surface are worth adding before more bridge and K2 work lands

Implementation notes:

- the player component remains context only
- database routing remains a service concern, but typed row behavior still belongs to the table layer
- do not add player-typed CRUD back to the subsystem public surface

Exit criteria:

- player inventory/quest/stats style workflows can stay fully table-centric and typed in C++


### Phase C++-5: Add Native Typed Async Runtime

Status:

- started

Goal:

- make async the default runtime-safe typed path without forcing everything back through raw `AsyncExecuteSQL`

Primary files:

- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.h`
- `Source/UnrealExtendedSQL/TableAsset/ESQLTableAsset.cpp`
- add one runtime async file only if the signatures become too large to keep in `ESQLTableAsset`

Required additions:

- async typed load/save/find operations that route through the same typed runtime logic as the sync path
- explicit completion callbacks marshalled back to the game thread

Recommended operations:

- `AsyncLoadTypedRow<T>(...)`
- `AsyncLoadTypedRows<T>(...)`
- `AsyncSaveTypedRow<T>(...)`
- `AsyncFindRows<T>(...)`

Implemented so far:

- the first C++ async typed callbacks now exist directly on `UESQLTableAsset`
- completion is marshalled back to the game thread
- query building and struct binding still reuse the same typed table internals as the sync path
- `AsyncFindFirstRow<T>()` now exists so async filtered reads match the sync first-row helper
- `AsyncLoadPage<T>()` now exists on top of `FESQLQuerySpec::Limit` and `Offset`

Still to finish in this phase:

- decide whether additional async helpers beyond the current first-row/page surface are worth adding before the reflection bridge phase
- add any cancellation / lifetime-hardening only if gameplay call sites prove it necessary

Implementation notes:

- the worker-thread implementation should reuse the same internal typed query/save helpers as the sync API
- do not build a second behavior path just because the function is async

Exit criteria:

- gameplay/UI code can stay on the typed table path even when the work must happen asynchronously


### Phase C++-6: Add The Reflection-Friendly Runtime Bridge

Status:

- started

Goal:

- expose the minimum runtime reflection surface required for Blueprint graphs and K2 nodes to call the typed C++ layer

Recommended file layout:

- add one runtime bridge pair:
	- `Source/UnrealExtendedSQL/Blueprint/ESQLBlueprintBridge.h`
	- `Source/UnrealExtendedSQL/Blueprint/ESQLBlueprintBridge.cpp`
- if async Blueprint actions are needed, add one consolidated async file pair rather than many tiny files:
	- `Source/UnrealExtendedSQL/Blueprint/ESQLAsyncTableActions.h`
	- `Source/UnrealExtendedSQL/Blueprint/ESQLAsyncTableActions.cpp`

Required bridge responsibilities:

- `FESQLId` utility UFUNCTIONs
- table-centric `CustomThunk` load/save helpers for wildcard row structs
- generic fallback helpers for dynamic mode
- optional async Blueprint action wrappers over the typed async runtime

Implemented so far:

- the first runtime bridge pair now exists under `Blueprint/ESQLBlueprintBridge.*`
- `FESQLId` utility UFUNCTIONs now exist for creation, validation, equality, existence, and display-text resolution
- table-centric wildcard `CustomThunk` bridge functions now exist for typed row load/save plus typed row-array load/query
- basic typed delete, exists, and structured-query bridge wrappers now exist on the runtime bridge for shared tables
- shared-table count and page bridge coverage now exists for the structured query-spec runtime
- a field-focused bridge helper now exists for `Find SQL Rows By Field`-style editor nodes without exposing query-spec construction in the graph
- the first player-scoped bridge wrappers now exist for single-row load/save/delete plus typed row-array load/query

Still to finish in this phase:

- add any player-id-based bridge overloads the public API still needs, plus any remaining generic fallback dynamic helpers
- add reflection-friendly async wrappers only if the upcoming K2 node work needs them before dedicated async Blueprint actions land

Do include:

- `Make SQL Id`
- `Get SQL Id Value`
- `Is Valid SQL Id`
- `SQL Ids Equal`
- `Does SQL Id Exist`
- `Get SQL Id Display Text`
- typed table bridge calls such as load/save/delete/exists

Do not include:

- generic database-name + table-name + column-name public helpers as the normal path
- a reborn `UESQLBlueprintLibrary` that defines the product surface around raw SQL wrappers

Exit criteria:

- everything the future K2 nodes need already exists in reflection-friendly runtime C++
- there is no node-specific runtime logic left to invent in the editor module


## 5. Remaining K2 Node Phases

All K2 work is editor-only and should live under `UnrealExtendedSQLEditor`.

### Phase K2-1: Editor Module Preparation

Status:

- completed

Goal:

- add the editor-only plumbing required to compile and own SQL K2 nodes cleanly

Primary file:

- `Source/UnrealExtendedSQLEditor/UnrealExtendedSQLEditor.Build.cs`

Required dependency additions:

- `BlueprintGraph`
- `KismetCompiler`
- `GraphEditor`
- `KismetWidgets`

Recommended file layout:

- start with one consolidated folder:
	- `Source/UnrealExtendedSQLEditor/K2Node/`
- keep the initial node implementation in as few files as practical:
	- `ESQLK2Node_Base.h/.cpp` only if shared reconstruction logic is substantial
	- `ESQLK2Nodes.h/.cpp` for the first wave of concrete nodes

Exit criteria:

- the editor module can compile SQL custom nodes without polluting the runtime module

Implemented so far:

- `UnrealExtendedSQLEditor.Build.cs` now includes the editor-only dependencies required for SQL K2 node compilation
- the first SQL K2 node implementations now exist in `Source/UnrealExtendedSQLEditor/K2Node/ESQLRowQueryK2Nodes.*` and `Source/UnrealExtendedSQLEditor/K2Node/ESQLTypedRowK2Nodes.*`
- the editor module now compiles those SQL K2 nodes cleanly in a single-action editor build


### Phase K2-2: Ship The Essential Sync Typed Nodes

Status:

- completed

Goal:

- prove the public SQL Blueprint UX with the smallest node set that actually changes usability

First node set:

- `Get SQL Row`
- `Get SQL Rows`
- `Save SQL Row`
- `Delete SQL Row`
- `Does SQL Row Exist`

Required behavior:

- primary input is `UESQLTableAsset*`
- row reference is `FESQLId` or raw row id where appropriate
- typed struct output/input is inferred from the table asset row struct in typed mode
- no manual `FESQLRow` parsing for the normal path

Typed mode rules:

- if the table asset is a literal/default asset on the node, infer struct pins from `RowStruct`
- reconstruct pins when the asset changes

Dynamic fallback rules:

- if the table asset is dynamic at runtime, the node may fall back to generic output
- dynamic mode is allowed for advanced cases, but it is not the primary UX

Exit criteria:

- a designer can load/save/delete a row from a literal SQL table asset without touching raw SQL or parsing result maps

Implemented so far:

- custom typed nodes now exist for `Get SQL Row`, `Get SQL Rows`, and `Save SQL Row`
- those nodes infer their typed struct pins from a literal `UESQLTableAsset` row struct and expand into the runtime bridge instead of inventing editor-only runtime logic
- `Delete SQL Row` and `Does SQL Row Exist` are already satisfied through the existing runtime bridge Blueprint surface, which does not require custom typed pin reconstruction
- the essential sync typed node set now builds cleanly in the editor module


### Phase K2-3: Ship Async Typed Nodes

Status:

- completed

Goal:

- make the designer-facing runtime path async-first while preserving typed outputs

First async node set:

- `Async Get SQL Row`
- `Async Get SQL Rows`
- `Async Save SQL Row`
- `Async Find SQL Rows`

Implementation notes:

- these should wrap the typed async runtime from Phase C++-5
- do not revive the old generic raw-SQL async action model as the public story
- success/failure exec flow should surface typed outputs and a useful error channel

Implemented so far:

- the first consolidated async runtime action layer now exists in `Source/UnrealExtendedSQL/Blueprint/ESQLAsyncTableActions.*`
- shared-table async load, load-many, find, and save actions now route through `UESQLTableAsset` async helpers instead of inventing a second SQL behavior path
- runtime bridge helpers now exist to populate a typed row or typed row array from an async `FESQLQueryResult`, which gives the editor layer a stable typed hydration surface for the upcoming custom nodes
- dedicated editor-facing async typed K2 nodes now exist in `Source/UnrealExtendedSQLEditor/K2Node/ESQLAsyncTypedK2Nodes.*` for `Async Get SQL Row`, `Async Get SQL Rows`, `Async Save SQL Row`, and `Async Find SQL Rows`
- those nodes stay on typed row or typed row array pins, hydrate outputs through the runtime bridge, and keep success and failure exec flow on the public node surface
- the async typed node surface now builds cleanly in the editor target

Exit criteria:

- UI/gameplay Blueprints can stay on typed nodes even when work must happen asynchronously


### Phase K2-4: Ship Field-Aware Query Nodes

Status:

- completed

Goal:

- expose the structured query-spec layer as designer-friendly graph nodes

Node set:

- `Find SQL Rows By Field`
- `Find First SQL Row By Field`
- `Count SQL Rows`
- `Load SQL Page` if paging is required in the first pass

Required behavior:

- field pin should be a dropdown populated from the selected table asset
- comparison-value pin type should adapt to the selected field where practical
- node expansion should target the structured query-spec runtime contract, not raw SQL text

Exit criteria:

- common filtered table queries are authorable without SQL text or manual column strings

Implemented so far:

- field-aware nodes now exist for `Find SQL Rows By Field`, `Find First SQL Row By Field`, `Count SQL Rows`, and `Load SQL Page`
- the field pin now resolves through a SQL-specific editor pin factory so a literal table asset drives a dropdown of row-struct fields
- the comparison `Value` pin now retargets to the selected field type where practical and falls back to `FESQLBindingValue` when the field cannot be resolved statically
- node expansion now routes typed field values through `MakeSQLBindingValue(...)` and then into the structured query-spec runtime bridge instead of raw SQL text
- the field-aware query node surface now builds cleanly in the editor target


### Phase K2-5: Ship Player-Scoped Typed Nodes

Status:

- completed

Goal:

- expose player-scoped table workflows without putting player-db string details back into Blueprint graphs

Node set:

- `Get Player SQL Row`
- `Get Player SQL Rows`
- `Save Player SQL Row`
- `Delete Player SQL Row`
- `Find Player SQL Rows`

Required inputs:

- `UESQLPlayerDBComponent*`
- `UESQLTableAsset*`

Rule:

- player context is explicit, but schema/table identity still comes from the table asset

Exit criteria:

- player inventory/quest/state graphs stay table-centric and typed

Implemented so far:

- dedicated player-scoped typed K2 nodes now exist in `Source/UnrealExtendedSQLEditor/K2Node/ESQLPlayerTypedK2Nodes.*`
- the shipped node set covers `Get Player SQL Row`, `Get Player SQL Rows`, `Save Player SQL Row`, `Delete Player SQL Row`, and `Find Player SQL Rows`
- each node takes an explicit `UESQLPlayerDBComponent*` plus `UESQLTableAsset*`, keeping player context explicit while leaving schema identity table-driven
- typed row and row-array pins still infer from a literal table asset or connected struct pins instead of forcing raw bridge output handling in normal Blueprint usage
- this player-scoped typed node surface now builds cleanly in the editor target


### Phase K2-6: Utility And UI Support Nodes

Status:

- completed

Goal:

- finish the UX helpers needed by UI-heavy content once the core row/query nodes are stable

Candidate nodes:

- `Get SQL Id Display Text`
- `Does SQL Id Exist`
- `Resolve SQL Id Row` generic fallback
- batch label-resolution helpers for inventory/shop/list UI

Deferred until after the essential row/query nodes:

- query assets
- reusable query authoring assets
- caching/invalidation strategy for large UI screens

Implemented so far:

- the single-item utility node surface already exists through the runtime bridge for `Get SQL Id Display Text` and `Does SQL Id Exist`
- a typed editor fallback node now exists for `Resolve SQL Id Row` on top of the existing typed-row K2 infrastructure
- a batch UI helper now exists in the runtime bridge for `Get SQL Id Display Texts`, which resolves arrays of `FESQLId` values into arrays of `FText` plus per-entry errors for list-heavy screens
- the K2-6 utility/UI helper surface now builds cleanly in the editor target


## 6. Recommended File Strategy

To keep the remaining implementation consolidated:

### Runtime

- continue growing `ESQLTableAsset.h/.cpp` for typed runtime behavior
- continue growing `ESQLTypes.h` for shared value/query-spec/result helpers
- add only one runtime bridge file pair for reflection-facing table/id helpers
- add one async action file pair only if typed async Blueprint exposure cannot stay clean inside the bridge

### Editor

- keep K2 work inside one `K2Node/` folder in `UnrealExtendedSQLEditor`
- keep the first node wave in a small number of focused files while the shared reconstruction patterns stabilize
- split into multiple node files only after the shared reconstruction/typing patterns stabilize


## 7. Recommended Execution Order From Today

1. Phase C++-1: finish typed table runtime helpers, especially label-resolution and clearer typed failure surfaces.
2. Phase C++-2: upgrade `FESQLBindingValue`, `FESQLRow`, and `FESQLQueryResult` so later runtime and K2 work is not built on weak string-only plumbing.
3. Phase C++-3: implement query-spec based typed find/count/page helpers on `UESQLTableAsset`.
4. Phase C++-4: add player-scoped typed table overloads.
5. Phase C++-5: add native typed async runtime.
6. Phase C++-6: add the reflection-friendly runtime bridge and any required async Blueprint actions.
7. Phase K2-1 and K2-2: wire the editor module and ship the essential sync typed nodes.
8. Phase K2-3: ship async typed nodes.
9. Phase K2-5: ship player-scoped node variants.
10. Phase K2-6: finish utility/UI helpers and only then consider query assets.


## 8. First Concrete Next Step

The next implementation step should be:

- build the editor-facing typed async K2 nodes on top of `ESQLAsyncTableActions` and the new result-hydration bridge helpers

Most valuable exact slice:

1. add `Async Get SQL Row` as the first typed async node by expanding into the new async load action plus `Populate SQL Row From Result`
2. add `Async Get SQL Rows` and `Async Find SQL Rows` on the same expansion pattern before widening into player-scoped async variants
3. introduce the first structured query-spec types and one typed `FindRows<T>` path

That sequence gives the project:

- a complete typed table contract for gameplay C++
- a better generic fallback layer
- the exact runtime substrate the first `Get SQL Row` and `Find SQL Rows By Field` K2 nodes will need


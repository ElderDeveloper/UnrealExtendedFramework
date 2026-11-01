# Modular placement support plan

Draft 2 · 2026-11-01 · Planning only. Supersedes Draft 1's mandatory catalog, controller session, registry, move transaction, RPC and persistence architecture. No runtime implementation or project migration has been performed.

Build **placement support** under `UnrealExtendedGameplay/Systems/Placement`: reusable geometry, evaluation and optional preview/session helpers that projects can use without handing over inventory, world lifecycle or game rules. A project may use only the math, use evaluation with its existing preview, or opt into default preview/session helpers. Additional project integration is expected and acceptable.

The references are STG Studio's Fab Advanced Placement System and PawnStarzSimulator's native TalesOfTrade placement system. Their different gameplay models must remain possible. All new types and signatures below are proposals; follow [framework architecture](../../ARCHITECTURE.md) and confirm engine APIs during implementation.

## 1. Findings from both references

### Fab Advanced Placement System

Read-only inspection of the installed pack under `/Game/Assets/Gameplay/STGStudio/AdvancedPlacementSystem/AdvancedPlacement` found:

- `AC_AdvancedPlacement` combines catalog/UI, camera tracing, spawning, placement, rotation, grabbing and network events.
- `BP_PlacementActor` and its shadow subclass manage placed/preview state, overlaps and snapped-parent/dependent relationships.
- `DT_PlacementObjects` supplies 16 demo entries with mesh/actor references, categories, offsets, permitted surfaces and overlap-box settings.

These are inspected capabilities, not a new end-to-end or multiplayer test. The pack provides a complete example, but its actor hierarchy, UI and player workflow are not framework requirements. [Fab listing](https://www.fab.com/listings/6cb15549-61b3-43f9-834f-bb5a720e0b37).

### PawnStarzSimulator / TalesOfTrade

Inspected native source in `C:/Users/kemal/Desktop/PawnStarzSimulator`, especially `Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/` and `Player/Components/TOTPlacementComponent.*`:

| Source | Observed behavior | Reuse boundary |
| --- | --- | --- |
| `TOTPlacementMath.*` | Stateless surface classification, surface-plane grid snapping, rotation about a normal, footprint math and bounded zone sampling | Extract independent native helpers without TOT row/trait types |
| `ValidatePlacement` | Preview and host call the same predicate; result includes measured support and the snapped pose, not just validity | Preserve a shared evaluation path with a canonical returned pose |
| `UTOTPlacementComponent` | Local ghost, cached held eligibility, independent held free-position/free-rotation modifiers, distance adjustment | Useful optional targeting/input helpers; equipment and input tiers stay project-owned |
| `ResolveAim` | An accepted shelf target wins; an incompatible shelf can fall through to world placement; previews can differ | Project chooses target priority and operation; no shelf-specific enum in shared core |
| `TryCommitPreview` | Invalid confirm still consumes Use, preventing accidental equipment use | Input consumption is separate from validity and operation success |
| `HandlePlaceCommand` | Host resolves SQL ownership/traits, revalidates, atomically changes inventory/placement rows, then spawns a representation | SQL, item identity, commands and transactions stay in TOT |
| Move/origin/return paths | Move means pickup into inventory followed by placement, with host origin memory for return | A valid project strategy; do not require Draft 1's in-place edit/lock model |
| `UTOTPlacedItemComponent` | Composes placed state onto different actor types; functionality remains on the actor | Favor composition without requiring this component or its decal base class |
| Zone components | Non-colliding tagged boxes registered for queries, composed and transformed with buildings | Optional spatial helpers, with consumer-controlled availability/invalidation |
| Revalidation/proximity code | Revalidates stored rows, distinguishes unavailable zones, handles support-loss wobble/drop/return and spawns around every player's pawn | Project lifecycle policy; shared evaluation never drops items, deletes rows or starts a spawner |

The older `Documents/item_world_placement_design V2.md` explains intent but is not current implementation evidence. Current source, for example, has separate free-position/free-rotation controls and a proximity spawn layer. Source was read; no PawnStarz files, assets or saves were changed or executed.

### Learn from the implementation without copying its assumptions

- The zone-union test uses a capped lattice. It is an approximation, not an exact proof that an entire solid is covered; small gaps can fall between samples.
- Clearance ignores the supporting actor. General support should allow contact with the supporting surface without exempting unrelated blocking geometry on the same actor.
- The return-moved command explicitly leaves occupied-origin overlap unchecked. Reusable contracts must allow revalidation, reservation or another fallback, rather than unconditional restoration.
- Revalidation can remove a row whose definition is missing. Shared support reports missing data and leaves recovery to the consumer; it never deletes application state.
- TOT marks a preview before `FinishSpawning`/BeginPlay. Tinting an arbitrary gameplay actor after initialization does not make a safe ghost.

## 2. Ownership boundary

**The framework helps answer where and whether a proposed placement fits. The project decides what placement means and applies the operation.**

| Shared placement support | Project-owned behavior |
| --- | --- |
| Surface frames, snapping, offsets and footprint math | SQL, DataTables, data assets, traits, catalog/search UI and item eligibility |
| Evaluation pipeline and optional geometry rules | Ownership, combat, trade reservations, unlocks, prices, refunds and inventory transfers |
| Canonical pose, measured support and reason codes | Spawn, transform edit, pickup/re-place, slot transfer, removal and dependent consequences |
| Optional ray targeting, preview and local session helper | Input stack, camera/hand setup, modal routing, Folio/Slate/UMG and feedback |
| Optional regions and anchor primitives | Durable IDs, save schema/migrations, actor streaming, collision/nav and support-loss policy |
| Intent/evaluation/outcome contracts | RPCs/command dispatcher, authority checks, transactions, deduplication and replication |

A new consumer normally writes a small specification mapper, any targeting/policy customization, a commit adapter and its input/UI glue. No framework code knows `UTOTDatabase`, `FTOTItem`, `UGQSaveSubsystem`, shelves, a particular currency or equipped-item state.

### Decisions replaced from Draft 1

- No required Primary Data Asset catalog or `FPrimaryAssetId` identity; accept resolved project data.
- No mandatory player-controller component, pawn subclass, placed actor or live actor to evaluate.
- No universal committed-object registry, runtime GUID generator or save-record schema.
- No required placement RPC route, server lock system, move transaction or cost-participant protocol.
- No automatic spawning, inventory mutation, persistence or input binding during evaluation.
- Anchors, group editing and integrations are optional increments, not prerequisites for the first useful release.

## 3. Independent levels of adoption

1. **Math only:** use transform/snapping/footprint helpers; retain the project's evaluator, preview and commands.
2. **Evaluation:** map project data into a resolved spec; evaluate on client and host; retain existing session/rendering.
3. **Preview/session helpers:** additionally use local state and default mesh preview; the project still selects targets and commits.
4. **Optional conveniences:** use data-asset wrappers, regions, anchors or input/interaction adapters where useful.

No level silently enables the next. Unconfigured functionality is unavailable/unhandled, never an implicit free-build mode or unconditional permission grant.

## 4. Source layout and small API

Use the existing `UnrealExtendedGameplay` runtime module and requested Systems folder. Respect its feature layout; public-facing headers are explicit, implementation/tests stay private, and unrelated systems are not reorganized.

```text
Source/UnrealExtendedGameplay/Systems/Placement/
  Core/          spec/intent/result, pure math and evaluation
  Rules/         optional geometry rules and project customization
  Targeting/     optional ray/surface proposal helpers
  Preview/       renderer contract and default mesh preview
  Session/       optional local session helper
  Spatial/       optional tagged regions and anchors
  Adapters/      optional Enhanced Input / InteractionSystem conveniences
  Private/       implementation and Tests/
Source/UnrealExtendedGameplayEditor/Placement/
                 optional authoring validation/visualization
Docs/Placement.md
Docs/PlacementPlan.md
```

| Proposed piece | Contract |
| --- | --- |
| `EGPlacementMath` | Stateless native math using geometry/engine primitives; no database, world or actor |
| `FEGPlacementSpec` | Immutable resolved geometry/constraints, independent of definition storage |
| `FEGPlacementIntent` | Requested pose/target, independent position/rotation snapping choices, opaque source reference |
| `FEGPlacementContext` | Explicit world/query access, requester/ignored primitives, support/region availability and read-only project policy |
| `FEGPlacementEvaluation` | Canonical pose, measured support, pending/valid/invalid state, reason codes and relevant revision/token |
| `FEGPlacementEvaluator` | One evaluation pipeline; returns facts and performs no commit side effects |
| `UEGPlacementSessionComponent` | Optional local selection/preview/dispatch state with caller-owned data and commit hook |
| Preview renderer | Optional Begin/Update/End lifecycle; stock mesh renderer or project replacement |

Use native functions/delegates for simple seams. Add reflected structs and BlueprintNativeEvent wrappers where consumers need them; do not require a UObject interface for every helper. Internal query pointers do not belong in wire formats.

The optional session can live on a pawn, controller or other actor with explicit context. Local-player lookup is adapter behavior, never `GetPlayerController(0)` in the solver. Independent views/sessions must not share global preview/input flags. Scripted/headless callers can evaluate without the component.

A passive metadata component or simple mesh actor can be added when useful, but must not become the only integration path. Forward markers, sounds, durable identity, replication and saving remain separate responsibilities.

### Dependencies

Reuse Foundation result/async/asset helpers where needed and existing interaction infrastructure for optional adapters. Keep placement contracts in Gameplay; do not expand Foundation without a demonstrated cross-plugin need. No TOT, GQ, Folio, save-plugin or online-provider dependency enters shared placement.

Declare modules used by public headers as public dependencies; implementation-only dependencies stay private. Editor dependencies stay in the editor module. A runtime toggle does not make a hard module reference optional.

## 5. Project-resolved data and identity

The minimal spec describes authoritative footprint/clearance geometry, pivot/contact offsets, permitted surfaces/slope, rotation policy and optional grid frame/step. Optional support, region and anchor requirements compose with it. Positive uniform scale is the initial supported transform convention; reject unsupported transforms explicitly rather than silently changing them.

Names, icons, prices, quantities, actor classes and persistent IDs are not required to run geometry. A catalog or inventory UI keeps those in its own model.

Source/definition/support references are opaque project handles. If a common reflected representation is needed, use a namespaced string preserving the exact project value; do not generate a second identity or convert SQL IDs into GUIDs. The project resolves trusted values on authority. A client-supplied full spec is never authoritative item data.

A data-asset wrapper is an optional adapter. TOT maps cached Placeable traits; GQ can map native tuning or a DataTable. Neither duplicates its source of truth for the plugin. Content owners handle cook inclusion and async loading. Session startup may wait for data; geometry never loads synchronously during preview.

## 6. Evaluation pipeline

Proposed API sketch: `EvaluatePlacement(Spec, Intent, Context) -> Evaluation`.

1. Check spec/context readiness, finite values and supported transforms.
2. Measure/resolve the proposed surface using the supplied query policy.
3. Build a surface frame, derive spin/orientation, snap in the chosen grid frame and re-ground after snapping as needed.
4. Apply offsets and optional anchor alignment in a fixed order.
5. Run enabled support, region, exclusion and clearance checks against the same footprint.
6. Run project read-only eligibility policies and return a canonical pose, structured reasons and relevant revisions.

Preview draws the returned pose, including a useful invalid pose rather than blinking away. The authoritative project command resolves fresh data and calls the same evaluator, then applies **that** result's pose. Consumers must not independently rebuild the transform. A shared function prevents algorithm drift; different world snapshots can still produce different answers.

Rules are opt-in: there is no compulsory shop boundary, trace channel, slope limit, 25 cm grid or one-level stacking rule. Built-in support covers surface frames, snapping, offsets, authored oriented boxes and optional support/region queries, with project predicates for other needs.

Geometry details:

- Authoritative shapes come from project data, never the ghost's current bounds. Bounds may be precomputed/cached by a mapper.
- Clearance, interaction focus, pawn blocking and navigation are distinct policies; a focus sphere must not become a clearance footprint.
- Support-contact tolerance cannot exempt a whole building from collision.
- Regions can be non-colliding tagged components registered on demand, not physics volumes scanned globally every frame.
- Exclusion-box overlap requires oriented intersection, including thin blockers. Corner/lattice sampling can miss intersections.
- Provide single-box containment as a clear primitive. Union sampling, if included, exposes spacing/caps and its approximate nature. Strict union coverage can use a different project/provider predicate.
- Distinguish unavailable/unloaded region/support data from loaded-but-rejecting geometry. Report pending/unknown so the project chooses fail-closed, retry or later revalidation; never evict a stored object.
- Evaluation cannot consume items, change collision, spawn actors, mutate data, poll a database or make backend calls.

Cache with explicit invalidation for moving blockers/supports, changed regions, item changes and policy revisions. A stationary aim does not prove a cached placement is still valid.

## 7. Target selection, preview and input

A provider proposes a pose plus optional target/reference. It need not imply a universal operation. Providers can represent camera surfaces, shelf slots, puzzle sockets, pointers, tracked hands or scripted transforms. Projects decide priority, fallback and commit routing.

For TOT, accepted shelf stocking can outrank world placement while a rejecting shelf falls through. That arbitration, the two preview classes and inventory transfer belong to TOT. Shared types must not force `ShelfSlot` onto GQ or other consumers.

The optional ray helper supports distance adjustment and bounded surface search. Whether a floor prop aimed at a wall searches down, and how far, is provider configuration. Free position and free rotation are independent intentions. Optional anchor hysteresis only affects local candidate selection; authority still validates the selected anchor.

The stock ghost is mesh-only and cosmetic: no collision, nav, gameplay ticking, replication, interaction or save participation. Valid/invalid/pending appearance is configurable. Projects can supply an existing safe actor preview; its adapter guarantees inertness before gameplay initialization. Core does not spawn arbitrary gameplay classes and convert them after BeginPlay.

Optional session flow: `Idle → Preparing → Previewing → AwaitingOutcome`. Preparing can be skipped when data is ready; after an outcome the project chooses continue or end. Selection/cancellation invalidates stale load callbacks. The helper never equips, reserves inventory, hides an original actor or deletes a placement.

Projects map Confirm/Cancel/Rotate/AdjustDistance/FreePosition/FreeRotation through their input stack. Optional Enhanced Input glue binds requested actions and releases only its own bindings; it cannot replace a project's mapping-tier or modal policy. Held modifiers clear on release, cancellation and focus loss.

**Handled input is not successful placement.** Replies distinguish NotHandled/Handled from dispatch and mutation outcomes. An invalid confirm can be consumed, preventing TOT's equipment-use fallthrough. Repeat placement is driven by actual project inventory/equipment changes, not an assumed stack count in the plugin.

## 8. Commit, authority and cancellation

The project commit adapter takes the evaluated proposal and dispatches its own operation. It may complete synchronously or return a pending correlation token with an eventual applied/rejected/failed/cancelled result. Dispatch acceptance, local rumble and actor replication are not automatically per-request success acknowledgements.

The consumer is responsible for:

- Trusted requester identity, item data, permissions, ownership, ignore lists and support validation.
- Authoritative re-evaluation immediately before mutation, including revision/range/line-of-sight checks and any needed reservations.
- Atomic paying/consuming/creating/storing under its existing transaction model; load required content before consumption.
- Duplicate/stale commands, disconnection cleanup and authoritative replication. The evaluator introduces no second RPC/command system.
- Declared acknowledgement/correlation semantics. A legacy denial-only adapter must not fabricate reliable success; add correlation before enabling automatic retries.
- Unknown outcomes: reconcile a timed-out token before repeating a potentially applied operation.

Optional session completion helpers follow Foundation's per-call exactly-once callback conventions. They do not make distributed project transactions exactly-once. Broadcasts are observer notifications, not substitutes for the initiating operation's result.

### Move and remove remain project strategies

| Strategy | Project work outside shared geometry |
| --- | --- |
| In-place edit | Claim the actor, retain its original pose during preview, validate destination, change transform and restore temporary state on cancel |
| Inventory pickup/re-place | Capacity/identity checks, pickup/equip flow, origin memory, repeat placement and explicit return/keep-in-inventory policy |
| Slot/container transfer | Check compatibility/occupancy and transfer atomically; may use an exact target pose with world rules disabled |
| Dependent/support editing | Own graph, cycle/capacity checks and block/detach/group-move/wobble/drop/return policy |

The plugin imposes no universal `BeginMove`. It evaluates destinations for any of these. Clearing a ghost is local cleanup; reversing a pickup or accepted commit is a separate project operation. Pending outcomes must resolve before claiming world restoration. Occupied origins, missing supports and full inventories need explicit outcomes, not unconditional teleportation/deletion.

No save writer, committed-object registry, proximity spawner, collision/nav reconfiguration, tombstone schema or automatic migration is part of core. Future utilities need demonstrated reuse and their own scoped contracts.

## 9. Consumer integration sketches

### PawnStarzSimulator / TalesOfTrade

Adopt incrementally in `Plugins/TalesOfTrade`; no new gameplay plugin or project migration in this documentation task.

1. Map compatible pure-math calls first, retaining TOT-facing types and tests.
2. Map cached Placeable traits and world policy into shared evaluation inside `ValidatePlacement`. SQL hydration stays in `UTOTDatabase`; support/region providers use caches or explicit query snapshots rather than new per-frame database reads.
3. Preserve `UTOTPlacementComponent`, equipment/held state, shelf/world arbitration, distinct ghosts, independent free modifiers, mapping-tier replacement and handled-invalid confirm behavior.
4. Keep `FTOTCmd_*`, `UTOTCommandDispatcher`, stack splitting, instance identity, rarity/upgrades, `ExecuteAtomic` and ledger events in TOT.
5. Keep move-origin return, functional actors, support-loss recovery, collision/nav thresholds and co-op proximity spawning project-owned. Fix identified edge cases separately with TOT tests, not silently during extraction.
6. For a later migration, follow PawnStarz `AGENTS.md` for both Unreal and HTML contracts. Shared SQL remains authored authority; a C++ helper does not automatically update the HTML implementation.

### Grandfather's Quarry

- Start with an opt-in fixture mapping native tuning/DataTable data and using either its own preview or the stock helper.
- Existing controllers own Enhanced Input, mining/phone gates, aim and Folio presentation. Shared placement does not depend on Folio.
- Existing interaction providers can request actions without surrendering claim/input ownership. A project command applies transforms or creates selected objects after revalidation.
- Economy/unlocks and `UGQSaveSubsystem` retain data/schema ownership. Add records only when a concrete editable-object feature needs them; this plan does not invent a furniture economy or new save collection.
- Preserve portal-rock/socket puzzles, ore GUIDs, interaction IDs and gallery/progress. Any adoption is a deliberate feature migration with regression evidence.

### Modularity acceptance gate

Prove a small actor-transform consumer without an inventory/database and a TOT-shaped consumer using existing IDs, external specs and an externally committed inventory/slot operation. Small native fixtures are sufficient; full project migrations are not a release prerequisite. Neither consumer may patch the shared solver or duplicate item/save state.

## 10. Delivery and validation

All work below is planned. The first release delivers useful support, not full parity with both games.

| Phase | Shared deliverable | Acceptance |
| --- | --- | --- |
| 0 — Contracts | Minimal spec/context/result and consumer/query/commit seams | Direct evaluator needs no session, actor subclass, catalog asset, save or transport |
| 1 — Geometry | Math, canonical evaluation and opt-in surface/grid/footprint/clearance rules | Floor/wall/ceiling, rotated shapes and invalid poses tested; no side effects/game dependencies |
| 2 — Optional helpers | Mesh ghost, local state/outcome handling and semantic actions | Existing previews can bypass helpers; invalid confirm stays handled; cancellation/load/focus races clean up; independent views |
| 3 — Spatial extensions | Tagged regions and anchors where consumers need them | Unknown versus rejected data; rotated regions, seams, sampling limits, thin blockers and moving supports tested |
| 4 — Consumer proof | Two different fixtures and adoption guide | Different IDs/storage/move policies remain intact; server re-evaluation and async correlation demonstrated |
| Later, scoped by need | Data assets, authoring conveniences or input/interaction adapters | Added for actual reuse without mandatory save/network/economy/lifecycle ownership |

Meaningful native tests belong under feature `Private/Tests`, named `UnrealExtendedGameplay.Placement.*`. Test math separately, then world queries and optional session/adapters. Cover context divergence, same-actor support/wall clearance, stale candidates, destroyed targets, cancelled loads, duplicate outcome callbacks, no-renderer/no-feedback mode and multiple views.

Project tests own SQL/inventory atomicity, saves, movement recovery, network loss/late join and disconnect behavior. Passing shared geometry tests does not prove those application systems. No build/relaunch or editor changes are required for this document.

Measure evaluation time, query counts and allocations per active session. Core adds no idle per-object tick, database poll, global actor scan per frame or preview-transform RPC. Load visuals asynchronously and invalidate caches explicitly. Determine CPU/GPU bound status before performance analysis; set numerical budgets from measurements.

Use framework lint for dependency/descriptor changes. Future native builds and authoring follow each project's approved tools. Unreal asset operations use MCP/Python, never filesystem reads/moves of `.uasset`/`.umap`. Inspect native captures after visible implementation changes; real user saves are not fixtures.

## 11. Shared checkout and compatibility

The canonical proposed feature stays in `UnrealExtendedGameplay/Systems/Placement` with `EG` names. Framework consumers currently use different checkout paths: GQ `Plugins/ExtendedFramework` and PawnStarz `Plugins/UnrealExtendedFramework`. This plan is changed only in GQ's framework checkout. Later delivery updates consumer revisions through their normal repository/submodule workflow, rather than maintaining divergent hand-patched implementations.

No shared header references TOT/GQ types or their content. Optional authoring wrappers declare their own cook/dependency ownership. Keep shipped reflected APIs compatible and add redirects when required by framework conventions.

**First implementation step:** agree the small spec/evaluation contract against the two fixtures, then implement geometry. Catalogs, persistence, networking and gameplay operations stay with their existing project owners.

## Source references

Framework/project boundaries:

- [Architecture](../../ARCHITECTURE.md), [interaction system](../Source/UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemComponent.h) and [provider/claims](../Source/UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h).
- [Foundation results](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Result/EFResult.h) and [asset loading](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Assets/EFAssetLoading.h).
- [GQ UI/framework plan](../../../../Documents/Unreal-Plan.md), [save owner](../../../../Source/GrandfathersQuarry/Story/GQSaveSubsystem.h) and [interactions](../../../../Source/GrandfathersQuarry/World/GQInteractions.h).

PawnStarz evidence (local inspection links, not plugin dependencies):

- [Subsystem contract](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacementSubsystem.h) and [validation/commands/lifecycle implementation](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacementSubsystem.cpp).
- [Math contract](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacementMath.h) and [math implementation](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacementMath.cpp).
- [Player component contract](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Player/Components/TOTPlacementComponent.h) and [target/ghost/input implementation](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Player/Components/TOTPlacementComponent.cpp).
- [Placed-item composition](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacedItemComponent.h), [move interaction](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTInteraction_MovePlaced.cpp), [zone component](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/TOTPlacementZoneComponent.h) and [zone tests](C:/Users/kemal/Desktop/PawnStarzSimulator/Plugins/TalesOfTrade/Source/TalesOfTrade/Systems/Placement/Tests/TOTPlacementZoneTests.cpp).
- [PawnStarz project instructions](C:/Users/kemal/Desktop/PawnStarzSimulator/AGENTS.md).

Fab inspection receipts in GQ: `Saved/PackageResearch/PlacementBloodBatch/system_inspection.json` and `placement_flow.json`. Vendor assets are behavioral references, not shared runtime dependencies.

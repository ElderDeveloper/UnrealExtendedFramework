# Placement support plan

Draft 3 · 2026-11-01 · Implemented in code (phases 1 to 5, and the guide from phase 6); see [Placement.md](Placement.md) for use and for what is not done yet (automation tests, asset-preview drawing, the PIE, performance and network checks).

Placement support is a toolkit in `UnrealExtendedGameplay` for answering two questions about an object that a player, a script or an AI wants to put somewhere: **where exactly does it go**, and **is it allowed there**. It solves poses, checks rules, draws a preview and runs a local placement session. It does not own inventory, item data, saving, networking or what a placement means to the game. The project keeps those and plugs into the toolkit at the seams described below.

Every piece can be replaced on its own. A project can use only the math, use the evaluator with its own preview, or use the whole stack with its own rules, aim and commit. Project code at the seams is expected; editing the plugin to fit one project is not.

## 1. Targets

| | |
| --- | --- |
| Engine | Unreal Engine 5.6 and later |
| Plugin | `UnrealExtendedGameplay`, folder `Source/UnrealExtendedGameplay/Systems/Placement/`; editor parts in `Source/UnrealExtendedGameplayEditor/Placement/` |
| Game types | Single-player and co-op, listen and dedicated servers, first and third person; other input devices through a custom aim provider |
| Placement kinds | Free placement on floors, walls and ceilings; grid placement; sockets and slots; uneven ground; moving existing objects |
| Callers | Players through a session; scripts and AI through the evaluator directly; editor tools through the same evaluator |

## 2. What the reference systems teach

Two existing systems were studied as behaviour references. Neither is a dependency, and nothing is copied from them.

**Fab Advanced Placement System (STG Studio)** ([listing](https://www.fab.com/listings/6cb15549-61b3-43f9-834f-bb5a720e0b37)). One Blueprint component does everything: catalog and UI, camera trace, spawning, rotation, grab and move, destroy, and network events. Its placement actors track a snapped parent and their dependents; catalog entries carry permitted surfaces (for example horizontal only), spawn offsets and overlap boxes. It shows the feature set games expect, and the cost of fusing it: a project cannot take the trace and the surface check without also taking the catalog, the actor base class and the input keys.

**A native co-op shop game built on this framework.** Its placement code is the source of most rules in this plan:

- The client preview and the host command call the same validation function, and the result carries the snapped pose and the measured support, not only yes or no.
- The client sends its pose, the kind of surface it believes it aimed at, and what it believes the object rests on. The host snaps that pose again, measures again with a short probe along the claimed normal, and refuses the command as stale when the measured support differs from the claimed one.
- A confirm on an invalid spot is still consumed, so the press does not fall through to the equipped item's own use action.
- Position snapping and rotation snapping are switched off independently: nudging a table off the grid must not also lose its facing.
- Previews are marked before `FinishSpawning`, so their gameplay never starts.
- Placed state is a component composed onto different actor types; the actors keep their own behaviour.
- Placement zones are non-colliding tagged boxes that move with the building they belong to.
- One target (a shelf) outranks world placement when it accepts the item and lets world placement through when it refuses.

It also shows traps to avoid:

- Testing coverage by a union of zones on a capped point lattice is an approximation; small gaps fall between the samples.
- Ignoring the whole supporting actor during the clearance test also ignores unrelated walls of that actor.
- Returning a moved object to its origin without checking that the origin is still free.
- Deleting stored placements whose definition is missing, instead of reporting them.

Each consuming project keeps its own mapping notes in its own repository.

## 3. Ownership

The plugin answers where and whether. The project decides what a placement means and carries it out.

| The plugin provides | The project owns |
| --- | --- |
| Surface frames, snapping, offsets, footprint and box math | Item data, catalogs, search UI, eligibility |
| Pose solvers and rules, with built-in ones for geometry | Ownership, prices, unlocks, inventory, combat or trade locks |
| Canonical pose, measured support, reasons, revisions | Spawning, moving, picking up, slot transfer, removal, dependents |
| Aim providers, preview renderers, a local session | Input mapping contexts and priorities, camera, modal routing, UI and feedback |
| Region and anchor components and their query subsystem | Durable IDs, save schema, streaming, collision and navigation of placed actors, support-loss policy |
| A compact network proposal struct and a commit-handler seam with per-call outcomes | RPCs or command system, authority checks, transactions, de-duplication, replication |

No plugin code knows a project's database, item type, save subsystem, shelves, currency or equipment.

Nothing turns on implicitly. A missing solver, rule list or commit handler is reported as not configured; there is no default free-build mode and no implicit permission.

## 4. Layers

Each layer uses only the layers above it, and a project can stop at any of them.

1. **Math:** stateless functions on vectors, frames and oriented boxes. No world.
2. **Evaluation:** spec, profile, intent and context; pose solvers and rules; one evaluator. Reads the world, changes nothing.
3. **Spatial:** region and anchor components and their query subsystem.
4. **Presentation:** aim providers, preview renderers, debug drawing.
5. **Session:** the local placement session, modes, cache and commit handlers.
6. **Adapters and authoring:** Enhanced Input, InteractionSystem, definition assets, editor visualizers and tools.

## 5. Source layout

The Gameplay module is flat (no `Public`/`Private` split) and keeps each feature's tests beside it, as SoftCollision and InteractionSystem do. Placement follows that.

```text
Source/UnrealExtendedGameplay/Systems/Placement/
  Core/       types, tags, EGPlacementMath, the Blueprint math library, the evaluator, network types
  Solvers/    UEGPlacementPoseSolver and the built-in solvers
  Rules/      UEGPlacementRule and the built-in rules
  Profile/    UEGPlacementProfile, UEGPlaceableDefinition, FEGPlacementSpecRow
  Spatial/    region and anchor components, UEGPlacementSpatialSubsystem
  Aim/        UEGPlacementAimProvider and the built-in providers
  Preview/    UEGPlacementPreviewRenderer, the built-in renderers, debug drawing
  Session/    UEGPlacementSessionComponent, modes, cache, commit handlers
  Adapters/   Enhanced Input binder, InteractionSystem interaction
  Tests/      UnrealExtendedGameplay.Placement.* automation tests
Source/UnrealExtendedGameplayEditor/Placement/
              component visualizers, asset validation, fitting spec boxes from meshes
Docs/Placement.md       user guide, written with the code
Docs/PlacementPlan.md   this plan
```

The folder is called `Aim`, not `Targeting`, so it is not confused with the module's lock-on `Targeting` system.

The runtime module already has the dependencies this needs as public dependencies: `GameplayTags`, `EnhancedInput`, `PhysicsCore` and `ExtendedFoundation`. `FInstancedStruct` is in `CoreUObject`. Editor dependencies go in the editor module's Build.cs only.

## 6. Data model

All data types are reflected from the start (`USTRUCT(BlueprintType)`, `UCLASS(BlueprintType)`), so Blueprint pins never have to change later. Names are proposals; confirm engine APIs while implementing.

### Spec: per item, `FEGPlacementSpec`

What the object is, geometrically. A project builds it from whatever it stores: a DataTable row, a data asset, a database row or native tuning.

| Field | Meaning |
| --- | --- |
| `Shape` | `FEGPlacementShape`: one or more oriented boxes in pivot space. `Footprint` boxes rest on the support; `Clearance` boxes must stay free. Clearance defaults to the footprint |
| `ContactOffset` | Distance from the pivot to the contact point along the surface normal |
| `AllowedOrientations` | Bitmask of Floor, Wall and Ceiling |
| `MaxSlopeDegrees` | Optional; when unset the profile's value applies |
| `Rotation` | `FEGPlacementRotationPolicy`: align to the surface or to world up, spin step, whether free spin is allowed, a fixed spin |
| `Grid` | Optional override of the profile's grid step |
| `Tags` | `FGameplayTagContainer` for rules to match, for example "needs a timber support" or "hangs on hooks" |
| `ProjectData` | `FInstancedStruct` holding project rules' own per-item data |
| `Source` | `FEGPlacementHandle`: the project's identity for the item, opaque to the plugin |

Shapes come from data, never from a preview mesh's current bounds. A mapper or the editor's fitting tool may compute them from meshes ahead of time.

Positive uniform scale is supported. Any other transform is refused with a reason, never silently changed.

`FEGPlacementHandle` is a namespaced string (`Namespace` and `Value`) that holds the project's own ID unchanged. The plugin never generates IDs or converts them, for example SQL IDs to GUIDs. Clients may send handles but never specs: the host resolves the spec from its own data.

### Profile: per placement mode, `UEGPlacementProfile`

A `UPrimaryDataAsset`, like `UEGSoftCollisionProfile`; native code can also create one at runtime with `NewObject`. There is one profile per way of placing things, for example "world", "shelf slot" or "portal socket".

| Field | Meaning |
| --- | --- |
| `Solver` | Instanced `UEGPlacementPoseSolver` |
| `Rules` | Ordered array of instanced `UEGPlacementRule` |
| `Query` | Trace channel or collision profile, complex or simple collision, probe distances |
| `SurfaceBands` | Normal thresholds that separate Floor, Wall and Ceiling |
| `Grid` | Default step and grid frame (world or support-local) |
| `DefaultMaxSlopeDegrees`, `ContactTolerance` | Defaults that a spec can override |
| `SurfaceTagMap` | Physical material to gameplay tag table for surface rules |
| `bCollectAllReasons` | Run every rule (for UI that lists all problems) or stop at the first failure |

### Intent: per call, `FEGPlacementIntent`

What the caller asks for:

- a proposed pose, from an aim provider, a script or a client's command;
- an optional target (`FEGPlacementTarget`: a component, a hit or an anchor);
- independent `bFreePosition` and `bFreeRotation` flags and a requested spin;
- the caller's **claims** (`FEGPlacementClaims`): the surface orientation and the support handle it believes are true, and the revisions it saw.

### Context: per call, `FEGPlacementContext`

The world, the requester actor, ignored actors and components, the profile, the evaluation scope (`Preview` or `Authority`), the spatial subsystem to query, and an optional `FInstancedStruct ContextData` for project rules (for example a snapshot of the requester's inventory state). Local-player lookup happens in adapters; the evaluator never calls `GetPlayerController(0)`.

### Working state and result

`FEGPlacementState` is what solvers fill in and rules read: the pose, the surface hit, the measured normal and orientation, the slope, the support component, actor and handle, the footprint and clearance boxes in world space, and the matched anchor and regions.

`FEGPlacementEvaluation` is what callers get back:

- `Status`: `Valid`, `Invalid` or `Pending`. Pending means data needed for an answer is not available, such as an unloaded region or an asset that is still loading.
- `Pose`: always set, also when invalid, so a preview shows a red ghost where the player aims instead of blinking out.
- The surface orientation, the support (component and handle) and the anchor.
- `Reasons`: an array of `FEGPlacementReason` with a gameplay tag, a display text and the rule that raised it.
- `ClaimMismatches`: which claims disagreed with what was measured.
- `Revisions`: the revisions of the support, regions and anchor the answer depended on.

### Reasons and surface kinds are open

Reasons are gameplay tags, declared natively (`UE_DECLARE_GAMEPLAY_TAG_EXTERN` and `UE_DEFINE_GAMEPLAY_TAG`) under `Extended.Gameplay.Placement.Reason.*`: `NotConfigured`, `InvalidInput`, `UnsupportedTransform`, `NoSurface`, `SurfaceOrientation`, `TooSteep`, `SurfaceTag`, `Blocked`, `Overhang`, `OutsideRegion`, `InExclusion`, `OutOfReach`, `NoLineOfSight`, `SupportRejected`, `AnchorUnavailable`, `ClaimMismatch` and `Stale`, with `Pending.*` children for unavailable data. Projects add reasons under their own tag roots and map tags to text and feedback themselves.

Floor, Wall and Ceiling stay a small enum because they follow from the normal. Finer kinds of surface (shelf, rail, rock wall, timber) are tags: component tags, the profile's physical material table, or a region's tags, checked by a rule.

## 7. Solving and validating

Evaluation is two steps that a caller can run together or separately:

```text
Evaluate(Profile, Spec, Intent, Context)     = SolvePose, then ValidatePose
SolvePose(Profile, Spec, Intent, Context)    -> State        (the profile's solver)
ValidatePose(Profile, Spec, State, Context)  -> Evaluation   (the profile's rules)
```

All three are available natively (`EGPlacement::Evaluate` and so on) and through a Blueprint function library. A project that gets its pose elsewhere (a slot, a scripted spawn, a solver of its own) calls `ValidatePose` and still gets every rule.

### Pose solvers

`UEGPlacementPoseSolver` is an `Abstract, EditInlineNew, DefaultToInstanced, Blueprintable` class with a BlueprintNativeEvent `Solve`, the same pattern as `UEGSoftCollisionDriver` and `UEGChessFeedbackRenderer`. Built-in solvers:

| Solver | What it does |
| --- | --- |
| `Surface` | Probes along the intent's normal, classifies the surface, builds the surface frame, applies the spin, snaps in the grid frame, grounds again after snapping and applies the contact offset |
| `ExactPose` | Takes the intent's pose unchanged: slots, scripted placement, editor tools |
| `Anchor` | Snaps to the best compatible anchor near the intent: sockets, hooks, slots |
| `GroundConform` | Probes under the footprint's corners and centre and fits the contact plane, for uneven ground such as rock floors |

Projects subclass for anything else.

**Solving is idempotent.** Solving the pose that a solve produced gives the same pose, within a fixed tolerance. Grids are defined in a stable frame (the world, or the support component's own frame), never relative to the aim. This is what lets a host solve a client's pose again and land on the ghost's grid cell rather than the neighbouring one.

### Rules

`UEGPlacementRule` uses the same pattern and returns `Pass`, `Fail` (with reasons) or `Pending`. Each rule has a scope (`Preview`, `Authority` or both) and the profile runs them in order. Rules get read-only access to the spec, intent, context and state and must not change the world. Native rules get `const` inputs; for Blueprint rules this is a documented requirement, because the compiler cannot enforce it.

Built-in rules:

| Rule | Checks |
| --- | --- |
| `SurfaceOrientation` | The measured orientation is one the spec allows |
| `Slope` | The slope is within the limit |
| `SurfaceTags` | Surface tags (component tags, physical material, region) meet the spec's requirements |
| `Clearance` | Clearance boxes overlap nothing on the configured channel, tested as oriented boxes. Contact with the support is allowed only within `ContactTolerance` of the contact face; the rest of the supporting actor still blocks |
| `SupportCoverage` | Enough of the footprint rests on support (probe pattern with configurable spacing and minimum fraction), so objects do not hang off edges |
| `SupportFilter` | Accepts or refuses the support by class, tag or interface: no placing on pawns, on previews, or on a shelf that is stocked rather than decorated |
| `RegionContainment` | The footprint is inside the required regions |
| `RegionExclusion` | The footprint touches no exclusion region, tested by exact oriented-box intersection, thin boxes included |
| `Reach` | Distance from the requester |
| `LineOfSight` | The requester's view can see the pose |
| `ClaimMatch` | Authority scope: the claims match the measured facts; otherwise `ClaimMismatch` |
| `Revision` | Authority scope: the revisions the client saw are still current; otherwise `Stale` |

Project rules (ownership, inventory, combat lock, shop open) subclass `UEGPlacementRule` and read `ProjectData` and `ContextData`.

Results combine as follows: any `Fail` makes the evaluation `Invalid`; otherwise any `Pending` makes it `Pending`; otherwise it is `Valid`. Pending is never treated as valid. The caller decides whether to wait, retry or refuse.

### Geometry precision

- Box against box uses the separating-axis test (15 axes), never corner sampling, so thin blockers and crossing edges are found.
- A box inside one region box is tested exactly.
- A box inside a union of regions is tested exactly when the regions and the footprint share a plane, which is the common case of floor zones: the footprint polygon is subtracted from the union of the region polygons in that plane. For regions that do not share a plane the rule samples, with the spacing and sample cap exposed in the profile, and reports the answer as approximate.
- Clearance, interaction focus, pawn blocking and navigation are separate concerns. A focus or pickup sphere is never used as the clearance shape.

### What evaluation never does

It never consumes items, spawns or moves actors, changes collision, writes data, loads assets synchronously, polls a database or calls a back end. Missing data is reported as Pending or as a reason, never repaired or deleted.

## 8. Spatial: regions and anchors

- **`UEGPlacementRegionComponent`:** a non-colliding scene component holding an oriented box, an include or exclude mode, and tags. It moves with its actor, so a building's shop floor moves with the building.
- **`UEGPlacementAnchorComponent`:** a socket for exact placement, with a pose, compatible spec tags and a snap radius. Whether it is occupied is project state, asked through a BlueprintNativeEvent on the anchor or through a project rule; the plugin does not track occupancy.
- **`UEGPlacementSpatialSubsystem`:** a world subsystem. Components register and unregister themselves, and queries by oriented box go through a spatial hash. There are no per-frame actor scans and no physics volumes.
- **Revisions:** each region and anchor has a revision that changes when its transform, tags or settings change. Supports report a revision through `IEGPlacementSupportInterface`; without it, transform changes count. Revisions drive the session cache and let a host see that something a client evaluated has changed since.
- **Availability:** unloaded data is not a refusal. A `UEGPlacementAvailabilityProvider`, which projects can replace, tells the subsystem when an area's regions may be missing, and rules then answer Pending. The default asks World Partition whether streaming is complete for the query bounds; worlds without World Partition are always available.

## 9. Aim, preview and session

### Aim providers

`UEGPlacementAimProvider` turns a view into an intent. The caller builds the view (`FEGPlacementView`: origin, direction, requester, optional player controller) from a camera, a motion controller or a script. Built-in providers:

- **`ViewTrace`:** traces from the view with an adjustable distance, plus a bounded surface search; for example, a floor item aimed at a wall searches downwards, over a configurable distance.
- **`AnchorPick`:** picks an anchor from the view, with hysteresis so the choice does not flicker between two close anchors. Hysteresis only affects the local choice; the host still validates the chosen anchor.

### Modes and arbitration

A session holds an ordered list of `FEGPlacementMode` entries, each with a `ModeTag`, an aim provider, a profile and an `OnInvalid` setting (`Stop` or `FallThrough`). The session evaluates the modes in order: the first valid result wins, a `FallThrough` mode passes to the next one when it is invalid, and a `Stop` mode shows its invalid result. The winning `ModeTag` goes to the commit handler, which routes it, for example "stock the shelf" or "place in the world". Shared types never carry a shelf or slot concept.

### Preview renderers

`UEGPlacementPreviewRenderer` has `Begin`, `Update(Evaluation)` and `End`, and is instanced like the chess feedback renderer. Built-in renderers:

- **`Mesh`:** draws an `FEGPlacementVisual` (static and skeletal meshes with relative transforms and material overrides) with a valid, invalid and pending look, through an override material or material parameters. No collision, no navigation, no replication, and no tick unless it animates.
- **`ActorClass`:** builds the same visual from an actor class without spawning it. It reads the mesh components of the class default object and of the Blueprint construction script templates, including parent classes and inherited-component overrides, caches the result per class and loads the meshes asynchronously.
- **Project previews:** a project can supply an actor-based preview. Its adapter makes the actor inert before `FinishSpawning`. The plugin never spawns a gameplay class and then tries to neutralise it after BeginPlay.

Debug drawing, switched with `Extended.Placement.Debug` console variables, shows footprint and clearance boxes, probes, regions, anchors and the current reasons, in game and in the editor viewport.

### Session

`UEGPlacementSessionComponent` is a local helper that can live on a pawn, a controller or any other actor. Each instance is independent: two local players or two views never share preview or input state.

The states are `Idle`, `Preparing`, `Previewing` and `AwaitingOutcome`. After an outcome the project chooses between previewing again (place another) and ending the session. Preparing loads the selection's visual and any soft data with `EFAssets::Preload` and is skipped when everything is loaded; a new selection or a cancel drops stale load callbacks.

The caller starts a session with a selection: spec, visual, source handle and modes. Input arrives as semantic calls (`Confirm`, `Cancel`, `Rotate`, `AdjustDistance`, `SetFreePosition`, `SetFreeRotation`), each returning `Handled` or `NotHandled`. Confirm on an invalid or pending result returns `Handled` and dispatches nothing, so the press never reaches the equipped item. Held modifiers clear on release, on cancel and when the window loses focus. Placing again depends on the project's real inventory or equipment state, never on a count the plugin assumes.

**Cache.** The session keeps the last solve, keyed by the quantized intent, the spec, the profile and the revisions of everything the result depended on, and reuses it while those match. Clearance can change without any revision changing (a pawn walks in), so rule results also expire after a configurable age. Keeping the aim still never keeps a stale answer alive.

The session never equips, reserves inventory, hides an original actor or deletes a placement.

## 10. Commit and authority

The plugin's part of a commit is small and fixed:

- **`UEGPlacementCommitHandler`** (instanced, Blueprintable) has `Commit(Request, OnComplete)`. It answers at once with `Dispatched` or `Refused`, and later calls `OnComplete` exactly once with `Applied`, `Rejected`, `Failed` or `Cancelled`, tracked with `TEFPendingCalls`. A handler that cannot report outcomes says so, and the session then treats dispatch as final without inventing a success.
- **`FEGPlacementProposal`** is a compact `NetSerialize` struct (quantized pose, mode tag, source handle, claims, revisions, free flags) that a project puts in its own RPC or command. The quantization is finer than any grid step, so solving again stays idempotent.
- **On the host,** the project resolves the spec from its own data, calls `Evaluate` with `Authority` scope on the received proposal, and applies **that** result's pose. The `ClaimMatch` and `Revision` rules catch a world that changed between aiming and committing.
- **Built-in handlers** for games that do not need their own: `SpawnActor` spawns a class at the pose where the caller has authority, and `MoveActor` moves an existing actor in place and restores it on cancel. Both refuse with a reason when the caller lacks authority; a networked client routes through the project's own command instead.

Everything else belongs to the project: who the requester is, permissions, transactions and payment, duplicate and stale commands, disconnects, replicating the result, and reconciling an outcome that never arrived before retrying.

### Move and remove

| Strategy | Plugin support | Project work |
| --- | --- | --- |
| Move in place | `MoveActor` handler, InteractionSystem move interaction | Claiming the actor; what happens to its dependents |
| Pick up and place again | Evaluation of the new spot | Inventory, origin memory, return policy |
| Slot or container transfer | `ExactPose` or `Anchor` solver with world rules left out of the profile | Compatibility, occupancy, atomic transfer |
| Dependents (objects resting on objects) | The support handle in every result | The graph and the policy: block, detach, move together, drop |

Returning an object to its origin is a new placement and is evaluated like one. An occupied origin, a missing support or a full inventory gets an explicit outcome, never a teleport or a deletion.

## 11. Adapters and authoring

- **Enhanced Input:** `UEGPlacementInputBinder` adds a mapping context at the priority the project chooses and binds the configured actions to a session. It removes only its own bindings and never replaces the project's own context stack or modal rules.
- **InteractionSystem:** `UEGInteraction_PlacementMove` starts a move session on the interactor for the interacted actor and holds the provider's `Claim` while the move is open. Projects subclass it to pick up instead of moving.
- **Messages:** sessions announce on the foundation message bus under `Extended.Gameplay.Placement.Event.*` (session started, commit dispatched, outcome, cancelled), with USTRUCT payloads, for listeners such as tutorials or analytics. Listeners never replace the per-call outcome.
- **Definitions:** `UEGPlaceableDefinition` (a primary data asset with spec, visual and an optional actor class) and `FEGPlacementSpecRow` (a DataTable row) serve projects without their own item data. Projects with their own data map it to a spec and skip these. Cook inclusion of those assets is the owning project's job.
- **Editor:** component visualizers for regions and anchors; `IsDataValid` on profiles and definitions (missing solver, empty shapes, rules that can never run); a tool that fits footprint and clearance boxes from a static mesh's bounds or simple collision and writes them into the spec; spec boxes drawn in asset previews.
- **Status:** active sessions, cache hit rate and evaluation time are reported through `FEFServiceStatusRegistry` for `Extended.Status`.

## 12. Consumer shapes

The design is checked against these shapes. Each must work without changing plugin code.

| Shape | Uses |
| --- | --- |
| Single-player free placement | Session, `ViewTrace`, `Surface` or `GroundConform`, `Mesh` or `ActorClass` preview, `SpawnActor` handler |
| Sockets and slots | Anchors, `AnchorPick`, `Anchor` or `ExactPose` solver, a project commit handler |
| Co-op placement from a database-backed inventory | The evaluator on both machines, the project's own spec mapping, its own or the stock preview and session, `FEGPlacementProposal` inside the project's command, a commit handler with deferred outcomes |
| Several targets with priorities | Modes with `FallThrough` |
| Moving existing actors | The InteractionSystem interaction with the `MoveActor` handler, or the project's pick-up strategy |
| Scripted or AI placement | `Evaluate` or `ValidatePose` without a session |

## 13. Build order and checks

The phases are an order of work, not a reduced first release. The whole plan is built.

| Phase | Builds | Check |
| --- | --- | --- |
| 1. Core | Types, tags, math, Blueprint math library | Math tests pass |
| 2. Evaluation | Solvers, rules, profile, evaluator, proposal struct, debug drawing | Evaluation tests pass; in PIE, debug drawing shows the right pose and reasons on floors, walls, ceilings and slopes |
| 3. Spatial | Regions, anchors, subsystem, revisions, availability, exact coplanar union | In PIE, placing inside, across and outside rotated regions and at anchors gives the right answers |
| 4. Presentation and session | Aim providers, renderers including `ActorClass`, session, modes, cache, commit handlers, messages | In PIE, a full place, cancel and place-again loop with each built-in handler |
| 5. Adapters and authoring | Input binder, interaction, definitions, editor tools, status | In PIE, moving an actor through an interaction; box fitting on a real mesh in the editor |
| 6. Proof | Performance capture, network run, `Docs/Placement.md` | Every shape in §12 works without plugin changes; a listen-server client places through a project-style command with a deferred outcome |

PIE checks run in an existing level of any consuming project; no check assets are kept.

**Tests.** Automation tests cover what is pure and shared by every consumer: the math, the solvers and the rules against a test world, under `Systems/Placement/Tests`, named `UnrealExtendedGameplay.Placement.*`. They include the idempotence property, box-intersection edge cases, region unions, Pending against Invalid, and claim mismatches. Everything else is checked by playing it, once per phase. Project behaviour (inventory, saving, networking, recovery) is tested by the project.

**Performance.** Evaluation cost and query count per active session are measured with Unreal Insights in phase 6, and budgets are set from those numbers. There is no idle per-object tick, no per-frame world scan and no preview-transform RPC.

**Repository checks.** Run `python3 Scripts/lint_repository.py` after Build.cs or descriptor changes. Unreal assets are changed through the editor (MCP or Python), never by moving `.uasset` files on disk.

## 14. Compatibility

- All names use the `EG` prefix. Reflected types and Blueprint pins stay stable once released; anything that must move gets a `[CoreRedirects]` entry in the plugin's config, as [ARCHITECTURE.md](../../ARCHITECTURE.md) requires.
- No shared header references a project type or project content.
- Consumers keep the framework at different checkout paths and revisions and update it through their normal repository workflow, never by hand-patching a copy.

## References

- [Architecture and conventions](../../ARCHITECTURE.md)
- Instanced strategy and profile patterns this plan follows: [soft collision drivers](../Source/UnrealExtendedGameplay/SoftCollision/Drivers/EGSoftCollisionDriver.h), [soft collision profile](../Source/UnrealExtendedGameplay/SoftCollision/EGSoftCollisionProfile.h), [chess feedback renderer](../Source/UnrealExtendedGameplay/Systems/Minigames/Chess/Feedback/EGChessFeedbackRenderer.h)
- [Interaction system](../Source/UnrealExtendedGameplay/InteractionSystem/EGInteractionSystemComponent.h) and [provider claims](../Source/UnrealExtendedGameplay/InteractionSystem/EGInteractionProviderComponent.h)
- Foundation: [results](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Result/EFResult.h), [pending calls](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Async/EFPendingCalls.h), [asset loading](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Assets/EFAssetLoading.h), [message bus](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Messages/EFMessageSubsystem.h), [status](../../UnrealExtendedFoundation/Source/ExtendedFoundation/Public/Status/EFServiceStatus.h)

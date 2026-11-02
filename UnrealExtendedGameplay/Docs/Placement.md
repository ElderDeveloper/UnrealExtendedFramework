# Placement support

A toolkit for answering two questions about an object a player, a script or an AI wants to put somewhere: **where exactly does it go**, and **is it allowed there**. It solves poses, checks rules, draws a preview and runs a local placement session. Inventory, item data, saving, networking and what a placement means stay with the project. The design and its reasoning are in [PlacementPlan.md](PlacementPlan.md).

Code: `Source/UnrealExtendedGameplay/Systems/Placement/`, editor tools in `Source/UnrealExtendedGameplayEditor/Placement/`.

## Pieces

| Layer | Types | Use it for |
| --- | --- | --- |
| Math | `EGPlacementMath`, `UEGPlacementMathLibrary` | Surface frames, spin, grid snapping, exact oriented-box tests, plane polygons, plane fitting |
| Evaluation | `EGPlacement::Evaluate / SolvePose / ValidatePose`, `UEGPlacementLibrary`, `UEGPlacementProfile`, solvers, rules | Asking where and whether, from anything: a session, a script, an AI, an editor tool |
| Spatial | `UEGPlacementRegionComponent`, `UEGPlacementAnchorComponent`, `UEGPlacementSpatialSubsystem`, `UEGPlacementAvailabilityProvider` | Placement zones, no-build zones, sockets and hooks |
| Presentation | `UEGPlacementAim_ViewTrace`, `UEGPlacementAim_AnchorPick`, `UEGPlacementPreview_Mesh`, `UEGPlacementPreview_ActorClass`, `EGPlacementDebug` | Turning a view into an intent, drawing the ghost |
| Session | `UEGPlacementSessionComponent`, `FEGPlacementMode`, `UEGPlacementCommitHandler`, `UEGPlacementCommit_SpawnActor`, `UEGPlacementCommit_MoveActor` | The place, cancel and place-again loop |
| Adapters | `UEGPlacementInputBinder`, `UEGInteraction_PlacementMove`, `UEGPlaceableDefinition`, `FEGPlacementSpecRow` | Enhanced Input, moving actors through the interaction system, data for projects without their own |
| Editor | Region and anchor visualizers, `UEGPlacementEditorLibrary`, `UEGPlaceableDefinition` **Fit Shape** buttons | Seeing zones and sockets, fitting boxes from meshes |

Nothing turns on implicitly: a profile without a solver or without rules, or a session without modes or a commit handler, answers `NotConfigured`.

## Data

- **Spec** (`FEGPlacementSpec`), per item: footprint and clearance boxes in pivot space, the contact offset (pivot to contact point along the object's up), allowed orientations, slope limit, rotation policy, grid override, tags, project data and the project's source handle. Shapes come from data; fit them ahead of time with the editor tools, never from a preview's current bounds.
- **Profile** (`UEGPlacementProfile`), per way of placing things: the solver, the ordered rules, the trace query, surface bands, grid, default slope, contact tolerance and the physical-material tag table. A data asset, or `NewObject` at runtime.
- **Intent** (`FEGPlacementIntent`), per call: a *pivot pose* (the pivot `ContactOffset` above the aimed point, up along the surface normal), an optional target, the free-position and free-rotation flags, a spin and the caller's claims.
- **Context** (`FEGPlacementContext`), per call: world, requester, ignored actors and components, scope (`Preview` or `Authority`), view location and project context data.

The result (`FEGPlacementEvaluation`) is `Valid`, `Invalid` or `Pending`, always with a pose, plus the orientation, support, anchor, reasons (gameplay tags under `Extended.Gameplay.Placement.Reason`), claim mismatches, revisions and the full working state.

## Solvers

| Solver | |
| --- | --- |
| Surface | Probes along the intent's normal; snaps across the surface and grounds again along the third grid axis; applies spin and contact offset. Floors, walls, ceilings |
| Ground conform | Probes straight down under the footprint's corners, edge midpoints and centre, fits the plane by least squares and rests the object on the highest contact. Rock floors, rubble |
| Anchor | The intent's anchor, or the nearest compatible, available anchor whose snap radius reaches the intent's pivot |
| Exact pose | The intent's pose unchanged; optionally measures the support below |

Solving is idempotent: a solved pose fed back in solves to itself. Spin is stored as a twist about the object's up in the stable grid frame (world or support), so an authority re-solving a client's pose lands on the same cell and angle.

## Rules

`SurfaceOrientation`, `Slope`, `SurfaceTags`, `Clearance` (physics overlap of oriented boxes; the support may touch only the band within `ContactTolerance` of the contact face), `SupportCoverage`, `SupportFilter`, `RegionContainment` (exact for one box and for coplanar unions, sampled and marked approximate otherwise), `RegionExclusion` (exact box intersection), `Reach`, `LineOfSight`, and the authority-only `ClaimMatch` and `Revision`.

Any `Fail` makes the answer `Invalid`; otherwise any `Pending` makes it `Pending`. Region rules answer `Pending` where the availability provider says regions may not be loaded. Project rules subclass `UEGPlacementRule` and read the spec's `ProjectData` and the context's `ContextData`.

## A session

```cpp
// Setup, once: modes are tried in order; a FallThrough mode lets the next try when it is invalid.
FEGPlacementMode& Shelf = Session->DefaultModes.AddDefaulted_GetRef();
Shelf.ModeTag = TAG_Game_Placement_Shelf;
Shelf.Aim = NewObject<UEGPlacementAim_AnchorPick>(Session);
Shelf.Profile = ShelfProfile;
Shelf.OnInvalid = EEGPlacementModeOnInvalid::FallThrough;

FEGPlacementMode& World = Session->DefaultModes.AddDefaulted_GetRef();
World.ModeTag = TAG_Game_Placement_World;
World.Aim = NewObject<UEGPlacementAim_ViewTrace>(Session);
World.Profile = WorldProfile;

Session->PreviewRenderer = NewObject<UEGPlacementPreview_ActorClass>(Session);
Session->CommitHandler = NewObject<UMyCommitHandler>(Session);

// When the player picks something to place:
FEGPlacementSelection Selection = Definition->MakeSelection(); // or build one from your own data
Session->BeginSession(Selection);

// Input (or UEGPlacementInputBinder):
Session->Confirm();   // Handled even when invalid, so the press never reaches the equipped item
Session->Rotate(1.f);
Session->SetFreePosition(bHeld);
```

The session loads the visual first (`Preparing`, skipped when loaded), previews every frame after the camera moves, reuses a solve while the quantized intent, the profile and every revision it depended on are unchanged (rule results still expire after `RuleResultMaxAge`), and on `Confirm` evaluates once more and hands the fresh result to the commit handler. `AfterCommit` picks between previewing again and ending; the project decides whether there is another item.

## Commit handlers and networking

`UEGPlacementCommitHandler::Commit` answers at once with `Dispatched` or `Refused`; the outcome (`Applied`, `Rejected`, `Failed`, `Cancelled`) arrives exactly once, later, through `CompleteCommit(CallId, Outcome)`. A handler that cannot report returns false from `ReportsOutcomes`; dispatch is then final and no success is invented.

For a networked game, put `Request.Proposal` (`FEGPlacementProposal`, `NetSerialize`, pose quantized to 0.01 cm) in your own RPC with the item's source handle. On the host: resolve the spec from your own data, `Proposal.ToIntent()`, evaluate in `Authority` scope with `ClaimMatch` and `Revision` in the profile, and apply **that** result's pose. Supports spawned at runtime should implement `IEGPlacementSupportInterface::GetPlacementSupportHandle`, and anchors whose occupancy changes should call `SetExternalRevision` with a value both machines agree on.

`UEGPlacementCommit_SpawnActor` and `UEGPlacementCommit_MoveActor` need authority and refuse with `NoAuthority` otherwise.

## Regions and anchors

Add `UEGPlacementRegionComponent` (include or exclude, tags, box extent) and `UEGPlacementAnchorComponent` (pose, accepted spec tags, snap radius, spin) to any actor. They register with the world's spatial subsystem and move with their actor. Occupancy of anchors is project state: override `IsAvailableFor` or add a project rule.

## Preview

`UEGPlacementPreview_Mesh` draws an `FEGPlacementVisual` as transient mesh components on the session's owner: no collision, navigation, replication, shadows or tick. The ghost material defaults to `/UnrealExtendedGameplay/Placement/M_PlacementGhost` (Project Settings > Extended Placement), which reads the `Color` vector parameter; add `/UnrealExtendedGameplay/Placement` to **Directories To Always Cook**. `UEGPlacementPreview_ActorClass` builds the visual from an actor class without spawning it, Blueprint construction scripts and inherited overrides included.

## Debugging

- `Extended.Placement.Debug 1` draws the evaluated boxes, pose, normal, support and reasons; `2` adds nearby regions and anchors.
- `Extended.Status` reports active sessions, evaluations, average evaluation time, cache hit rate and commits.
- Sessions announce `Extended.Gameplay.Placement.Event.SessionStarted`, `CommitDispatched`, `Outcome`, `Cancelled` and `SessionEnded` on the message bus with `FEGPlacementSessionMessage`.

## Not yet done

- The automation tests from the plan (math, solver idempotence, box edge cases, region unions, Pending against Invalid, claim mismatches) are not written.
- Spec boxes are not drawn in an asset preview; data assets have no viewport. Fit them with the **Fit Shape** buttons and check them in a level with `Extended.Placement.Debug 1`.
- Phase checks (PIE runs, the performance capture and the listen-server run) have not been done.

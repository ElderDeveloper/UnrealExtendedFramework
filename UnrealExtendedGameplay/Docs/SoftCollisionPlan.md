# Soft Collision plan

Soft Collision replaces pawn-vs-pawn blocking with soft, overlapping pushes. Pawns walk into each other, overlap slightly, and are pushed apart smoothly. A player's own pushes are predicted on their client and the server reaches the same result, so moving through crowds does not cause corrections.

It is a drop-in system: one component on the pawn. It needs no Character, CharacterMovementComponent or Pawn subclass, and no Gameplay Ability System.

## Using it

1. Add `UEGSoftCollisionComponent` to the pawn (Blueprint or C++). That is all a pawn needs: without a profile the class defaults of `UEGSoftCollisionProfile` apply, and the drivers are picked from the pawn's movement.
2. Optionally create a `UEGSoftCollisionProfile` data asset per kind of pawn (players, small AI, large AI, vehicles) and assign it to the component.
3. Optionally implement `IEGSoftCollisionInterface` on the pawn for game rules: who may push whom (`GetSoftCollisionFilter`), a profile override, and `ModifySoftPushVelocity` to reshape or veto pushes. Call `RefreshFilter` when the rules change (death, mounting, team change).
4. Bind `OnSoftPush` and `OnBump` for animation, sound and voice lines. `SetSoftCollisionEnabled(false)` on every machine for dead or ragdolled pawns.
5. Turn on push-model replication (`net.IsPushModelEnabled 1`).

Docs/SoftCollisionTesting.md is the verification procedure: the engine checks of Phase 0, the network test matrix and the performance captures.

## Targets

| | |
| --- | --- |
| Engine | Unreal Engine 5.6 and later |
| Scale | Hundreds to about a thousand pushing pawns in one world |
| Game type | Co-op first; competitive must remain possible through configuration |
| Pawn types | Characters (predicted) and non-Character pawns (pawn movement, physics bodies, custom drivers) |
| Plugin | `UnrealExtendedGameplay`, folder `Source/UnrealExtendedGameplay/SoftCollision/` |

Performance budgets, to be confirmed with Unreal Insights in Phase 6:

- Server, 1000 registered pawns, 900 of them AI: under 0.5 ms game thread per frame for registry, grid and push application, with the narrow phase on worker threads.
- Client: under 0.1 ms game thread per frame. A client scans only its own pawn.
- Bandwidth: about 12 bytes per push request from a client. Nothing extra per AI pawn except throttled cosmetic bump events.

## Core rules

1. **A pawn only ever pushes itself.** Each pawn moves itself away from what it overlaps; nothing moves another pawn. Every push is decided by the machine that already owns that pawn's movement, which is what makes prediction possible. Pushing someone out of the way comes from the strength model: when a runner hits an idle pawn, the idle pawn pushes itself away hard and the runner is barely deflected.
2. **A predicted push is a CharacterMovementComponent root motion source.** Root motion sources are the only way to add predicted movement to an unmodified CMC: the client stores the active sources in its saved moves and replays them after a correction.
   - Rejected: `AddForce`, `AddImpulse` and `LaunchCharacter` on a player-controlled pawn. They are not stored in saved moves; a server-only impulse on a remote client's pawn gets corrected. They are fine on pawns the server alone simulates (see drivers).
   - Rejected: `AddMovementInput`. It is predicted for free, but it changes acceleration, which turns characters that orient to movement and starts walk animations.
   - Rejected: a CMC subclass with custom move data. It would force every project onto our movement component and block modular use.
3. **One push per pawn at a time.** All neighbours are summed into a single vector and a new push replaces the active one. Never one source per neighbour.

## Engine facts

Checked against Epic's API reference and documentation (through search excerpts; see the access note), community code and forum reports. The engine source has not been read yet.

Status: **Confirmed** = Epic documentation text. **Likely** = secondhand or consistent reports. **Unknown** = needs the engine source or a test map.

| Fact | Status | What it means for the design |
| --- | --- | --- |
| `FRootMotionSource::StartTime` is in character movement client time and exists to handle inconsistent tick times | Confirmed | The client move timestamp is the right key for when a push starts |
| `IsStartTimeValid()` tells whether the start time was set, so callers may set it | Confirmed | The server can set the client's start time on the source it applies |
| `FRootMotionSourceGroup::SetPendingRootMotionSourceMinStartTimes` "can be used on servers to match client-side start times" | Confirmed | The engine itself supports matching a client start time on the server |
| `ApplyTimeStampReset` shifts the start times of sources when the timestamp resets | Confirmed | Active sources survive resets; only requests in flight need handling |
| Unset start time defaults to `ClientData->CurrentTimeStamp` on the autonomous proxy and `ServerData->CurrentClientTimeStamp` on the authority | Likely | Both sides start the source on the move after that timestamp |
| How `PrepareRootMotion` treats a start time in the past (trimmed first tick, no catch-up?) | Unknown | Decides how a late request is applied; Phase 0 |
| `Matches` decides which local source a server correction applies to; Epic suggests child classes use their own unique ID | Confirmed | Our source matches on `Sequence` |
| A source that exists only on the client is never removed by server corrections | Likely | The client must remove a push the server rejected or never acknowledged |
| Root motion sources are saved in `FSavedMove_Character` and replayed on correction (autonomous proxies) | Confirmed | Predicted pushes replay correctly |
| Moves are not combined while root motion sources are active | Likely | A pushing player sends every move, which costs bandwidth and server CPU; pushes must be short and rare |
| `RepRootMotion.AuthoritativeRootMotion` replicates active sources to simulated proxies, resent while active | Confirmed / Likely | Extra traffic per pushing player; only player pawns use root motion |
| `ServerMovePacked` is unreliable and carries new, pending and old important moves | Confirmed | Push requests copy the same redundancy idea |
| RPC execution order is kept for all RPCs on an actor and its subobjects; reliable vs unreliable order is not guaranteed; under Iris unicast RPCs go to the reliable queue | Confirmed | A push request sent before the move arrives before it, if it arrives |
| Client RPCs run during network receive at the start of the server frame, before the tick groups | Likely | The server applies a remote player's push inside the RPC handler |
| `bServerAcceptClientAuthoritativePosition` and `bIgnoreClientMovementErrorChecksAndCorrection` are public, Blueprint read-write, read only on the server | Confirmed | The safety net can toggle them from a component |
| `MAXPOSITIONERRORSQUARED` is a squared distance; default 3.0, about 1.7 cm | Confirmed / Likely | Tiny timing differences exceed it, so Level 1 matters |
| `IsClientTimeStampValid` is a public const check that reports timestamp resets; `OnClientTimeStampResetDetected` needs a subclass | Confirmed | Resets are detected by calling the check or watching the timestamp jump back |
| `APawn::ReceiveControllerChangedDelegate (Pawn, OldController, NewController)` fires on the server and owning client | Confirmed | Switching between predicted and authority drivers on possession |
| `AddForce` uses mass; `AddImpulse` with `bVelocityChange` ignores it; both accumulate and apply on the next movement update | Confirmed / Likely | Basis of the AI driver |
| `FTickableGameObject` and tickable world subsystems tick after the tick groups, at the end of the frame | Likely | Where the scan is snapshotted and launched |
| Iris is Experimental in 5.6; Iris replicates structs with custom `NetSerialize` from their reflected properties and warns about it | Confirmed / Likely | Every replicated field is a `UPROPERTY`; a real Iris serializer comes later |
| Mover is Experimental in 5.6, its prediction backend does not work with Iris in 5.6, and layered moves are being reworked | Confirmed | The Mover driver is deferred |
| Open engine issue UE-206616: root motion source desync under forced adjustment | Confirmed | Watch it in the network tests |

### Access note

Epic's documentation and forum sites (`dev.epicgames.com`, `forums.unrealengine.com`, `docs.unrealengine.com`) are blocked by this environment's network policy, and the engine repository (`EpicGames/UnrealEngine`) is not available to this session's GitHub account. Facts were taken from search excerpts of those pages. Before Phase 0 closes, the Unknown and Likely rows should be checked in the 5.6 engine source.

## Who does what

| Machine and pawn | Scans | Applies | Network |
| --- | --- | --- | --- |
| Standalone, listen-server host's own pawn | Yes | Directly | Normal movement replication |
| Server, AI or other server-controlled pawn | Yes | Directly, through an authority driver | Normal movement replication |
| Owning client, its own player pawn | Yes | Immediately, predicted | Sends push requests to the server |
| Server, a remote player's pawn | No | In the request's RPC handler, before the matching move is processed | Ack or reject |
| Client, everyone else's pawns | No | No | Follows replicated movement; receives cosmetic bump events |

A client scans one pawn. The server scans only the pawns it controls.

Possession changes are followed through `APawn::ReceiveControllerChangedDelegate`: a pawn switches between the predicted and authority paths when a player possesses or leaves it.

## Prediction keys

```cpp
struct FEGSoftPushKey
{
    uint16 Sequence;        // per pawn, wraps
    float  ClientTimeStamp; // CMC client move timestamp the push starts after
};
```

- `Sequence` identifies a push for de-duplication, acks, rejects and source matching, like the interaction system's `ActivationId`.
- `ClientTimeStamp` is `GetPredictionData_Client_Character()->CurrentTimeStamp` at the moment the push is applied. It is the clock the engine already uses for root motion start times, so the key names the move the push starts on.

### Frame timing

1. **Start of the client frame**, from `FWorldDelegates::OnWorldPreActorTick`, before any movement ticks: apply the pending push results. The source's start time is the current client timestamp, so it starts on this frame's move. Send the request at once, before the CMC sends that move.
2. **Server, network receive.** The request runs before the move it belongs to, when both arrive. The handler applies the source with the client's start time, so the next move simulates exactly what the client simulated.
3. **End of the frame**, in a tick function in `TG_PostUpdateWork` after every pawn moved: snapshot positions and launch the scan for the next frame (see Threading).

### Request flow

1. **Client.** Quantize the push, apply it with its start time set to `ClientTimeStamp`, and queue the request.
2. **Client, once per frame.** Send `ServerSoftPush(Batch)`. The RPC is unreliable and every batch repeats the requests not yet acknowledged from the last 100 to 150 ms, as the CMC does for important moves. A reliable RPC that gets resent arrives too late to be useful.
3. **Server.** Drop duplicates by `Sequence`, validate, and apply with the client's start time. A request that arrives after its move (its first copy was lost) is applied the way Phase 0 decides: either trimmed so it ends at the same timestamp as on the client, or rejected.
4. **Ack.** An owner-only, push-model replicated `LastAcceptedSequence`.
5. **Reject.** `ClientRejectSoftPush(Sequence)`. The client removes its source and the CMC's correction fixes the position.
6. **No answer.** A client-only source is never removed by server corrections. If a push is neither acknowledged nor rejected within the round-trip time plus a margin, the client removes it.
7. **Timestamp resets.** Active sources are shifted by the engine. Requests in flight across a reset are applied immediately instead of scheduled; the server detects the reset with `IsClientTimeStampValid`.

### Quantization is for correctness, not only bandwidth

The client applies the de-quantized values locally, so client and server build identical sources. Corrections compare server and client sources, so any difference shows up as an error.

| Field | Size |
| --- | --- |
| Sequence | 2 B |
| Client timestamp | 4 B |
| Direction (yaw) | 2 B |
| Strength | 2 B |
| Flags and main neighbour | 1 to 3 B |

Duration and falloff come from the profile and are never sent.

## Root motion source: `FEGRootMotionSource_SoftPush`

A `USTRUCT` deriving from `FRootMotionSource`, with `TStructOpsTypeTraits` `WithNetSerializer` and `WithCopy`, and overrides of `Clone`, `GetScriptStruct`, `Matches`, `MatchesAndHasSameState`, `UpdateStateFrom`, `PrepareRootMotion` and `NetSerialize`.

- Every replicated field is a `UPROPERTY` of a small type (quantized yaw and strength, `Sequence`), so it replicates correctly under both the classic path and Iris.
- `Matches` calls the base and then compares `Sequence`.
- Falloff is an analytic formula (constant, linear, ease-out), so no curve asset reference has to replicate.
- Additive, low priority, `IgnoreZAccumulate` so gravity still works, and keeps or clamps its last velocity when it ends.
- Costs that come with root motion: moves are not combined while it is active, and simulated proxies receive it through `RepRootMotion`. Both are limited to player pawns and kept small by short pushes and the reissue rule. Phase 6 measures them.

## Sync safety net

Set per profile.

- **Level 0, always on.** The move-anchored prediction keys above.
- **Level 1, on by default.** While a push is active, enable `bServerAcceptClientAuthoritativePosition`. The engine documents it for short bursts of movement that are hard to sync. The server still checks its normal error tolerance; inside the tolerance it adopts the client's position, so tiny timing differences do not accumulate into corrections. It adds no way to cheat.
- **Level 2, opt-in, a reasonable choice for co-op.** Also enable `bIgnoreClientMovementErrorChecksAndCorrection` during the push window. Together the two flags give the client full authority, so the server checks each move itself: if the client moved further than max speed times delta time plus the push allowance, it closes the window and forces a correction.
- Both flags are shared CMC state. The component takes a counted lease and restores the previous values when the last push ends. A game that toggles the same flags itself must be told in the documentation.

## Threading

The calculation runs on worker threads inside one frame. It never runs asynchronously across frames: a push must land on a specific move, and late results would be built from stale positions.

1. **Snapshot, game thread, end of frame.** Copy position, velocity, shape and cached rule flags of every registered pawn into flat arrays and update the grid. Worker threads only ever read this copy, never an actor.
2. **Compute, worker threads.** Launch a `UE::Tasks` task that runs the neighbour tests and strength sums with `ParallelFor`. Each scanner writes only its own result slot, so no locks are needed. Neighbour lists use fixed-size buffers, so the loop never allocates. The game thread is not blocked.
3. **Apply, game thread, start of the next frame.** In `OnWorldPreActorTick`, wait on the task (normally already finished), apply the pushes, and send requests.

Below about 64 scanners the computation runs inline, because starting tasks would cost more than the work. A client scans one pawn, so it never uses worker threads.

Game rules that need UObjects (the interface's filter and profile override) are resolved into plain values at BeginPlay and on `RefreshFilter`, and copied into the snapshot, never called from a worker thread.

## Scanning and performance

Built for a thousand pawns from the start.

- **Registry.** `UEGSoftCollisionSubsystem`, a world subsystem, keeps every registered pawn in flat arrays of plain structs (`FEGSoftBody`: position, velocity, shape, weight, filter) next to the registry entries (cell, scan schedule, contact state). Components never tick. Registrations and removals are deferred to the snapshot, so the arrays never change while the scan task reads them.
- **Spatial grid.** A uniform 2D hash grid with cells twice the largest push radius. An entry is re-binned only when its cell changes. A neighbour query reads the 3x3 cells around a pawn. No physics traces at all.
- **Narrow phase.** Plain math: vertical cylinder (capsules) and oriented box, plus a height-overlap test.
- **Sleeping.** A pawn that is not moving and has no neighbour in its 3x3 cells is skipped. The grid wakes it when a neighbour moves into range.
- **Scan rate by relevance.** On the server, AI far from every player scans less often or not at all. Distances and rates are in the profile.
- **Reissue rule.** A new push is sent only when the direction changes more than a set angle, the strength changes more than a set ratio, or the current push is about to end. A per-pawn token bucket also caps requests, which protects the server from a misbehaving client.
- **Client registry.** A client registers only the pawns replication relevancy gives it, and only its own pawn scans.
- **Replication cost.** All replicated properties use the push model, so a thousand components cost nothing to compare while idle.
- **Avoidance.** Profiles scale push strengths down for Characters using RVO avoidance (`AvoidanceScale`), so the two systems do not fight. Detour crowd avoidance is not detected; give crowd-steered pawns their own profile.
- **Collision.** The component sets the push primitive's response to the soft channel to Ignore by default, not Overlap: Overlap makes the physics scene report every touching pair, which is expensive with hundreds of pawns.

The CMC itself is the main cost of a thousand moving Characters. Soft Collision must stay a small fraction of it, and the Phase 6 numbers are reported against that baseline.

## Strength model

- **Penetration** `p` = overlap depth divided by combined radius, 0 to 1.
- **Momentum share** `share_me = M_other / (M_me + M_other)`, where `M = Weight * max(speed toward the other pawn, IdleFloor)`. An idle pawn hit by a runner takes almost all of the push, the runner almost none, and two idle pawns split it evenly.
- **Strength** per neighbour = `SeparationStrength * Response(p) * 2 * share`, with the deep-overlap ramp applied to `Response(p)`. The factor 2 gives two idle equal pawns the full strength each. Summed as vectors, then capped at `MaxTotalStrength`. Pawns a filter excludes contribute nothing.
- **Separation band.** Gentle and continuous while pawns overlap.
- **Bump band.** When the share is at least `BumpMinShare` and the relative approach speed at least `BumpMinRelativeSpeed`: the strongest such neighbour adds `BumpStrength * min(relative speed / BumpReferenceSpeed, 1.5) * min(2 * share, 2)`, and the push fires the bump event. A bump inside its cooldown falls back to the separation part alone.
- **Deep-overlap guard.** Past a set penetration the strength ramps steeply so pawns never fully merge.
- **No blocking core.** The plan considered a small inner collider that still blocks. Dropped: the CMC only sweeps its updated component (the root capsule), so a child collider would never stop a Character, and the deep-overlap ramp already keeps pawns from merging.

## Drivers

A driver decides how a push is applied. `UEGSoftCollisionDriver` is an instanced object on the profile; the component holds one predicted driver and one authority driver and uses whichever matches the pawn's current control.

| Driver | Pawns | Network |
| --- | --- | --- |
| `_CharacterRootMotion` | Player-controlled Characters | Predicted with the keys above |
| `_CharacterForce` | Server-controlled Characters (AI) | Authority only, through `AddImpulse` with velocity change; keeps root motion out of AI replication |
| `_PawnMovement` | Any other `UPawnMovementComponent`, such as floating pawn movement | Authority only, adds to velocity |
| `_PhysicsBody` | Pawns whose root primitive simulates physics, such as vehicles | Authority only, adds force to the body |
| Custom | Anything else, in C++ or Blueprint | Chosen by the driver |

Each driver declares where it runs: predicted, server authority, or owner authority. Owner authority covers pawns whose movement the owning client already controls.

A Mover driver is deferred until Mover leaves Experimental and its layered-move rework lands. It will go in a separate optional module so the main plugin never depends on the Mover plugin.

## Modularity and API

- **`UEGSoftCollisionComponent`.** Add it to any pawn. It sets the push primitive's response to the soft channel (Pawn by default) to Ignore, restores it at `EndPlay`, registers with the subsystem, and picks its drivers. Blueprint API: `SetSoftCollisionEnabled`, `SetProfile`, `RefreshFilter`, `IsPushActive`, `GetActivePushVelocity`, `GetEffectiveProfile`, `GetPushPrimitive`, `FindSoftCollisionComponent`.
- **`UEGSoftCollisionProfile`**, a `UPrimaryDataAsset`: shape and radius scale, weight and push power, separation and bump bands, durations and falloff, scan rates and relevance distances, safety level, drivers.
- **`FEGSoftCollisionFilter`**: a layer (0 to 31), the layers allowed to push this pawn, and can-push and can-be-pushed switches. Set on the component, or computed by the interface.
- **`IEGSoftCollisionInterface`**, optional on the pawn, all `BlueprintNativeEvent`s: `GetSoftCollisionFilter`, `GetSoftCollisionProfileOverride`, and `ModifySoftPushVelocity` (runs once per issued push on the deciding machine; return zero to veto).
- **Drivers in Blueprint.** `UEGSoftCollisionDriver_Blueprint` exposes apply, tick, cancel and supports as implementable events. Slower than native drivers, so meant for a few special pawns.
- **Events.** `OnSoftPush` and `OnBump` fire at once on the owner's machine and authoritatively on the server.
- **Cosmetic bumps.** Other machines receive bumps through a throttled, push-model replicated struct with a counter, skipping the owner. This is the hook for shove montages, physical hit reactions and voice lines.

## Debugging

- Console variables: `eg.SoftCollision.Enable`, `eg.SoftCollision.Debug`, `eg.SoftCollision.ShowGrid`, `eg.SoftCollision.ForceServerOnly`, `eg.SoftCollision.Parallel`, `eg.SoftCollision.ParallelThreshold`, and for the engine checks `eg.SoftCollision.SafetyLevelOverride` and `eg.SoftCollision.Debug.RejectEvery`.
- Visual Logger entries for scans, requests, accepts, rejects, late arrivals and unanswered pushes.
- `stat EGSoftCollision`: snapshot, inline scan, wait, apply and frame update timings; registered pawns, scanners, sleeping pawns, grid cells, neighbour tests, pushes issued, requests sent, accepted, rejected, late, unanswered. Insights CPU scopes `EGSoftCollision_*` cover the same steps, including the worker-thread scan.
- The engine's `p.NetShowCorrections` shows the corrections that still happen.

## Tests

Automation tests, in the style of `EGSkillCheckTests` and `EGInteractionRegressionTests`:

- Strength model and momentum share.
- Quantize round trips produce identical values on both sides.
- Grid insert, re-bin and neighbour queries.
- `Sequence` wrap-around and de-duplication.

Play-in-editor matrix:

- 2 to 4 clients; lag 0, 100 and 250 ms; packet loss 0 and 5 percent.
- Walking into an idle player, head-on collision, 30 AI around a player, a doorway choke point, listen-server host against a client.
- Stress: 1000 AI pawns on a dedicated server with 4 clients.
- Run with Iris both off and on.
- Metrics: corrections per minute against the engine's default blocking collision, frame time, bandwidth per client, server movement cost per pushing player.

## File layout

```
Docs/SoftCollisionPlan.md                  this document
Docs/SoftCollisionTesting.md               engine checks, network matrix, performance captures
Source/UnrealExtendedGameplay/SoftCollision/
    EGSoftCollisionTypes.h/.cpp            shapes, filter, tuning, keys, requests, events, enums, log and stats
    EGSoftCollisionMath.h/.cpp             narrow phase, strength model, quantization, token bucket (plain C++)
    EGSoftCollisionGrid.h/.cpp             spatial hash grid (plain C++)
    EGSoftCollisionProfile.h/.cpp
    EGSoftCollisionInterface.h
    EGSoftCollisionComponent.h/.cpp        control, results, requests, server application, events
    EGSoftCollisionSubsystem.h/.cpp        registry, snapshot tick, scan task, apply, console variables
    EGRootMotionSource_SoftPush.h/.cpp
    Prediction/EGSoftCollisionPrediction.h/.cpp   client queue, server state, safety lease
    Drivers/EGSoftCollisionDriver.h/.cpp          base and Blueprint base
    Drivers/EGSoftCollisionDriver_CharacterRootMotion.h/.cpp
    Drivers/EGSoftCollisionDriver_CharacterForce.h/.cpp
    Drivers/EGSoftCollisionDriver_PawnMovement.h/.cpp
    Drivers/EGSoftCollisionDriver_PhysicsBody.h/.cpp
    Tests/EGSoftCollisionTests.cpp
```

## Phases

"Written" means the code exists on this branch. It was written without an engine or compiler available, so nothing has been compiled or run yet. "Verified" means checked in a running engine; those steps are in Docs/SoftCollisionTesting.md.

| Phase | Written | Verified |
| --- | --- | --- |
| Documentation pass | Yes. Engine facts above | Not needed |
| 0. Engine spike | Procedure and debug switches (`SafetyLevelOverride`, `Debug.RejectEvery`) | No. Needs the 5.6 engine source or a test map |
| 1. Data and math | Yes. Types, profile, interface, narrow phase, strength model, quantization, tests | No. Tests not run |
| 2. Registry and scanning | Yes. Subsystem, grid, snapshot tick, task pipeline, relevance, sleeping, debug drawing, `_CharacterForce` | No |
| 3. Predicted push | Yes. `FEGRootMotionSource_SoftPush`, `_CharacterRootMotion` | No |
| 4. Prediction | Yes. Keys, batched redundant requests, server application in the RPC handler, validation, ack, reject, timeout, scheduled removal, safety lease with the Level 2 check | No |
| 5. Drivers and API | Yes. `_PawnMovement`, `_PhysicsBody`, Blueprint driver base, events, cosmetic bumps, Blueprint API | No |
| 6. Optimization | Code side: inline threshold, worker-thread scan, sleeping, relevance, reissue rule, push model, Ignore response, movement delegate bound only while needed, stats and Insights scopes | No. Captures at 100, 500 and 1000 pawns not taken |
| 7. Verification and documentation | Header documentation; test procedure and result tables | No. Network matrix not run |

What Phase 0 still has to answer, in order of risk:

1. Whether the server applies a source from a client start time exactly, and what it does with a start time in the past (Testing checks 1 and 2).
2. Whether a push rejected by the server comes back when the client replays its saved moves after a correction (check 5). If it does, the reject path has to remove it from the saved moves as well.
3. Whether moves really are not combined while a push is active, and what that costs (check 3).
4. Whether the request reliably arrives before its move, with and without Iris (check 6).
5. `RepRootMotion` cost on other clients (check 7) and timestamp resets (check 8).

The Mover driver stays deferred until Mover leaves Experimental.

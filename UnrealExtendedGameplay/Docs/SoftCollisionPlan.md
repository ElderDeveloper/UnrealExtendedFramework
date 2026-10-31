# Soft Collision plan

Soft Collision replaces pawn-vs-pawn blocking with soft, overlapping pushes. Pawns walk into each other, overlap slightly, and are pushed apart smoothly. A player's own pushes are predicted on their client and the server reaches the same result, so moving through crowds does not cause corrections.

It is a drop-in system: one component on the pawn. It needs no Character, CharacterMovementComponent or Pawn subclass, and no Gameplay Ability System.

## Targets

| | |
| --- | --- |
| Engine | Unreal Engine 5.6 and later |
| Scale | Hundreds to about a thousand pushing pawns in one world |
| Game type | Co-op first; competitive must remain possible through configuration |
| Pawn types | Characters (predicted) and non-Character pawns (pawn movement, physics bodies, custom drivers) |
| Plugin | `UnrealExtendedGameplay`, folder `Source/UnrealExtendedGameplay/SoftCollision/` |

Performance budgets, to be confirmed with Unreal Insights in Phase 6:

- Server, 1000 registered pawns, 900 of them AI: under 0.5 ms game thread per frame for registry, grid and push application, with the narrow phase spread across worker threads.
- Client: under 0.1 ms game thread per frame. A client scans only its own pawn.
- Bandwidth: about 12 bytes per push request from a client. Nothing extra per AI pawn except throttled cosmetic bump events.

## Core rules

1. **A pawn only ever pushes itself.** Each pawn moves itself away from what it overlaps; nothing moves another pawn. Every push is decided by the machine that already owns that pawn's movement, which is what makes prediction possible. Pushing someone out of the way comes from the strength model: when a runner hits an idle pawn, the idle pawn pushes itself away hard and the runner is barely deflected.
2. **A predicted push is a CharacterMovementComponent root motion source.** Root motion sources are the only way to add predicted movement to an unmodified CMC: the client stores the active sources in its saved moves and replays them after a correction.
   - Rejected: `AddForce`, `AddImpulse` and `LaunchCharacter` on a player-controlled pawn. They are not stored in saved moves, so client and server diverge. They are fine on pawns the server alone simulates (see drivers).
   - Rejected: `AddMovementInput`. It is predicted for free, but it changes acceleration, which turns characters that orient to movement and starts walk animations.
   - Rejected: a CMC subclass with custom move data. It would force every project onto our movement component and block modular use.
3. **One push per pawn at a time.** All neighbours are summed into a single vector and a new push replaces the active one. Never one source per neighbour.

## Who does what

| Machine and pawn | Scans | Applies | Network |
| --- | --- | --- | --- |
| Standalone, listen-server host's own pawn | Yes | Directly | Normal movement replication |
| Server, AI or other server-controlled pawn | Yes | Directly, through an authority driver | Normal movement replication |
| Owning client, its own player pawn | Yes | Immediately, predicted | Sends push requests to the server |
| Server, a remote player's pawn | No | Validates the request, applies the same push on the same move | Ack or reject |
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

- `Sequence` identifies a push for de-duplication, acks and rejects, like the interaction system's `ActivationId`.
- `ClientTimeStamp` is read from `GetPredictionData_Client_Character()->CurrentTimeStamp`. Client and server already share this clock for the pawn's moves, so the key names the move the push starts on. A GAS prediction key names an activation, not a move; this is where the system can be more precise than GAS.

### Flow

1. **Client.** Compute the push, quantize it, apply it locally with its start time set to `ClientTimeStamp`, and queue a request.
2. **Client, once per frame.** Send `ServerSoftPush(Batch)`. The RPC is unreliable and every batch repeats the requests not yet acknowledged from the last 100 to 150 ms, the same redundancy the CMC uses for its own moves. A reliable RPC that gets resent arrives too late to be useful.
3. **Server.** Drop duplicates by `Sequence`. A request normally arrives before or with the move it belongs to; the server then starts the push at the same timestamp and both machines simulate the same thing. A late request is applied with the time already missed trimmed off, and the safety net covers the remainder.
4. **Ack.** An owner-only, push-model replicated `LastAcceptedSequence`. It is delta replicated and costs almost nothing.
5. **Reject.** `ClientRejectSoftPush(Sequence)`. The client removes its push and the CMC's own correction fixes the position.
6. **Timestamp resets.** The CMC periodically resets client timestamps. When a reset is detected, the server applies pending requests immediately instead of scheduling them.

### Quantization is for correctness, not only bandwidth

The client applies the de-quantized values locally, so client and server build bit-identical sources. During a correction the CMC pairs server and client root motion sources by comparing them, so any difference would break the pairing.

| Field | Size |
| --- | --- |
| Sequence | 2 B |
| Client timestamp | 4 B |
| Direction (yaw) | 2 B |
| Strength | 2 B |
| Flags and main neighbour | 1 to 3 B |

Duration and falloff come from the profile and are never sent.

## Root motion source: `FEGRootMotionSource_SoftPush`

A `USTRUCT` deriving from `FRootMotionSource`.

- Falloff is an analytic formula (constant, linear, ease-out), so no curve asset reference has to replicate.
- Small custom `NetSerialize` of the quantized values.
- `Matches` compares `Sequence`, so server and client copies pair reliably during corrections.
- Additive, low priority, does not accumulate vertical velocity so gravity still works, and keeps or clamps its last velocity when it ends.
- `ACharacter` replicates active root motion sources to simulated proxies through `RepRootMotion`. Only player pawns use this source, which keeps that cost limited to players. Phase 0 measures it.
- Moves that carry an active root motion source combine less often, so a pushing player sends somewhat more movement RPCs. Short pushes and the reissue rule keep this down.

## Sync safety net

Set per profile.

- **Level 0, always on.** The move-anchored prediction keys above.
- **Level 1, on by default.** While a push is active, enable the CMC's `bServerAcceptClientAuthoritativePosition`. The engine documents it for short bursts of movement that are hard to sync. The server still checks its normal error tolerance; inside the tolerance it adopts the client's position, so tiny timing differences do not accumulate into corrections. It adds no way to cheat.
- **Level 2, opt-in, a reasonable choice for co-op.** Also enable `bIgnoreClientMovementErrorChecksAndCorrection` during the push window. The server checks each move: if the client moved further than max speed times delta time plus the push allowance, it closes the window and forces a correction.
- Both flags are shared CMC state. The component takes a counted lease and restores the previous values when the last push ends. A game that toggles the same flags itself must be told in the documentation.

## Scanning and performance

Built for a thousand pawns from the start.

- **Registry.** `UEGSoftCollisionSubsystem`, a world subsystem, keeps every registered pawn in flat arrays (structure of arrays): position, velocity, shape, cached profile values, flags and cell. Components never tick.
- **Spatial grid.** A uniform 2D hash grid with cells twice the largest push radius. Updated once per frame; an entry is re-binned only when its cell changes. A neighbour query reads the 3x3 cells around a pawn. No physics traces at all.
- **Narrow phase.** Plain math: vertical cylinder (capsules) and oriented box, plus a height-overlap test.
- **One tick.** A single tick function after physics, when positions are final for the frame. Results are applied on the next frame's move.
- **Parallel narrow phase.** With many scanners the neighbour tests and strength sums run in `ParallelFor` over read-only arrays, writing one result per scanner. Applying pushes stays on the game thread.
- **Sleeping.** A pawn that is not moving and has no neighbour in its 3x3 cells is skipped. The grid wakes it when a neighbour moves into range.
- **Scan rate by relevance.** On the server, AI far from every player scans less often or not at all. Distances and rates are in the profile.
- **Reissue rule.** A new push is sent only when the direction changes more than a set angle, the strength changes more than a set ratio, or the current push is about to end. A per-pawn token bucket also caps requests, which protects the server from a misbehaving client.
- **Client registry.** A client only needs entries near its own pawn; replication relevancy already limits what exists there, and entries outside the scan area are not re-binned.
- **Replication cost.** All replicated properties use the push model, so a thousand components cost nothing to compare while idle. Iris compatibility is checked in Phase 0.
- **Avoidance.** Profiles can scale pushes down for pawns using RVO or crowd avoidance, so the two systems do not fight.

The CMC itself is the main cost of a thousand moving Characters. Soft Collision must stay a small fraction of it, and the Phase 6 numbers are reported against that baseline.

## Strength model

- **Penetration** `p` = overlap depth divided by combined radius, 0 to 1.
- **Momentum share** `share_me = M_other / (M_me + M_other)`, where `M = Weight * max(speed toward the other pawn, IdleFloor)`. An idle pawn hit by a runner takes almost all of the push, the runner almost none, and two idle pawns split it evenly.
- **Strength** per neighbour = `MaxStrength * Falloff(p) * share * RelationScale`. Summed as vectors, then clamped.
- **Separation band.** Gentle and continuous while pawns overlap.
- **Bump band.** When `share * approach speed` crosses a threshold: stronger, and fires the bump event.
- **Deep-overlap guard.** Past a set penetration the strength ramps steeply so pawns never fully merge.
- **Optional blocking core.** A small inner collider that still blocks, off by default. It brings back desync, but only at deep overlap.

## Drivers

A driver decides how a push is applied. `UEGSoftCollisionDriver` is an instanced object on the profile; the component holds one predicted driver and one authority driver and uses whichever matches the pawn's current control.

| Driver | Pawns | Network |
| --- | --- | --- |
| `_CharacterRootMotion` | Player-controlled Characters | Predicted with the keys above |
| `_CharacterForce` | Server-controlled Characters (AI) | Authority only, through the CMC's force and impulse API; keeps root motion out of AI replication |
| `_PawnMovement` | Any other `UPawnMovementComponent`, such as floating pawn movement | Authority only, adds to velocity |
| `_PhysicsBody` | Pawns whose root primitive simulates physics, such as vehicles | Authority only, adds force to the body |
| Custom | Anything else, in C++ or Blueprint | Chosen by the driver |

Each driver declares where it runs: predicted, server authority, or owner authority. Owner authority covers pawns whose movement the owning client already controls.

A Mover driver goes in a separate optional module so the main plugin does not depend on the Mover plugin. Mover predicts its own layered moves, so that driver may not need our keys at all; this is investigated in Phase 5.

## Modularity and API

- **`UEGSoftCollisionComponent`.** Add it to any pawn. It sets the collision response to the Pawn channel to overlap on the chosen primitive, restores it at `EndPlay`, registers with the subsystem, and picks its drivers.
- **`UEGSoftCollisionProfile`**, a `UPrimaryDataAsset`: shape and radius scale, weight and push power, separation and bump bands, durations and falloff, scan rates and relevance distances, safety level, drivers.
- **`IEGSoftCollisionInterface`**, optional on the pawn: `CanPush` and `CanBePushedBy` (teammates, dead or mounted pawns), a profile override, and `ModifyPush` as a `BlueprintNativeEvent`.
- **Events.** `OnSoftPush` and `OnBump` fire at once on the owner's machine and authoritatively on the server.
- **Cosmetic bumps.** Other machines receive bumps through a throttled, push-model replicated struct with a counter, skipping the owner. This is the hook for shove montages, physical hit reactions and voice lines.

## Debugging

- Console variables: `eg.SoftCollision.Enable`, `eg.SoftCollision.Debug`, `eg.SoftCollision.ShowGrid`, `eg.SoftCollision.ForceServerOnly`.
- Visual Logger entries for scans, requests, accepts, rejects and late arrivals.
- A stat group: registered pawns, active scanners, sleeping pawns, neighbour tests, requests per second, rejects, late arrivals.
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
- Metrics: corrections per minute against the engine's default blocking collision, frame time, bandwidth per client.

## File layout

```
Source/UnrealExtendedGameplay/SoftCollision/
    EGSoftCollisionTypes.h                 shapes, keys, requests, events, enums
    EGSoftCollisionMath.h/.cpp             narrow phase, strength model, quantization (plain C++)
    EGSoftCollisionGrid.h/.cpp             spatial hash grid (plain C++)
    EGSoftCollisionProfile.h/.cpp
    EGSoftCollisionInterface.h
    EGSoftCollisionComponent.h/.cpp
    EGSoftCollisionSubsystem.h/.cpp        registry, tick function, parallel scan
    EGRootMotionSource_SoftPush.h/.cpp
    Prediction/EGSoftCollisionPrediction.h/.cpp   client queue, server scheduling, safety lease
    Drivers/EGSoftCollisionDriver.h/.cpp
    Drivers/EGSoftCollisionDriver_CharacterRootMotion.h/.cpp
    Drivers/EGSoftCollisionDriver_CharacterForce.h/.cpp
    Drivers/EGSoftCollisionDriver_PawnMovement.h/.cpp
    Drivers/EGSoftCollisionDriver_PhysicsBody.h/.cpp
    Tests/EGSoftCollisionTests.cpp
```

## Phases

- [ ] **0. Engine spike.** The riskiest assumptions, verified in the 5.6 source and in a test map:
  - Whether the server honours a client-provided start time exactly when it is in the past or the future.
  - Whether a custom root motion source pairs correctly during corrections.
  - The cost of `RepRootMotion` with the custom source.
  - How client timestamp resets behave.
  - Iris compatibility of the custom source and the component's replicated state.
  - The results decide the details of Level 0 and are written back into this document.
- [ ] **1. Data and math.** Types, profile, narrow phase, strength model, quantization, unit tests.
- [ ] **2. Registry and scanning.** Subsystem, grid, tick function, parallel narrow phase, sleeping, debug drawing. Authority pushes only (standalone and server AI) through `_CharacterForce`.
- [ ] **3. Predicted push.** The custom root motion source and `_CharacterRootMotion`, local authority first.
- [ ] **4. Prediction.** Keys, batched redundant requests, server scheduling, validation, ack and reject, safety leases.
- [ ] **5. Drivers and API.** `_PawnMovement`, `_PhysicsBody`, events, cosmetic bumps, interface, Blueprint API, Mover investigation.
- [ ] **6. Optimization.** Insights captures and bandwidth numbers at 100, 500 and 1000 pawns against the budgets above.
- [ ] **7. Verification and documentation.** The network test matrix, and header documentation in the plugin's style.

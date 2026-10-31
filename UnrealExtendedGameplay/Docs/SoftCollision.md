# Soft Collision

Soft pawn-vs-pawn collision for `UnrealExtendedGameplay`. Pawns stop blocking each other: they overlap slightly and push themselves apart, smoothly and without network corrections.

* There is no character or movement-component base class. A pawn needs one component.
* A player's own pushes are predicted on their client; the server applies the same push on the same move.
* Up to about a thousand pawns per world: a spatial grid, worker-thread scans, sleeping pawns, and no physics queries.

> Code: `Source/UnrealExtendedGameplay/SoftCollision`. Tests: `UnrealExtendedGameplay.SoftCollision.*`. Design, engine facts and verification status: [SoftCollisionPlan.md](SoftCollisionPlan.md). Engine checks and test matrix: [SoftCollisionTesting.md](SoftCollisionTesting.md).

---

## 1. Goals and non-goals

**Goals**

* **Drop-in.** Add `UEGSoftCollisionComponent` to any pawn; everything else has a default.
* **No corrections when pushed.** A pawn only ever pushes itself, so every push is decided by the machine that owns that pawn's movement and can be predicted there.
* **Feels physical.** A runner hitting an idle pawn shoves it aside and is barely slowed; two idle pawns split the separation evenly; a fast approach is a *bump* with its own event.
* **Every pawn type.** Characters (predicted), floating pawn movement, physics bodies, and custom drivers.
* **Plays well with the rest of the plugin:** turn in place, gameplay root motion, time dilation, movement modifiers and the anim instance.

**Non-goals**

* Pushing *other* pawns. A shove that moves someone else is a gameplay effect (a knockback), not soft collision.
* Mover. A Mover driver waits until Mover leaves Experimental.
* Pawns colliding with the world. Only pawn-vs-pawn blocking changes.

---

## 2. The model

```
Contact      overlap of the two push shapes (cylinder or rotated box), horizontally; heights must overlap
Penetration  overlap / both shapes' extent along the push direction: 0 touching, 1 centres meet
Share        M_other / (M_self + M_other), M = weight × max(speed towards the other pawn, idle floor)
Separation   SeparationStrength × Response(penetration) × 2 × share, summed over neighbours, capped
Bump         a fast approach with a large share adds BumpStrength × speed ratio × 2 × share
```

The sum is quantized (direction in 65536 steps, speed in 0.1 cm/s steps) and becomes one push for a short duration. A new push replaces the active one only when it says something different (the reissue rule): it turned, its strength changed, or the active push is about to end. An active bump plays out before separation takes over.

---

## 3. Pieces

| Piece | Kind |
|:--|:--|
| `UEGSoftCollisionComponent` | Actor component on the pawn. Control, results, requests, server application, events, modifiers. Also an `IEGAnimStateProvider` |
| `UEGSoftCollisionSubsystem` | World subsystem: registry, grid, frame pipeline, console variables, `Extended.Status` report |
| `UEGSoftCollisionProfile` | Data asset: body, strengths and bands, scan rates, network settings, drivers |
| `UEGSoftCollisionSettings` | Project Settings > Extended Framework > Extended Soft Collision |
| `IEGSoftCollisionInterface` | Optional on the pawn: filter, profile override, push veto or reshape |
| `UEGSoftCollisionDriver` and `_CharacterRootMotion`, `_CharacterForce`, `_PawnMovement`, `_PhysicsBody`, `_Blueprint` | How a push moves the pawn |
| `FEGRootMotionSource_SoftPush` | The predicted push, a root motion source |
| `UEGSoftCollisionModifierNotify` | Anim notify state: a modifier for the window (immovable by default) |
| `EGSoftCollisionMath`, `FEGSoftCollisionGrid` | Plain C++: contacts, strength model, quantization, grid. Unit tested |
| `Prediction/EGSoftCollisionPrediction.h` | Client request queue, server state, safety lease |
| `Prediction/EGSoftCollisionMoveData.h` | Opt-in move-data transport (§8.2) |

### 3.1 `UEGSoftCollisionComponent`

| Call | Purpose |
|:--|:--|
| `SetSoftCollisionEnabled(bool)` | Off: neither pushes nor is pushed (dead, ragdolled). Call on every machine |
| `PushModifier(Modifier)` → handle; `RemoveModifier(Handle)` | Weight and push-power scales, immovable, pushes nobody (§6) |
| `SetProfile(Profile)`, `RefreshFilter()` | Change the profile; re-read the interface's filter and override |
| `IsPushActive()`, `GetActivePushVelocity()` | The active push |
| `OnSoftPush`, `OnBump` | Events (§7) |
| `FindSoftCollisionComponent(Actor)` | Lookup |

| Setting | Default | Meaning |
|:--|:--|:--|
| `Profile` | none | The project default profile (settings), else the class defaults |
| `Filter` | layer 0, pushed by all | Layer, layers that may push this pawn, can-push, can-be-pushed |
| `bConfigureCollision` | true | Stop the push primitive from blocking `SoftChannel` at BeginPlay, restore at EndPlay |
| `SoftChannel` | Pawn | The object channel soft pawns stop blocking |
| `SoftResponse` | Ignore | Overlap makes the physics scene report every touching pair, which is expensive with hundreds of pawns |

### 3.2 `UEGSoftCollisionProfile`

| Group | Settings |
|:--|:--|
| Body | Shape (from the capsule, sphere or box by default), `RadiusScale` (1.1), `Weight`, `PushPower`, `bSuppressDuringRootMotion` (true), `bPushWhileFalling` (false), `AvoidanceScale` (0.5 for RVO) |
| Push | Response curve, `SeparationStrength` (220 cm/s), `SeparationDuration`, decay, idle floor, deep-overlap ramp, bump thresholds, `BumpStrength`, `BumpDuration`, `BumpCooldown`, `MaxTotalStrength` (650 cm/s), `MinPushStrength`, reissue angle, ratio and lead time |
| Scan | Player, near and far scan intervals, near and cull distances to the nearest player, sleep speed |
| Network | Safety level (§8.3), late policy, request rate and burst, redundancy window, resend interval, unanswered timeout, request age, validation slack, safety grace, Level 2 tolerance, cosmetic bump interval |
| Drivers | `PlayerDriver`, `AIDriver`; empty picks one from the pawn's movement (§5) |

Durations must be the same on every machine, so they come from the profile and are never sent.

---

## 4. Frame flow

```
End of frame N (TG_PostUpdateWork, after every pawn moved)
  └─ Snapshot: deferred registrations, each pawn into FEGSoftBody, grid re-bin, scan list
     (who decides the pawn, distance to players, sleep, scan interval)
  └─ Launch the scan: UE::Tasks + ParallelFor, or inline below the threshold

Network receive of frame N+1 (server): push requests from clients, applied in the RPC handler

Start of frame N+1 (FWorldDelegates::OnWorldPreActorTick, before any movement)
  └─ Wait for the scan (normally finished)
  └─ Each scanned pawn: reissue rule → interface → quantize → driver; predicted clients queue a request
  └─ Frame updates: force drivers top up velocity; clients send request batches, time out unanswered pushes

Movement ticks of frame N+1 simulate the new pushes.
```

Worker threads read only the snapshot and the grid and write one result slot each. Registrations and removals that happen between the snapshot and the apply are deferred.

---

## 5. Drivers

| Driver | Default for | Network |
|:--|:--|:--|
| `_CharacterRootMotion` | Player-controlled Characters | Predicted: a `FEGRootMotionSource_SoftPush` on the owning client, the same source on the server from the request |
| `_CharacterForce` | Server-controlled Characters (AI; unpossessed ones only while their movement runs: `bRunPhysicsWithNoController` or root motion) | Server only: `AddImpulse` with velocity change tops the speed along the push up to the push speed every frame |
| `_PawnMovement` | Any other pawn movement component | Server, or owner authority |
| `_PhysicsBody` | A simulating physics root | Server; physics replication carries it |
| `_Blueprint` | Never by default | Implementable events; slower, for a few special pawns |

A pawn switches between its player and AI driver when it is possessed (`ReceiveControllerChangedDelegate`, and checked every frame).

---

## 6. Gameplay integration

* **Modifiers.** `PushModifier` returns a handle, like the movement modifier component. Scales multiply, switches add up. Push them on every machine that simulates the pawn; montage notifies already run on the server and the owning client.
* **Soft Collision Modifier notify.** Put it over an attack's commit frames to make the attacker immovable, or set `WeightScale` for a heavy stance.
* **Root motion.** With `bSuppressDuringRootMotion` a Character moved by a networked root-motion montage or a gameplay root motion source (Root Motion Move, knockbacks) gets no new pushes, and an active push contributes nothing on those moves: the source checks inside its own simulation step, so the owning client and the server agree on every move. A crowd therefore cannot bend a lunge off its warp target. Root-motion locomotion (Root Motion From Everything) does not count, so such characters are still pushed (`EGSoftCollision::IsDrivenByGameplayRootMotion`). Soft pushes themselves never count as gameplay root motion: turn in place uses `EGSoftCollision::HasGameplayRootMotion` / `HasGameplayRootMotionSources`, which skip them, so being jostled never pauses turning.
* **Falling.** Not pushed unless `bPushWhileFalling`, so jumps and launches stay exact.
* **Time dilation.** Durations, decay and cooldowns run on the pawn's own clock, which follows `CustomTimeDilation` (`UEGTimeDilationSubsystem::PushActorTimeDilation`), the same time its movement simulates in.
* **Anim state tags.** While a push is active the component reports `ExtendedGameplay.Anim.State.SoftCollision.Pushed`, and `.Bumped` for bumps, to `UEGAnimInstance` (`HasStateTag`). Other clients see `.Bumped` for the bump's duration from the cosmetic replay. `UEGAnimInstance::bShouldMove` needs acceleration, so a push never starts locomotion on its own.
* **Filters.** Layers and masks on the component or from the interface: teammates that do not push each other, a mount the rider never pushes.

---

## 7. Events

| Event | Fires |
|:--|:--|
| `OnSoftPush` | Every push issued. Owning client at once (`bPredicted`), server when applied, standalone and AI directly |
| `OnBump` | Bumps only. Also on other clients, throttled, as a cosmetic replay (`bCosmetic`) |

---

## 8. Networking

### 8.1 Default: requests

1. The owning client applies the push at once with its start time set to the movement component's current client timestamp, and queues a request (`FEGSoftPushRequest`, about 11 bytes: sequence, timestamp, quantized push, band).
2. Once per frame it sends `ServerSoftPush` with new requests and repeats of unacknowledged ones from the redundancy window. Unreliable, before the move it belongs to.
3. The server validates (band, strength cap, rate, timestamp age and epoch, the bump cooldown on the client's move clock, a neighbour within reach that the filters let push it) and applies the same source with the client's start time, in the RPC handler, before the move. Validation ignores the server's view of falling, root motion and modifiers, which lags a predicting client by a frame or two. The push it replaces is removed after the server processes the client move ending where the new one starts.
4. Ack: an owner-only, push-model `LastAcceptedSequence`. Reject: `ClientRejectSoftPush`; the client removes the push. Neither within the round-trip time plus a margin: the client removes it.

### 8.2 Opt-in: move data

A project that already has its own movement component (turn in place's hooks, the recommended setup for multiplayer player characters, use one) can carry pushes inside the moves instead: `Prediction/EGSoftCollisionMoveData.h` has a saved-move template, prediction data, network move data and a container, plus `EGSoftCollision::HandleMoveAutonomous` for the server. The header shows the four lines of integration, stacked with turn in place's saved move.

The push then arrives with the move it starts on and the server applies it just before simulating that move, from that move's own start time, which also holds across a client timestamp reset. A move carrying a push is important, so the movement component keeps resending it until the server acknowledges it; the request also goes out once by RPC after a round trip unless the server answered it by then, and the server takes whichever copy comes first. That removes the timing risk of 8.1. Everything else (validation, rejects, the safety lease) is shared. The component switches by itself the first time the saved move reports in, as long as packed move RPCs are on (`p.NetUsePackedMovementRPCs`, the engine default).

### 8.3 Safety net

| Level | While a push runs on the server |
|:--|:--|
| Keys Only | Nothing beyond the start-time keys |
| Accept Client Position (default) | `bServerAcceptClientAuthoritativePosition`: within the normal error tolerance the server keeps the client's position |
| Client Authoritative Window | Also `bIgnoreClientMovementErrorChecksAndCorrection`, with a per-move check that closes the window on impossible displacement. Reasonable for co-op |

The flags are leased and restored. If the game had either flag on already, that is reported once.

---

## 9. Project integration

1. Add `UEGSoftCollisionComponent` to the pawn Blueprints that should push and be pushed.
2. Optional: profiles per pawn kind, and a project default under Project Settings > Extended Framework > Extended Soft Collision.
3. Optional: `IEGSoftCollisionInterface` on the pawn for filters and vetoes.
4. Optional: Soft Collision Modifier notifies in attack montages.
5. Optional: the move-data transport (§8.2) in the project's movement component.
6. Turn on push-model replication (`net.IsPushModelEnabled 1`).

---

## 10. Validation and debugging

* **Setup problems are logged once each, with the fix:** no primitive to push with, the component not replicating, push model off, another primitive on the pawn that still blocks the soft channel, the CMC network flags already owned by the game.
* **`Extended.Status`** (and `Extended.Status.Overlay 1`) shows one line per world: pawns, scanners, sleeping pawns, and Degraded when rejected, late or unanswered pushes pass a tenth of the accepted ones. Clients count the acknowledgements and rejects they receive; the server counts what it accepts, rejects and accepts late.
* `stat EGSoftCollision`, Insights scopes `EGSoftCollision_*`, Visual Logger entries.
* Console variables: `eg.SoftCollision.Enable`, `.Debug` (1 pushes, 2 shapes, 3 network log), `.ShowGrid`, `.ForceServerOnly`, `.Parallel`, `.ParallelThreshold`, `.SafetyLevelOverride`, `.Debug.RejectEvery`.
* Automation: `Contacts`, `Strength`, `Quantize`, `Grid`, `Prediction`, `Modifiers`, and `World`, which runs the component, subsystem and force driver together on spawned Characters.

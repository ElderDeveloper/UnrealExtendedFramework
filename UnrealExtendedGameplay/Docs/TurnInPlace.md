# Turn In Place — design

> **Status:** design, not implemented yet. This document is the specification the implementation follows.

A component-based turn-in-place system for `UnrealExtendedGameplay`.

* There is no character or movement-component base class to inherit from.
* A project adds one component and forwards a few engine calls from its own classes, using helpers that make those few lines hard to get wrong.
* Everything else (the math, networking, anim graph glue and safety checks) lives in the plugin.

---

## 1. Goals and non-goals

**Goals**

* Turn in place for the three common character setups:
  * **Strafe Direct:** `bUseControllerRotationYaw`, snaps to the camera.
  * **Strafe Desired:** `bUseControllerDesiredRotation`, interpolated.
  * **Orient To Movement:** `bOrientRotationToMovement`. After input is released, the character keeps turning towards the last input direction.
* **Actor-based.** The capsule really rotates and the mesh is never counter-rotated. Mesh smoothing on simulated proxies, sockets, aim offsets and every other system see one true rotation.
* **Multiplayer correct out of the box:** predicted on the owning client, cheap to replicate, and stable on simulated proxies and dedicated servers.
* **Modular:** one component, a data asset, and a small set of required hooks. Optional hooks add polish.
* **Per-stance animation sets** (standing, crouched, armed, and so on), selected by gameplay tag.
* **Pause and lock from anywhere:** gameplay code, montages, anim curves and anim event windows can all pause or lock turn in place, and the result is deterministic.
* **Self-checking.** A missing hook or a bad setup is reported once, with the fix, instead of failing silently.

**Non-goals**

* Mesh-based turn in place (Rotate Root Bone with a root yaw offset).
* Shipping animations or an anim blueprint. The plugin provides C++, a curve-generating animation modifier, and a step-by-step anim graph recipe (§9).
* Pawns without `UCharacterMovementComponent`. The math is reusable, but only the character path is integrated.

---

## 2. The model

```
DesiredYaw   where the character should face (control rotation, or last input direction)
ActorYaw     where the capsule actually faces
TurnOffset   DesiredYaw − ActorYaw, the yaw the character still has to turn
```

* **Standing still:**
  * the movement hook holds the actor back, so that `ActorYaw = DesiredYaw − TurnOffset`;
  * `TurnOffset` grows as the camera turns, clamped to `MaxTurnAngle`. Past that angle the actor is dragged along.
* **Starting a turn:** once `|TurnOffset| ≥ MinTurnAngle`, the anim graph plays a turn animation.
* **Each turn animation carries two curves:**
  * `RemainingTurnYaw`: the yaw the animation still has to turn. The frame-to-frame change in this value is subtracted from `TurnOffset`, so the actor rotates exactly as much as the animation does.
  * `TurnYawWeight`: 1 while the turn is happening, 0 during the settle (recovery) frames at the end.
* **Moving:** the offset is cleared and normal character rotation takes over. In Strafe Direct the change is eased out over `MovingInterpOutRate`, so the actor never snaps.

`TurnOffset` is the opposite sign of Lyra's `RootYawOffset`. It is never applied to the mesh, and aim offsets and locomotion should not use it, because the actor's rotation is already true.

---

## 3. Pieces

| Piece | Kind | Location |
|:--|:--|:--|
| `UEGTurnInPlaceComponent` | Replicated actor component, the runtime owner | `Animation/TurnInPlace` |
| `FEGTurnInPlaceSolver` | Plain C++ math, no world needed, unit tested | `Animation/TurnInPlace` |
| `UEGTurnInPlaceSet` | Data asset: per-stance angles, steps, play rates, left/right sequences | `Animation/TurnInPlace` |
| `EGTurnInPlace` hooks | Static helpers the project calls from `FaceRotation` / `PhysicsRotation` | `Animation/TurnInPlace` |
| `TEGTurnInPlaceSavedMove<TBase>` / `TEGTurnInPlacePredictionData<TMove>` | Templates added to the project's saved move | `Animation/TurnInPlace` |
| `IEGTurnInPlaceAnimSource` | Interface on the anim instance: worker-thread curve cache | `Animation/TurnInPlace` |
| `UEGTurnInPlaceAnimLibrary` | Thread-safe anim graph functions (setup/update per state) | `Animation/TurnInPlace` |
| `UEGAnimInstance` support | Built-in gather, worker update and interface implementation | `Animation/AnimInstance` |
| `UEGTurnInPlaceCurveModifier` | Animation modifier that writes the two curves from root yaw | **new** `UnrealExtendedGameplayEditor` module |
| Native tags | `ExtendedGameplay.TurnInPlace.*` | `Animation/EGAnimationTags` |

### 3.1 `UEGTurnInPlaceSet`

```
UEGTurnInPlaceSet : UPrimaryDataAsset
  Default            FEGTurnInPlaceEntry
  Stances            TMap<FGameplayTag, FEGTurnInPlaceEntry>   // matched against the owner's anim state tags
  Settings           curve names (RemainingTurnYaw, TurnYawWeight, PauseTurnInPlace, LockTurnInPlace)

FEGTurnInPlaceEntry
  State              Enabled | Locked | Paused                 // the entry's own default
  AnglesByMode       TMap<Tag, FEGTurnAngles>                  // ExtendedGameplay.TurnInPlace.Mode.Movement / .Strafe
  StepSizes          [60, 90, 180]                             // ascending; one left and one right sequence per step
  SelectMode         Greater | Nearest
  SelectOffset       degrees added before selecting a step
  LeftTurns / RightTurns   TArray<TObjectPtr<UAnimSequence>>   // index = step
  PlayRateAtMaxAngle, PlayRateOnDirectionChange, bKeepMaxAnglePlayRate
  MovingInterpOutRate
  Montages           FEGTurnInPlaceMontageRules                // overrides, ignored slots/montages, ignore additive

FEGTurnAngles
  MinTurnAngle       start a turn at this offset
  MaxTurnAngle       hard clamp; 0 disables
```

* **Stance resolution:**
  * Happens **once per frame on the game thread**, from the owner's `IEGAnimStateProvider` tags; the first matching key wins, otherwise `Default`.
  * Projects can override it with `GetStanceTag()` (a BlueprintNativeEvent).
  * The resolved entry is cached on the component, and the movement hook and the anim graph both read that same cache. They can never disagree about which set is active.
* **Validation:** `IsDataValid` checks that steps are ascending, that each step has a left and right sequence, that Min ≤ Max, and that the curves exist on each sequence.

### 3.2 `UEGTurnInPlaceComponent`

* **Replication:** it carries `SimulatedTurnOffset`, a `uint16` sent only to simulated proxies (`COND_SimulatedOnly`), push-model, and marked dirty only when the value changes beyond a tolerance.
* **Tick:** it ticks itself at `TG_PostPhysics`. There it runs simulated-proxy curve simulation (§7.3), debug drawing and the integration self-check (§11). The project never adds a Tick override.
* **Anim instance tracking:** it caches the mesh's anim instance through `OnAnimInitialized` and checks that it implements `IEGTurnInPlaceAnimSource`.
* **Public API (BlueprintCallable unless noted):**

| Call | Purpose |
|:--|:--|
| `GetTurnOffset()` | Current offset (degrees) |
| `IsTurning()`, `IsRecovering()` | From the worker-cached curves |
| `GetEnabledState()` | Resolved Enabled / Locked / Paused, with the reason (for debug) |
| `PushPause(Reason)`, `PushLock(Reason)` → handle; `Remove(Handle)` | Handle-based requests, the same pattern as movement modifiers |
| `SetRotationMode(Mode)` | Sets the character and movement-component flags for one of the three setups |
| `GetStanceTag()` / `ShouldIgnoreMontage(...)` / `CanAbortTurn()` | BlueprintNativeEvents for project rules |

### 3.3 The solver

`FEGTurnInPlaceSolver` holds all the decisions as pure functions over plain structs, so they can be tested without a world:

* `YawDeltaAroundUp(From, To, Up)`: heading difference about the gravity up axis. It uses plain yaw subtraction on flat ground and projected headings when roll or custom gravity is present.
* `UpdateOffset(Inputs, State&)`: one movement step. It resets the offset, measures desired vs actual, applies the curve delta (with the guards in §10), clamps to the maximum, and returns the yaw the actor should rotate by.
* `SelectStep(Entry, Offset, bOutRight)`
* `ComputePlayRate(Entry, Offset, bTurning, bTurningRight, bLatchedMax&)`
* `ResolveState(EntryState, Requests, Curves, Montage, Cvar)`: the enabled-state priority order (§6).
* `AdvancePseudoState(...)`: the dedicated-server state machine (§7.4).

---

## 4. Frame flow

```
Movement tick (game thread, inside CharacterMovementComponent)
  └─ PhysicsRotation / FaceRotation hook → component:
       resolve state → solver.UpdateOffset (reads curve cache from the last worker pass)
       → SetActorRotation (yaw only: no sweep) → mark replication dirty if changed

Anim update, game thread (UEGAnimInstance::GatherGameThreadData or library call)
  └─ copy: offset, resolved entry, enabled state, step, turn direction, mode angles

Anim update, worker thread (ComputeThreadSafeData or library call)
  └─ read curves NOW → cache in the anim instance (IEGTurnInPlaceAnimSource)
  └─ refresh "is turning" from the fresh curves, latch "turned this entry"
  └─ outputs: bWantsToTurn, bWantsRecovery, bAbortTurn, play rate, sequence, explicit time

State machine (worker): Idle ⇄ Turn → Recovery → Idle, Abort alias → Idle

Next movement tick reads the curve cache written by that worker pass.
```

The curve cache is written at exactly one point, after the pose that produced it. Gameplay never calls `GetCurveValue` on its own schedule.

---

## 5. Data that crosses threads

| Struct | Written by | Read by |
|:--|:--|:--|
| `FEGTurnInPlaceMoveState` (offset, last curve value, interp-out alpha, first-frame flag) | Movement hook | Movement hook, saved move snapshot |
| `FEGTurnInPlaceGraphInput` (entry, offset, state, step, direction, angles) | Game-thread anim gather | Worker |
| `FEGTurnInPlaceCurves` (remaining yaw, weight, pause, lock) | Worker | Movement hook (through the anim source interface), sim-proxy simulation |
| `FEGTurnInPlaceGraphOutput` (transitions, play rate, sequence, time) | Worker | Anim graph |
| `FEGTurnInPlaceNodeState` (step, time, right/left, recovery side, latches) | Anim graph node functions | Anim graph node functions |

Everything the worker touches lives on the anim instance. The worker never reads the component.

---

## 6. Enabled state

The resolved state is the first match in this list, top to bottom:

1. `eg.TurnInPlace.Override` console variable (non-shipping): 1 Enabled, 2 Locked, 3 Paused.
2. **Paused** if a pause request exists.
3. **Locked** if a lock request exists.
4. **Paused** if the `PauseTurnInPlace` curve is ≥ 0.95.
5. **Locked** if the `LockTurnInPlace` curve is ≥ 0.95.
6. **Paused** if an anim event window `ExtendedGameplay.TurnInPlace.Pause` is open (through `UEGAnimEventComponent` state tags).
7. **Locked** if an anim event window `ExtendedGameplay.TurnInPlace.Lock` is open.
8. **Paused** while a networked root-motion montage plays, unless the montage rules ignore it (ignored montage, ignored slot, or additive). A per-montage override can force Enabled, Locked or Paused.
9. **Paused** while the movement component has root motion sources (Root Motion Move, motion warping through root motion).
10. **Locked** while `UEGMovementModifierComponent`'s combined `RotationRateScale` is 0.
11. Otherwise, the entry's own `State`.

* **Locked:** no rotation at all; the body stays upright to gravity.
* **Paused:** no offset builds up, and the character rotates normally, as if turn in place did not exist.
* **Abort:** if the state leaves Enabled during a turn and `CanAbortTurn()` is true, the graph's Abort alias blends to Idle. The Abort alias is mandatory in the recipe.

---

## 7. Networking

### 7.1 Owning client (autonomous proxy)

* The offset is computed inside movement. That is predicted with the move, so the rotation is replayed correctly on correction.
* The project adds `TEGTurnInPlaceSavedMove` to its saved move. It saves `FEGTurnInPlaceMoveState` in `SetInitialPosition`, and **restores the older move's start state in `CombineWith`**.
* Why that matters: combining two moves rewinds the rotation to the older move's start and then calls `FaceRotation` once with the current control rotation. That call must re-simulate from the matching start state; otherwise the client turns about half as far as the server at high frame rates.

### 7.2 Server

* Runs the same hooks in `ServerMove`.
* After each hook it compresses the offset and marks `SimulatedTurnOffset` dirty, only when it has changed and only outside standalone.

### 7.3 Simulated proxies

* Rotation comes from movement replication; the offset comes from `SimulatedTurnOffset`.
* **Between updates**, the component's tick subtracts the local curve delta from the offset, so a proxy never sits mid-turn waiting for a slow server tick. This is on by default.
* **Never discard the replicated offset** because of local lock signals (a local curve, montage or tag). The server only resends when its value changes, so a discarded value would not come back while the player holds still.

### 7.4 Dedicated server animation

The server must produce the same curves the client sees. There are two modes, chosen on the component:

* **Animation** (default): the mesh must use `AlwaysTickPoseAndRefreshBones`. The component checks this at BeginPlay and warns once with the fix. `AlwaysTickPose` does not run the turn states.
* **Pseudo:** a small state machine (Idle / Turn / Recovery) runs on the game thread and evaluates curves directly from the selected sequences at the tracked time. Bones never need refreshing. It obeys the same transitions as the graph, including Abort.

---

## 8. Project integration

### Required

**1. Add the component** to the character and assign a `UEGTurnInPlaceSet`.

**2. Forward `FaceRotation`.** Needed for Strafe Direct. Add it anyway; it costs nothing otherwise.

```cpp
// MyCharacter.h
virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;

// MyCharacter.cpp
void AMyCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (!EGTurnInPlace::HandleFaceRotation(this, NewControlRotation, DeltaTime))
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
	}
}
```

**3. Use a movement component subclass** that forwards `PhysicsRotation` and allocates turn-aware prediction data.

```cpp
// MyMovement.h
virtual void PhysicsRotation(float DeltaTime) override;
virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

// MyMovement.cpp
void UMyMovement::PhysicsRotation(float DeltaTime)
{
	if (!EGTurnInPlace::HandlePhysicsRotation(this, DeltaTime))
	{
		Super::PhysicsRotation(DeltaTime);
	}
}

FNetworkPredictionData_Client* UMyMovement::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		// No saved move of your own: use the ready-made one.
		const_cast<UMyMovement*>(this)->ClientPredictionData = new FEGTurnInPlacePredictionData(*this);
	}
	return ClientPredictionData;
}
```

If the project already has its own saved move, it inherits through the mixin:

```cpp
class FMySavedMove : public TEGTurnInPlaceSavedMove<FSavedMove_Character>
{
	// existing fields; call Super:: in Clear / SetInitialPosition / CombineWith as usual
};
using FMyPredictionData = TEGTurnInPlacePredictionData<FMySavedMove>;
```

**4. Anim instance.** Either derive the AnimBP's parent from `UEGAnimInstance`, which has it built in, or, in a custom anim instance:

```cpp
// game thread
void UMyAnimInstance::NativeUpdateAnimation(float Dt)
{
	Super::NativeUpdateAnimation(Dt);
	UEGTurnInPlaceAnimLibrary::GatherTurnInPlace(TurnInPlaceComponent, TurnInPlace);
}

// worker thread
void UMyAnimInstance::NativeThreadSafeUpdateAnimation(float Dt)
{
	Super::NativeThreadSafeUpdateAnimation(Dt);
	UEGTurnInPlaceAnimLibrary::ThreadSafeUpdateTurnInPlace(this, TurnInPlace, Dt);
}

// IEGTurnInPlaceAnimSource
FEGTurnInPlaceCurves UMyAnimInstance::GetTurnInPlaceCurves_Implementation() const
{
	return TurnInPlace.Curves;
}
```

**5. Anim graph:** follow the recipe in §9.

**6. Turn animations:** run `UEGTurnInPlaceCurveModifier` on every turn sequence. Sequences must have root motion; root lock is ignored while the curves are extracted.

### Optional

* **Rotate to the last input direction (orient to movement)** while stopping as well as when standing still: forward `ComputeOrientToMovementRotation` to `EGTurnInPlace::ComputeOrientToMovementRotation`.
* **Separate idle rotation rate:** set `IdleRotationRate` on the component. The hook uses it while stationary, so no `GetDeltaRotation` override is needed.
* **Project rules:** override `GetStanceTag`, `ShouldIgnoreMontage` or `CanAbortTurn` in a Blueprint or C++ subclass of the component.

The last input direction is tracked inside `HandlePhysicsRotation`, which runs on every movement tick including root motion, so projects do not override `CalcVelocity` or `ApplyRootMotionToVelocity`.

---

## 9. Anim graph recipe

A `TurnInPlace` state machine, used as the idle pose (or placed inside the locomotion Idle state):

| State | Node | Functions (from `UEGTurnInPlaceAnimLibrary`) |
|:--|:--|:--|
| **Idle** | idle pose | Become relevant: `SetupIdle` |
| **Turn** | Sequence Evaluator | State: `SetupTurnPose`. Node become relevant: `SetupTurnAnim` → Set Sequence, Explicit Time 0. Node update: `UpdateTurnAnim` → Set Sequence With Inertial Blending (0.2), **Set Explicit Time to the returned time**. |
| **Recovery** | Sequence Player | State: `SetupRecoveryPose`. Node update: `UpdateRecoveryAnim` → Set Sequence With Inertial Blending (0.2) |
| **Abort** (state alias over Turn and Recovery) | | Transition to Idle |

| Transition | Rule | Blend |
|:--|:--|:--|
| Idle → Turn | `bWantsToTurn` | Inertialization 0.2 |
| Turn → Turn (re-entry) | `bWantsToTurn` and the step or side changed | Inertialization 0.2 |
| Turn → Recovery | `bWantsRecovery` | Inertialization 0.2 |
| Recovery → Turn | `bWantsToTurn` | Inertialization 0.2 |
| Recovery → Idle | automatic rule on remaining time | Inertialization 0.2 |
| Abort → Idle | `bAbortTurn`, priority −1 | Inertialization 0.15 |

Locomotion helpers in the output:

* `bStopToIdleForTurn`: leave Stop early to start a turn.
* `bStartToCycleFromTurn`: strafing only; skip Start when the offset already exceeds the minimum angle.

**Curve filtering:** add `RemainingTurnYaw` and `TurnYawWeight` to every Inertialization node's *Filtered Curves*. Otherwise blends invent curve values and the offset drifts.

---

## 10. Pitfalls and how the design prevents them

| Pitfall | What goes wrong | Prevention |
|:--|:--|:--|
| Combined moves | The client turns ~half as far as the server at high frame rates | Saved-move mixin restores the older move's start state in `CombineWith` (§7.1) |
| Stale curves on entry | Turn → Recovery fires on the first frame and oscillates | Curves are cached on the worker before the state machine; "is turning" is refreshed from them; recovery needs a "turned this entry" latch (§4) |
| Turn never observed | The weight curve never registers, so the turn sits at its last frame forever | Recovery also fires once the evaluator reaches the end of the sequence; that cannot be true on the entry frame, so it is oscillation-safe |
| First relevant frame | Delta from a zero or stale previous value jumps the offset | The first frame with weight > 0 only records the curve; deltas apply from the second frame |
| Direction change | Curve sign flips mid-turn and the actor snaps | A delta is skipped when the sign flips (0 counts as same direction) |
| Over 180° | The offset wraps and the actor snaps the other way | A delta that would push \|offset\| past 180 is held until the animation removes it |
| Curve overshoot | Inertialization pushes pause/lock curves past 1 and the state flickers | One-sided threshold ≥ 0.95, plus curve filtering in the recipe |
| Rotating platforms | The engine calls `FaceRotation` with DeltaTime 0 for based movement; treating it as input fights the base and loops the camera | DeltaTime 0 passes straight through to the engine |
| Pitch / roll / custom gravity | Euler yaw subtraction picks up cross-talk and starts spurious turns | Yaw is measured as a heading about the gravity up axis; flat ground keeps the cheap path |
| Proxy loses its offset | A local lock zeroes a replicated value the server will not resend | Simulated proxies never reset the offset on local state (§7.3) |
| Slow server updates | Proxies stay mid-turn until the next update | Local curve simulation between updates (§7.3) |
| Server without bones | `AlwaysTickPose` never runs the turn states, so server and client diverge | BeginPlay check with a one-time warning, or Pseudo mode (§7.4) |
| Pseudo vs graph mismatch | Server state machine ends turns differently from clients | Pseudo mode implements the same transitions, including Abort, and is tested against the same tables |
| Jumping | Air velocity overwrites the last input direction and the character turns on landing | Only ground velocity (Z removed) and real input acceleration update it |
| Root motion | Movement does not see input during root motion, so the last direction goes stale | During root motion it is the capsule's forward, with a 0.25 s grace period after root motion ends before velocity counts again |
| Stick bounce | Releasing a gamepad stick bounces it past centre and flips the character | Acceleration only counts above an analog threshold (0.5) |
| Mouse at max angle | The offset keeps re-entering the max angle and the play rate jitters | The max-angle play rate is latched for the rest of the current turn |
| Inverted montage rule | A root-motion montage check gets the condition backwards and pauses when it should not | One named, tested function decides montage handling, with per-montage overrides |
| Upper-body montages | Additive or upper-body montages pause the system | Ignored slots, ignored montages and "ignore additive" in the montage rules |
| Start moving | Strafe Direct snaps to the camera on the first step | Interp-out alpha eases the offset away over `MovingInterpOutRate` |
| Moving with stale state | A stop after a run resumes an old half-turn | Offset, curve value and the first-frame flag reset while moving |
| Yaw-only sweeps | `SetActorRotation` sweeps every frame for a rotation that cannot collide | Sweep only when pitch or roll changes |
| Missing setup | No interface or no hook, and nothing happens with no error | Integration self-check (§11) |
| Replication cost | A float offset sent to everyone every frame | `uint16`, simulated proxies only, push model, only on change |
| Blueprint events in PostLoad | Calling a BlueprintNativeEvent while loading asserts | Owner and anim instance are cached in OnRegister / BeginPlay / OnAnimInitialized only |

---

## 11. Validation and debugging

**Integration self-check** (non-shipping builds). Each problem is warned about once per owner class, with the exact fix:

* The rotation mode needs `FaceRotation`, but `HandleFaceRotation` was never called while stationary with an offset.
* The mode needs `PhysicsRotation`, but `HandlePhysicsRotation` was never called.
* An autonomous proxy saved moves without `TEGTurnInPlaceSavedMove`: the snapshot hook never ran.
* The anim instance does not implement `IEGTurnInPlaceAnimSource`.
* A dedicated server is in Animation mode without `AlwaysTickPoseAndRefreshBones`.
* The resolved entry is missing a sequence for the selected step or side, or a sequence lacks the curves.
* No angles exist for the current mode tag.

**Console variables:**

* `eg.TurnInPlace.Debug 1`: on-screen offset per role, plus arrows from the feet (actor forward, control yaw, offset target).
* `eg.TurnInPlace.Debug.State 1`: resolved enabled state and its reason (request, curve, window, montage, root motion, modifier, cvar).
* `eg.TurnInPlace.Override 0..3`: force the enabled state (cheat).

**Profiling:** `TRACE_CPUPROFILER_EVENT_SCOPE` on the hooks, the gather and the worker update.

**Anim state tags** (through `IEGAnimStateProvider`): `ExtendedGameplay.TurnInPlace.State.Turning`, `.Recovering`, `.Locked`, `.Paused`, `.Left`, `.Right`.

---

## 12. Integration with the rest of the plugin

| System | Interaction |
|:--|:--|
| `UEGAnimInstance` | Built-in gather and worker update; exposes `TurnInPlace` (output, node state, curves) to the AnimBP; implements `IEGTurnInPlaceAnimSource` |
| Anim state tags | Stance selection reads them; turn in place publishes its own (§11) |
| Anim events | `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` windows pause or lock for their length, from any animation |
| `UEGMovementModifierComponent` | A combined rotation scale of 0 locks turn in place |
| Root Motion Move / Motion Warping | Root motion sources and root-motion montages pause it |
| Cover | Pushes a lock while in cover (cover owns rotation there) |
| Chess seat | Pushes a pause while seated (scripted transform) |
| Soft Lock / Set Warp Target | Push a pause for their window, so the two rotation drivers do not fight |
| Montage library | Montage rules apply to montages played through `UEGMontageLibrary` and directly |
| `EGAnimDebug` | Shared once-per-source warnings |

These integrations only use the request API and tags, so turn in place stays optional for every other system.

---

## 13. Tests

Automation tests under `UnrealExtendedGameplay.Animation.TurnInPlace.*`, all on the solver (no world):

* Yaw delta: flat ground, wrap at ±180, pitch only, roll present, custom gravity up, near-vertical look.
* Offset update: clamp to max; first-frame capture; delta application; direction-flip skip; > 180 hold; reset while moving; Paused accumulates nothing; Locked rotates nothing.
* Step selection: Greater and Nearest at and between steps, with select offset, and an empty step list.
* Play rate: max-angle latch, direction change, both together.
* Enabled state: every row of the priority list in §6, and per-montage overrides.
* Graph outputs: the recovery latch blocks the entry frame; the reached-end escape fires; abort beats recovery.
* Pseudo state machine: Idle → Turn → Recovery → Idle, abort mid-turn, and a missing sequence.
* Saved move: combine restores the older start state (solver-level re-simulation of two combined steps equals two separate steps).

---

## 14. Implementation order

1. **Solver and data.** Types, `UEGTurnInPlaceSet` with validation, `FEGTurnInPlaceSolver`, and its tests.
2. **Component and hooks.** `HandleFaceRotation` / `HandlePhysicsRotation`, last input tracking, idle rotation rate, the saved-move mixin and prediction data, the request API, the enabled-state resolution, and the self-check.
3. **Animation.** `IEGTurnInPlaceAnimSource`, `UEGTurnInPlaceAnimLibrary`, `UEGAnimInstance` support, the new `UnrealExtendedGameplayEditor` module with `UEGTurnInPlaceCurveModifier`, and the recipe verified against the outputs.
4. **Networking.** Simulated-only compressed replication, proxy curve simulation, and dedicated server Pseudo mode.
5. **Plugin integration and tooling.** Cover, chess, Soft Lock and Warp Target requests; modifier and root-motion rules; state tags; console variables and debug drawing; final docs.

Each step lands on `merge` as its own commit.

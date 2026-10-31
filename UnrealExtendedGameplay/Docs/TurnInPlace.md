# Turn In Place

A component-based turn-in-place system for `UnrealExtendedGameplay`.

* There is no character or movement-component base class.
* A project adds one component and forwards a few engine calls from its own classes; helpers make those lines hard to get wrong.
* The math, networking, anim graph glue and setup checks live in the plugin.

> Code: `Source/UnrealExtendedGameplay/Animation/TurnInPlace`. Curve modifier: `Source/UnrealExtendedGameplayEditor/TurnInPlace`. Tests: `UnrealExtendedGameplay.Animation.TurnInPlace.*`.

---

## 1. Goals and non-goals

**Goals**

* Turn in place for the three common character setups (`UEGTurnInPlaceComponent::SetRotationMode` sets the flags):
  * **Strafe Direct:** `bUseControllerRotationYaw`, snaps to the camera.
  * **Strafe Desired:** `bUseControllerDesiredRotation`, interpolated.
  * **Orient To Movement:** `bOrientRotationToMovement`. After input is released, the character keeps turning towards the last input direction.
* **Actor-based.** The capsule really rotates and the mesh is never counter-rotated. Mesh smoothing on simulated proxies, sockets, aim offsets and every other system see one true rotation.
* **Multiplayer correct out of the box:** predicted on the owning client, cheap to replicate, and stable on simulated proxies and dedicated servers.
* **Per-stance animation sets,** selected by gameplay tag.
* **Pause and lock from anywhere:** gameplay code, montages, anim curves and anim event windows can all pause or lock it, and the result is deterministic.
* **Self-checking.** A missing hook or a bad setup is reported once, with the fix.

**Non-goals**

* Mesh-based turn in place (Rotate Root Bone with a root yaw offset).
* Shipping animations or an anim blueprint. The plugin provides C++, the curve modifier, and the anim graph recipe (§9).
* Pawns without `UCharacterMovementComponent`.

---

## 2. The model

```
DesiredYaw   where the character should face (control rotation, or last input direction)
ActorYaw     where the capsule actually faces
TurnOffset   DesiredYaw − ActorYaw, the yaw the character still has to turn
```

* **Standing still:**
  * the movement hook holds the actor back, so that `ActorYaw = DesiredYaw − TurnOffset`;
  * the offset grows as the camera turns, clamped to `MaxTurnAngle`. Past that angle the actor is dragged along.
* **Starting a turn:** once `|TurnOffset| ≥ MinTurnAngle`, the anim graph plays a turn animation.
* **Each turn animation carries two curves:**
  * `RemainingTurnYaw`: the yaw the animation still has to turn. Its frame-to-frame change is subtracted from the offset, so the actor rotates exactly as much as the animation does.
  * `TurnYawWeight`: 1 while turning, 0 during the recovery frames.
* **Moving:** the offset is cleared and normal character rotation takes over. In Strafe Direct the change is eased out over `MovingInterpOutRate`.

`TurnOffset` is the opposite sign of Lyra's `RootYawOffset`. Do not use it for aim offsets or locomotion: the actor's rotation is already true.

---

## 3. Pieces

| Piece | Kind |
|:--|:--|
| `UEGTurnInPlaceComponent` | Replicated actor component, the runtime owner. Also an `IEGAnimStateProvider` |
| `FEGTurnInPlaceSolver` | Every decision as plain functions (no world), unit tested |
| `UEGTurnInPlaceSet` | Data asset: per-stance entries and curve names |
| `EGTurnInPlace::HandleFaceRotation` / `HandlePhysicsRotation` / `ComputeOrientToMovementRotation` | What the project calls from its own classes (`EGTurnInPlaceHooks.h`) |
| `TEGTurnInPlaceSavedMove<TBase>`, `TEGTurnInPlacePredictionData<TMove, TBaseData>`, `FEGTurnInPlaceSavedMove`, `FEGTurnInPlacePredictionData` | Saved-move support (`EGTurnInPlaceHooks.h`) |
| `IEGTurnInPlaceAnimSource` | On the anim instance: returns the worker-cached curves |
| `UEGTurnInPlaceAnimLibrary` | Game-thread gather, worker-thread update, anim graph node functions |
| `UEGAnimInstance` | Runs both halves itself and implements `IEGTurnInPlaceAnimSource` |
| `UEGTurnInPlaceCurveModifier` | Animation modifier "Turn In Place Curves" (`UnrealExtendedGameplayEditor`, an Editor module) |

### 3.1 `UEGTurnInPlaceSet`

```
UEGTurnInPlaceSet : UPrimaryDataAsset
  Default            FEGTurnInPlaceEntry
  Stances            TMap<FGameplayTag, FEGTurnInPlaceEntry>   // the first key found in the owner's anim state tags wins
  Curves             FEGTurnInPlaceCurveNames (RemainingTurnYaw, TurnYawWeight, PauseTurnInPlace, LockTurnInPlace)

FEGTurnInPlaceEntry
  State              Enabled | Locked | Paused
  MovementAngles     FEGTurnAngles (60, 0)       // ExtendedGameplay.TurnInPlace.Mode.Movement
  StrafeAngles       FEGTurnAngles (60, 135)     // ExtendedGameplay.TurnInPlace.Mode.Strafe
  CustomModeAngles   TMap<Tag, FEGTurnAngles>    // modes returned by an overridden GetTurnModeTag
  StepSizes          [60, 90, 180]               // ascending
  SelectMode         Greater | Nearest
  SelectOffset
  LeftTurns / RightTurns   one UAnimSequence per step
  PlayRateAtMaxAngle (1.3), PlayRateOnDirectionChange (1.7), bKeepMaxAnglePlayRate
  MovingInterpOutRate
  Montages           FEGTurnInPlaceMontageRules (Overrides, bIgnoreAdditive, IgnoredSlots, IgnoredMontages)
```

* **Stance resolution:**
  * Happens **once per frame on the game thread**, from the tags of every other `IEGAnimStateProvider` on the owner.
  * `GetStanceTag()` overrides it.
  * The movement hook and the anim graph read the same result. The worker thread reads entries straight from the asset, which is read-only at runtime.
* **Validation:** `IsDataValid` checks:
  * steps are positive and ascending;
  * there is one left and one right sequence per step;
  * every sequence has both curves;
  * Min ≤ Max.

### 3.2 `UEGTurnInPlaceComponent`

| Call | Purpose |
|:--|:--|
| `GetTurnOffset()` | Current offset (degrees) |
| `IsTurningInPlace()` | Weight curve above 0 |
| `GetEnabledState(OutReason)` | Resolved state and why (§6) |
| `PushPause(Reason)`, `PushLock(Reason)` → handle; `RemoveRequest(Handle)` | Handle-based requests |
| `SetRotationMode(Character, Mode)` | Sets the character and movement flags for one of the three setups |
| `GetLastInputDirection()` | What Orient To Movement turns towards when input stops |
| `GetStanceTag`, `GetTurnModeTag`, `ShouldIgnoreMontage`, `CanAbortTurn` | BlueprintNativeEvents for project rules |

| Setting | Default | Meaning |
|:--|:--|:--|
| `TurnSet` | — | Required |
| `bRotateToLastInputDirection` | true | Orient To Movement keeps turning towards the last input |
| `DedicatedServerAnimMode` | Animation | `Animation` or `Pseudo` (§7.4) |
| `bSimulateCurvesOnProxies` | true | §7.3 |
| `bReportSetupProblems` | true | §11 |

The component ticks itself (`TG_PostPhysics`) for proxy simulation, pseudo states, self-checks and debug drawing. The project never adds a Tick override.

### 3.3 `FEGTurnInPlaceSolver`

| Function | Does |
|:--|:--|
| `YawDeltaAroundUp` | Heading difference about the gravity up axis. Plain yaw subtraction on flat ground, projected headings with roll or custom gravity |
| `UpdateOffset` | One standing step: measure, apply the curve delta with its guards (§10), clamp. Returns the yaw the actor turns by. Locked returns 0 and clears the state; Paused turns all the way with no offset; simulated proxies keep and advance the replicated offset and never rotate |
| `SelectStep`, `ComputePlayRate` | Which step plays; max-angle rate (latched) and direction-change rate. The direction change compares the offset with the *playing* turn's side |
| `ResolveState`, `EvaluateRootMotionMontage` | §6 |
| `ComputeGraphOutput`, `AdvanceAnimTime`, `UpdateNodePlayRate`, `GetTurnAnimation` | Anim graph logic |
| `AdvancePseudoState` | §7.4 |
| `UpdateLastInputDirection` | §10 (jumping, root motion, stick bounce) |

---

## 4. Frame flow

```
Movement tick (game thread, inside CharacterMovementComponent)
  └─ HandlePhysicsRotation / HandleFaceRotation → component:
       state (§6) → solver.UpdateOffset (curves from the last worker pass) → rotate (yaw only: no sweep)
       → replicate the offset if it changed

Anim update, game thread   UEGTurnInPlaceAnimLibrary::GatherTurnInPlace     (component.MakeGraphInput)
Anim update, worker        UEGTurnInPlaceAnimLibrary::ThreadSafeUpdateTurnInPlace
  └─ read curves NOW → refresh "is turning" → latch "turned this entry" → outputs
State machine (worker)     Idle ⇄ Turn → Recovery → Idle, Abort → Idle

Next movement tick reads that worker pass's curves through IEGTurnInPlaceAnimSource.
```

| Struct | Written by | Read by |
|:--|:--|:--|
| `FEGTurnInPlaceMoveState` | Movement hook | Movement hook, saved-move snapshot |
| `FEGTurnInPlaceGraphInput` | Game-thread gather | Worker |
| `FEGTurnInPlaceCurves` | Worker | Movement hook, proxy simulation |
| `FEGTurnInPlaceGraphOutput` | Worker | Anim graph |
| `FEGTurnInPlaceNodeState` | Node functions | Node functions |
| `FEGTurnInPlaceAnimData` | All of the above, kept on the anim instance | |

---

## 5. Gameplay tags

| Tag | Use |
|:--|:--|
| `ExtendedGameplay.TurnInPlace.Mode.Movement` / `.Strafe` | Turn mode (which angles) |
| `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` | Anim event windows (Anim Event Window notify, read through `UEGAnimEventComponent`) |
| `ExtendedGameplay.TurnInPlace.State.Turning`, `.Locked`, `.Paused`, `.Left`, `.Right` | Published as anim state tags (`UEGAnimInstance::HasStateTag`) |

---

## 6. Enabled state

The resolved state is the first match in this list, top to bottom (`FEGTurnInPlaceSolver::ResolveState`):

1. `eg.TurnInPlace.Override` (non-shipping): 1 Enabled, 2 Locked, 3 Paused.
2. **Paused**: a pause request.
3. **Locked**: a lock request.
4. **Paused**: `PauseTurnInPlace` curve ≥ 0.95.
5. **Locked**: `LockTurnInPlace` curve ≥ 0.95.
6. **Paused**: an `ExtendedGameplay.TurnInPlace.Pause` window is open.
7. **Locked**: an `ExtendedGameplay.TurnInPlace.Lock` window is open.
8. A networked root-motion montage, checked in this order:
   * nothing, if `ShouldIgnoreMontage` returns true (the project's own rule wins);
   * the montage's entry in `Montages.Overrides`, if it has one;
   * nothing, if it is in `IgnoredMontages`, uses an `IgnoredSlots` slot, or is additive;
   * otherwise **Paused**.
9. **Paused**: the movement component has root motion sources (Root Motion Move, root-motion based abilities).
10. **Locked**: `UEGMovementModifierComponent`'s combined `RotationRateScale` is 0.
11. Otherwise, the stance entry's `State`.

What each state means:

* **Locked:** no rotation at all, standing or moving; the body stays upright to gravity.
* **Paused:** no offset builds up, and the character rotates normally, as if turn in place did not exist.
* **Abort:** if the state leaves Enabled during a turn and `CanAbortTurn()` is true, `Output.bAbortTurn` blends the graph to Idle. The Abort alias is mandatory in the recipe.

---

## 7. Networking

### 7.1 Owning client

* The offset is computed inside movement, so it is predicted with the move.
* `TEGTurnInPlaceSavedMove` saves the move state in `SetInitialPosition` and **restores the older move's start state in `CombineWith`**.
* Why that matters: combining two moves rewinds the rotation to the older move's start and re-runs the rotation once. Without the restore the client turns only part of the way at high frame rates. The `CombinedMoves` test reproduces this and checks the fix.

### 7.2 Server

* Runs the same hooks inside `ServerMove`.
* `SimulatedOffset` (16 bits) is marked dirty only when the offset changes. It is push-model, `COND_SimulatedOnly`, and not used in standalone.

### 7.3 Simulated proxies

* Rotation comes from movement replication; the offset comes from `SimulatedOffset`.
* Between updates the component's tick consumes the offset with the local curves.
* A local lock never discards the replicated value. The server only resends on change, so a discarded value would not come back.

### 7.4 Dedicated server

* **Animation** (default): the mesh needs `AlwaysTickPoseAndRefreshBones`. Otherwise the component warns once at BeginPlay with the fix.
* **Pseudo:** the component runs Idle / Turn / Recovery on the game thread (`AdvancePseudoState`, the same transitions as the graph, Abort included, with Recovery continuing the turn clip as in the graph) and evaluates the curves from the selected sequence at the tracked time. The anim graph stays idle on that server.

  Limitation: in Pseudo mode the server only sees `PauseTurnInPlace` / `LockTurnInPlace` curves on the turn sequences themselves, not on montages. To pause or lock from a montage that must also hold on a Pseudo server, use an `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` anim event window, or a request, instead of a curve.

---

## 8. Project integration

The project's module needs `UnrealExtendedGameplay` in its Build.cs dependencies. Then:

```cpp
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceHooks.h"
```

### Required

**1. Add the component** to the character and assign a `UEGTurnInPlaceSet`. Set the rotation setup with `UEGTurnInPlaceComponent::SetRotationMode`, or directly.

**2. Forward `FaceRotation`.** Needed for Strafe Direct; add it anyway.

```cpp
void AMyCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (!EGTurnInPlace::HandleFaceRotation(this, NewControlRotation, DeltaTime))
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
	}
}
```

**3. Use a movement component subclass** that forwards `PhysicsRotation` and allocates turn-aware prediction data. Set it on the character with `ObjectInitializer.SetDefaultSubobjectClass<UMyMovement>(ACharacter::CharacterMovementComponentName)`.

```cpp
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
		const_cast<UMyMovement*>(this)->ClientPredictionData = new FEGTurnInPlacePredictionData(*this);
	}
	return ClientPredictionData;
}
```

With a saved move of its own, the project inherits through the mixin. With its own prediction data base, it passes that as the second argument:

```cpp
class FMySavedMove : public TEGTurnInPlaceSavedMove<FSavedMove_Character>
{
	// Super must be the mixin, not FSavedMove_Character: skipping it in CombineWith silently drops the restore.
	using Super = TEGTurnInPlaceSavedMove<FSavedMove_Character>;
	// ... call Super::Clear / SetInitialPosition / CombineWith in your overrides
};
using FMyPredictionData = TEGTurnInPlacePredictionData<FMySavedMove /*, FMyPredictionDataBase */>;
```

**4. Anim instance.** Either parent the AnimBP to `UEGAnimInstance`, which needs nothing more, or, in a custom anim instance that implements `IEGTurnInPlaceAnimSource`:

```cpp
UPROPERTY(Transient, BlueprintReadWrite, Category = "Turn In Place")
FEGTurnInPlaceAnimData TurnInPlace;

void UMyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UEGTurnInPlaceAnimLibrary::GatherTurnInPlace(UEGTurnInPlaceComponent::FindFor(GetOwningActor()), TurnInPlace);
}

void UMyAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	UEGTurnInPlaceAnimLibrary::ThreadSafeUpdateTurnInPlace(this, TurnInPlace);
}

FEGTurnInPlaceCurves UMyAnimInstance::GetTurnInPlaceCurves_Implementation() const
{
	return TurnInPlace.Curves;
}
```

**5. Anim graph:** follow §9.

**6. Turn animations:** apply the **Turn In Place Curves** animation modifier to every turn sequence. They need root motion; root lock is ignored while sampling. The curve names must match the set's `Curves`.

### Optional

* **Keep turning towards the last input while sliding to a stop** (Orient To Movement):

  ```cpp
  FRotator UMyMovement::ComputeOrientToMovementRotation(const FRotator& Current, float DeltaTime, FRotator& DeltaRotation) const
  {
  	FRotator Result;
  	if (EGTurnInPlace::ComputeOrientToMovementRotation(this, Current, Result))
  	{
  		return Result;
  	}
  	return Super::ComputeOrientToMovementRotation(Current, DeltaTime, DeltaRotation);
  }
  ```

* **Project rules:** override `GetStanceTag`, `GetTurnModeTag`, `ShouldIgnoreMontage` or `CanAbortTurn` in a subclass of the component (Blueprint or C++).

The last input direction is tracked inside `HandlePhysicsRotation` and the component's tick, so projects do not override `CalcVelocity` or `ApplyRootMotionToVelocity`.

---

## 9. Anim graph recipe

A `TurnInPlace` state machine, used as the idle pose. `TurnInPlace` below is the anim instance's `FEGTurnInPlaceAnimData`; the functions are in `UEGTurnInPlaceAnimLibrary`.

Set the state machine's **Max Transitions Per Frame to 1**. The transitions read outputs computed before the graph update, so allowing several per frame would let a single update chain through Turn on stale outputs.

| State | Node | Functions |
|:--|:--|:--|
| **Idle** | Idle pose | State On Become Relevant: `SetupIdle(TurnInPlace)` |
| **Turn** | Sequence Evaluator | State On Become Relevant: `SetupTurnPose(TurnInPlace)`. Evaluator On Become Relevant: Set Sequence to `SetupTurnAnim(TurnInPlace)`, Set Explicit Time 0. Evaluator On Update: `UpdateTurnAnim(TurnInPlace, DeltaTime, OutSequence)` → Set Sequence With Inertial Blending (OutSequence, 0.2), **Set Explicit Time to the returned time** |
| **Recovery** | Sequence Evaluator | State On Become Relevant: `SetupRecoveryPose(TurnInPlace)`. Evaluator On Update: `UpdateRecoveryAnim(TurnInPlace, DeltaTime, OutSequence)` → Set Sequence With Inertial Blending (OutSequence, 0.2), **Set Explicit Time to the returned time**. Recovery continues the turn clip from where the turn left it; restarting the clip would replay its turning frames and turn the character twice |
| **Abort** | State alias over Turn and Recovery | |

| Transition | Rule | Blend |
|:--|:--|:--|
| Idle → Turn | `TurnInPlace.Output.bWantsToTurn` | Inertialization 0.2 |
| Turn → Recovery | `Output.bWantsRecovery` | Inertialization 0.2 |
| Recovery → Turn | `Output.bWantsToTurn` | Inertialization 0.2 |
| Recovery → Idle | `TurnInPlace.Node.bRecoveryFinished` | Inertialization 0.2 |
| Abort → Idle | `Output.bAbortTurn`, priority −1 | Inertialization 0.15 |

Locomotion helpers:

* `Output.bStopToIdleForTurn`: leave Stop early.
* `Output.bStartToCycleFromTurn`: strafing only; skip Start.

**Curve filtering:** add `RemainingTurnYaw` and `TurnYawWeight` to every Inertialization node's *Filtered Curves*.

---

## 10. Pitfalls and how they are prevented

| Pitfall | Prevention |
|:--|:--|
| Combined moves lose part of the turn on the owning client | Saved-move mixin restores the older move's start state (`CombinedMoves` test) |
| Stale curves fire Recovery on the turn's first frame and it oscillates | Curves read on the worker before the state machine; "is turning" refreshed from them; recovery needs the "turned this entry" latch; the latch and the end flag are cleared on every state entry (`SetupIdle`, `SetupTurnPose`, `SetupTurnAnim`, `SetupRecoveryPose`); one transition per frame (§9) |
| A turn whose weight never registers parks at its last frame | Recovery also fires once the sequence played to its end (`bReachedAnimEnd`), cleared on entry like the latch |
| Recovery replays the turn and turns twice | Recovery continues the clip from the turn's time (`UpdateRecoveryAnim`), as the pseudo states do |
| First weighted frame jumps the offset | That frame only records the curve |
| Curve sign flips mid-turn and snaps | Delta skipped on a sign flip (0 counts as same side) |
| Offset passes 180 and wraps | A delta past 180 is held |
| Inertialization overshoot flickers pause/lock | One-sided ≥ 0.95 thresholds, plus curve filtering |
| Rotating platforms call `FaceRotation` with DeltaTime 0 | Passed straight to the engine |
| Pitch, roll, custom gravity cause spurious turns | Heading measured about the gravity up axis; the body is kept upright to gravity |
| Proxy loses its replicated offset to a local lock | Proxies never reset the offset |
| Proxies stuck mid-turn on slow server updates | Local curve simulation |
| Server without bone refresh diverges | BeginPlay warning, or Pseudo mode |
| Pseudo vs graph differ | Same transitions incl. Abort and the recovery continuation, tested; montage pause/lock curves are the one exception (§7.4) |
| Jumping changes the last input direction | Only ground velocity and real input count; in the air the velocity along gravity is dropped |
| Root motion leaves the last direction stale | The facing is recorded during root motion (the component's tick covers anim root motion, which skips `PhysicsRotation`), plus a 0.25 s grace period after it |
| Gamepad stick bounce flips the character | Input only counts above 0.5 analog |
| Mouse at the max angle jitters the play rate | Max-angle rate latched for the rest of the turn |
| Direction-change rate never triggers | Compared with the playing turn's side, not with the offset's own sign |
| Root-motion montage handling inverted or too broad | One tested function; ignored slots, montages and additive; per-montage overrides |
| Strafe Direct snaps to the camera on the first step | Interp-out alpha |
| A stop resumes an old half-turn | State cleared while moving |
| Yaw-only rotation sweeps every frame | Standing rotation does not sweep unless pitch or roll changes (moving Strafe Direct keeps the engine's `SetActorRotation`) |
| Game-thread reads race the anim worker | Game-thread code reads a copy of the curves taken in the component's tick, which runs after the mesh's by prerequisite |
| A stance override or provider that queries the component recurses | The stance is marked resolved (Default) before it is resolved |
| Missing setup fails silently | Self-check (§11) |
| Replication cost | 16 bits, simulated proxies only, push model, on change |
| Blueprint events during load assert | Caching happens in OnRegister / BeginPlay / OnAnimInitialized |

---

## 11. Validation and debugging

**Self-check** (development builds). Each problem is warned about once per owner class in `LogEGAnimation`:

* the rotation setup needs `FaceRotation` or `PhysicsRotation`, but the hook never ran (counted only while the engine would call it: `FaceRotation` for player-controlled characters, which call it every frame);
* the owning client predicts moves without `TEGTurnInPlaceSavedMove`;
* the anim instance lacks `IEGTurnInPlaceAnimSource`;
* no `TurnSet`, or no character mesh;
* a dedicated server in Animation mode without `AlwaysTickPoseAndRefreshBones`;
* no sequence for the chosen step or side;
* no angles for the turn mode.

**Console variables:**

* `eg.TurnInPlace.Debug 1`: on-screen offset per role, and arrows from the feet (pink actor, black control, green turn target).
* `eg.TurnInPlace.Debug.State 1`: resolved state and its reason.
* `eg.TurnInPlace.Override 0..3`: force the state.

---

## 12. Integration with the rest of the plugin

| System | Interaction |
|:--|:--|
| `UEGAnimInstance` | Runs the anim side, exposes `TurnInPlace`, publishes turn state tags |
| Anim state tags | Pick the stance; the component publishes its own |
| Anim events | `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` windows |
| `UEGMovementModifierComponent` | A combined rotation scale of 0 locks |
| Root Motion Move | Root motion sources pause |
| Set Warp Target / Motion Warping | Root-motion montages pause through the montage rules |
| Cover | Locks while in cover |
| Chess seat | Pauses while the seat holds the pawn |
| Soft Lock | Pauses for its window |

These only use the request API and tags, so turn in place stays optional for every other system.

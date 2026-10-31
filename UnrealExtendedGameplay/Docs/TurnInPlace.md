# Turn In Place

A component-based turn-in-place system for `UnrealExtendedGameplay`.

* There is no character, movement-component or anim instance base class.
* **No code is needed to start:** add the component, a turn set and the **Turn In Place** anim node.
* Multiplayer player characters forward a few engine calls for prediction; helpers make those lines hard to get wrong.
* The math, networking, anim playback, an optional foot-lock IK and setup checks live in the plugin.

> Code: `Source/UnrealExtendedGameplay/Animation/TurnInPlace`, `Source/UnrealExtendedGameplay/Animation/IK`. Anim graph nodes' editor side: `Source/UnrealExtendedGameplayAnimGraph` (UncookedOnly). Curve modifier: `Source/UnrealExtendedGameplayEditor`. Tests: `UnrealExtendedGameplay.Animation.TurnInPlace.*`.

---

## 1. Goals and non-goals

**Goals**

* Turn in place for the three common character setups (`UEGTurnInPlaceComponent::SetRotationMode` sets the flags):
  * **Strafe Direct:** `bUseControllerRotationYaw`, snaps to the camera.
  * **Strafe Desired:** `bUseControllerDesiredRotation`, interpolated.
  * **Orient To Movement:** `bOrientRotationToMovement`. After input is released, the character keeps turning towards the last input direction.
* **Actor-based.** The capsule really rotates and the mesh is never counter-rotated. Mesh smoothing on simulated proxies, sockets, aim offsets and every other system see one true rotation.
* **Easy first, exact when needed.** It works with no code (the component drives rotation). Forwarding the hooks makes it predicted on the owning client.
* **Any project:** any anim instance (one node), any movement system (a movement source), clips with or without curves, and steps without clips (procedural turns, with foot locking).
* **Multiplayer correct** with the hooks: predicted on the owning client, cheap to replicate, and stable on simulated proxies and dedicated servers.
* **Per-stance animation sets,** selected by gameplay tag.
* **Pause and lock from anywhere:** gameplay code, montages, anim curves and any anim state tag provider can pause or lock it, and the result is deterministic.
* **Self-checking.** A missing hook or a bad setup is reported once, with the fix.

**Non-goals**

* Mesh-based turn in place (Rotate Root Bone with a root yaw offset).
* Shipping animations or an anim blueprint.
* A movement source for Mover or other movement systems. The interface is there (§3.4); the plugin ships the Character Movement one.

---

## 2. The model

```
DesiredYaw   where the character should face (control rotation, or last input direction)
ActorYaw     where the capsule actually faces
TurnOffset   DesiredYaw − ActorYaw, the yaw the character still has to turn
```

* **Standing still:**
  * the rotation driver holds the actor back, so that `ActorYaw = DesiredYaw − TurnOffset`;
  * the offset grows as the camera turns, clamped to `MaxTurnAngle`. Past that angle the actor is dragged along.
* **Starting a turn:** once `|TurnOffset| ≥ MinTurnAngle`, a turn plays.
* **Each turn yields two curves:**
  * `RemainingTurnYaw`: the yaw the turn still has to turn. Its frame-to-frame change is subtracted from the offset, so the actor rotates exactly as much as the animation does.
  * `TurnYawWeight`: 1 while turning, 0 during the recovery frames.
* **Where the curves come from**, first match:
  1. the clip's authored curves (the **Turn In Place Curves** modifier);
  2. the clip's root track, sampled once per clip by the Turn In Place node;
  3. a procedural turn, when the step has no clip: the remaining yaw eases from the offset to 0 at `Procedural.TurnRate`, on the idle pose.
* **Moving:** the offset is cleared and normal character rotation takes over. In Strafe Direct the change is eased out over `MovingInterpOutRate`.

What applies the rotation is the **rotation driver** (§4):

* **Hooks:** the project forwards `FaceRotation` / `PhysicsRotation`. Predicted.
* **Component:** the component rotates the owner from its own tick, after the movement component and before the mesh. No code, not predicted.
* **Auto** (default): the hooks once they are seen, otherwise the component.

`TurnOffset` is the opposite sign of Lyra's `RootYawOffset`. Do not use it for aim offsets or locomotion: the actor's rotation is already true.

---

## 3. Pieces

| Piece | Kind |
|:--|:--|
| `UEGTurnInPlaceComponent` | Replicated actor component, the runtime owner. Also an `IEGAnimStateProvider` |
| `UEGTurnInPlaceSet` | Data asset: per-stance entries and curve names |
| `UEGTurnInPlaceMovementSource` / `UEGTurnInPlaceCharacterSource` / `UEGTurnInPlacePawnSource` | How the owner is read and rotated (instanced on the component) |
| **Turn In Place** node (`FAnimNode_EGTurnInPlace`) | The whole anim side in one node, any anim instance |
| **Foot Lock** node (`FAnimNode_EGFootLock`) | Optional IK: planted feet stay put while the capsule turns; procedural steps |
| `FEGTurnInPlaceSolver` | Every decision as plain functions (no world), unit tested |
| `EGTurnInPlace::HandleFaceRotation` / `HandlePhysicsRotation` / `ComputeOrientToMovementRotation` | Optional hooks for prediction (`EGTurnInPlaceHooks.h`) |
| `TEGTurnInPlaceSavedMove<TBase>`, `TEGTurnInPlacePredictionData<TMove, TBaseData>`, `FEGTurnInPlaceSavedMove`, `FEGTurnInPlacePredictionData` | Saved-move support (`EGTurnInPlaceHooks.h`) |
| `UEGTurnInPlaceAnimLibrary`, `IEGTurnInPlaceAnimSource`, `UEGAnimInstance` | For a project that builds its own state machine instead of using the node (§9.3) |
| `UEGTurnInPlaceCurveModifier` | Animation modifier "Turn In Place Curves" (`UnrealExtendedGameplayEditor`) |

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
  LeftTurns / RightTurns   one UAnimSequence per step (missing ones turn procedurally)
  PlayRateAtMaxAngle (1.3), PlayRateOnDirectionChange (1.7), bKeepMaxAnglePlayRate
  MovingInterpOutRate
  Montages           FEGTurnInPlaceMontageRules (Overrides, bIgnoreAdditive, IgnoredSlots, IgnoredMontages)
  Procedural         FEGTurnInPlaceProcedural
                       bTurnWithoutAnimation (true), TurnRate (180 deg/s), MinDuration (0.25 s)
                       bSettle (false), SettleAngle (15), SettleDelay (1 s)
```

* **Stance resolution:**
  * Happens **once per frame on the game thread**, from the tags of every other `IEGAnimStateProvider` on the owner. Providers added at runtime are picked up.
  * `GetStanceTag()` overrides it.
  * Movement and animation read the same result. The anim worker reads entries straight from the asset, which is read-only at runtime.
* **Procedural turns:** a step without a clip for its side turns procedurally while `bTurnWithoutAnimation` is on. With `bSettle`, an offset between `SettleAngle` and the min turn angle, held for `SettleDelay` while standing, is closed by a procedural turn too. Procedural turns keep the idle pose: add the Foot Lock node so the feet do not slide.
* **Validation:** `IsDataValid` checks:
  * steps are positive and ascending;
  * one left and one right sequence per step (fewer is allowed while `bTurnWithoutAnimation` is on);
  * every sequence has both curves (a warning: the node reads the root track without them);
  * Min ≤ Max.

### 3.2 `UEGTurnInPlaceComponent`

| Call | Purpose |
|:--|:--|
| `GetTurnOffset()` | Current offset (degrees) |
| `IsTurningInPlace()` | Weight curve above 0 |
| `GetEnabledState(OutReason)` | Resolved state and why (§6) |
| `GetActiveRotationDriver()` | Hooks or Component, this frame |
| `GetMovementInfo(OutInfo)` | What the movement source reports |
| `ResetTurn()` | Forget the offset and take the current facing as the last input. **Call after teleporting or setting the rotation from gameplay** |
| `PushPause(Reason)`, `PushLock(Reason)` → handle; `RemoveRequest(Handle)` | Handle-based requests |
| `SetRotationMode(Character, Mode)` | Sets the character and movement flags for one of the three setups |
| `GetLastInputDirection()` | What Orient To Movement turns towards when input stops |
| `GetStanceTag`, `GetTurnModeTag`, `ShouldIgnoreMontage`, `CanAbortTurn` | BlueprintNativeEvents for project rules |

| Setting | Default | Meaning |
|:--|:--|:--|
| `TurnSet` | — | Required |
| `MovementSource` | Character Movement | §3.4 |
| `RotationDriver` | Auto | Auto, Hooks, Component (§4) |
| `bRotateToLastInputDirection` | true | Orient To Movement keeps turning towards the last input |
| `DedicatedServerAnimMode` | Animation | `Animation` or `Pseudo` (§7.4) |
| `bSimulateCurvesOnProxies` | true | §7.3 |
| `bReportSetupProblems` | true | §11 |

The component has three ticks of its own, so the project never adds a Tick override:

* the **snapshot tick** (after the controller, before the movement component) records the rotation the movement starts from;
* the **driver tick** (`TG_PrePhysics`, after the movement component, before the mesh) rotates the owner when the driver is Component;
* the **main tick** (`TG_PostPhysics`, after the mesh) copies the curves, runs settle timing, proxy simulation, pseudo states, self-checks and debug drawing.

### 3.3 `FEGTurnInPlaceSolver`

| Function | Does |
|:--|:--|
| `YawDeltaAroundUp`, `HeadingAroundUp` | Heading difference about the gravity up axis. Plain yaw subtraction on flat ground, projected headings with roll or custom gravity |
| `MakeUprightRotation` | Upright to gravity at a heading plus a yaw, for any up axis |
| `UpdateOffset` | One standing step: measure, apply the curve delta with its guards (§10), clamp. Returns the yaw the actor turns by. Locked returns 0 and clears the state; Paused turns all the way with no offset; simulated proxies keep and advance the replicated offset and never rotate |
| `SelectStep`, `ComputePlayRate` | Which step plays; max-angle rate (latched) and direction-change rate. The direction change compares the offset with the *playing* turn's side |
| `ResolveState`, `EvaluateRootMotionMontage` | §6 |
| `ComputeGraphOutput`, `BeginTurn`, `AdvanceTurnTime`, `EvaluateTurnCurves`, `UpdateNodePlayRate`, `GetTurnAnimation` | Turn playback, shared by the node, the library and the pseudo states |
| `GetProceduralDuration`, `EvaluateProceduralTurn`, `UpdateSettleTimer` | Procedural turns and settling |
| `BuildYawTrack`, `EvaluateYawTrack` | A clip's turn from root yaw samples: what the node reads and what the curve modifier writes |
| `AdvancePseudoState` | §7.4 |
| `UpdateLastInputDirection` | §10 (jumping, root motion, stick bounce) |

### 3.4 Movement source

`UEGTurnInPlaceMovementSource` is picked on the component (an instanced object, like a material in a details panel): **Character Movement** (default) for `ACharacter`, **Pawn** for any pawn that only rotates (root rotation, velocity, control rotation as the facing), or a subclass. It has four overridable functions, in C++ or Blueprint:

| Function | Default | Character Movement |
|:--|:--|:--|
| `GatherMovement(Owner, OutInfo)` | Root rotation, velocity, control rotation as the desired facing | Rotation flags, velocity, acceleration, custom gravity, root motion and montages, controller desired rotation, movement base |
| `ApplyRotation(Owner, Rotation)` | Root component, no sweep for a yaw-only change | (same) |
| `GetMesh(Owner)` | First skeletal mesh | `ACharacter::GetMesh` |
| `GetMovementComponent(Owner)` | The pawn's movement component | The character movement component |

For Mover or a custom pawn, subclass it: fill `FEGTurnInPlaceMovementInfo` from that system and apply rotation the way it expects (Mover, for example, needs the orientation set through its own state, not on the root component). The hooks are character-only, so such pawns use the Component driver.

---

## 4. Frame flow

```
Controller tick            FaceRotation (Strafe Direct) ─┐ hooks, when forwarded
Movement tick              PhysicsRotation              ─┘   state (§6) → UpdateOffset (last frame's curves) → rotate
Snapshot tick              the rotation before the movement (after the controller's input handling)
Driver tick                (Component driver) start from that snapshot, so the movement's own turn is
                           replaced and gameplay rotations are kept → state → UpdateOffset → rotate
Mesh tick, game thread     Turn In Place node PreUpdate: component.MakeGraphInput
Mesh tick, worker          node: Idle ⇄ Turn → Recovery → Idle (abort, moving → Idle), crossfades,
                           writes RemainingTurnYaw / TurnYawWeight into the pose
Main tick                  copy the evaluated curves (IEGTurnInPlaceAnimSource, else the anim instance's curves)
                           → settle timer → proxy simulation, pseudo states, self-check, debug
```

Ordering is enforced with tick prerequisites: snapshot after the controller, movement after the snapshot, driver after the movement, mesh after the driver, main tick after the mesh (whose parallel evaluation it then waits for).

| Struct | Written by | Read by |
|:--|:--|:--|
| `FEGTurnInPlaceMovementInfo` | Movement source | Component |
| `FEGTurnInPlaceMoveState` | Rotation driver | Rotation driver, saved-move snapshot |
| `FEGTurnInPlaceGraphInput` | Component (game thread) | Node, library, pseudo states |
| `FEGTurnInPlaceCurves` | Anim (node or graph), copied after the mesh | Rotation driver, proxy simulation |
| `FEGTurnInPlaceNodeState` | Node / library / pseudo states | Same |

---

## 5. Gameplay tags

| Tag | Use |
|:--|:--|
| `ExtendedGameplay.TurnInPlace.Mode.Movement` / `.Strafe` | Turn mode (which angles) |
| `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` | Read from anim state tags. `UEGAnimEventComponent` publishes open anim event windows as such, so an Anim Event Window notify with the tag pauses or locks |
| `ExtendedGameplay.Movement.RotationLocked` | Read from anim state tags. `UEGMovementModifierComponent` publishes it while the combined rotation rate scale is 0 |
| `ExtendedGameplay.TurnInPlace.State.Turning`, `.Locked`, `.Paused`, `.Left`, `.Right` | Published by the component as anim state tags (`UEGAnimInstance::HasStateTag`) |

Any project system can pause or lock turn in place by publishing these tags from an `IEGAnimStateProvider`; turn in place knows nothing about the systems that do.

---

## 6. Enabled state

The resolved state is the first match in this list, top to bottom (`FEGTurnInPlaceSolver::ResolveState`):

1. `eg.TurnInPlace.Override` (non-shipping): 1 Enabled, 2 Locked, 3 Paused.
2. **Paused**: a pause request.
3. **Locked**: a lock request.
4. **Paused**: `PauseTurnInPlace` curve ≥ 0.95.
5. **Locked**: `LockTurnInPlace` curve ≥ 0.95.
6. **Paused**: the `ExtendedGameplay.TurnInPlace.Pause` anim state tag.
7. **Locked**: the `ExtendedGameplay.TurnInPlace.Lock` anim state tag.
8. A networked root-motion montage, checked in this order:
   * nothing, if `ShouldIgnoreMontage` returns true (the project's own rule wins);
   * the montage's entry in `Montages.Overrides`, if it has one;
   * nothing, if it is in `IgnoredMontages`, uses an `IgnoredSlots` slot, or is additive;
   * otherwise **Paused**.
9. **Paused**: the movement has gameplay root motion sources (Root Motion Move, root-motion based abilities). Soft collision pushes do not count: being jostled must not stop a turn.
10. **Locked**: the `ExtendedGameplay.Movement.RotationLocked` anim state tag.
11. Otherwise, the stance entry's `State`.

What each state means:

* **Locked:** no rotation at all, standing or moving; the body stays upright to gravity.
* **Paused:** no offset builds up, and the character rotates normally, as if turn in place did not exist.
* **Abort:** if the state leaves Enabled during a turn and `CanAbortTurn()` is true, the turn blends back to the source pose.

---

## 7. Networking

### 7.1 Owning client

* **With the hooks**, the offset is computed inside movement, so it is predicted with the move.
  * `TEGTurnInPlaceSavedMove` saves the move state in `SetInitialPosition` and **restores the older move's start state in `CombineWith`**.
  * Why that matters: combining two moves rewinds the rotation to the older move's start and re-runs the rotation once. Without the restore the client turns only part of the way at high frame rates. The `CombinedMoves` test reproduces this and checks the fix.
* **With the Component driver**, client and server each rotate the character from their own tick. Standing rotation does not move the capsule, so nothing is corrected, but the owning client's facing is not predicted and can differ briefly from the server's. A server runs a remote player's moves before that tick, so in Strafe Desired the movement has already turned the character there: the server, and the other players who see its rotation, turn at the rotation rate instead of with the turn animations. Fine for single player, AI and listen-server hosts; player characters in multiplayer should use the hooks (the self-check says so).

### 7.2 Server

* Runs the same hooks inside `ServerMove`, or the driver tick.
* `SimulatedOffset` (16 bits) is marked dirty only when the offset changes. It is push-model, `COND_SimulatedOnly`, and not used in standalone.

### 7.3 Simulated proxies

* Rotation comes from movement replication; the offset comes from `SimulatedOffset`.
* Between updates the component's tick consumes the offset with the local curves.
* A local lock never discards the replicated value. The server only resends on change, so a discarded value would not come back.

### 7.4 Dedicated server

* **Animation** (default): the mesh needs `AlwaysTickPoseAndRefreshBones`. Otherwise the component warns once at BeginPlay with the fix.
* **Pseudo:** the component runs Idle / Turn / Recovery on the game thread (`AdvancePseudoState`, the same transitions as the node and the recipe: Abort from Turn or Recovery, a stop once the character moves, Recovery continuing the turn clip) and evaluates the curves at the tracked time: the clip's authored curves, or the procedural turn. The anim graph stays idle on that server.

  Limitations, both reported by the self-check where they apply:
  * Pseudo servers read **authored curves only**: clips without them need the curve modifier (procedural turns need nothing).
  * Pseudo servers only see `PauseTurnInPlace` / `LockTurnInPlace` curves on the turn sequences themselves, not on montages. To pause or lock from a montage that must also hold on a Pseudo server, use an `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` anim event window, or a request, instead of a curve.

---

## 8. Project integration

### 8.1 Quick start (no code)

1. **Component:** add `UEGTurnInPlaceComponent` to the character and assign a `UEGTurnInPlaceSet`. Set the rotation setup with `SetRotationMode` (Blueprint-callable), or the usual flags.
2. **Turn set:** add the turn clips per step. Steps without clips turn procedurally. Clips need root motion (the root bone turns); curves are optional with the node.
3. **Anim graph:** add the **Turn In Place** node after the locomotion (or inside its Idle state) with that pose as **Source** (§9.1). Optionally add **Foot Lock** after it (§9.2).

That is all: `RotationDriver` Auto drives rotation from the component.

### 8.2 Predicted multiplayer (hooks)

For player characters in multiplayer. The project's module needs `UnrealExtendedGameplay` in its Build.cs dependencies. Then:

```cpp
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceHooks.h"
```

**1. Forward `FaceRotation`.** Needed for Strafe Direct; add it anyway.

```cpp
void AMyCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (!EGTurnInPlace::HandleFaceRotation(this, NewControlRotation, DeltaTime))
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
	}
}
```

**2. Use a movement component subclass** that forwards `PhysicsRotation` and allocates turn-aware prediction data. Set it on the character with `ObjectInitializer.SetDefaultSubobjectClass<UMyMovement>(ACharacter::CharacterMovementComponentName)`.

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

With `RotationDriver` Auto the component switches to the hooks the first time it sees one. Set it to **Hooks** to make a missing hook an error the self-check reports.

### 8.3 Optional

* **Keep turning towards the last input while sliding to a stop** (Orient To Movement, hooks):

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
* **Other movement systems:** a movement source (§3.4).
* **Curves on the clips:** run the **Turn In Place Curves** modifier when using the state machine recipe (§9.3) or Pseudo servers. The curve names must match the set's `Curves`.

The last input direction is tracked inside `HandlePhysicsRotation` and the driver tick, so projects do not override `CalcVelocity` or `ApplyRootMotionToVelocity`.

---

## 9. Anim graph

### 9.1 The Turn In Place node (recommended)

`Source` is the pose shown while not turning. Two placements work:

* **After the locomotion:** `Locomotion → Turn In Place → Slot → Output`. While moving it passes the source through; a turn in progress blends back to it when the character starts moving.
* **Inside the locomotion's Idle state:** `Idle pose → Turn In Place → state output`. The transitions into Idle must be Inertialization, not crossfades: a crossfade scales the node's curves while it lasts, and a turn starting during it would be consumed short.

| Setting | Default | Meaning |
|:--|:--|:--|
| `TurnBlendTime` | 0.2 s | Source → turn, and turn → next turn |
| `RecoveryBlendTime` | 0.25 s | Back to the source once the recovery has played out |
| `AbortBlendTime` | 0.15 s | Back to the source when the character moves or the state leaves Enabled |
| `bLockRootBone` | true | The root stays at its reference pose while a clip plays; the capsule turns instead |
| `RootYawFinishedTolerance`, `RootYawRecoveryTime`, `RootYawSampleRate` | 1°, 0.2 s, 30 | Reading clips without curves (the same rules as the curve modifier) |

* Clips play through standalone sequence evaluators, so their notifies (footsteps) fire.
* A turn keeps the clip, procedural settings and curve names it started with, even if the stance or the turn set changes during it.
* It reads the component in `PreUpdate` (game thread) and needs no anim instance support: no parent class, no interface, no variables.
* It writes `RemainingTurnYaw` and `TurnYawWeight` into its output pose **unblended**. Nothing after it may scale them: no blend that fades it out while standing, and add both curves to any downstream Inertialization's *Filtered Curves*.
* The AnimBP's Root Motion Mode must not be *Root Motion From Everything*: the turn clips would then also rotate the capsule through root motion.
* `Show Debug Animation` lists the phase, offset, time, remaining yaw and weight.

### 9.2 The Foot Lock node (optional IK)

Put it after Turn In Place, in component space (`Turn In Place → Local To Component → Foot Lock → Component To Local`).

* While the owner stands on the ground (slower than `MaxOwnerSpeed`, and the mesh itself not moving through the world, so lifts and platforms do not lock) a foot the animation holds still **relative to the mesh** is locked where it is in the world. A capsule turning under an idle pose (procedural turns, settles, the max-angle drag) leaves the feet planted.
* A foot the animation moves or pivots faster than `ReleaseSpeed` / `ReleaseAngularSpeed` follows the animation, so turn clips, stepping and montages play as authored.
* A locked foot steps (an arc of `StepHeight` over `StepDuration`) once the animation would put it more than `StepDistance` away or `StepAngle` turned. One foot steps at a time.
* Two-bone IK on foot → parent → grandparent; the knee bends in the animation's own plane. `bAllowStretching` lets the leg stretch to reach.
* `bOnlyWhileTurning` limits locking to turns (needs the component). Locks drop on teleports (`TeleportDistance`), rotation snaps (more than twice `StepAngle` in a frame) and after an update the node missed (alpha 0, its LOD threshold, a branch that stopped playing).
* Defaults are the UE5 mannequins' `foot_l` / `foot_r`. It works without a turn in place component.

Large procedural turns with only the idle pose still look like what they are, a character rotating on planted feet with occasional steps. Small angles (settle, drag) look natural; for large ones, author clips for the big steps and let procedural turns cover the rest.

### 9.3 Your own state machine (library)

For a project that wants its own states. Parent the AnimBP to `UEGAnimInstance` (it runs both halves and serves the curves), or in a custom anim instance:

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

// Optional: implement IEGTurnInPlaceAnimSource so procedural turns reach the component.
FEGTurnInPlaceCurves UMyAnimInstance::GetTurnInPlaceCurves_Implementation() const
{
	return TurnInPlace.Curves;
}
```

A `TurnInPlace` state machine, used as the idle pose. Set its **Max Transitions Per Frame to 1**: the transitions read outputs computed before the graph update, so several per frame would let one update chain through Turn on stale outputs. The clips need the authored curves.

| State | Node | Functions |
|:--|:--|:--|
| **Idle** | Idle pose | State On Become Relevant: `SetupIdle(TurnInPlace)` |
| **Turn** | Sequence Evaluator | State On Become Relevant: `SetupTurnPose(TurnInPlace)`. Evaluator On Become Relevant: Set Sequence to `SetupTurnAnim(TurnInPlace)`, Set Explicit Time 0. Evaluator On Update: `UpdateTurnAnim(TurnInPlace, DeltaTime, OutSequence)` → Set Sequence With Inertial Blending (OutSequence, 0.2), **Set Explicit Time to the returned time**. For procedural turns (`TurnInPlace.Node.bProcedural`) blend in the idle pose instead of the evaluator |
| **Recovery** | Sequence Evaluator | State On Become Relevant: `SetupRecoveryPose(TurnInPlace)`. Evaluator On Update: `UpdateRecoveryAnim(TurnInPlace, DeltaTime, OutSequence)` → Set Sequence With Inertial Blending (OutSequence, 0.2), **Set Explicit Time to the returned time**. Recovery continues the turn clip from where the turn left it |
| **Abort** | State alias over Turn and Recovery | |

| Transition | Rule | Blend |
|:--|:--|:--|
| Idle → Turn | `TurnInPlace.Output.bWantsToTurn` | Inertialization 0.2 |
| Turn → Recovery | `Output.bWantsRecovery` | Inertialization 0.2 |
| Recovery → Turn | `Output.bWantsToTurn` | Inertialization 0.2 |
| Recovery → Idle | `TurnInPlace.Node.bRecoveryFinished` | Inertialization 0.2 |
| Abort → Idle | `Output.bAbortTurn`, priority −1 | Inertialization 0.15 |

Locomotion helpers (both paths can use them from `UEGAnimInstance`): `Output.bStopToIdleForTurn` leaves Stop early; `Output.bStartToCycleFromTurn` (strafing) skips Start.

**Curve filtering:** add `RemainingTurnYaw` and `TurnYawWeight` to every Inertialization node's *Filtered Curves*.

---

## 10. Pitfalls and how they are prevented

| Pitfall | Prevention |
|:--|:--|
| Combined moves lose part of the turn on the owning client | Saved-move mixin restores the older move's start state (`CombinedMoves` test) |
| Stale curves fire Recovery on the turn's first frame and it oscillates | The node decides on the curves at the time it last evaluated; the recipe reads curves on the worker before the state machine, latches "turned this entry", clears the latch on every state entry, and allows one transition per frame |
| A turn whose weight never registers parks at its last frame | Recovery also fires once the turn played to its end (`bReachedAnimEnd`) |
| Recovery replays the turn and turns twice | Recovery continues the clip from the turn's time, in the node, the recipe and the pseudo states |
| First weighted frame jumps the offset | That frame only records the curve |
| Curve sign flips mid-turn and snaps | Delta skipped on a sign flip (0 counts as same side) |
| Offset passes 180 and wraps | A delta past 180 is held |
| Inertialization overshoot flickers pause/lock | One-sided ≥ 0.95 thresholds, plus curve filtering |
| Curves scaled by a blend reach the component wrong | The node writes them unblended at its output; nothing after it may scale them (§9.1) |
| A clip without curves turns nothing | The node reads its root track (the curve modifier's rules, one shared function); a clip whose root does not turn is approximated from the step size and reported |
| A clip with Force Root Lock reads as not turning | Reported; the node locks the root itself (`bLockRootBone`), so the clip's own lock is not needed |
| Sampling a clip at its end wraps to its first frame | The node's players stop just short of the end |
| No clip for a step | A procedural turn, or a report when procedural turns are off |
| Procedural turns slide the feet | Foot Lock keeps planted feet in place and steps them |
| Foot lock fights a turn clip | Feet the animation moves or pivots relative to the mesh follow the animation |
| Both feet step at once | One procedural step at a time |
| Feet stay locked through a teleport | Locks drop past `TeleportDistance` |
| Rotating platforms call `FaceRotation` with DeltaTime 0 | Passed straight to the engine; the component driver carries the base's rotation over |
| The component driver fights the movement component | It starts from the rotation just before the movement (snapshot tick), so the movement's own turn towards the desired facing is replaced, not added |
| The component driver undoes gameplay rotations (cover, abilities facing a target) | The snapshot runs after the controller and before the movement, so rotations made outside the movement are kept; for Strafe Direct, a facing other than the controller's snap is kept |
| A stance or turn set change mid-turn snaps the capsule | The node keeps the turn's clip and settings until the turn ends |
| A deactivated component keeps holding the rotation back | Hooks and driver stop when the component's tick is off, and the graph treats it as Paused |
| Turns repeat while nothing can rotate the owner (no controller, movement off) | The offset is cleared while the movement source reports it cannot rotate |
| A moving base drags or stretches locked feet | Locking needs the mesh itself still in the world; vertical drift triggers a step too |
| The component driver rotates after the animation used the old rotation | Its tick runs after the movement component and before the mesh (prerequisites) |
| The character turns back after a teleport | `ResetTurn()` |
| Root motion turns the capsule twice | Anim root motion pauses turn in place (montage rules) and the driver leaves it alone; the node's clips must not be root motion (no Root Motion From Everything) |
| Pitch, roll, custom gravity cause spurious turns | Heading measured about the gravity up axis; the body is kept upright to gravity for any up axis |
| Proxy loses its replicated offset to a local lock | Proxies never reset the offset |
| Proxies stuck mid-turn on slow server updates | Local curve simulation |
| Server without bone refresh diverges | BeginPlay warning, or Pseudo mode |
| Pseudo vs graph differ | Same transitions incl. Abort, procedural turns and the recovery continuation, tested; montage pause/lock curves and curve-free clips are the reported exceptions (§7.4) |
| Jumping changes the last input direction | Only ground velocity and real input count; in the air the velocity along gravity is dropped |
| Root motion leaves the last direction stale | The facing is recorded during root motion, plus a 0.25 s grace period after it |
| Gamepad stick bounce flips the character | Input only counts above 0.5 analog |
| Mouse at the max angle jitters the play rate | Max-angle rate latched for the rest of the turn |
| Direction-change rate never triggers | Compared with the playing turn's side, not with the offset's own sign |
| Root-motion montage handling inverted or too broad | One tested function; ignored slots, montages and additive; per-montage overrides |
| Strafe Direct snaps to the camera on the first step | Interp-out alpha (hooks and component driver) |
| A stop resumes an old half-turn | State cleared while moving |
| Yaw-only rotation sweeps every frame | Standing rotation does not sweep unless pitch or roll changes |
| Game-thread reads race the anim worker | Game-thread code reads a copy of the curves taken in the main tick, which waits for the mesh's evaluation by prerequisite; the node reads the component only in `PreUpdate` |
| A system that pauses turn in place needs to know about it, or the reverse | Soft links through anim state tags (§5); providers added at runtime are picked up |
| A stance override or provider that queries the component recurses | Stance and state tags are marked resolved before they are gathered |
| Missing setup fails silently | Self-check (§11) |
| Replication cost | 16 bits, simulated proxies only, push model, on change |
| Blueprint events during load assert | Caching happens in OnRegister / BeginPlay / OnAnimInitialized |

---

## 11. Validation and debugging

**Self-check** (development builds). Each problem is warned about once per owner class in `LogEGAnimation`:

* a remote player's character in multiplayer is driven by the component (not predicted) after Auto found no hooks (a listen-server host's own character is fine);
* the offset has asked for a turn for seconds and none played: no Turn In Place node or recipe in the AnimBP, or its curves are scaled away;
* `RotationDriver` is Hooks, but the rotation setup's hook never ran (counted only while the engine would call it);
* the owning client predicts moves with the hooks but without `TEGTurnInPlaceSavedMove`;
* no `TurnSet`, no mesh, no anim instance, or a movement source that cannot read the owner;
* a dedicated server in Animation mode without `AlwaysTickPoseAndRefreshBones`;
* no sequence for the chosen step or side while procedural turns are off;
* a clip without curves on a Pseudo server, or a clip with neither curves nor a turning root (from the node);
* no angles for the turn mode.

**Console variables:**

* `eg.TurnInPlace.Debug 1`: on-screen offset per role, and arrows from the feet (pink actor, black control, green turn target).
* `eg.TurnInPlace.Debug.State 1`: resolved state, its reason, the active driver, and settling.
* `eg.TurnInPlace.Override 0..3`: force the state.

---

## 12. Integration with the rest of the plugin

| System | Interaction |
|:--|:--|
| `UEGAnimInstance` | Runs the library path, exposes `TurnInPlace`, publishes turn state tags. Not needed with the node |
| Anim state tags | Pick the stance; pause and lock (§5); the component publishes its own |
| Anim events | `ExtendedGameplay.TurnInPlace.Pause` / `.Lock` windows, published as anim state tags |
| `UEGMovementModifierComponent` | Publishes `ExtendedGameplay.Movement.RotationLocked` when its rotation scale is 0 |
| Root Motion Move | Root motion sources pause |
| Set Warp Target / Motion Warping | Root-motion montages pause through the montage rules |
| Cover | Locks while in cover |
| Chess seat | Pauses while the seat holds the pawn |
| Soft Lock | Pauses for its window |

Other systems only use the request API and tags, and turn in place reads them only through `IEGAnimStateProvider`, so each side stays optional for the other.

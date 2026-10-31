# Animation systems

The animation side of **UnrealExtendedGameplay** is built on one rule: **notifies say *when*; components and subsystems decide *what***. A notify object belongs to the animation asset and is shared by every character playing it, so it carries a tag and a little data, and the behaviour lives on the character.

| Piece | Where | What it is for |
|:--|:--|:--|
| Anim events | `Animation/Events` | Tagged notifies that any actor, component or anim instance can react to |
| Melee | `Combat` | Swept hit detection, a pluggable damage policy, hit pause |
| Time dilation | `Time` | Stacked slow motion, per-actor time and per-mesh anim rate (hit stop) |
| Movement modifiers | `Movement` | Stacked acceleration / walk speed / rotation rate changes |
| Root motion & warping | `Animation/RootMotion`, `Animation/MotionWarping` | Predicted dashes and lunges; warp targets from lock-on |
| Montage playback | `Animation/Montage` | One mesh/set/playback path, animation sets by tag, async montage nodes |
| Anim instance | `Animation/AnimInstance` | A thread-safe AnimBP base and gameplay state as tags |
| Turn in place | `Animation/TurnInPlace` | Actor-based turn in place: a component and the Turn In Place anim node; see [TurnInPlace.md](TurnInPlace.md) |
| Foot lock | `Animation/IK` | The Foot Lock anim node: planted feet stay put while a standing character rotates; procedural steps |
| Footsteps | `Footstep` | A footstep notify driving `UEGFootstepComponent` |
| Soft collision | `SoftCollision` | Pawn-vs-pawn pushes; the Soft Collision Modifier notify, `SoftCollision.Pushed` / `.Bumped` state tags; see [SoftCollision.md](SoftCollision.md) |

---

## Anim events

* **Anim Event** (`UEGAnimEventNotify`) and **Anim Event Window** (`UEGAnimEventNotifyState`) carry a gameplay tag and an `FEGAnimEventPayload` (socket, magnitude, extra tags, optional object).
* They are delivered to `IEGAnimEventReceiver` on the owning actor, each of its components, and the mesh's anim instance.
* Windows send **Begin** and **End**, and **Tick** when `bSendTickEvents` is on. Each window has a `WindowId`, so two overlapping windows with the same tag stay distinct.
* `UEGAnimEventComponent` turns events into Blueprint delegates. It also answers `IsWindowActive(Tag)` ("is the combo input window open?") and reports open windows to the anim instance as state tags.

## Melee

1. Add a `UEGMeleeTraceComponent` to the character.
2. Add shapes. Each shape has a tag under `ExtendedGameplay.Melee.Window` and is either a sphere at a socket or a blade between two sockets with `PointCount` spheres along it. For a weapon that is a separate actor, set `SourceComponentName`, or call `SetShapeSource` when the weapon is equipped.
3. Put a **Melee Window** notify in the attack. Its tag opens every shape at or under it; an empty tag opens all shapes. `DamageMultiplier` and `AttackTags` travel with each hit.

How a window behaves:

* While the window is open, each point is swept from last frame's position to this frame's, so a fast swing cannot skip a target between frames.
* Each actor is hit once per window.
* Hits go to the `DamagePolicy` (instanced on the component), then to optional hit pause, then to `OnHit`.

The default `Engine Damage` policy applies point (or plain) damage and an optional knockback. The knockback is a launch velocity, so it gives the same result at any frame rate. To route damage elsewhere (a GameplayEffect, your own health component), subclass `UEGMeleeDamagePolicy` in Blueprint or C++.

`Detection` decides where traces run:

* **AuthorityOnly:** the server only.
* **AuthorityAndLocalOwner** (the default): the owning client also traces, for instant feedback. Its hits arrive with `bAuthoritative = false` and never apply damage.
* **Everywhere:** every machine that plays the animation, for cosmetic-only setups.

> The older **EGDamageTrace_Notify** still works for existing animations, but it traces only the current frame and owns its damage settings. Prefer Melee Window for new work.

## Time dilation

`UEGTimeDilationSubsystem` is the one owner of time scaling.

* **Requests:** each has a priority (higher wins, newest on a tie), blend-in and blend-out times, and an optional duration in real seconds. The value from before the first request is restored after the last one ends.
* **`PushGlobalTimeDilation`:** server / standalone only. It is refused on clients because time dilation replicates from the server.
* **`PushActorTimeDilation`:** `CustomTimeDilation` on one actor.
* **`PushAnimRateScale` / `HitPause`:** the mesh's `GlobalAnimRateScale`. Only the animation slows, not movement, so it is safe for hit stop in multiplayer.

The **EGSlowMotionNotify** notify uses this subsystem, so overlapping slow-motion windows from different characters no longer end each other early.

## Movement modifiers

`UEGMovementModifierComponent` holds handle-based modifiers (acceleration, max walk speed and rotation rate scales) that multiply together. The **EGControlMovementNotify** notify pushes one for its window. By default it zeroes acceleration, as it always did, but it now exposes the full modifier. The component is added on demand. If other code changes one of these values while modifiers are active, the change is picked up as the new base.

## Root motion and Motion Warping

* **Root Motion Move** applies a constant-force root motion source to the character movement component for the window: a dash, lunge or step. It is simulated and predicted by the movement component, unlike the legacy *Move Forward* notifies, which set the actor location directly.
* **Set Warp Target** feeds a Motion Warping target from the lock-on target of `UEGTargetingComponent` / `UEGDynamicTargetingComponent`. With nothing locked on, it uses the closest pawn in front within `SearchRadius`. It stops `StopDistance` short of the target and never warps further than `MaxWarpDistance`.
  * Cover the montage's Motion Warping window with it, using the same warp target name.
  * The character needs a `UMotionWarpingComponent`.
  * For the server and the owning client to agree, the target has to be known on both.
* **EGSoftLockNotify** now prefers the lock-on target as well. Use it when turning towards the target is enough.

These run on the server and the owning client; simulated proxies get the result through movement replication.

## Montage playback

* **`IEGMontagePlayerInterface`** (optional on any actor) lets the actor choose the mesh for a purpose tag (`ExtendedGameplay.Anim.Mesh.Body` / `.FirstPerson`), supply its `UEGAnimationSet`, and take over playback (GAS, a replicated montage system).
* **`UEGAnimationSet`** maps tags to montages, with:
  * variants (random, sequential or first) and per-entry section, start time and play rate;
  * fallback up the tag hierarchy (`Cover.GetIn.Crouched` → `Cover.GetIn`) and then to a parent set;
  * soft references, loaded on first use or preloaded with `GatherSoftPaths`.

  Random picks differ between machines; pass a variant index for anything that has to match in multiplayer.
* **`UEGMontageLibrary`** provides the shared resolution and playback for Blueprint and C++.
* **Play Montage And Wait For Notify** and **Play Animation And Wait For Notify** are async Blueprint nodes for actors not driven by GAS. They report blend out, completion, interruption and this play's *Play Montage Notify* events. The tag version loads the montage asynchronously.

Cover, interaction and chess all play through this path:

* **Cover:** set `AnimationSet` on the component, or on the character via the interface, using the `ExtendedGameplay.Anim.Cover.*` tags. The existing montage slots stay as the last fallback, and the crouched get-in/out slots are now used when crouched.
* **Interaction and chess:** they resolve the mesh through the library (the body mesh, not the first skeletal mesh found) and honour the playback override.

## Anim instance

Parent your AnimBP to `UEGAnimInstance`.

* **Game thread:** it reads velocity, acceleration, falling/crouching, aim and **state tags** from every `IEGAnimStateProvider` on the owner.
* **Thread-safe update:** it derives speed, ground speed, direction, aim pitch/yaw, lean and "should move".
* **Reading state:** use `HasStateTag` (thread safe) for gameplay state. For example `ExtendedGameplay.Anim.State.Cover.Crouched` comes from the cover component, and any open anim event window comes from `UEGAnimEventComponent`.
* **Extending it:** override `GatherGameThreadData` / `ComputeThreadSafeData` in C++ to add your own values.

## Turn in place

Actor-based turn in place: the capsule is held back while standing, and turn animations rotate it through their curves, their root track, or a procedural turn where a step has no clip.

* **No code to start:** add `UEGTurnInPlaceComponent` with a turn set, and the **Turn In Place** node to any AnimBP. The component rotates the character itself.
* **Predicted multiplayer:** forward `FaceRotation` and `PhysicsRotation` and add the saved-move mixin; the component switches to them.
* **Any movement system:** a movement source reads and rotates the owner (Character Movement ships).
* **Foot Lock:** an optional IK node that keeps planted feet in place while the capsule turns under them, and steps them when they drift.

The full guide, including the anim graph setups and how each known pitfall is prevented, is in [TurnInPlace.md](TurnInPlace.md).

## Footsteps

The **Footstep** notify calls the owner's `UEGFootstepComponent` at `FootSocket`. Effects are skipped on dedicated servers. It can also report an AI hearing noise, and it raises `ExtendedGameplay.Anim.Footstep` for any other listener.

## Debugging

* `eg.Anim.Debug 1` draws melee sweeps (green, red on hit) and warp targets.
* Setup problems are logged once each to `LogEGAnimation`, for example a Melee Window on a character without a melee component, Set Warp Target without a Motion Warping component, or a missing foot socket.
* Animation sets validate their parent chains and empty entries in the editor.
* The automation tests live under `UnrealExtendedGameplay.Animation.*`.

## Plugin dependencies

The Gameplay plugin now also enables **MotionWarping** (an engine plugin) for the Set Warp Target notify.

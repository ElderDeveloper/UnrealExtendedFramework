# EG State Machine — Design Specification

**Status:** implemented. Phases 1–5 and 7 done and green (11 automation tests); phase 6 (PawnStarz) is code-complete but not compiled — see §15.2
**Target module:** `UnrealExtendedGameplay` (runtime)
**Target path:** `Source/UnrealExtendedGameplay/AI/StateMachine/`
**Engine:** Unreal Engine 5.8
**Author:** Kemal Erdem YILMAZ

---

## 1. Why this exists

Two projects currently ship the same architecture twice, diverging:

| | PawnStarzSimulator (`Plugins/TalesOfTrade`) | DevilOfPlague (`Source/DevilOfThePlague/AI`) |
|---|---|---|
| Base state | `UTOTState` — 5 lifecycle events + 4 transition flags | `UDOPState` — 3 lifecycle events, no flags |
| Component | `UTOTStateMachineComponent` — registry, stack, Start/Stop, debug draw | `UDOPStateMachineComponent` — brain tier + stack, replicated |
| Instancing | register once, reuse, `Instanced` editor array | `NewObject` per push, discarded on pop |
| Replication | none (server-only, clients see plain pawns) | current state + stack + targets replicated |
| Content on top | ~11 NPC states + ~12 enemy archetypes | 1 boss + 5 archetypes |

Each has something the other needs. TalesOfTrade has the correct **object model** (registration, pause/resume, transition policy). DevilOfPlague has the correct **network model**. A third project is planned, so the merged version belongs in the shared plugin repo, not in either game.

This document specifies that merged version. Nothing here is game-specific: no targeting, no GAS, no quest, no perception. Those stay in the games or in their own `UnrealExtendedGameplay` systems.

---

## 2. Requirements → design decisions

The eight rules this design is built to satisfy, and where each is answered.

| # | Rule | Decision | §  |
|---|---|---|---|
| 1 | One `StateMachineComponent`, one `State` object | `UEGStateMachineComponent` + `UEGState`. One optional third class, `UEGBrainState`, is a `UEGState` subclass — not a parallel system | 4 |
| 2 | Must replicate | Server simulates; one atomic replicated struct carries current + stack + context; clients mirror the lifecycle. State-internal data replicates via opt-in subobject registration | 8 |
| 3 | States are not shared between AI actors | Every instance is outered to its own component. There is no shared pool, no CDO execution, no subsystem-owned state | 6 |
| 4 | States have instanced properties, editable on the component | `TArray<TObjectPtr<UEGState>> StateDefinitions` marked `Instanced` — each placed actor / Blueprint edits its own values in the details panel | 6 |
| 5 | States can be given, removed, etc. | `GiveState` / `RemoveState` / `HasState` at runtime, with `bPushWhenGiven` and `bRemoveWhenExited` policy | 7.3 |
| 6 | Big states that do a lot are acceptable | Full 7-event lifecycle, per-state tick interval, sub-phase support, latent-friendly. No node-graph, no micro-state pressure | 5 |
| 7 | A brain-like state that controls other states is mandatory | `UEGBrainState` is required to start the machine. Component and AI-actor code stay empty of behaviour logic | 9 |
| 8 | Possibly register once and reuse | Register-once is the **only** model. It is what makes rules 2 and 4 work at all — see the invariant in §6.2 | 6 |

### 2.1 Hard constraint: nothing is removed from either game

Neither project deletes its state machine component or its state classes. `UDOPState`, `UDOPStateMachineComponent`, `UTOTState` and `UTOTStateMachineComponent` all survive as **thin adapters reparented onto the plugin classes** (§13). Every existing state subclass, Blueprint child, placed actor, component reference and serialized property value stays valid.

This constraint is not cosmetic — it shapes the API:

- **Lifecycle event names are TalesOfTrade's names** (`OnEnter` / `OnTick` / `OnExit` / `OnPause` / `OnResume` / `CanEnterState`), because that is the larger surviving codebase (~5,800 lines of states). TOT states then reparent with **zero signature edits**; DevilOfPlague gets forwarding shims instead (§13.2, hazard 3).
- **Property names are TalesOfTrade's names** verbatim, so BP- and level-authored values survive the reparent by name-match (§13.2, hazard 2).
- **Client-side lifecycle mirroring is opt-in and defaults to off** (§8.3), because both games are written assuming state code is authority-only. Turning it on by default would double-fire every `OnEnter` that spawns, applies or writes.

---

## 3. Non-goals

Explicitly out of scope, to keep the module honest:

- **Target acquisition.** Lives in `Systems/Targeting` or the game. The machine only stores a `ContextActor`.
- **Perception / EQS / navigation.** States call whatever they want; the machine knows nothing about them.
- **GAS coupling.** No `AbilitySystemComponent` dependency. States expose an optional `StateTag`; a game maps tags to gameplay effects itself.
- **Save/load.** No serialization of the running stack. A game that needs it re-derives from its own save data.
- **Visual graph editor.** Transitions are code/Blueprint decisions inside the brain, not authored edges.
- **Client-authoritative prediction.** Simulation is server-only. Clients mirror for cosmetics.

---

## 4. Class roster

```
Source/UnrealExtendedGameplay/AI/StateMachine/
├── EGState.h/.cpp                       UEGState              — base state object
├── EGBrainState.h/.cpp                  UEGBrainState         — required root decision state
├── EGStateMachineComponent.h/.cpp       UEGStateMachineComponent
├── EGStateMachineTypes.h                FEGStateMachineNetState, enums, delegates
└── Tests/
    └── EGStateMachineTests.cpp          automation coverage (§14)
```

Naming follows the module: `EG` prefix, `UNREALEXTENDEDGAMEPLAY_API`, `#pragma once`, categories under `Extended|State Machine`.

---

## 5. `UEGState` — the state object

```cpp
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UNREALEXTENDEDGAMEPLAY_API UEGState : public UObject
```

`EditInlineNew, DefaultToInstanced` is what makes rule 4 work — the component's `StateDefinitions` array shows each state expanded inline with its own editable properties.

### 5.1 Lifecycle

Seven `BlueprintNativeEvent`s. Two more than TalesOfTrade, four more than DevilOfPlague.

| Event | Fires when | Runs on |
|---|---|---|
| `OnGiven()` | Instance is registered with the component (BeginPlay or runtime `GiveState`) | Authority; clients if mirroring |
| `OnEnter()` | Becomes the active state | Authority; clients if mirroring |
| `OnTick(Delta)` | Every tick step while active (see 5.3) | Authority; clients if mirroring **and** `bTickOnSimulatedProxy` |
| `OnPause()` | Another state is pushed on top of it | Authority; clients if mirroring |
| `OnResume()` | The state above it popped | Authority; clients if mirroring |
| `OnExit()` | Stops being active (switched away, popped, or machine stopped) | Authority; clients if mirroring |
| `OnRemoved()` | Instance is unregistered | Authority; clients if mirroring |

"Mirroring" means the state sets `bRunOnSimulatedProxy = true`. It is **off by default**: an unmodified state runs on the server only, which is what both existing games assume (§2.1, §8.3).

Plus one query:

| Query | Purpose |
|---|---|
| `CanEnterState()` → `bool` | Destination-side veto. Checked by every transition entry point. Default `true`. Name kept from TalesOfTrade so its states reparent unedited. |

**Ordering guarantees** (contractual, covered by tests):

- `OnGiven` precedes any `OnEnter`; `OnRemoved` follows any `OnExit`.
- On switch: `Old.OnExit()` → stack unwound → `New.OnEnter()`.
- On push: `Old.OnPause()` → `New.OnEnter()`.
- On pop: `Top.OnExit()` → `Below.OnResume()`.
- `OnPause`/`OnResume` never pair with `OnExit`/`OnEnter` for the same activation. A paused state is still *entered*.

### 5.2 Transition policy — declared by the state, not the caller

`EditDefaultsOnly` so designers set them per instance:

| Property | Default | Meaning |
|---|---|---|
| `bPushWhenGiven` | `false` | Being given at runtime immediately pushes it (interrupt idiom: give a Stun state → it fires) |
| `bRemoveWhenExited` | `false` | Unregisters itself after exiting (transient one-shot states) |
| `bBlocksInterrupts` | `false` | While active, nothing may force-push over it (invulnerability windows, cinematics) |
| `bRunOnSimulatedProxy` | `false` | Client mirrors run this state's lifecycle locally (cosmetic states). Off = server-only, matching both games' current assumption |
| `bTickOnSimulatedProxy` | `false` | Mirrored states also tick. Requires `bRunOnSimulatedProxy` |
| `TickInterval` | `0.0` | Seconds between `OnTick` calls; `0` = every frame (see 5.3) |
| `StateTag` | empty | Optional `FGameplayTag` identity. The component publishes the active state's tag; games map it to whatever they like |

TalesOfTrade's `bSingleStackEntry` is **deliberately absent** — it becomes structurally impossible to violate (§6.2).

### 5.3 Tick budget

`TickInterval` accumulates delta and calls `OnTick` with the accumulated value, not the frame delta. A brain evaluating twice a second sets `TickInterval = 0.5` instead of hand-rolling a `DecisionTimer` — which both projects currently do, identically, in every brain.

Only the **active** state ticks. Paused states in the stack do not tick. There is no always-on background tier (see §9.3).

### 5.4 State-side API

States drive the machine without touching the component's raw API:

```cpp
// Transitions
void RequestSwitchState(TSubclassOf<UEGState> StateClass, AActor* ContextActor = nullptr);
void RequestPushState(TSubclassOf<UEGState> StateClass, AActor* ContextActor = nullptr);
void RequestPopState();

// Owner access
AActor*        GetOwningActor() const;
APawn*         GetOwningPawn() const;
AController*   GetOwningController() const;
AAIController* GetOwningAIController() const;
UEGStateMachineComponent* GetOwningMachine() const;

// Shared data
AActor* GetContextActor() const;
void    SetContextActor(AActor* Actor);

// Net
bool HasAuthority() const;

// Sibling access — the reason register-once exists (rule 8)
UEGState* GetSiblingState(TSubclassOf<UEGState> StateClass) const;
template<typename T> T* GetSiblingState() const;

// Timers — automatically cleared on exit (see 5.5)
FTimerHandle SetStateTimer(float Delay, FTimerDelegate Callback, bool bLoop = false);
void         ClearStateTimer(FTimerHandle& Handle);

// Debug
void DebugLog(const FString& Message) const;

UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine|Debug")
FString GetStateDebugString() const;   // internal phase, shown in the overlay (see 5.6)

virtual UWorld* GetWorld() const override;   // routed through the component
```

`GetSiblingState` is the parameter-passing channel: a brain configures the next state's properties before pushing it. TalesOfTrade uses exactly this (`RoamState->MaxRoamDuration = ...`), DevilOfPlague cannot because it discards instances.

### 5.5 State-scoped timers

Rule 6 makes states large, and large states are made of timers. Both projects hand-roll them today — `RepathTimer`, `DecisionTimer`, `CalmTimerHandle`, `RevealElapsed`, `ElapsedStunTime` — and each one carries the same latent bug: a timer that fires after the state has exited. `DOPBossState_Passive` only avoids it because someone remembered to clear the calm timer in `ExitState`.

`SetStateTimer` registers the handle with the state; **every handle is cleared automatically in the exit path**, immediately after `OnExit` returns and before the state is marked inactive — so a handle can never fire against an inactive state, whether or not the subclass cleaned up after itself. States that want a timer to survive a pause keep using the world timer manager directly.

### 5.6 `GetStateDebugString()`

A state's *internal phase* is exactly what the stack view cannot show, and with big states as an explicit goal it is the thing you most need to see — `EDOPJokerPhase`, ShopVisit's move → wait → serve → leave, Hunt's repath countdown. Overriding `GetStateDebugString()` appends one line to the world overlay and the gameplay debugger (§10). Default returns empty.

---

## 6. Ownership and instancing

### 6.1 Where instances come from

```cpp
UPROPERTY(EditAnywhere, Instanced, Category = "Extended|State Machine")
TArray<TObjectPtr<UEGState>> StateDefinitions;
```

Two authoring routes, both producing per-actor instances (rule 3):

- **C++:** `StateMachine->AddStateDefinition(CreateDefaultSubobject<UMyBrainState>(TEXT("BrainState")));` in the owner's constructor. Subobjects, so Blueprint children and placed actors inherit and can override the values.
- **Blueprint / level:** add entries to `StateDefinitions` in the details panel and edit their properties inline.

At `BeginPlay` (and `ReadyForReplication`, §8.3) each definition is registered. `GiveState(TSubclassOf<UEGState>)` registers additional ones at runtime by constructing from the class CDO.

### 6.2 The core invariant

> **At most one instance of any given `UEGState` class exists per component, and a class therefore appears at most once in the machine at a time.**

Consequences, all of them load-bearing:

1. **A class reference is a stable network identity for an instance.** That is why replication can send `TSubclassOf<UEGState>` instead of object pointers, and why clients can resolve the referenced instance locally (§8).
2. **Duplicate stack entries are impossible.** The re-push that grows DevilOfPlague's stack without bound today is rejected structurally, not by a flag someone forgot to set.
3. **State memory persists across activations.** Cooldown maps, accumulated timers, cached references survive exit → re-enter. Big states (rule 6) depend on this.
4. **Editor-tuned values are never lost.** Nothing is reconstructed at runtime, so nothing reverts to class defaults.

**Cost:** recursive/nested activation of the same state class is unsupported. Accepted — rule 6 says a state should absorb its sub-phases internally rather than recursing.

### 6.3 Registry

```cpp
TMap<TObjectPtr<UClass>, TObjectPtr<UEGState>> StatesByClass;   // lookup
TArray<TObjectPtr<UEGState>>                   RegisteredStates; // stable order, ownership
```

Duplicate registration of a class is rejected with a warning and returns the existing instance — never silently replaces it.

---

## 7. `UEGStateMachineComponent`

```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGStateMachineComponent : public UActorComponent
```

### 7.1 Runtime control

| API | Notes |
|---|---|
| `Start()` | Authority only. Locates the brain, enters it, enables tick. Fails loudly if no brain (§9.1) |
| `Stop()` | Unwinds the stack top-down calling `OnExit`, disables tick |
| `IsRunning()` | |
| `bAutoStart` | `true` — starts at `BeginPlay` once definitions are registered |
| `MaxStackDepth` | default `5`, clamp 1–16. Pushes beyond it are rejected with a warning |

### 7.2 Transitions

```cpp
bool SwitchState(TSubclassOf<UEGState> StateClass, AActor* ContextActor = nullptr);
bool PushState  (TSubclassOf<UEGState> StateClass, AActor* ContextActor = nullptr);
bool ForcePushState(TSubclassOf<UEGState> StateClass, AActor* ContextActor = nullptr);
void PopState();
void PopToBrain();          // unwind everything above the brain
```

Every entry point runs the same gate, in order:

1. State class registered? → else fail (warn).
2. Already current or in the stack? → else fail (invariant §6.2).
3. `CanEnterState()` → else fail (quiet, this is a normal outcome).
4. Push only: `StateStack.Num() < MaxStackDepth` → else fail (warn).
5. `ForcePushState` only: current state's `bBlocksInterrupts == false` → else fail.

All five are **authority-only**. A client call is a no-op with a warning; clients never decide transitions.

`SwitchState` empties the stack (`OnExit` on each, top-down) before entering. `PushState` pauses the current state and stacks it.

### 7.3 Give / remove (rule 5)

```cpp
UEGState* GiveState(TSubclassOf<UEGState> StateClass);   // register (+ force-push if bPushWhenGiven)
bool      RemoveState(TSubclassOf<UEGState> StateClass); // pop out of the way if needed, then unregister
bool      HasState(TSubclassOf<UEGState> StateClass) const;
UEGState* GetState(TSubclassOf<UEGState> StateClass) const;
```

`RemoveState` on the currently-active state pops it first (so `OnExit` runs) and then unregisters. Removing the brain is refused.

The interrupt idiom — TalesOfTrade's stun — becomes: `GiveState(UMyStunState::StaticClass())` where the state has `bPushWhenGiven = true, bRemoveWhenExited = true`. It fires, runs, pops itself, and disappears. No brain bookkeeping.

### 7.4 Queries and events

```cpp
TSubclassOf<UEGState>        GetCurrentStateClass() const;
UEGState*                    GetCurrentState() const;
TArray<TSubclassOf<UEGState>> GetStateStack() const;
int32                        GetStackDepth() const;
bool                         IsCurrentState(TSubclassOf<UEGState>) const;
FGameplayTag                 GetCurrentStateTag() const;
AActor*                      GetContextActor() const;

UPROPERTY(BlueprintAssignable)
FOnEGStateChanged OnStateChanged;   // (TSubclassOf<UEGState> New, TSubclassOf<UEGState> Old)
```

`OnStateChanged` fires on **both** server and clients — it is the hook animation Blueprints and cosmetic components bind to.

---

## 8. Replication (rule 2)

### 8.1 Authority model

**The server simulates; clients observe, and optionally mirror.** Transitions are decided only on authority.

Every client always gets the replicated struct and the `OnStateChanged` broadcast — that is the cheap path both games already use. A state that additionally sets `bRunOnSimulatedProxy = true` has its lifecycle replayed locally, so cosmetic work (montages, VFX, audio, widget state) can live inside the state that owns it instead of in a pile of `OnRep` handlers on the character.

Mirroring is off by default and that default is load-bearing (§2.1): every state in both games today is written assuming it only ever runs on the server. Enabling mirroring globally would double-fire every `OnEnter` that spawns an actor, applies an effect, or writes gameplay data. Opting in is a per-state, per-author decision.

### 8.2 One atomic replicated struct

```cpp
USTRUCT()
struct FEGStateMachineNetState
{
    UPROPERTY() TSubclassOf<UEGState>         CurrentStateClass;
    UPROPERTY() TArray<TSubclassOf<UEGState>> StackClasses;   // bottom → top, excludes current
    UPROPERTY() TObjectPtr<AActor>            ContextActor;
    UPROPERTY() uint8                         Serial = 0;     // bumped every transition
};

UPROPERTY(ReplicatedUsing = OnRep_NetState)
FEGStateMachineNetState NetState;
```

One property, one `OnRep`. Rationale:

- **Atomicity.** DevilOfPlague replicates `ActiveStateClass`, the stack, and the target as three separate properties; they can arrive in different frames and be observed inconsistent. A single struct cannot tear.
- **`Serial`** makes re-entry into the same state observable (A → B → A still changes the struct), which a bare class pointer cannot express.
- **Class refs, not object refs.** Legal because of the one-instance-per-class invariant (§6.2): the client resolves the class to its own local instance.

### 8.3 Client mirroring algorithm

`OnRep_NetState` always updates the observable state and broadcasts `OnStateChanged`. It then diffs the replicated stack against the local mirror and emits the minimum lifecycle **for the states that opted in**:

1. Resolve every class in `StackClasses` + `CurrentStateClass` to a local instance; `GiveState` any that is not registered yet (constructed from CDO).
2. Pop locally (calling `OnExit`) until the local stack is a prefix of the replicated stack.
3. Push locally (calling `OnPause` on the displaced state, `OnEnter` on the new one) until the stacks match.
4. Enter the new current state; `OnResume` instead of `OnEnter` if it was already paused beneath.

Any state with `bRunOnSimulatedProxy == false` is tracked in the mirror stack but receives **no** lifecycle calls — it is a placeholder that keeps the stack shape correct so its neighbours pause and resume in the right order.

Clients never tick states unless `bTickOnSimulatedProxy` is also set. Component tick on a client is enabled only while at least one mirrored state opts in; otherwise a client-side machine costs one `OnRep` per transition and nothing per frame.

### 8.4 State-internal replication (opt-in)

States are plain `UObject`s and do not replicate by default. A state that needs replicated properties sets `bReplicateState = true`, implements `IsSupportedForNetworking()` and `GetLifetimeReplicatedProps`, and the component registers it as a replicated subobject:

```cpp
bReplicateUsingRegisteredSubObjectList = true;   // component ctor
AddReplicatedSubObject(State);                   // on register, from ReadyForReplication/GiveState
RemoveReplicatedSubObject(State);                // on unregister
```

Registration happens in `ReadyForReplication()` for definition-authored states (before `BeginPlay`) and inline for runtime-given ones.

**Value-fidelity rule, worth knowing before authoring:**

| State origin | Client-side property values |
|---|---|
| From `StateDefinitions` | Identical to the server — both sides construct from the same archetype, including inline editor edits |
| From runtime `GiveState` | Class CDO defaults only |

So per-run data on a runtime-given state must either be replicated (`bReplicateState`) or passed through the replicated `ContextActor`. This is a documented constraint, not a bug.

### 8.5 Relevance

The struct replicates to all connections observing the actor. No condition by default. Games that need it can set `COND_SkipOwner`-style conditions later; not in v1.

---

## 9. `UEGBrainState` (rule 7)

### 9.1 Contract

```cpp
UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGBrainState : public UEGState
```

- The machine **requires exactly one** registered brain. `Start()` logs an error and refuses to run with zero or with two — the ambiguity that silently half-works today is a hard failure instead.
- The brain is the stack floor. `PopState` will not pop it; `PopToBrain` unwinds to it.
- On `OnResume` (a child finished) the brain re-evaluates immediately. This is the event-driven decision loop; the periodic tick is a fallback for when a push was rejected.

### 9.2 What the base class implements, so games stop rewriting it

Today DevilOfPlague has four brains that are the same file with names swapped, and TalesOfTrade has ~12 enemy brains sharing one shape. That shape moves into the base class:

```cpp
UFUNCTION(BlueprintNativeEvent, Category = "Extended|State Machine|Brain")
TSubclassOf<UEGState> EvaluateBrain();     // the ONLY thing a game overrides

UPROPERTY(EditDefaultsOnly) float DecisionInterval = 0.25f;
UPROPERTY(EditDefaultsOnly) bool  bEvaluateOnEnter  = true;
UPROPERTY(EditDefaultsOnly) bool  bEvaluateOnResume = true;
UPROPERTY(EditDefaultsOnly) bool  bSwitchInsteadOfPush = false;  // exclusive vs stacked children
```

The base handles the decision timer, the "already current → skip" check, `CanEnter` re-check, push-vs-switch, and debug logging. A game brain reduces to a function returning a class. Returning `nullptr` means "no change".

`bSwitchInsteadOfPush` covers both existing styles: DevilOfPlague's boss brain is exclusive (`ClearStateStack` + push), the archetype brains are stacked.

### 9.3 No separate persistent tier

DevilOfPlague's component has a `PersistentStates` list that ticks alongside the brain. It has no callers in either project and is not carried over. Always-on behaviour belongs in the brain's own `OnTick`, or in a normal actor component. Adding a second always-ticking tier makes the "who is running right now?" question ambiguous, which is the one question a state machine must answer cleanly.

---

## 10. Debugging

Both projects independently built a ring-buffer debug log; TalesOfTrade also draws it in-world. Merged and extended:

| Feature | Detail |
|---|---|
| Ring log | `DebugLog()` from states; `DebugLogMaxEntries` (default 8), optionally cleared on state change |
| World overlay | `bDebugDraw` → `DrawDebugString` above the owner: current state, stack depth, stack contents, last N log lines. Active/paused colours configurable |
| Sub-phase line | `GetStateDebugString()` from the active state, appended to the overlay — the internal phase of a big state (§5.6) |
| Verbose transitions | `bLogStateChanges` → `LogEGStateMachine` category, including *rejected* transitions and the reason (unregistered / already active / `CanEnter` / depth / blocked). Silent rejection is the hardest failure mode to diagnose in both current implementations |
| Net-side labelling | Overlay marks `[SERVER]` / `[CLIENT]` so mirroring desync is visible at a glance |
| Gameplay Debugger | A `FGameplayDebuggerCategory_EGStateMachine` showing the same data for the selected actor. `UnrealExtendedQuest` already ships a debugger category — follow that pattern |

---

## 11. Blueprint surface

- `UEGState` and `UEGBrainState` are `Blueprintable` — a full state can be authored in Blueprint, including `EvaluateBrain`.
- All lifecycle events are `BlueprintNativeEvent`; C++ states override `_Implementation`, Blueprint states override the event.
- Component transition/query API is `BlueprintCallable` / `BlueprintPure`.
- `OnStateChanged` is `BlueprintAssignable`.
- `StateDefinitions` is `EditAnywhere, Instanced` — authoring is a details-panel task.

---

## 12. Module wiring

`UnrealExtendedGameplay.Build.cs` already carries everything needed — `Core`, `CoreUObject`, `Engine`, `AIModule`, `GameplayTags`, `NavigationSystem`. **No new dependencies.**

`NetCore` is not required: replication uses standard `UPROPERTY` + `GetLifetimeReplicatedProps` and the registered-subobject list, both in `Engine`.

Both projects already have the plugin present (same repo, cloned into each). DevilOfPlague lists `UnrealExtendedGameplay` in `PublicDependencyModuleNames`; PawnStarz will need it added to `TalesOfTrade.Build.cs`.

---

## 13. Migration — reparent, don't replace

Neither game deletes a class. `UDOPState`, `UDOPStateMachineComponent`, `UTOTState` and `UTOTStateMachineComponent` are **reparented onto the plugin classes and kept as thin adapters**: they hold their game-specific accessors, their legacy API as forwarding shims, and nothing else. Every existing state subclass, Blueprint child, placed actor and component reference keeps pointing at a class that still exists.

### 13.1 Resulting class graph

```
UEGState
├── UDOPState                        adapter: legacy Enter/Exit/TickState shims
│   ├── UDOPBossStateBase            unchanged
│   └── ~20 archetype states         unchanged
└── UTOTState                        adapter: NPC/shop/SQL/bark accessors
    └── ~80 NPC + enemy states       unchanged

UEGBrainState
├── UDOPBrainState                   new, thin
│   └── 6 DOP brains                 reparented off UDOPState
└── UTOTBrainState                   new, thin
    └── ~13 TOT brains               reparented off UTOTState

UEGStateMachineComponent
├── UDOPStateMachineComponent        adapter: CombatTarget + legacy API
└── UTOTStateMachineComponent        adapter: TOT-named API
```

**Non-obvious consequence:** brains cannot stay under the game's *state* adapter, because they now need `UEGBrainState` as a parent. Each game gains one extra thin class (`UDOPBrainState`, `UTOTBrainState`) and its brain classes reparent onto that. This is the only structural move either codebase makes.

### 13.2 The five reparent hazards

**1 · Duplicate storage.** A reparented adapter that keeps its own stack, registry or current-state members ends up with two parallel machines, one of them dead. These must be deleted from the child and their accessors re-pointed at the parent:

| Adapter | Delete |
|---|---|
| `UDOPStateMachineComponent` | `ActiveStateClass`, `ReplicatedStateStack`, `RuntimeStateStack`, `BrainState`, `PersistentStates`, `ContextActor`, `DebugLog`, `MaxDebugLogEntries` |
| `UTOTStateMachineComponent` | `RegisteredStates`, `RegisteredStateInstances`, `RegisteredStatesByClass`, `CurrentState`, `CurrentStateClassId`, `StateStack`, `DefaultStateClass`, `bIsRunning`, `StateContextActor`, `StateDefinitions`, `bAutoStart`, `MaxStackDepth`, all debug properties, `DebugLogEntries` |
| `UTOTState` | the four policy flags, `OwningComponent` |
| `UDOPState` | nothing — it has no properties |

`UDOPStateMachineComponent` **keeps** `CombatTarget` and its `DOREPLIFETIME`: that is game data, not machine data, and the plugin has no business owning it.

**2 · Serialized data must survive by name.** UE re-links properties on reparent by name-match, so the parent's names have to be the ones already in the games' assets. This is why §2.1 fixes the plugin's property names to TalesOfTrade's — its `StateDefinitions` arrays are authored in Blueprints and levels, and a rename would silently blank every NPC's state list.

Keep verbatim on `UEGStateMachineComponent`: `StateDefinitions`, `bAutoStart`, `MaxStackDepth`, `bDebugDraw`, `bDebugLogStateChanges`, `DebugActiveColor`, `DebugPausedColor`, `DebugTextOffset`, `DebugLogMaxEntries`, `bClearDebugLogOnStateChange`, and the delegate `OnStateChanged`. Anything genuinely renamed gets a redirect (§13.5).

**3 · Method-name shims (DevilOfPlague only).** TOT's lifecycle names are the plugin's names, so its states reparent with no signature edits at all. DOP's differ — its states override `EnterState_Implementation(UDOPStateMachineComponent*)`, `ExitState_Implementation(...)`, `TickState_Implementation(..., float)`. The adapter keeps that trio declared and forwards:

```cpp
// UDOPState : UEGState
virtual void OnEnter_Implementation()  override { EnterState(GetOwningMachine()); }
virtual void OnExit_Implementation()   override { ExitState (GetOwningMachine()); }
virtual void OnTick_Implementation(float Dt) override { TickState(GetOwningMachine(), Dt); }
```

All ~20 DOP state files then compile untouched, and any Blueprint state overriding `EnterState` keeps working.

**4 · Replication collision (DevilOfPlague only).** The parent replicates `NetState`; the child must stop replicating `ActiveStateClass`, `ReplicatedStateStack` and `ContextActor` or the same data goes over the wire twice and can disagree. `OnActiveStateChanged` stays as a declared delegate but is re-broadcast from a handler bound to the parent's `OnStateChanged`, so existing Blueprint bindings survive.

**5 · One real behaviour change.** The single-entry invariant (§6.2) means DOP's four archetype brains — which currently push on every alternation and never pop — now get their redundant push *rejected*. That is the fix, but it is a behaviour change, so those brains must move to `EvaluateBrain()` in the same commit rather than being left as-is. The boss brain sets `bSwitchInsteadOfPush = true` to preserve its exclusive `ClearStateStack` + push semantics exactly.

### 13.3 DevilOfPlague adapter

| Legacy member | Adapter implementation |
|---|---|
| `SetBrainState(Class)` | `GiveState(Class)` then `Start()`. The adapter sets `bAutoStart = false` so the 6 character classes calling this in `BeginPlay` need **no edit** |
| `PushState(Class, Context)` | `Super::PushState(Class, Context)` |
| `PopState()` / `ClearStateStack()` | `Super::PopState()` / `PopToBrain()` |
| `GetActiveStateClass()` / `GetTopStateClass()` | `GetCurrentStateClass()` |
| `IsTopStateClass(Class)` | `IsCurrentState(Class)` |
| `GetReplicatedStateStack()` | `GetStateStack()` |
| `GetBrainStateClass()` | brain lookup on the parent |
| `HasNetworkAuthority()` | `HasAuthority()` |
| `GetDebugLog()` | parent ring buffer |
| `OnActiveStateChanged` | forwarded from `OnStateChanged` (hazard 4) |
| `RegisterPersistentState()` | removed — no callers in the project, and §9.3 declines the tier |
| `SetCombatTarget` / `GetCombatTarget` | stay on the adapter, unchanged |

Files actually edited: 2 adapters + 1 new `UDOPBrainState` + 6 brains. Everything else is a recompile.

### 13.4 PawnStarzSimulator adapter

| Legacy member | Adapter implementation |
|---|---|
| `RegisterState` / `RegisterInstancedState` | `GiveState` / `AddStateDefinition` |
| `UnregisterState(FName)` | `RemoveState(Class)` — the FName registry key was always the class name |
| `SwitchStateByClass` / `PushStateByClass` / `ForcePushStateByClass` | same names minus `ByClass` |
| `IsCurrentStateClass` | `IsCurrentState` |
| `GetStateByClass` | `GetState` |
| `SetStateContextActor` / `GetStateContextActor` / `ClearStateContextActor` | context actor on the parent |
| `Start` / `Stop` / `IsRunning` / `PopState` / `GetStackDepth` / `GetStateStack` / `SetDefaultStateByClass` | identical names on the parent — no shim needed |
| `IsBrainState()` override | brains reparent to `UTOTBrainState` |
| `bSingleStackEntry` | deleted — structural (§6.2). Set in C++ constructors in all ~30 states, so nothing serialized is lost |
| TOT accessors (`GetNPCCharacter`, `ResolveShopLocation`, `PlayBark`, hit-react immunity, SQL helpers) | stay on `UTOTState`, unchanged |
| `GetCombatTargetActor` / `SetCombatTargetActor` | stay on `UTOTState` — they route to `UTOTEnemyTargetingComponent`, which is game code |
| Per-brain `DecisionTimer` boilerplate | optional cleanup; `DecisionInterval` on `UEGBrainState` supersedes it |

Files actually edited: 2 adapters + 1 new `UTOTBrainState` + ~13 brain reparents. The ~80 state files are untouched.

### 13.5 CoreRedirects

Renamed properties need redirects so designer-authored values survive. They reference game-owned classes, so they belong in each **game's** `DefaultEngine.ini`, not the plugin's:

```ini
[/Script/Engine.Engine]
+PropertyRedirects=(OldName="UTOTState.bForcePushWhenRegistered",        NewName="UEGState.bPushWhenGiven")
+PropertyRedirects=(OldName="UTOTState.bUnregisterWhenExited",           NewName="UEGState.bRemoveWhenExited")
+PropertyRedirects=(OldName="UTOTState.bBlocksForcedPushesWhileActive",  NewName="UEGState.bBlocksInterrupts")
+PropertyRedirects=(OldName="UDOPStateMachineComponent.MaxDebugLogEntries", NewName="UEGStateMachineComponent.DebugLogMaxEntries")
```

Removals (`bSingleStackEntry`, `PersistentStates`) cannot be redirected. Both are C++-only in practice; confirm no Blueprint state sets them before deleting.

---

## 14. Acceptance tests

Automation tests in `AI/StateMachine/Tests/`, following `UnrealExtendedQuest/Source/UnrealExtendedQuest/Tests`:

1. **Registration** — definitions register once; duplicate class registration returns the existing instance; instances are outered to their own component (rule 3).
2. **Invariant** — pushing an already-stacked class is rejected; stack depth is bounded under an A↔B alternation of 1000 iterations (the current DevilOfPlague regression).
3. **Lifecycle order** — switch / push / pop emit exactly the sequences in §5.1, verified by an event-recording test state.
4. **Policy flags** — `CanEnterState` veto; `bBlocksInterrupts` refuses force-push; `bPushWhenGiven` fires on give; `bRemoveWhenExited` unregisters after exit.
5. **Brain** — zero brains refuses to start; two brains refuses to start; `OnResume` triggers re-evaluation; `nullptr` from `EvaluateBrain` is a no-op.
6. **Depth** — pushes past `MaxStackDepth` are rejected and the stack is unchanged.
7. **Replication** — mirrored stacks converge on the authoritative snapshot, a late joiner converges in one pass, and a client transition call is a no-op. Driven by feeding `FEGStateMachineNetState` straight into `ApplyNetStateSnapshot` rather than standing up a net driver: the snapshot *is* the entire contract between the two sides, so this exercises the same mirroring code an `OnRep` would. Note the `Serial` counter guarantees the property replicates across an A→B→A cycle; it does not make the mirror replay the intermediate states, because a mirror tracks state, not events.
8. **Mirroring is opt-in** — a state with `bRunOnSimulatedProxy = false` receives **zero** lifecycle calls on a client while still holding its place in the mirror stack, so its neighbours pause/resume correctly. This is the test that protects both games' existing server-only assumption (§2.1).
9. **State timers** — handles from `SetStateTimer` never fire after `OnExit` returns, including when the state is popped mid-delay.
10. **Teardown** — `Stop()` exits the whole stack top-down; component destruction leaks no instances.

---

## 15. Implementation phases

| Phase | Content | Done when |
|---|---|---|
| 1 ✅ | `UEGState` + `UEGStateMachineComponent`: registration, stack, transitions, lifecycle, state timers. No replication | **Done** — tests 1–4, 6, 9, 10 green |
| 2 ✅ | `UEGBrainState` + brain enforcement | **Done** — test 5 green |
| 3 ✅ | Replication: net struct, opt-in mirroring, subobject opt-in | **Done** — tests 7–8 green |
| 4 ✅ | Debug: ring log, world overlay, `GetStateDebugString`, verbose rejection logging, gameplay debugger category | **Done** — category registered in the module startup |
| 5 ✅ | DevilOfPlague reparent (§13.3) | **Done** — builds; `DevilOfThePlague.AI.StateMachineAdapter` green; no class deleted |
| 6 ⏸ | PawnStarz reparent (§13.4) | **Code complete, not compiled** — see §15.2 |
| 7 ✅ | Plugin README section + this doc marked implemented | **Done** |

**Phases 1–4 add files only.** They touch no existing plugin file, so either project can pull the shared repo at any point mid-development and nothing changes for it — both games keep running their own machines until the reparent phase is deliberately taken. The only commits carrying risk are 5 and 6, and those live in the game repos.

### 15.1 Added during phase 1, beyond the spec above

- **`EEGStateTransitionResult`** — `TrySwitchState` / `TryPushState` return *why* a transition was refused (`NotRegistered`, `AlreadyPresent`, `VetoedByState`, `StackFull`, `BlockedByActiveState`, `NoAuthority`, `Invalid`); `SwitchState` / `PushState` / `ForcePushState` stay `bool` wrappers. Brains that need to react to a refusal, and the verbose log, both read the reason instead of guessing.
- **`SetAutoStart(bool)`** — spawners that configure a machine before `RegisterComponent` need to suppress the automatic start.
- **`IsBrainState()`** already lives on `UEGState` — phase 2's `UEGBrainState` overrides it rather than introducing the concept. `PopToBrain` and the duplicate-brain error path are live now.
- **`UnwindStack` exits paused states.** A switch away from a non-empty stack raises `OnExit` on every paused state instead of dropping them silently, so anything they took on enter (movement locks, immunity counters) is released. TalesOfTrade empties the stack without exiting — a second parity note for §13.2 hazard 5, though in practice its switches happen from an empty stack.

### 15.2 Phase 6 is blocked on two things outside the code

The TalesOfTrade side is written — adapters, `UTOTBrainState`, 13 reparented brains, the policy-flag
removal across 31 files, the module and plugin dependency, the CoreRedirects — but it has not been
compiled, because:

1. **The two projects hold separate checkouts of this repo.** PawnStarz's copy lives at
   `Plugins/UnrealExtendedFramework/` and does not contain `AI/StateMachine/` yet. The plugin work
   has to be committed here and pulled there before TalesOfTrade can even see `UEGState`.
2. **PawnStarz targets UE 5.6** (`EngineAssociation` 5.6, `BuildSettingsVersion.V5`,
   `IncludeOrderVersion.Unreal5_6`), while this plugin repo targets 5.8. Building its editor target
   against the 5.8 install is refused by UBT before compilation, over shared-build-environment
   warning levels — a pre-existing condition, unrelated to the state machine.

So phase 6 finishes as: commit and push the plugin, pull in PawnStarz, then build
`TalesOfTradeEditor` with the 5.6 toolchain (also installed) — or migrate that project to 5.8 first.

### 15.3 Two traps found while reparenting, worth knowing for the third project

Both are the same shape and neither is a compile error, which is what makes them dangerous:

- **`TSubclassOf<TGameState>` silently resolves to null for brains.** A brain derives from
  `UEGBrainState`, not from the game's state adapter, so any handle typed to the adapter — a
  `DefaultBrainState` property, a local holding `GetCurrentStateClass()`, a delegate parameter —
  compiles fine and reads back null the moment the brain is active. Type these to `UEGState`.
  Found in 6 DevilOfPlague properties, its state-changed delegate, and 4 TalesOfTrade locals that
  feed save data and debug names.
- **`PopToBrain` versus a brain that re-evaluates on resume.** Unwinding one `PopState` at a time
  resumes the brain between pops, and a brain that decides on resume immediately re-pushes what was
  just removed — an unwind that never terminates. The fix is to unwind in one pass and resume the
  brain once at the end; there is a regression test for it in the brain suite.

**Parity is the bar for phases 5–6, not improvement.** The reparent is done when behaviour is indistinguishable from before, with the single documented exception in §13.2 hazard 5. Cleanups that the new API enables (collapsing brain boilerplate, adopting `DecisionInterval`, `StateTag`, mirroring) come afterwards, in separate commits, so a regression is attributable.

---

## 16. Decisions taken

The three forks left open in the first draft are resolved:

1. **Module → `UnrealExtendedGameplay`.** Both projects clone the whole plugin repo and project plugins are enabled by default, so PawnStarz already has it — the only cost is adding `UnrealExtendedGameplay` to `TalesOfTrade.Build.cs`. `AI/`, `Systems/Targeting` and `Systems/LineOfSight` already live there; `UnrealExtendedFramework` stays the lean foundation.
2. **Client mirroring → full lifecycle, opt-in per state, default off** (§8.3). Forced by the no-break constraint: every state in both games is written assuming authority-only execution, so mirroring-by-default would double-fire any `OnEnter` that spawns, applies or writes. `OnStateChanged` still reaches every client for free.
3. **Brain → separate class** (`UEGBrainState`). A flag would keep the roster at two classes, but the decision loop would stay copy-pasted in every game — which is the thing rule 7 exists to eliminate.

## 17. Known follow-up, out of scope here

The state machine alone will not make DevilOfPlague's archetypes visibly work: nothing in that project sets a combat target. `UDOPTargetingComponent` is an unwired stub, and the plugin's existing `EGTargetingComponent` / `EGDynamicTargetingComponent` are **player lock-on** systems (widget, sticky rotation, gamepad switching), not AI acquisition.

The missing piece is an AI targeting component — aggro range, threat table, LOS acquire/forget with delay, vertical cutoff — and PawnStarz already has a working one in `UTOTEnemyTargetingComponent`. Same story as this document: one good implementation, currently living in one game. Worth scheduling into the shared plugin directly after phase 6, under `Systems/Targeting/`.

# EG Interaction — Design Specification

**Status:** implemented. Plugin + DevilOfPlague are done and green (6 automation tests). The
PawnStarz reparent is written and UHT-clean but **not compiled** — that project does not currently
build, for reasons predating this work (§12.1).
**Target module:** `UnrealExtendedGameplay` (runtime)
**Target path:** `Source/UnrealExtendedGameplay/Systems/Interaction/`
**Engine:** UE 5.8 (must also compile on 5.6 — see §8)
**Author:** Kemal Erdem YILMAZ

**Scope rule for this document:** the plugin ships what the two games already have, merged. Where they
conflict, one of them wins and §3 says which. The short list of things that are genuinely new is §9 —
if a line is not in §9, it already exists in one of the two codebases today.

---

## 1. Why this exists

| | DevilOfPlague (`Source/DevilOfThePlague/Interaction`) | PawnStarz (`Plugins/TalesOfTrade/.../Interaction`) |
|---|---|---|
| Execution | **Server-authoritative** — 3 server RPCs, 1 client RPC, full re-validation | Local only. `Interact()` runs wherever the input landed |
| Hold | **On the interactable** — per-actor duration, exclusive holder, multicast state, clients extrapolate | On the component — one duration for the player, local, no exclusivity |
| Modes | none | **Priority stack**, passed into every interface call |
| Blocking | ghost check hardcoded in the component | **ASC tag events** + target `BlockedTags` |
| Prompt data | one call → `{Text, Icon}` | two calls + `"(Press E)"` formatted in the component |
| Base actor | no components; ability grants, inventory gate, quest event, stencil outline | mesh + box; prompt text/icon/`bIsInteractable` |
| Debug | one `bDebugDraw` | colours, 3 verbosity levels, range sphere, focus readout |
| Content | 11 `ADOPInteractableActor` subclasses + `ADOPItem` + player | 6 direct implementers + 7 via `ATOTInteractableActor` |

DevilOfPlague has the network and hold model. TalesOfTrade has the context model. Same situation as the
state machine, so the same answer and the same migration pattern — see `EGStateMachine.md`.

---

## 2. What the plugin ships

The same five files both games already have, once:

```
Source/UnrealExtendedGameplay/Systems/Interaction/
├── EGInteractionTypes.h              presentation, context, mode-stack types, delegates
├── EGInteractableInterface.h/.cpp    IEGInteractableInterface
├── EGInteractionComponent.h/.cpp     UEGInteractionComponent
├── EGInteractableActor.h/.cpp        AEGInteractableActor
├── UI/EGInteractionPromptWidget.h/.cpp
└── Tests/
    ├── EGInteractionTestTypes.h/.cpp   test interactable + a derived interface (hazard 1)
    └── EGInteractionTests.cpp          the six tests in §11
```

**No GAS dependency, and no second module.** Both interact abilities stay in their games — they are ~50
lines each and they differ (DevilOfPlague's holds the ability open for the key press, TalesOfTrade's
fires and ends). Ability *granting* on the interactable is a DevilOfPlague-only feature and stays on
its adapter. So `UnrealExtendedGameplay.Build.cs` needs **no new dependency**.

---

## 3. The five conflicts, resolved

| Conflict | Winner | Why |
|---|---|---|
| Execution | **DevilOfPlague** — server RPCs + re-validation on authority | The reason for doing this at all |
| Hold owner | **DevilOfPlague** — the interactable | A duration is a property of the door, not the hand; exclusivity is only expressible where the lock is |
| Prompt data | **DevilOfPlague** — one call returning `{Text, Icon}` | One override instead of two |
| Base actor components | **DevilOfPlague** — the base owns none | Subclasses build their own hierarchy; TalesOfTrade's mesh+box move to its adapter, so its content is unaffected |
| Everything else | **TalesOfTrade** — modes, tag gating, filters, grace period, debug suite, mouse trace | DevilOfPlague has no equivalent to lose |

---

## 4. `IEGInteractableInterface`

**DevilOfPlague's signatures, verbatim.** The draft of this document put the mode and the hit on
every call, TalesOfTrade-style. Implementation rejected that: a game's own interface inherits this
one (§10), and a UFUNCTION cannot be redeclared with a different signature in a derived interface —
the plugin's parameter list *is* every implementer's parameter list. Adopting DevilOfPlague's costs
zero edits across its 13 implementers; adopting TalesOfTrade's would have cost 13 there and 13 here,
because its `ETOTInteractionMode` had to become a tag regardless.

```cpp
bool CanInteract(AActor* Interactor) const;
bool Interact   (AActor* Interactor);            // authority only

void InteractionStart(AActor* Interactor);
void InteractionTick (AActor* Interactor, float DeltaTime);
void InteractionEnd  (AActor* Interactor);

FEGInteractionPresentation GetInteractionPresentation(AActor* Interactor, const FHitResult& Hit) const;
void FocusStateChanged(bool bIsFocused, const FHitResult& Hit);
```

The mode and the hit are still reachable — they are per-call state on the component rather than
parameters, which is what lets the signature stay short:

```cpp
const FGameplayTag Mode = UEGInteractionComponent::GetInteractionMode(Interactor);
const FHitResult   Hit  = UEGInteractionComponent::GetInteractionHit(Interactor);
```

The server publishes the validated client hit into `LastHitResult` before dispatching, so
`GetInteractionHit` answers correctly on both sides.

**Call the `Execute_` form, never the bare name.** `IEGInteractableInterface::CanInteract(...)` is a
generated assert-only stub — the bare call compiles and then fires
`check(0 && "Do not directly call Event functions in Interfaces")` at runtime. DevilOfPlague's
committed `ADOPInteractableActor::InteractionStart_Implementation` had exactly this bug on three
call sites; the hold test caught it on the first run.

```cpp
USTRUCT(BlueprintType)
struct FEGInteractionPresentation      // DevilOfPlague's struct, unchanged
{
    UPROPERTY(BlueprintReadWrite) FText                  Text;
    UPROPERTY(BlueprintReadWrite) TObjectPtr<UTexture2D> Icon = nullptr;
};
```

`FEGInteractionContext` (`Interactor`, `Target`, `HitResult`, `Mode`) exists in both games already and
carries over as-is for the success delegate.

---

## 5. `UEGInteractionComponent`

Carries over unchanged from whichever game has it:

- **Trace** — line first, sphere sweep as fallback; channel or object type; complex opt-in;
  `InteractionRange`, `TraceRadius`, `TraceTickInterval`, `LostFocusGracePeriod`; mouse-cursor mode
  with the prompt following the cursor.
- **Filters** — `bIgnoreOwnerAndInstigator`, `AdditionalIgnoredActors`, target `BlockedTags` via
  `IGameplayTagAssetInterface`, facing cone, line of sight, `CanInteract`.
- **Candidate priority** — `Nearest` / `MostCentered`. TalesOfTrade's third value, `HighestPriority`,
  is a stub that falls back to nearest and is dropped.
- **Focus** — enter/exit notification, refresh when the hit component or the prompt text changes,
  grace period against jittery aim.
- **Mode stack** — `PushInteractionState` / `PushToolInteractionState` / `PushBlockingModalInteractionState`
  / `RemoveInteractionState` / `RemoveInteractionStatesForSource`, the same priority bands
  (baseline 0, tool 100, world 200, blocking modal 1000), weak-pointer source purge, highest priority wins.
- **Blocking** — `SetInteractionBlocked(bool)` plus a `CanOwnerInteract()` virtual. The ASC tag binding
  itself stays in the games (§6).
- **Prompt widget** — `PromptWidgetClass`, `Show/HidePromptWidget`, `SetPromptSuppressed`, created
  behind a local-controller guard. `CreatePromptWidget` / `DestroyPromptWidget` are virtual so
  TalesOfTrade's adapter can keep pushing onto `ATOTHUD`.
- **Delegates** — `OnFocusChanged`, `OnFocusLost`, `OnInteractionSucceeded`, `OnInteractionFailed`,
  `OnHoldProgressChanged`.
- **Server surface** — `ServerInteractionStart`, `ServerInteractionEnd`, `ServerTryInteract`,
  `ClientInteractionResult`, and `IsServerInteractionValid` exactly as DevilOfPlague has it: interface
  check, owner gate, hit-actor match, distance with slack, facing/LOS, `CanInteract`.
- **Debug** — TalesOfTrade's whole suite: hit/miss/blocked colours, `Basic`/`Detailed`/`Verbose`,
  range sphere, world and on-screen focus readout.

Ticks on the locally-controlled owner (trace, focus, prompt) and on authority (hold routing) —
DevilOfPlague's rule.

---

## 6. `AEGInteractableActor`

DevilOfPlague's actor, minus the game-specific parts:

- `ActivationMode` (`Instant` / `Hold`) and `HoldDuration`.
- Exclusive holder: `CanInteract` refuses everyone else while a hold runs; a stale holder never keeps
  the lock forever.
- Hold accumulates on the server from `InteractionTick`; `MulticastHoldStateChanged` lets clients
  extrapolate `GetHoldProgress()` from the hold start time.
- Hold clears *before* `Interact` runs, so a still-pressed input cannot re-trigger.
- `MulticastInteracted` → `OnInteracted` + `BP_OnInteracted` on every machine.
- Focus highlight via custom-depth stencil (`bHighlightOnFocus`, `HighlightStencilValue`).
- TalesOfTrade's `PromptText` / `PromptIcon` / `bIsInteractable`, feeding the default
  `GetInteractionPresentation` and `CanInteract`.
- Owns no components and no root.

**Stays in DevilOfPlague's adapter:** `InteractionAbilities` and the whole grant/revoke lifecycle,
`RequiredItemTag`, `InteractionEventTag`. **Stays in TalesOfTrade's adapter:** the mesh and box
components.

---

## 7. Prompt widget

`UEGInteractionPromptWidget` = both widgets merged: `ShowPrompt(Text, Icon)`, `HidePrompt()`,
`SetHoldProgress(float)` (`BlueprintNativeEvent`, so a radial material and a progress bar both work),
`SetCursorFollowMode(bool)` with the cursor offset.

One fix on the way in: **the component calls `SetHoldProgress` itself.** In DevilOfPlague nothing
bridges `GetHoldProgress()` to the widget, so the radial fill only animates if a Blueprint drives it.

---

## 8. Module wiring

No new dependencies — `GameplayTags`, `UMG` and `Slate` are already in
`UnrealExtendedGameplay.Build.cs`. DevilOfPlague already depends on the module; `TalesOfTrade.Build.cs`
needs it added.

**The 5.6 constraint is real.** PawnStarz is on UE 5.6 and DevilOfPlague on 5.8, and the plugin repo is
cloned separately into each project, so this reaches PawnStarz only through a commit/pull round trip and
must compile on both. That is what left `EGStateMachine.md` phase 6 uncompiled.

---

## 9. What is genuinely new

Five items. Everything else in this document exists today in one of the two games. Each of these is
here because moving the code into a shared plugin forces it, not because it seemed like a good idea:

| # | Change | Forced by |
|---|---|---|
| 1 | Modes are `FGameplayTag`, not `ETOTInteractionMode` | A shared plugin cannot enumerate a game's modes. TalesOfTrade keeps its enum and maps at the adapter (§10, hazard 3) |
| 2 | A blocking stack entry uses an explicit flag instead of the `None` mode value | Falls out of #1 — with tags there is no `None` to overload |
| 3 | The ghost check becomes a `CanOwnerInteract()` virtual | `UDOPHealthManagerComponent` cannot be referenced from the plugin. DevilOfPlague overrides it; behaviour identical |
| 4 | ASC tag blocking becomes `SetInteractionBlocked(bool)` on the component | Keeps GAS out of the module. TalesOfTrade's existing `RegisterGameplayTagEvent` binding moves to its adapter and calls the setter |
| 5 | `"(Press E) {0}"` leaves the component | A shared component cannot know a game's keybind. TalesOfTrade's widget adds it back (§10, hazard 4) |
| 6 | `GetInteractionMode()` / `GetInteractionHit()` statics on the component | Added during implementation. The interface signature had to be DevilOfPlague's (§4), so the mode and the hit needed somewhere else to live. TalesOfTrade's implementers read them from here instead of from parameters |

Plus two bug fixes — §7's unwired hold progress, and the `Execute_` assert in §4 that the hold test
caught — and one deletion (`HighestPriority`, a stub).

**Known limitations carried over as-is, deliberately not fixed here:** hold state is a multicast, so a
client that joins or becomes relevant mid-hold sees no progress until it ends; and hold elapsed time is
accumulated on the server while clients extrapolate from a start time, so the two can drift slightly
under heavy hitching. Both are pre-existing, both are cosmetic, and fixing either is a separate change.

---

## 10. Migration — reparent, don't replace

No class is deleted in either game. Each game's interface **inherits** the plugin interface and provides
default `_Implementation`s that forward to its legacy signatures; components and actors reparent.

```
IEGInteractableInterface
├── IDOPInteractableInterface     legacy 1-param defaults
└── ITOTInteractableInterface     legacy prompt+icon defaults, enum↔tag

UEGInteractionComponent
├── UDOPInteractionComponent      ghost gate, legacy method names
└── UTOTInteractionComponent      HUD prompt routing, ASC tag binding, enum-typed mode API

AEGInteractableActor
├── ADOPInteractableActor         ability grants, RequiredItemTag, InteractionEventTag
└── ATOTInteractableActor         mesh + box components
```

### 10.1 Four hazards

**1 · Interface inheritance dispatch — RESOLVED, it works.** The no-edit plan rested on
`Execute_CanInteract` for the *parent* interface resolving on a class that declares only the *child*
interface. All three links in that chain walk the interface inheritance chain via `IsChildOf`:
`UClass::ImplementsInterface` (Class.cpp), the `FindFunction` lookup (the generated name constant is
the plain function name, `"CanInteract"`), and the `GetNativeInterfaceAddress` fallback
(UObjectBaseUtility.cpp). Test 1 (§11) proves it end to end on a class that implements only a derived
interface. The fallback plan is not needed.

One constraint fell out of it, and it is the reason §4 uses DevilOfPlague's signatures: a derived
interface **cannot redeclare** an inherited UFUNCTION with a different signature, and a class cannot
implement two interfaces that both declare a UFUNCTION of the same name.

**2 · Serialized values survive by name.** Reparenting re-links by name, so the plugin uses
TalesOfTrade's names where the two disagree (`InteractionRange`, `TraceRadius`) — its values are
authored in Blueprints, DevilOfPlague's are set in C++. `TraceDistance` and `SphereRadius` get
`PropertyRedirects` in DevilOfPlague's `DefaultEngine.ini`.

**3 · `ETOTInteractionMode` is not deleted.** It is `BlueprintType` and threaded through ~40 files.
The adapter keeps it, keeps `GetActiveInteractionState()` returning it, and maps enum ↔ tag in one
table at its own boundary (`UTOTInteractionComponent::ResolveInteractionMode`). `None` maps to
pushing a blocking entry. What its 13 implementers *did* lose is the parameter list: their
`CanInteract`/`Interact` overrides now take `(AActor*)` and read the mode and the hit from the
component (§4). `GetInteractionPrompt`/`GetInteractionIcon` keep their three-parameter signatures —
the plugin has no functions by those names, so nothing collides, and
`ITOTInteractableInterface::GetInteractionPresentation_Implementation` packs the pair.

**4 · `"(Press E)"` must reappear in TalesOfTrade's widget** in the same commit, or every prompt in
that game silently loses its keybind hint.

### 10.2 Duplicate storage to delete from the adapters

A reparented adapter that keeps its own focus, hold or mode members ends up running two systems, one
dead. Delete from `UDOPInteractionComponent` and `UTOTInteractionComponent`: focus actor, prompt text
and icon, last hit, widget instance, trace accumulators, grace-period state, hold state, mode-stack
entries, and all trace/filter/RPC helpers. Delete from `ADOPInteractableActor`: `ActiveInteractor`,
hold members and both multicasts. `ATOTInteractableActor` has no state to delete.

### 10.3 One behaviour change

**PawnStarz interaction becomes server-authoritative.** Its 13 implementers were written for a
single-player game, so every `Interact_Implementation` needs an audit for client-only assumptions
(widget pushes, local subsystem calls, `GetFirstPlayerController`) before that phase ships. This is the
riskiest line in the document — single-player hides this class of bug perfectly.

Everything else is parity.

---

## 11. Acceptance tests

Parity suite in `Systems/Interaction/Tests/`, all six green under
`UnrealExtendedGameplay.Interaction.*`:

1. **InterfaceInheritance** — a class implementing only a derived game interface is found by
   `ImplementsInterface` and dispatched by `Execute_*` through the base. Hazard 1; gates the rest.
2. **Focus** — enter/exit fires once per transition, an unchanged re-trace does not re-fire, and a
   grace period holds focus through a missed trace.
3. **ModeStack** — highest priority wins, ties go to the last push, removal falls back correctly, a
   blocking entry clears focus and refuses re-acquisition, removal by source works.
4. **Hold** — completes at `HoldDuration`; a non-holder can neither advance nor claim it; the hold is
   cleared *before* `Interact` runs; a still-pressed input does not re-trigger; instant interactables
   complete on the press.
5. **ServerValidation** — each rejection path (`CanInteract` false, out of range, gated owner) runs
   no interactable code, and unblocking restores interaction.
6. **Teardown** — losing focus ends the running hold without completing it, and `EndPlay` releases
   both the component's active interaction and the interactable's hold.

---

## 12. Phases

| Phase | Content | Status |
|---|---|---|
| 1 | Hazard-1 spike, then types + interface + component (trace, focus, filters, prompt, debug) | ✅ Tests 1, 2 green |
| 2 | Mode stack + gating | ✅ Test 3 green |
| 3 | `AEGInteractableActor` + RPCs + validation | ✅ Tests 4–6 green |
| 4 | DevilOfPlague reparent | ✅ `DevilOfThePlagueEditor` builds; all 6 tests green; no class deleted |
| 5 | PawnStarz reparent | ⏸ Written and UHT-clean, **not compiled** — see §12.1 |

Phases 1–3 added files only and changed nothing for either project until its reparent phase was
taken.

### 12.1 Why phase 5 is not verified

`TalesOfTradeEditor` **does not build today, and did not before this work**. UnrealHeaderTool fails
on 11 errors left over from the state machine migration's own phase 6, which `EGStateMachine.md`
§15.2 records as "code complete, not compiled":

- `DecisionInterval` shadows `UEGBrainState::DecisionInterval` in 10 brains (BlowUp, Boar, Imp, Lich,
  Melee, Ranged, RockGolem, SkeletonCrossbow, SkeletonGrunt, SkeletonMage).
- `TickInterval` shadows `UEGState::TickInterval` in `UTOTState_SkeletonCrossbowArrowRainAttack`.
  That one is a genuine name clash, not a duplicate: the state means "seconds between arrow-rain
  damage ticks", the parent means "seconds between `OnTick` calls". It needs renaming, not deleting.

None of these are interaction. After the interaction changes landed, UHT reports **exactly** those 11
and nothing else, which is as far as this side can be verified without finishing the other migration:
every interaction header in TalesOfTrade is UHT-clean, and no interaction C++ has been compiled.

Finishing phase 5 means: clear those 11, build, then work through whatever the state machine phase 6
left at the C++ layer, and only then judge the §10.3 authority audit against a running game.

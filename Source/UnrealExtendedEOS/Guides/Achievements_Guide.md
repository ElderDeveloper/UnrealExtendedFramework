# Achievements Blueprint Guide

This guide shows how to use `EEOSAchievementSubsystem` to define, track, unlock, and display achievements — all from Blueprints.

---

## Architecture Overview

```
EOS DevPortal                    Your Game (Blueprints)
┌───────────────────┐            ┌──────────────────────────────┐
│ Achievement Defs  │◀──────────▶│ EEOSAchievementSubsystem     │
│ - FirstBlood      │   Query    │   QueryAchievements()        │
│ - DragonSlayer    │   Unlock   │   UnlockAchievement()        │
│ - Collector100    │   Progress │   SetAchievementProgress()   │
│ - Speedrunner     │            │   IncrementAchievementProgress│
└───────────────────┘            └──────────────────────────────┘
```

---

## Step 1: Create Achievements in EOS DevPortal

In the [EOS DevPortal](https://dev.epicgames.com) → Game Services → Achievements:

| Achievement ID | Display Name | How It Unlocks |
|---|---|---|
| `first_blood` | First Blood | Kill first enemy |
| `dragon_slayer` | Dragon Slayer | Defeat the final boss |
| `collector_100` | Master Collector | Collect 100 items (progress-based) |
| `speedrunner` | Speedrunner | Complete a level in under 2 minutes |
| `shopaholic` | Shopaholic | Spend 10,000 gold total |

> **Tip:** Set a `StatThreshold` on the stat linked to the achievement for auto-unlock. For example, link `collector_100` to a stat `ItemsCollected` with threshold `100` — the achievement unlocks automatically when the stat reaches 100.

---

## Step 2: Query Achievements on Login

Load all achievements and their progress when the player logs in:

```
[Login Complete / OnConnectLoginComplete]
        │
        ▼
┌────────────────────────────────┐
│ Get EEOSAchievementSubsystem   │
│ (Get Game Instance Subsystem)  │
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ QueryAchievements()            │
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ Bind: OnAchievementsQueried    │
│   bSuccess, Achievements array │
│   → Cache locally              │
│   → Populate UI if open        │
└────────────────────────────────┘
```

**Blueprint nodes:**
1. `Get Game Instance Subsystem` → class: `EEOSAchievementSubsystem`
2. Call `QueryAchievements()`
3. Bind to `OnAchievementsQueried` → receives `TArray<FEEOSAchievement>`

---

## Step 3: Unlock a One-Shot Achievement

For achievements that unlock on a single event (no progress bar):

```
[Player kills first enemy]
        │
        ▼
┌────────────────────────────────┐
│ IsAchievementUnlocked          │
│   ("first_blood")              │
└──────────────┬─────────────────┘
        ┌──────┴──────┐
    Already          Not yet
    unlocked           │
    → skip             ▼
               ┌────────────────────────────────┐
               │ UnlockAchievement("first_blood")│
               └──────────────┬─────────────────┘
                              │
                              ▼
               ┌────────────────────────────────┐
               │ Bind: OnAchievementUnlocked     │
               │   → Show popup notification     │
               │   → Play unlock sound/VFX       │
               └────────────────────────────────┘
```

**Blueprint nodes:**
1. `IsAchievementUnlocked("first_blood")` → returns `bool`
2. If `false` → `UnlockAchievement("first_blood")`
3. On `OnAchievementUnlocked` → show toast notification

---

## Step 4: Incremental Progress Achievement

For achievements that track progress (e.g., "collect 100 items"):

```
[Player picks up an item]
        │
        ▼
┌────────────────────────────────┐
│ IncrementAchievementProgress   │
│   AchievementId: "collector_100"│
│   IncrementAmount: 0.01        │  ← 1/100 = 0.01
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ Bind: OnAchievementProgressUpdated│
│   AchievementId, Progress, bUnlocked│
│   → Update progress bar widget │
│   → If bUnlocked: show popup   │
└────────────────────────────────┘
```

**Progress is 0.0 to 1.0:**
- Collect 1 of 100 items → `IncrementAmount = 0.01`
- Kill 10 of 50 enemies → `IncrementAmount = 0.02` per kill
- Spend 500 of 10000 gold → `SetAchievementProgress("shopaholic", 0.05)`

**Alternative: Use SetAchievementProgress directly:**
```
[Player's total items collected = 47]
        │
        ▼
┌────────────────────────────────┐
│ SetAchievementProgress         │
│   AchievementId: "collector_100"│
│   Progress: 0.47               │  ← 47/100
└────────────────────────────────┘
```

---

## Step 5: Auto-Unlock via Stats (Recommended)

The cleanest approach: let EOS automatically unlock achievements based on stat thresholds.

### Setup in DevPortal:
Link achievement `collector_100` to stat `ItemsCollected` with threshold `100`.

### In Blueprint:
```
[Player picks up an item]
        │
        ▼
┌────────────────────────────────┐
│ Get EEOSStatsSubsystem         │
│ IngestStat("ItemsCollected", 1)│  ← just update the stat
└────────────────────────────────┘
        │
        ▼
   EOS auto-unlocks "collector_100"
   when ItemsCollected reaches 100
        │
        ▼
┌────────────────────────────────┐
│ OnAchievementUnlocked fires    │
│   → Show popup notification    │
└────────────────────────────────┘
```

> **This is the recommended approach** because stats are more reliable than manual progress tracking. You just `IngestStat` and EOS handles the rest.

---

## Step 6: Display Achievement UI

### Show all achievements in a menu:

```
[Open Achievements Screen]
        │
        ▼
┌────────────────────────────────┐
│ GetAchievements()              │  ← returns cached array
│   → For Each achievement:      │
│     ├── Name, Description      │
│     ├── Progress (0.0 - 1.0)   │
│     ├── bUnlocked              │
│     └── UnlockTime             │
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ For each FEEOSAchievement:     │
│  ┌─────────────────────────┐   │
│  │ 🏆 Dragon Slayer        │   │
│  │ "Defeat the final boss" │   │
│  │ ████████████░░░ 80%     │   │
│  │ Unlocked: Not yet       │   │
│  └─────────────────────────┘   │
└────────────────────────────────┘
```

### Show summary stats:

```
┌────────────────────────────────┐
│ GetUnlockedCount() → 7         │
│ GetTotalCount()    → 15        │
│ GetOverallProgress() → 0.47   │
│                                │
│ Display: "7 / 15 (47%)"       │
└────────────────────────────────┘
```

---

## Step 7: Achievement Unlock Notification (Global)

Bind `OnAchievementUnlocked` once at game start so every unlock shows a popup:

```
[BeginPlay — GameInstance or PlayerController]
        │
        ▼
┌────────────────────────────────┐
│ Bind: OnAchievementUnlocked    │
│   → Create popup widget        │
│   → Set achievement name/icon  │
│   → Play animation + sound     │
│   → Auto-dismiss after 3 sec   │
└────────────────────────────────┘
```

---

## Quick Reference: Blueprint Nodes

| Node | What it does |
|------|-------------|
| `QueryAchievements()` | Fetch all achievements + progress from EOS |
| `UnlockAchievement(Id)` | Instantly complete an achievement |
| `SetAchievementProgress(Id, 0.0–1.0)` | Set absolute progress |
| `IncrementAchievementProgress(Id, Amount)` | Add to current progress |
| `ResetAchievement(Id)` | Clear progress (dev/testing) |
| `GetAchievements()` | Get cached array of all achievements |
| `GetAchievementById(Id)` | Get one specific achievement |
| `IsAchievementUnlocked(Id)` | Quick bool check |
| `GetUnlockedCount()` | Number of completed achievements |
| `GetTotalCount()` | Total achievement count |
| `GetOverallProgress()` | Overall completion 0.0–1.0 |

## Delegates

| Delegate | When it fires |
|----------|--------------|
| `OnAchievementsQueried` | After `QueryAchievements()` completes |
| `OnAchievementUnlocked` | When an achievement is successfully unlocked |
| `OnAchievementProgressUpdated` | When progress changes (includes `bUnlocked` flag) |

---

## Gating Game Content by Achievement (Blueprint)

The core idea: **query achievements on login → cache which are unlocked → gate content based on that cache.**

### Pattern 1: Lock a Level / Map

```
[Player clicks "Volcano Dungeon" on world map]
        │
        ▼
┌────────────────────────────────┐
│ IsAchievementUnlocked          │
│   ("dragon_slayer")            │
└──────────────┬─────────────────┘
        ┌──────┴──────┐
      true           false
        │              │
        ▼              ▼
  Open Level       Show locked popup:
  "VolcanoDungeon" "Defeat the Dragon
                    to unlock this area"
```

**Blueprint nodes:**
1. `IsAchievementUnlocked("dragon_slayer")` → `Branch`
2. **True** → `Open Level` or `Enable travel`
3. **False** → Show locked UI with hint text

---

### Pattern 2: Lock a Character / Skin

```
[Character Select Screen — For Each character]
        │
        ▼
┌────────────────────────────────┐
│ Switch on Character.RequiredAchievement│
│                                │
│ "none"       → Always available│
│ "speedrunner" → Check unlock   │
│ "collector_100" → Check unlock │
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ IsAchievementUnlocked(Required)│
│   true  → Show character       │
│           "Select" button      │
│   false → Greyed out + lock 🔒│
│           "Complete X to unlock"│
└────────────────────────────────┘
```

**Data setup:** Store a `RequiredAchievement` string on each character DataAsset/DataTable:

| Character | RequiredAchievement | Hint |
|---|---|---|
| Knight | *(none)* | Always available |
| Shadow Assassin | `first_blood` | "Get your first kill" |
| Dragon Rider | `dragon_slayer` | "Defeat the final boss" |
| Collector | `collector_100` | "Collect 100 items" |

---

### Pattern 3: Lock an Ability / Weapon

```
[Player opens Ability Tree or Weapon Rack]
        │
        ▼
┌────────────────────────────────┐
│ For Each ability in tree:      │
│   GetAchievementById(RequiredId│
│     , OutAchievement)          │
│                                │
│   If OutAchievement.bUnlocked: │
│     → Show as available        │
│     → Allow equip              │
│   Else:                        │
│     → Show progress bar        │
│     → "47/100 items collected" │
│     → Disable equip button     │
└────────────────────────────────┘
```

This pattern uses `GetAchievementById` to also show **progress toward unlocking** — the player can see how close they are.

---

### Pattern 4: Gate at Runtime (Gameplay)

For abilities/features that need checking during gameplay:

```
[Player tries to use "Double Jump"]
        │
        ▼
┌────────────────────────────────┐
│ IsAchievementUnlocked          │
│   ("reach_lvl50")              │
└──────────────┬─────────────────┘
        ┌──────┴──────┐
      true           false
        │              │
        ▼              ▼
  Execute          Ignore input
  Double Jump      (optional: show
                    "Reach Lvl 50
                     to unlock")
```

> **Performance tip:** Don't call `IsAchievementUnlocked` every frame. Cache the result on `BeginPlay` or after `OnAchievementsQueried`, store it in a local `bool`, and check that instead.

---

### Pattern 5: Full Content Gate Flow (Recommended)

The best practice — load everything on login and cache in a single place:

```
[Login Complete]
        │
        ▼
┌────────────────────────────────┐
│ QueryAchievements()            │
└──────────────┬─────────────────┘
               │
               ▼
┌────────────────────────────────┐
│ OnAchievementsQueried          │
│   → Store in GameInstance var: │
│     Map<String, bool>          │
│     UnlockedContent             │
│                                │
│   For Each achievement:        │
│     UnlockedContent.Add(       │
│       AchievementId,           │
│       Achievement.bUnlocked)   │
└──────────────┬─────────────────┘
               │
               ▼
  Now ANY Blueprint can check:
  GameInstance → UnlockedContent
    → Find("dragon_slayer") → true/false
```

**Why this is best:**
- Single query on login, no repeated server calls
- Any widget, actor, or system can check the map
- Updates live when `OnAchievementUnlocked` fires

---

### Pattern 6: Live-Unlock During Gameplay

When the player earns an achievement mid-session, immediately enable the content:

```
[Bind once on GameInstance BeginPlay]
        │
        ▼
┌────────────────────────────────┐
│ Bind: OnAchievementUnlocked    │
│   AchievementId                │
│                                │
│   → UnlockedContent.Add(       │
│       AchievementId, true)     │
│   → Show toast notification    │
│   → Broadcast custom event:    │
│       "OnContentUnlocked"      │
│       (pass AchievementId)     │
└────────────────────────────────┘
        │
        ▼
[Any widget listening to "OnContentUnlocked"]
   → Refresh its lock/unlock state
   → e.g., level select removes the lock icon
```

---

## Quick Reference: Content Gating Nodes

| Node | Use for |
|------|---------|
| `IsAchievementUnlocked(Id)` | Simple yes/no gate check |
| `GetAchievementById(Id)` | Gate + show progress toward unlock |
| `GetAchievements()` | Build full unlock map on login |
| `OnAchievementUnlocked` | Live-enable content mid-gameplay |
| `GetOverallProgress()` | Show "47% game completion" |
| `GetUnlockedCount() / GetTotalCount()` | Show "7 / 15 achievements" |

---

## Common Achievement Types

### "Kill X enemies" type:
```
IngestStat("EnemiesKilled", 1)  ← stat auto-unlocks linked achievement
```

### "Complete the game" type:
```
UnlockAchievement("game_complete")  ← one-shot unlock
```

### "Reach level 50" type:
```
SetAchievementProgress("reach_lvl50", CurrentLevel / 50.0)
```

### "Secret / hidden" type:
Same as above — hide it in the UI until unlocked by checking `bUnlocked`.

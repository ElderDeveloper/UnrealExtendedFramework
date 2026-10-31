# Unreal Extended Save

Save games that work the same in every project: named slots with metadata for menus, saves that
survive a crash mid-write, versioned migrations for old saves, saveable actors and objects,
autosave, and cloud sync through the foundation's platform layer.

Depends only on **Unreal Extended Foundation**.

## Setup

1. Enable **Unreal Extended Save** (it enables the foundation).
2. Open **Project Settings → Extended Framework → Extended Save** if you want autosave, cloud
   upload or a different backup policy. The defaults work as they are.

## Making things saveable

| What | How |
|:--|:--|
| An actor | Add the **Saveable** component. Tick **SaveGame** on the variables to keep (Blueprint variable → Advanced), or `UPROPERTY(SaveGame)` in C++. The transform is saved by default. |
| The actor's components | Their SaveGame variables are saved too (`bSaveComponents`). |
| A placed actor that gets destroyed (pickup, enemy) | Stays destroyed after loading (`bRememberDestruction`). |
| An actor spawned at run time | Spawned again on load, then restored. Its class must have a Saveable component. |
| A manager, quest log, inventory subsystem | Implement **Saveable** (`IESaveable`), return a fixed id from **Get Save Id**, call **Register Saveable**. |
| Flags and counters that belong to nothing | **Set Save Int / Float / Bool / String** and the matching getters. |

**On Before Save** and **On After Load** (on the interface and the component) let objects copy
runtime state into SaveGame variables and rebuild from them.

Values are stored as text per property name. Removing or renaming a variable never breaks a
save: unknown entries are skipped and new variables keep their defaults. Structs are saved whole
(their fields need no SaveGame flag). Object references survive only for objects with a stable
path (assets, placed actors).

## Saving and loading

```cpp
UESaveSubsystem* Save = UESaveSubsystem::Get(this);
Save->SetSlotExtra(TEXT("Difficulty"), TEXT("Hard"));
Save->SaveSlot(TEXT("Slot1"), [](const FEFResult& Result) { ... }, TEXT("Chapter 3 - The Docks"));
Save->LoadSlot(TEXT("Slot1"), EESaveLoadMode::OpenSavedLevel);
```

Blueprint: **Save Game To Slot (Extended)**, **Load Game From Slot (Extended)**, **Get Save
Slots** (for a load menu, newest first, without loading them), **Delete Save Slot**, **Start
New Save Game**.

- **Apply To Current World** restores what exists now and respawns saved run-time actors.
- **Open Saved Level** opens the level the save was made in. Actors restore themselves as they
  begin play, and run-time actors are spawned once the level has loaded.
- Saves and loads run one at a time, in the order they were asked for.
- The state of a streaming level that unloads is kept in memory, so it comes back when the level
  streams in again and is part of the next save (`bKeepStateOfUnloadedLevels`).

## Crash-safe saves

With `bKeepBackup` (on by default) each save is written over the *older* of two files, so the
last good save is never touched while a new one is written. Loading falls back to the other copy
when the newest cannot be read. The load callback and the **Extended.Save.Loaded** message then
report **Restored From Backup**.

## Versions and migrations

Every save records `CurrentSchemaVersion`. When the saved data changes in a way old saves can't
load as they are, bump the version and add a **UESaveMigration** subclass (Blueprint or C++)
whose **From Version** is the old version. Loading runs every step up to the current version.
`RenameSavedProperty` handles renamed variables. A save from a newer build is refused with
`NewerVersion` rather than half-loaded.

## Autosave

- `AutosaveIntervalSeconds` saves to `AutosaveSlotName` on a timer while a game world runs.
- **Request Autosave** saves now (checkpoints, level exits).
- **Set Autosave Blocked** (Reason, true/false) blocks autosaves during cutscenes, combat or
  menus. Each reason is on or off (blocking it twice still takes one unblock); autosave resumes
  when no reason is left.

## Cloud

With `bUploadToCloud`, every saved slot is uploaded through the platform layer's cloud storage
(Steam, EOS, or Local for testing). **Sync Save Slot From Cloud** downloads a slot and keeps
whichever copy wins under `CloudConflictPolicy` (newest, local or cloud). Steam Auto-Cloud can do
the same without code if you prefer it.

## Messages and status

- Message bus: **Extended.Save.Saving** (before capture), **Extended.Save.Saved** and
  **Extended.Save.Loaded**, all with an `FESaveMessage` payload.
- `Extended.Status` shows the active slot, the slot count, live saveables, queued requests and
  autosave blockers.

## Tests

Run `Automation RunTests UnrealExtendedSave`. The tests run offline and cover:

- round trip of actor, component, object and value state;
- removal of actors spawned after the save;
- fallback to the backup when the newest copy is damaged;
- migration of a renamed property;
- refusal of saves from a newer build;
- slot-name rules.

## Conventions

This plugin follows the repository's [ARCHITECTURE.md](../ARCHITECTURE.md).

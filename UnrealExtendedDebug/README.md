# Unreal Extended Debug

Debug tools that work in **packaged development builds**, where QA actually tests, not only in
the editor:

- cheats you can run from the console or a menu, including on the server for the player who asked;
- an in-game debug menu;
- on-screen watches;
- live views of `Extended.Status`, message bus traffic and analytics events.

Shipping builds compile all of it out (`EXTENDED_DEBUG_ENABLED` is 0). The module and its
Blueprint nodes still exist there, but they do nothing, so Blueprints that call them keep loading.

Depends only on **Unreal Extended Foundation**.

## Setup

Enable **Unreal Extended Debug**. That's all; the defaults are in **Project Settings → Extended
Framework → Extended Debug**:

| Setting | Default | |
|:--|:--|:--|
| `bEnabled` | on | Turns the tools off without removing the plugin, e.g. for a press build. |
| `MenuKey` | F10 | Opens and closes the menu. |
| `bGamepadChord` | on | Clicking both thumbsticks opens the menu (consoles, Steam Deck). |
| `bAllowRemoteCheats` | on | Clients may run server cheats on a development server. |
| `bShowWatches`, `WatchTimeoutSeconds` | on, 10 s | Draws the watch overlay; stale watches disappear. |

## Cheats

```cpp
// From a module's StartupModule: every game instance gets it.
FEDbgCheatInfo Info;
Info.Name = TEXT("GiveGold");
Info.Help = TEXT("Adds gold to the player");
Info.Category = TEXT("Economy");
Info.ArgumentHint = TEXT("<amount>");
Info.bRunOnServer = true;
UEDbgSubsystem::RegisterGlobalCheat(Info, [](const FEDbgCheatContext& Context)
{
    // Context.Player, Context.World, Context.GetInt(0, 100) ...
});
```

In Blueprint, call **Register Cheat** with a custom event, for example from the game mode's
BeginPlay. The cheat goes away when the object that owns the event is destroyed.

- Run cheats from the console with `Extended.Cheat GiveGold 500`; `Extended.Cheat.List` lists them.
- They also appear in the menu, grouped by category, with an argument box.
- **Run On Server** cheats travel from a client to the server through `UEDbgCheatComponent`. The
  plugin adds that component to every player controller when the player logs in. Register these
  cheats on the server too.
- Every cheat that runs records a `debug_cheat` analytics event, so playtest data from sessions
  that used cheats can be filtered out.

**Built in** (category Extended):

| Cheat | Argument | What it does |
|:--|:--|:--|
| `TimeDilation` | scale | Sets global time dilation; runs on the server. |
| `ShowStatus` | none | Writes `Extended.Status` to the log. |
| `UnlockAchievement` | id | Unlocks an achievement through the platform layer, by the game's id. |
| `AnalyticsConsent` | Granted, Denied or Unknown | Sets analytics consent. |
| `ToggleWatches` | none | Shows or hides the watch overlay. |

## Debug menu

Open it with F10, both thumbsticks or `Extended.Debug.Menu`. It has these tabs:

- **Cheats**
- **Status**: every Extended service, refreshed live.
- **Messages**: the latest message bus traffic.
- **Analytics**: the session, consent and the latest events.
- **Watches**

While the menu is open, the mouse cursor is shown and input goes to both the game and the menu.
Closing it (Escape, the gamepad's B button, or Close) restores what it changed.

## Watches

```cpp
EDBG_WATCH(this, TEXT("Speed"), TEXT("%.0f cm/s"), GetVelocity().Size());   // nothing in Shipping
```

In Blueprint, use **Set Debug Watch** (key, value) and **Clear Debug Watch**. Watches are drawn
at the top right. Call them every frame for live values; a watch nobody updates disappears after
`WatchTimeoutSeconds`. `Extended.Debug.Watches 0/1` hides or shows the overlay.

## Tests

Run `Automation RunTests UnrealExtendedDebug` (offline): cheat registration and arguments,
owned and global cheats, and watches.

## Conventions

This plugin follows the repository's [ARCHITECTURE.md](../ARCHITECTURE.md).

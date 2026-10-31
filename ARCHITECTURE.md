# Architecture and conventions

How the Extended plugins fit together, and the rules every plugin follows. New plugins start from the
template (`python3 Scripts/new_extended_plugin.py <Name> --prefix <Prefix>`), which already follows
all of this.

## Layers

```text
UnrealExtendedFoundation        engine modules only, no plugin dependencies
  ├─ UnrealExtendedFrameworkLog   EF_LOG category log files
  └─ ExtendedFoundation           HTTP, JSON, results, async nodes, settings, credentials, status, tests
        ▲
        │  every Extended plugin may depend on the foundation
        │
UnrealExtendedFramework (core)  settings system, libraries, AI/EQS, subtitles, loading screen, UI Lab
        ▲
        │  only plugins that need the core's features depend on it
        │
Gameplay · PlayFab · EOS        service and gameplay plugins
GAS · Quest · Steam · PerfSentinel · Atlassian   depend on the foundation only (or nothing)
```

- **The foundation stays light.** It may depend only on modules every engine install has without a
  plugin (Core, CoreUObject, Engine, DeveloperSettings, HTTP, Json, JsonUtilities, Projects). Anything
  tied to gameplay, UI, AI, audio or a specific online service belongs in the plugin that needs it.
- **The core is not a dependency dump.** Do not add a module to the core's Build.cs because a
  dependent plugin needs it; the dependent declares its own.
- A plugin that uses a module from another plugin lists that plugin in its `.uplugin`.
- `ExtendedFoundation` loads at `PostConfigInit`, the earliest phase any Extended module uses, so
  every module can derive from its classes. A module must never load earlier than a module it
  depends on.

## What the foundation gives you

| Need | Use | Header |
|:--|:--|:--|
| HTTP call with retries, timeouts, cancellation, safe logging | `FEFHttpOperation::Start`, `UEFHttpSubsystem`, **Http Request** node | `Http/EFHttpClient.h`, `Http/EFHttpSubsystem.h` |
| Offline HTTP in tests or demos | `FEFScopedHttpMock` | `Http/EFHttpMock.h` |
| JSON read/write/path lookup/struct conversion | `UEFJsonLibrary`, `EFJson::` | `Json/EFJsonLibrary.h` |
| A success/failure every plugin understands | `FEFResult`, `EFResultCodes` | `Result/EFResult.h` |
| An async Blueprint node | `UEFAsyncActionBase` | `Async/EFAsyncActionBase.h` |
| Answering each caller with its own result | `TEFPendingCalls`, `EFCallContext` | `Async/EFPendingCalls.h`, `Async/EFCallContext.h` |
| A Project Settings page | `UEFDeveloperSettings` | `Settings/EFDeveloperSettings.h` |
| Storing a developer's token or password | `FEFCredentialStore` | `Credentials/EFCredentialStore.h` |
| Loading soft references without hitches | `EFAssets::Preload`, `EFAssets::Resolve` | `Assets/EFAssetLoading.h` |
| Finding a subsystem from any object | `EF::GetGameInstanceSubsystem<T>(Context)` | `Subsystems/EFSubsystems.h` |
| Reporting state to `Extended.Status` (console and `Extended.Status.Overlay 1`) | `FEFServiceStatusRegistry::Register` | `Status/EFServiceStatus.h` |
| Test game instance, waiting, fixtures | `EFTest::FScopedGameInstance`, `FEFWaitUntilCommand` | `Testing/EFTestHelpers.h` |
| Compile-time foundation version check | `EF_REQUIRE_FOUNDATION_VERSION(1, 0, "Plugin")` | `EFFoundationVersion.h` |

## Naming

| Prefix | Plugin |
|:--|:--|
| `EF` | Foundation and core framework |
| `EG` | Gameplay and Quest |
| `EGAS` | GAS extensions |
| `EPF` | PlayFab |
| `EEOS` | EOS |
| `ESteam` | Steam |
| `ExtendedAtlassian` | Atlassian |
| `PerfSentinel` | Perf Sentinel |

Plugins are named `UnrealExtended<Name>`; their main runtime module is `Extended<Name>` (older
plugins keep the plugin name as the module name). Each source file starts with the plugin's
copyright line; third-party and vendored code keeps its original notice.

## Modules and Build.cs

- `Public/` holds headers other modules may include; everything else goes in `Private/`.
- Public dependencies are exactly the modules whose types appear in public headers. Everything used
  only in `.cpp` files is private.
- Editor-only code lives in an `Editor` or `UncookedOnly` module, never behind `WITH_EDITOR` in a
  runtime module's dependency list.
- Each plugin's module `.cpp` declares the foundation version it needs with
  `EF_REQUIRE_FOUNDATION_VERSION`.

## Asynchronous APIs

- **Every asynchronous call answers its own caller exactly once.** Give the call a completion
  callback (`TFunction` or a delegate parameter) and track it in `TEFPendingCalls`. Multicast
  delegates are fine for *listeners*, in addition to the per-call callback, never instead of it.
- Where an existing subsystem only reports through a shared multicast delegate, propagate
  `EFCallContext`: capture `EFCallContext::GetActive()` when the work starts, and wrap the broadcast
  in `EFCallContext::FScope`. Nodes then start their call in `CallWithContext` and ignore other
  nodes' results with `IsResultForThisCall()`.
- **Async Blueprint nodes derive from `UEFAsyncActionBase`** (directly or through the plugin's
  base, e.g. `UEPFAsyncActionBase`): factory stores inputs and calls `InitAsyncAction`, work starts
  in `ExecuteAction`, results go through `FinishAction()` (which returns false if the node already
  finished), and `CancelWork` stops the work. Cancellation, timeouts and failures found before the
  pins are bound come from the base.
- A node that already exposes its own Blueprint `Cancel` with parameters (for example
  `Cancel(BlendOutTime)`) cannot move onto `UEFAsyncActionBase` in place: the engine base declares a
  parameterless `Cancel`, and existing call nodes would lose their pins. Add a new node on the base
  and mark the old one `DeprecatedFunction` instead.
- Assets a node or component needs are preloaded with `EFAssets::Preload` when it starts and read
  with `EFAssets::Resolve` (blocking fallback, reported) or `EFAssets::TryResolve` (never blocks).
- Report outcomes as `FEFResult` or convert to it (`ToEFResult()`), so UI and retry logic work the
  same for every service.

## Settings and secrets

- Settings classes derive from `UEFDeveloperSettings` and appear under
  **Project Settings → Extended Framework**.
- `Default*.ini` files ship inside packaged builds. Never store secrets there. Mark properties that
  could hold one with `meta = (EFSecret)`; the editor then warns when one is saved to a shipped file.
- Developer tokens and passwords go in `FEFCredentialStore` (per user, encrypted with DPAPI on
  Windows and the Keychain on macOS). Secrets a shipped game needs at runtime belong on a server.
- Logs never contain tokens, keys or passwords. `FEFHttpOperation` already strips query strings,
  fragments and user info from the URLs it logs.

## Compatibility

- Renaming or moving a `UCLASS`, `USTRUCT`, `UENUM` or `UFUNCTION` needs a redirect in the owning
  plugin's `Config/Default<Plugin>.ini` `[CoreRedirects]` section, so Blueprints and assets keep
  loading.
- Keep Blueprint pin names and types stable. Replace a node's behaviour behind the same pins, or add
  a new node and mark the old one `DeprecatedFunction` with a `DeprecationMessage`.
- Bump `EF_FOUNDATION_VERSION_MINOR` (and the foundation `.uplugin` `VersionName`) when adding
  foundation API, `MAJOR` when removing or changing it.

## Tests and checks

- Automation tests live in each module's `Private/Tests` (or the plugin's test module), run offline,
  and use `FEFScopedHttpMock` instead of real services. Test names start with the plugin or module
  name, so `Automation RunTests ExtendedFoundation` or `Automation RunTests UnrealExtendedPlayFab`
  runs one plugin.
- `python3 Scripts/lint_repository.py` checks descriptors, cross-plugin and cross-module
  dependencies, copyright headers and redirect syntax without Unreal. CI runs it on every push.
- Building and running the automation tests needs Unreal Engine, so it runs on a developer machine
  or a self-hosted runner:

  ```text
  UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests ExtendedFoundation+UnrealExtended;Quit" -unattended -nop4 -NullRHI -TestExit="Automation Test Queue Empty"
  ```

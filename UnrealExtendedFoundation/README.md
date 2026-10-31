# Unreal Extended Foundation

The shared base every Extended plugin builds on. It depends on no other plugin, only on engine modules
every install has, so even the standalone plugins (Steam, Quest, PerfSentinel, Atlassian) can use it.

It also holds the contracts several plugins meet at: a message bus every plugin can publish to, an
analytics layer every back end can feed, and a store-neutral platform layer that Steam, EOS and
PlayFab implement. Game code calls the contract once; the plugin for each store does the rest.

## Modules

| Module | What it holds |
|:--|:--|
| `ExtendedFoundation` | HTTP, JSON, results, async nodes, settings base, credentials, status, message bus, analytics, platform layer, test helpers |
| `UnrealExtendedFrameworkLog` | `EF_LOG` category log files, the Extended Log settings and viewer feed |

Add `ExtendedFoundation` (and `UnrealExtendedFrameworkLog` if you log with `EF_LOG`) to your module's
dependencies and `UnrealExtendedFoundation` to your plugin's `.uplugin`.

## At a glance

```cpp
// HTTP with retries, against a base URL set once per game instance.
UEFHttpSubsystem* Http = EF::GetGameInstanceSubsystem<UEFHttpSubsystem>(this);
FEFHttpRequest Request = UEFHttpLibrary::MakeHttpRequest(TEXT("leaderboards/top"));
Request.MaxRetries = 2;
Http->Send(Request, [](const FEFHttpResponse& Response)
{
	const FEFResult Result = FEFResult::FromHttp(Response, TEXT("MyService"));
	FEFJsonObject Body;
	if (Result.IsSuccess() && UEFHttpLibrary::GetResponseJson(Response, Body)) { /* ... */ }
});

// The same code, offline, in an automation test.
FEFScopedHttpMock Mock;
Mock->AddResponse(EEFHttpVerb::Get, TEXT("leaderboards/top"), 200, TEXT("{\"entries\":[]}"));

// A developer's token, never written to the project.
FEFCredentialStore::Save(TEXT("MyPlugin"), Account, Token);

// One message, any number of listeners that never reference the sender.
UEFMessageSubsystem::Get(this)->BroadcastMessage(TAG_Game_EnemyKilled, FEnemyKilled{ Enemy, Weapon });
Handle = UEFMessageSubsystem::Get(this)->RegisterListener<FEnemyKilled>(TAG_Game_EnemyKilled, this, &UMyTracker::OnKill);

// Analytics to every configured sink (log, JSON-lines file, HTTP endpoint, PlayFab), with consent.
UEFAnalyticsSubsystem::Get(this)->RecordEvent(TEXT("level_completed"), { { TEXT("level"), LevelName } });

// Achievements, stats, leaderboards, cloud files and presence on whichever store the build runs on.
UEFPlatformSubsystem::Get(this)->UnlockAchievement(TEXT("FirstBlood"));
UEFPlatformSubsystem::Get(this)->SubmitScore(TEXT("HighScore"), Score, [](const FEFResult& Result) { /* ... */ });
```

## Message bus

`UEFMessageSubsystem` (one per game instance) sends any USTRUCT on a gameplay-tag channel.

- Listeners pick **Exact** or **Partial** match: `Game.Combat` hears `Game.Combat.EnemyKilled`.
- A listener can filter by payload type.
- A listener bound to an object is removed when that object dies.
- Listeners may send messages and add or remove listeners while they run.

Blueprint: **Broadcast Message** (payload from **Make Instanced Struct**) and the **Listen For
Message** async node (read the payload with **Get Instanced Struct Value**).

`Extended.Messages.Log 1` logs every message, and the debug plugin shows the latest ones.
Messages stay inside the game instance; they are not replicated.

## Analytics

`UEFAnalyticsSubsystem` records events once and delivers them in batches to every sink. Settings are
under **Project Settings → Extended Framework → Extended Analytics**.

**What every event carries:** a session id, a sequence number, a UTC timestamp, the user id, and
global attributes. Build, config, platform and engine are added automatically.

**Consent:** `bRequireConsent` holds every event in memory until **Set Analytics Consent**
(Granted). Denied deletes what waited and what sinks saved. The decision is remembered.

**Sinks:**
- **Log**
- **File:** JSON lines per session under `Saved/Analytics`, for playtests.
- **Http:** POSTs `{"events": [...]}` to your endpoint. It retries, and saves batches it could
  not send for the next run. Deduplicate on `sessionId` + `sequence`.
- Any `IEFAnalyticsSink` another plugin adds. PlayFab does this.
- A sink that isn't ready yet (not signed in) keeps its events until it is.

**Message bus forwarding:** channels listed in `ForwardedMessageChannels` become events
automatically. The payload's fields become attributes.

## Platform layer

`UEFPlatformSubsystem` routes each feature to one provider: achievements, stats, leaderboards,
per-user cloud files, and presence. Settings are under **Project Settings → Extended Framework →
Extended Platform**.

**Providers:**
- Steam, EOS and PlayFab come from their plugins.
- **Local** is built in. It keeps everything in `Saved/ExtendedPlatform`, for the editor, for
  builds sold without a store, and for offline tests.

**Choosing a provider:**
- `ProviderOrder` picks the first provider that can serve the feature now.
- `FeatureProviders` overrides a single feature, for example leaderboards on PlayFab and
  everything else on Steam.
- Local only takes a feature that no enabled store supports. In the editor it also takes
  features whose store isn't running (`bLocalFallbackInEditor`).

**Ids:** a **Platform Catalog** data asset maps the game's ids (`FirstBlood`) to each store's ids.

**Writes while no store is ready:** achievement, stat and score writes made before a store can
take them (before sign-in) are queued. They are sent once a provider can take them.

**Async nodes:** Unlock Achievement, Set Achievement Progress, Update Stat, Submit Score, Query
Achievements / Stats / Leaderboard, and Read / Write / Delete / List Cloud File. Every call answers
once, never before it returns, with an `FEFResult`.

**Messages:** unlocks and submitted scores are announced as `Extended.Platform.AchievementUnlocked`
and `Extended.Platform.ScoreSubmitted`.

**Adding a store:** subclass `UEFPlatformProviderSubsystem`. It registers itself; see the header
for the rules.

| Header | Use it for |
|:--|:--|
| `Http/EFHttpClient.h` | `FEFHttpOperation`: one request with retries, timeouts, cancellation; transport override |
| `Http/EFHttpSubsystem.h` | Base URL, default headers, cancellable handles per game instance |
| `Http/EFAsyncHttpRequest.h` | The **Http Request** Blueprint node |
| `Http/EFHttpMock.h` | `FEFScopedHttpMock`: canned answers and request recording |
| `Json/EFJsonLibrary.h` | JSON read/write, path lookup, struct conversion, files |
| `Result/EFResult.h` | `FEFResult`, `EFResultCodes` |
| `Async/EFAsyncActionBase.h` | Base class for async Blueprint nodes |
| `Async/EFPendingCalls.h` | Per-call completion callbacks in a subsystem |
| `Async/EFCallContext.h` | Telling results apart on delegates every caller shares |
| `Settings/EFDeveloperSettings.h` | Base for Project Settings pages; `EFSecret` warnings |
| `Credentials/EFCredentialStore.h` | Per-user secrets (DPAPI / Keychain) |
| `Subsystems/EFSubsystems.h` | Subsystem lookup from any world context |
| `Status/EFServiceStatus.h` | Reporting to the `Extended.Status` console command |
| `Messages/EFMessageSubsystem.h`, `Messages/EFMessageLibrary.h` | Message bus, Broadcast Message, Listen For Message |
| `Analytics/EFAnalyticsSubsystem.h`, `Analytics/EFAnalyticsTypes.h` | Analytics events, consent, `IEFAnalyticsSink` |
| `Platform/EFPlatformSubsystem.h`, `Platform/EFPlatformProvider.h` | Store-neutral platform calls; base class for providers |
| `Platform/EFPlatformAsyncActions.h`, `Platform/EFPlatformCatalog.h` | Platform Blueprint nodes; id mapping |
| `Testing/EFTestHelpers.h` | Test game instance, wait-until latent command, fixtures |
| `EFFoundationVersion.h` | `EF_REQUIRE_FOUNDATION_VERSION` compile-time check |

The rules for using these across the suite are in [ARCHITECTURE.md](../ARCHITECTURE.md).

## Tests

`Automation RunTests ExtendedFoundation` (all offline). They cover:

- message routing and re-entrancy;
- analytics consent, batching, forwarding, the HTTP sink against the mock, and persistence of
  unsent batches;
- the Local provider end to end, id mapping, and the write queue.

# Unreal Extended Steam

Steamworks client and game server subsystems, the Extended Steam online subsystem, Steam sockets,
the Steam Web API and publishing tools, for Win64, Linux and macOS. The
[repository README](../README.md) has the full overview.

## Platform layer

`UESteamPlatformProvider` (provider name `Steam`, in the `UnrealExtendedSteam` module) serves the
foundation's store-neutral platform layer, `UEFPlatformSubsystem`, on top of this plugin's own
subsystems. It exists in every game instance and registers itself; pick providers per feature in
**Project Settings → Extended Framework → Extended Platform** (Steam is first in the default order).
Ids reach it already mapped through the platform catalog, so give the catalog your Steamworks API
names.

| Platform call | Steam |
|:--|:--|
| `UnlockAchievement` | `SetAchievement`, then `StoreStats`; answers when Steam reports the upload. Already unlocked succeeds at once. |
| `SetAchievementProgress` | Below 1: `IndicateAchievementProgress` (the overlay toast), scaled to the achievement's progress-stat limits, else to 100. At 1: unlock. |
| `QueryAchievements` | Every achievement of the app: unlocked state, unlock time and display name. |
| `UpdateStats` | `GetStatInt` / `SetStatInt` (Increment adds to the current value), then `StoreStats`. |
| `QueryStats` | `GetStatInt`. An empty id list reads the stats listed in the platform catalog. |
| `SubmitScore` | `FindLeaderboard` (handle cached per name), then `UploadLeaderboardScore`, keep-best. |
| `QueryLeaderboard` | `DownloadLeaderboardEntries`: Top → Global `1..Count`, AroundUser → GlobalAroundUser centred on the user, Friends → Friends trimmed to `Count`. |
| `WriteCloudFile` / `ReadCloudFile` | `FileWriteAsync` / `FileReadAsync` (Steam Cloud). |
| `DeleteCloudFile` / `ListCloudFiles` | `FileDelete` / `GetFileCount`, `GetFileNameAndSize`, `GetFileTimestamp`. |
| `SetPresence` | `SetRichPresence`: Status → `status`, each property → its own key. |

Caveats:

- **Availability.** Features are available while the Steam client API is initialized and the user is
  logged on to Steam; cloud storage needs only the client, since Steam Cloud keeps working offline
  and syncs later. Built without the Steamworks SDK, or on a dedicated server, the provider supports
  no feature, so another provider (such as Local) takes over. With the SDK present but Steam not
  running, Steam keeps its features and the platform layer queues writes for it.
- **Stats are 32-bit INT stats.** Values and scores outside `int32` are clamped, with a warning.
  Float and AVGRATE stats stay on `UESteamUserStatsSubsystem`.
- **Achievement progress is not stored by Steam.** Progress below 1 only shows the toast; persist it
  in the stat the achievement is bound to in Steamworks. `QueryAchievements` reports progress 0 or 1.
- **Uploads are confirmed together.** Steam reports `StoreStats` without a request id, so the next
  `UserStatsStored` answers every unlock and stat update waiting for one.
- **No call hangs.** A call without an answer from Steam fails with `TimedOut` after 30 seconds
  (`UESteamPlatformProvider::CallTimeoutSeconds`); a timed-out upload stays in the Steam client and
  goes up with the next `StoreStats`. Pending calls fail with `NotAvailable` when the Steam client
  shuts down and with `Cancelled` when the game instance ends.
- **Leaderboards must exist in Steamworks**; the provider never creates one, and the board's sort
  method decides which score is kept. Friends results carry global ranks. Names of players who are
  not friends can be empty the first time; the provider asks Steam for them for later queries.
- **Steam Cloud** lower-cases file names, cannot store an empty file (`InvalidArgument`) and uses the
  quota set for the app in Steamworks.
- **Rich presence** takes at most 30 keys (keys up to 64, values up to 256 characters). `status`
  shows in Steam's game info view; for the friends list, pass `steam_display` with a token from your
  rich presence localization file in the properties.
- **Steamworks SDKs before 1.61** load stats on request: the provider calls `RequestCurrentStats`
  itself and holds achievement and stat calls until the stats arrive.

`Automation RunTests UnrealExtendedSteam.Platform` checks the provider offline.

## Conventions

This plugin follows the repository's [ARCHITECTURE.md](../ARCHITECTURE.md).

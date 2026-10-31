# Unreal Extended PlayFab

PlayFab client integration: one game instance subsystem per PlayFab area (auth, player data, stats,
leaderboards, inventory, analytics, ...) with matching async Blueprint nodes. Settings live under
**Project Settings → Extended Framework → Extended PlayFab** (`UEPFSettings`).

## Platform layer

`UEPFPlatformProvider` registers PlayFab as the `PlayFab` provider of the foundation's platform layer
(`UEFPlatformSubsystem`), so game code that calls `UpdateStat`, `SubmitScore` or `QueryLeaderboard`
reaches PlayFab when the Extended Platform settings route those features to it.

| Platform call | PlayFab |
|:--|:--|
| `UpdateStats` | `UEPFStatsSubsystem::UpdateStats` → `/Statistic/UpdateStatistics` |
| `QueryStats` | `UEPFStatsSubsystem::GetStats` (`GetAllStats` for no ids) → `/Statistic/GetStatistics` |
| `SubmitScore` | updates the statistic named by the leaderboard's provider id |
| `QueryLeaderboard` Top / AroundUser / Friends | `UEPFLeaderboardSubsystem::GetLeaderboard` / `GetLeaderboardAroundPlayer` / `GetFriendsLeaderboard` |
| `GetLocalUser` | PlayFabId and display name from the last login |
| Achievements, cloud storage, presence | not served: `SupportsFeature` is false, so the Local provider can take them |

- Available while the player is logged in with an entity token (Progression APIs are entity APIs).
- Statistics are 32-bit: values are clamped. `Set` and `Increment` both report the value and PlayFab
  applies the statistic's aggregation, so a stat updated with `Increment` must use **Sum** in Game
  Manager. In one batch, repeated increments of a stat are added together first.
- Leaderboard entries' `UserId` is the `title_player_account` entity id, which is what PlayFab
  leaderboards report; `GetLocalUser` reports the PlayFabId.
- PlayFab's `*NotFound` errors arrive as `NotFound`, a missing session as `NotSignedIn`. A call PlayFab
  has not answered within **Platform Call Timeout** (30 s) answers `TimedOut`.
- Cloud storage stays off because `UEPFEntityFilesSubsystem` stores entity *objects* (small JSON
  values), not entity files.
- Turn the provider off with **Enable Platform Provider**.

## Analytics sink

With **Send Analytics Events** on (the default) and a Title ID set, `UEPFAnalyticsSinkSubsystem` adds
a `PlayFab` sink to each game instance's `UEFAnalyticsSubsystem`. Events recorded there (Record Event,
forwarded message channels, session events) go to PlayFab as telemetry through
`UEPFAnalyticsSubsystem::WriteEvents`, up to 200 per request.

- The sink is ready once the player is logged in; until then the analytics subsystem holds the events.
- Names are cut down to what PlayFab accepts: letters, digits and underscores, at most 64 characters
  (`Game.Combat.EnemyKilled` → `Game_Combat_EnemyKilled`); the original name is sent as `original_name`
  when it changed.
- Attributes become the payload's `properties`, next to `session_id`, `sequence` and `user_id`.
  `event_source` is `extended`, which tells these events apart from Auto Analytics (`auto`) and
  `LogEvent` (`manual`).
- A batch that fails with a retryable error is sent again after a growing delay (5 s to 5 min), with
  at most 1000 events held; other failures drop the batch. Nothing is written to disk.
- To turn it off for one project without changing PlayFab settings, list `PlayFab` in the analytics
  settings' `DisabledSinks`.

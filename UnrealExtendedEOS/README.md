# Unreal Extended EOS

Epic Online Services for Unreal: Auth and Connect login, sessions, lobbies, matchmaking, friends and presence, stats, achievements, leaderboards, player and title storage, voice, anti-cheat and e-commerce. Each service is a game instance subsystem deriving from `UEEOSSubsystem`; settings live under Project Settings → Extended Framework → Extended EOS.

## Platform layer

`UEEOSPlatformProvider` (provider name `EOS`) serves the foundation's store-neutral platform layer, `UEFPlatformSubsystem`, through this plugin's subsystems. Ids reach it already mapped through the platform catalog, so a catalog entry's `EOS` id is the EOS achievement id, stat name or leaderboard id.

| Platform call | EOS |
|:--|:--|
| `UnlockAchievement` | `UEEOSAchievementSubsystem::UnlockAchievement` |
| `SetAchievementProgress` | 1 unlocks as above; below 1 answers `NotAvailable` |
| `QueryAchievements` | `UEEOSAchievementSubsystem::QueryAchievements` |
| `UpdateStats` | `UEEOSStatsSubsystem::IngestStatsBatch`, one ingest for the batch |
| `QueryStats` | `UEEOSStatsSubsystem::QueryLocalStats` |
| `SubmitScore` | `UEEOSLeaderboardSubsystem::UploadScore`: ingests the stat named by the leaderboard id |
| `QueryLeaderboard` Top | `UEEOSLeaderboardSubsystem::QueryLeaderboard` (ranks 1 to Count) |
| `QueryLeaderboard` AroundUser | `QueryLeaderboardByRange(1, 1000)`, then the Count entries around the user |
| `QueryLeaderboard` Friends | `QueryFriendsLeaderboard`, then `QueryLeaderboardAroundPlayer` for the user's own row |
| `WriteCloudFile` / `ReadCloudFile` / `DeleteCloudFile` | `UEEOSPlayerStorageSubsystem` `WritePlayerData` / `ReadPlayerData` / `DeletePlayerData` |
| `ListCloudFiles` | `UEEOSPlayerStorageSubsystem::QueryPlayerFiles` |
| `SetPresence` | `UEEOSPresenceSubsystem::SetPresence(Status, Status)`, then `SetPresenceKey` per property |
| `GetLocalUser` | Product User ID from `UEEOSConnectSubsystem`; display name from `UEEOSAuthSubsystem`, `UEEOSUserInfoSubsystem` or the Device ID name |

A feature is available while the EOS platform is running, the user has a Connect login (a Product User ID, which achievements, stats, leaderboards and storage need) or, for presence, an Epic account login, and its toggle under Features is on. Turning off `bEnableAchievements` or `bEnableLeaderboards` also makes the feature unsupported, so the Local provider may serve it; otherwise writes made before login wait in the platform layer's queue.

Every call answers once. Results are matched to their call through `EFCallContext` where the subsystem passes it on, and otherwise by file name or presence request id. A call with no EOS result after 30 seconds answers `TimedOut`; calls still running at shutdown answer `Cancelled`. When a subsystem is busy with the same kind of request (one leaderboard read at a time, one transfer per file, a full presence queue), the call waits for it.

Things EOS does differently:

- **Achievement progress.** EOS has no client API for partial progress: it derives progress from the stat thresholds set in the Developer Portal, and the online subsystem turns any achievement write into a permanent unlock. Progress below 1 is therefore never sent; update the stat instead.
- **Stats aggregate on the server.** An ingest is aggregated as the stat is configured (sum, max, min or latest), so `Set` and `Increment` both ingest the value. Updates to one stat in a batch combine first (set replaces, increment adds). Values are clamped to int32, names are upper-cased, and a stat never ingested reads as 0.
- **Leaderboards are defined over a stat.** `SubmitScore` ingests the stat named by the leaderboard's id (upper-cased), and the friends read looks scores up by the same name, so give the board the same id as its stat, in upper case. EOS ranks only the top 1000: `AroundUser` answers `NotAvailable` when the user is not among them or has no score.
- **Friends boards** rank the friends' scores and the user's among themselves (highest first), not by global rank. They use the friends list the EOS online subsystem has already read; EOS fails the friends read when there are none, and the board then holds only the user.
- **Cloud files.** EOS reports a missing file as a plain failure, so a failed read or delete lists the files to tell the two apart: a read of a file not in the list answers `NotFound`, a delete of one succeeds. The storage subsystem reports a failed listing as an empty list, so a listing that fails at the same time also reads as "not there". `ListCloudFiles` returns names only (no size or date).
- **Presence.** Status is the rich text friends see (at most 255 characters); each property is a presence data key, set one write at a time, with up to 31 keys besides the rich text.

The offline tests are `UnrealExtendedEOS.Platform.*`.

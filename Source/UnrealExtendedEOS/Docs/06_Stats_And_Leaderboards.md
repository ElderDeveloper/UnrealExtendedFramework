# 06 — Stats & Leaderboards

## Stats: `UEEOSStatsSubsystem`

### Actions
| Function | Description |
|---|---|
| `IngestStat(StatName, Amount)` | Ingest a single stat value |
| `IngestStatsBatch(StatsMap)` | Ingest multiple stats (`TMap<FString, int32>`) |
| `QueryPlayerStats(UserId)` | Query stats for a specific user |

### Queries
| Function | Returns |
|---|---|
| `GetCachedStats()` | `TArray<FEEOSStat>` |
| `GetStatValue(StatName)` | int32 (0 if not found) |
| `HasStat(StatName)` | bool — exists in cache |

### Delegates
| Delegate | Params |
|---|---|
| `OnStatsQueried` | bSuccess, Stats array |
| `OnStatIngested` | bSuccess, StatName |

---

## Leaderboards: `UEEOSLeaderboardSubsystem`

### Actions
| Function | Description |
|---|---|
| `QueryLeaderboard(LeaderboardId)` | Global leaderboard query |
| `QueryFriendsLeaderboard(LeaderboardId)` | Friends-only query |
| `QueryLeaderboardAroundPlayer(LeaderboardId, Range)` | Around local player's rank |
| `QueryLeaderboardByRange(LeaderboardId, StartRank, EndRank)` | Specific rank range (e.g., top 10) |
| `UploadScore(LeaderboardId, Score)` | Score upload (EOS uses stat-driven leaderboards) |

> **Note:** EOS leaderboards are stat-driven. `UploadScore` is a convenience wrapper — scores should be ingested via `StatsSubsystem::IngestStat` for actual EOS integration.

### Queries
| Function | Returns |
|---|---|
| `GetLeaderboardEntries()` | `TArray<FEEOSLeaderboardEntry>` |
| `GetLocalPlayerRank()` | int32 (-1 if not found) |
| `GetLocalPlayerScore()` | int32 (0 if not found) |
| `GetEntryCount()` | int32 |

### Delegates
| Delegate | Params |
|---|---|
| `OnLeaderboardQueried` | bSuccess, Entries array |
| `OnScoreUploaded` | bSuccess, LeaderboardId |

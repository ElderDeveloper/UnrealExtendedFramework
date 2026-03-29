# 07 — Achievements

## Subsystem: `UEEOSAchievementSubsystem`

### Actions
| Function | Description |
|---|---|
| `QueryAchievements()` | Fetch achievement definitions + player progress |
| `UnlockAchievement(AchievementId)` | Unlock (set progress to 100%) |
| `SetAchievementProgress(AchievementId, Progress)` | Set progress (0.0 to 1.0) |
| `IncrementAchievementProgress(AchievementId, Amount)` | Add to current cached progress |
| `ResetAchievement(AchievementId)` | Reset progress (stub — EOS doesn't support client reset) |

### Queries
| Function | Returns |
|---|---|
| `GetAchievements()` | `TArray<FEEOSAchievement>` |
| `IsAchievementUnlocked(AchievementId)` | bool |
| `GetUnlockedCount()` | int32 |
| `GetTotalCount()` | int32 |

### Delegates
| Delegate | Params |
|---|---|
| `OnAchievementsQueried` | bSuccess, Achievements array |
| `OnAchievementUnlocked` | AchievementId |
| `OnAchievementProgressUpdated` | AchievementId, Progress |

### Achievement Data (`FEEOSAchievement`)
```
AchievementId : FString
DisplayName   : FString
Description   : FString
Progress      : float (0.0–1.0)
bUnlocked     : bool
```

### Flow
```
QueryAchievements() → OnAchievementsQueried
SetAchievementProgress("kill_100", 0.5) → OnAchievementProgressUpdated
IncrementAchievementProgress("kill_100", 0.1) → progress = 0.6
UnlockAchievement("kill_100") → OnAchievementUnlocked
```

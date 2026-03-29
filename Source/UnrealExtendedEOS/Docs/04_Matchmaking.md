# 04 — Matchmaking

## Subsystem: `UEEOSMatchmakingSubsystem`

### Actions
| Function | Description |
|---|---|
| `StartMatchmaking(QueueName)` | Start matchmaking in a queue |
| `StartMatchmakingWithAttributes(QueueName, Attributes)` | Start with custom key-value filters |
| `CancelMatchmaking()` | Cancel current matchmaking |
| `AcceptMatch()` | Accept a found match |
| `RejectMatch()` | Reject a found match |

### Queries
| Function | Returns |
|---|---|
| `IsMatchmaking()` | bool — currently in queue |
| `GetCurrentQueueName()` | Active queue name |
| `GetMatchmakingElapsedTime()` | Seconds since queue started |
| `GetEstimatedWaitTime()` | Estimated wait (seconds, -1 if unknown) |

### Delegates
| Delegate | Params |
|---|---|
| `OnMatchFound` | SessionName |
| `OnMatchmakingCancelled` | (none) |
| `OnMatchmakingFailed` | ErrorMessage |
| `OnMatchmakingStatusChanged` | StatusString |

### Flow
```
StartMatchmaking("Ranked") → OnMatchFound → AcceptMatch() → Session created
                            → RejectMatch() → Re-queue or cancel
```

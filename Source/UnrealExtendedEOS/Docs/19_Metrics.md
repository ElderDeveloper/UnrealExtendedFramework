# 19 — Metrics

## Subsystem: `UEEOSMetricsSubsystem`

Track player session metrics for EOS Dashboard analytics.

### Session Tracking
| Function | Description |
|---|---|
| `BeginPlayerSession(SessionId, ServerIp, GameMode)` | Start tracking a play session |
| `EndPlayerSession()` | End the current session |

### Queries
| Function | Returns |
|---|---|
| `IsSessionActive()` | bool |
| `GetCurrentSessionId()` | FString |
| `GetSessionDuration()` | float (seconds) |

### Delegates
| Delegate | Params |
|---|---|
| `OnPlayerSessionStarted` | SessionId |
| `OnPlayerSessionEnded` | SessionId |

### Usage
```
// When joining a match
BeginPlayerSession("match_abc123", "192.168.1.1:7777", "DeathMatch")

// During gameplay
float Duration = GetSessionDuration()  // e.g., 342.5 seconds

// When leaving
EndPlayerSession()  // auto-called on subsystem shutdown
```

> **Note:** Full EOS SDK integration requires `EOS_Metrics_BeginPlayerSession` / `EOS_Metrics_EndPlayerSession`. Comments in code mark SDK insertion points.

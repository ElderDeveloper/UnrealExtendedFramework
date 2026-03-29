# 11 — Anti-Cheat

## Subsystem: `UEEOSAntiCheatSubsystem`

Requires `bEnableAntiCheat = true` in Settings.

### Violation Types (`EEOSAntiCheatViolationType`)
| Type | Description |
|---|---|
| `SpeedHack` | Movement speed manipulation |
| `AimBot` | Automated aiming |
| `WallHack` | See-through-walls exploit |
| `Teleport` | Position manipulation |
| `DamageHack` | Damage value manipulation |
| `ResourceHack` | Currency/resource manipulation |
| `CustomViolation` | Other violations |

### Session Management
| Function | Description |
|---|---|
| `BeginSession()` | Start anti-cheat monitoring |
| `EndSession()` | Stop monitoring + unregister all peers |

### Peer Management (Server-Side)
| Function | Description |
|---|---|
| `RegisterPeer(PeerId)` | Register a client for monitoring |
| `UnregisterPeer(PeerId)` | Remove a client |
| `UnregisterAllPeers()` | Remove all clients |

### Player Action Reporting
| Function | Description |
|---|---|
| `ReportPlayerAction(PlayerId, ActionType, ActionData)` | Log a player action for validation |
| `ReportViolation(PlayerId, ViolationType, Details)` | Report a suspected violation |
| `RequestIntegrityCheck(PlayerId)` | Request client integrity verification |

### Queries
| Function | Returns |
|---|---|
| `IsSessionActive()` | bool |
| `GetRegisteredPeers()` | `TArray<FString>` |
| `GetRegisteredPeerCount()` | int32 |
| `IsPeerRegistered(PeerId)` | bool |

### Delegates
| Delegate | Params |
|---|---|
| `OnClientActionRequired` | Action (enum), Message |
| `OnPeerAuthStatusChanged` | PeerId, bAuthenticated |
| `OnViolationDetected` | PlayerId, ViolationType, Details |
| `OnIntegrityChanged` | bIntegrityValid |

# 21 — Progression Snapshot

## Subsystem: `UEEOSProgressionSnapshotSubsystem`

Cloud save-state snapshots — capture game progress at checkpoints.

### Lifecycle
| Function | Description |
|---|---|
| `BeginSnapshot()` | Start a new snapshot → returns SnapshotId |
| `AddProgressData(Id, Key, Value)` | Add key-value progress data |
| `EndSnapshot(Id)` | Submit snapshot to EOS cloud |
| `DeleteSnapshot(Id)` | Delete a submitted snapshot |

### Queries
| Function | Returns |
|---|---|
| `IsSnapshotInProgress()` | bool |
| `GetActiveSnapshotId()` | int32 (-1 if none) |
| `GetSnapshotData(Id)` | TMap<String,String> |

### Delegates
`OnSnapshotComplete(bSuccess, SnapshotId)`, `OnSnapshotDeleted(bSuccess)`

### Usage
```
int32 Id = BeginSnapshot()
AddProgressData(Id, "Level", "5")
AddProgressData(Id, "Score", "12500")
AddProgressData(Id, "Inventory", "sword,shield")
EndSnapshot(Id) → OnSnapshotComplete
```

> **Note:** Full EOS SDK integration requires `EOS_ProgressionSnapshot_*` calls.

# 12 — Sanctions & Reports

## Subsystem: `UEEOSSanctionsSubsystem`

### Appeal Types (`EEOSSanctionAppealType`)
| Type | SDK String | Use Case |
|---|---|---|
| `IncorrectSanction` | "IncorrectSanction" | Wrongly applied sanction |
| `CompromisedAccount` | "CompromisedAccount" | Account was hacked |
| `UnfairPunishment` | "UnfairPunishment" | Disproportionate penalty |
| `AppealForMercy` | "AppealForMercy" | Request leniency |

### Actions
| Function | Description |
|---|---|
| `QueryActiveSanctions()` | Fetch active sanctions for local user |
| `SendPlayerReport(TargetUserId, Reason)` | Report a player |
| `SendPlayerReportWithCategory(TargetUserId, Reason, Category)` | Report with category |
| `AppealSanction(SanctionId, AppealType, Message)` | Appeal a sanction |

### Queries
| Function | Returns |
|---|---|
| `GetActiveSanctions()` | `TArray<FEEOSSanction>` |
| `HasActiveSanctions()` | bool |
| `GetSanctionById(SanctionId)` | `FEEOSSanction` (empty if not found) |
| `GetSanctionCount()` | int32 |

### Delegates
| Delegate | Params |
|---|---|
| `OnSanctionsQueried` | bSuccess, Sanctions array |
| `OnPlayerReported` | bSuccess |
| `OnSanctionAppealSent` | bSuccess, SanctionId |

### Sanction Data (`FEEOSSanction`)
```
SanctionId     : FString
Action         : FString
ExpirationTime : FDateTime
Reason         : FString
```

# 18 — Custom Invites

## Subsystem: `UEEOSCustomInvitesSubsystem`

Send custom game invites with payload data (map, mode, etc.) and handle Request-to-Join flow.

### Invite Management
| Function | Description |
|---|---|
| `SetCustomInvitePayload(Payload)` | Set game data to include with invites |
| `SendCustomInvite(TargetUserId)` | Send invite to one user |
| `SendCustomInviteBatch(UserIds)` | Send invite to multiple users |
| `AcceptCustomInvite(SenderId)` | Accept a pending received invite |
| `RejectCustomInvite(SenderId)` | Reject a pending received invite |

### Request to Join
| Function | Description |
|---|---|
| `SendRequestToJoin(TargetUserId)` | Request to join another player's game |
| `AcceptRequestToJoin(FromUserId)` | Accept a join request |
| `RejectRequestToJoin(FromUserId)` | Reject a join request |

### Queries
| Function | Returns |
|---|---|
| `GetCurrentPayload()` | FString — current invite payload |
| `HasPendingInvites()` | bool |
| `GetPendingInviteCount()` | int32 |

### Delegates
| Delegate | Params |
|---|---|
| `OnCustomInviteSent` | bSuccess, TargetUserId |
| `OnCustomInviteReceived` | SenderId, Payload |
| `OnCustomInviteAccepted` | SenderId, Payload |
| `OnCustomInviteRejected` | SenderId |
| `OnRequestToJoinReceived` | FromUserId |
| `OnRequestToJoinResponded` | bAccepted, FromUserId |

### Flow
```
SetCustomInvitePayload("Map=Castle&Mode=PvP")
SendCustomInvite("user123") → OnCustomInviteSent
  ↓ (other player receives)
OnCustomInviteReceived("sender", "Map=Castle&Mode=PvP")
AcceptCustomInvite("sender") → OnCustomInviteAccepted
```

> **Note:** Full EOS SDK integration requires `EOS_CustomInvites_*` calls. Comments in code mark SDK insertion points.

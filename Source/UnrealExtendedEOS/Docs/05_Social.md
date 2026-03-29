# 05 — Social (Friends & Presence)

## Friends: `UEEOSFriendsSubsystem`

### Actions
| Function | Description |
|---|---|
| `QueryFriends()` | Refresh friend list from EOS |
| `SendFriendInvite(UserId)` | Send a friend request |
| `AcceptFriendInvite(UserId)` | Accept incoming request |
| `RejectFriendInvite(UserId)` | Reject incoming request |
| `RemoveFriend(UserId)` | Remove from friend list |
| `BlockPlayer(UserId)` | Block a player |
| `UnblockPlayer(UserId)` | Unblock a player |

### Queries
| Function | Returns |
|---|---|
| `GetFriendsList()` | `TArray<FEEOSFriendInfo>` |
| `GetFriendsCount()` | int32 |
| `GetOnlineFriendsCount()` | Online friends count |
| `IsFriend(UserId)` | bool |

### Delegates
`OnFriendsListUpdated`, `OnFriendInviteReceived`, `OnFriendInviteAccepted`, `OnFriendInviteRejected`

---

## Presence: `UEEOSPresenceSubsystem`

### Actions
| Function | Description |
|---|---|
| `SetPresence(StatusString, RichText)` | Set status + rich presence |
| `SetPresenceWithStatus(OnlineStatus, RichText)` | Set with `EEOSOnlineStatus` enum |
| `SetPresenceKey(Key, Value)` | Set a custom rich presence key-value |
| `SetJoinInfo(JoinInfoString)` | Set joinable session info for friends |
| `ClearPresence()` | Set offline, clear all presence |
| `QueryPresence(UserId)` | Query another user's presence |

### Status Enum (`EEOSOnlineStatus`)
`Online`, `Away`, `DoNotDisturb`, `ExtendedAway`, `Offline`

### Queries
| Function | Returns |
|---|---|
| `GetLocalPresence()` | `FEEOSPresenceInfo` |
| `IsOnline()` | bool |
| `GetRichPresenceText()` | Cached rich text string |

### Delegates
| Delegate | Params |
|---|---|
| `OnPresenceUpdated` | FEEOSPresenceInfo |
| `OnPresenceSet` | bSuccess |

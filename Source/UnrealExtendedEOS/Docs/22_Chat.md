# 22 — Chat

## Subsystem: `UEEOSChatSubsystem`

Text chat with channel management, direct messages, and message history.

### Channel Management
| Function | Description |
|---|---|
| `JoinChannel(Name)` | Join a chat channel |
| `LeaveChannel(Name)` | Leave a channel |
| `LeaveAllChannels()` | Leave all channels |

### Messaging
| Function | Description |
|---|---|
| `SendMessage(Channel, Text)` | Send message to a channel |
| `SendDirectMessage(UserId, Text)` | Send a DM to a user |

### Queries
| Function | Returns |
|---|---|
| `GetJoinedChannels()` | TArray<FEEOSChatChannel> |
| `IsInChannel(Name)` | bool |
| `GetChannelHistory(Name, Max)` | TArray<FEEOSChatMessage> (last N msgs) |
| `GetUnreadMessageCount()` | int32 |

### Types
**`FEEOSChatMessage`**: SenderId, SenderDisplayName, Message, ChannelName, Timestamp
**`FEEOSChatChannel`**: ChannelName, MemberCount, bIsJoined

### Delegates
| Delegate | Params |
|---|---|
| `OnChatMessageReceived` | Message, ChannelName |
| `OnChannelJoined` | bSuccess, ChannelName |
| `OnChannelLeft` | ChannelName |
| `OnMessageSent` | bSuccess, ChannelName |
| `OnUserJoined` | UserId, ChannelName |
| `OnUserLeft` | UserId, ChannelName |

### Usage
```
JoinChannel("Global")
SendMessage("Global", "Hello everyone!")
→ OnMessageSent(true, "Global")
→ OnChatMessageReceived(msg, "Global")  // from other users

SendDirectMessage("user123", "Hey!")
GetChannelHistory("Global", 20)
```

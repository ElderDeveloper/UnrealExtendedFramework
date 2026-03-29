# 10 — Voice Chat

## Subsystem: `UEEOSVoiceSubsystem`

Built on `IVoiceChat` / `IVoiceChatUser` interfaces. Requires `VoiceChat` module dependency and `bEnableVoiceChat = true`.

### Room Management
| Function | Description |
|---|---|
| `JoinVoiceRoom(RoomName)` | Join via `IVoiceChatUser::JoinChannel()` |
| `LeaveVoiceRoom()` | Leave via `IVoiceChatUser::LeaveChannel()` |

### Per-Player Controls
| Function | Description |
|---|---|
| `MutePlayer(UserId)` | `SetPlayerMuted(true)` |
| `UnmutePlayer(UserId)` | `SetPlayerMuted(false)` |
| `SetPlayerVolume(UserId, Volume)` | 0.0–2.0 range |
| `GetPlayerVolume(UserId)` | Returns float |
| `IsPlayerTalking(UserId)` | Real-time talking state |
| `IsPlayerMuted(UserId)` | Mute state |

### Volume & Muting
| Function | Description |
|---|---|
| `SetOutputVolume(Volume)` | Speaker volume (0.0–1.0) |
| `SetInputVolume(Volume)` | Mic volume (0.0–1.0) |
| `GetInputVolume()` | Current mic volume |
| `SetLocalMuted(bMuted)` | Mute/unmute local mic |

### Transmit Modes
| Function | VoiceChat API |
|---|---|
| `TransmitToAllRooms()` | `TransmitToAllChannels()` |
| `TransmitToSelectedRoom(RoomName)` | `TransmitToSpecificChannels({RoomName})` |
| `TransmitToNoRoom()` | `TransmitToNoChannels()` |

### Device Management
| Function | Description |
|---|---|
| `GetInputDevices()` | `GetAvailableInputDeviceInfos()` → `FEEOSVoiceDeviceInfo` |
| `GetOutputDevices()` | `GetAvailableOutputDeviceInfos()` → `FEEOSVoiceDeviceInfo` |
| `SetInputDevice(DeviceId)` | `SetInputDeviceId()` |
| `SetOutputDevice(DeviceId)` | `SetOutputDeviceId()` |

### Queries
| Function | Returns |
|---|---|
| `IsInVoiceRoom()` | bool |
| `GetCurrentRoomName()` | FString |
| `IsLocalMuted()` | bool |
| `GetPlayersInRoom()` | `TArray<FString>` via `GetPlayersInChannel()` |
| `GetJoinedRooms()` | `TArray<FString>` via `GetChannels()` |

### Delegates
`OnVoiceRoomJoined`, `OnVoiceRoomLeft`, `OnPlayerTalking`, `OnPlayerJoinedRoom`, `OnPlayerLeftRoom`

# 09 — P2P Networking

## Subsystem: `UEEOSP2PSubsystem`

### Reliability Modes (`EEOSPacketReliability`)
| Mode | Description |
|---|---|
| `UnreliableUnordered` | Fire-and-forget, fastest |
| `UnreliableOrdered` | Ordered but may drop packets |
| `ReliableUnordered` | Guaranteed delivery, any order |
| `ReliableOrdered` | Guaranteed delivery + order |

### Relay Control (`EEOSRelayControl`)
| Mode | Description |
|---|---|
| `NoRelays` | Direct connections only |
| `AllowRelays` | Use relays as fallback |
| `ForceRelays` | Always relay (privacy mode) |

### Actions
| Function | Description |
|---|---|
| `SendPacket(TargetUserId, Data, Channel, Reliability)` | Send raw byte packet |
| `SendStringPacket(TargetUserId, Message, Channel)` | Send string (auto-converted) |
| `BroadcastPacket(Data, Channel, Reliability)` | Send to all connected peers |
| `AcceptConnection(RemoteUserId)` | Accept incoming P2P connection |
| `CloseConnection(RemoteUserId)` | Close connection to a peer |
| `CloseAllConnections()` | Disconnect from all peers |
| `QueryNATType()` | Query NAT traversal type |
| `SetRelayControl(RelayMode)` | Set relay connection policy |
| `SetPortRange(MinPort, MaxPort)` | Configure port range |

### Queries
| Function | Returns |
|---|---|
| `GetRelayControl()` | `EEOSRelayControl` |
| `GetConnectedPeers()` | `TArray<FString>` peer IDs |
| `IsConnectedToPeer(UserId)` | bool |
| `GetConnectionCount()` | int32 |

### Delegates
| Delegate | Params |
|---|---|
| `OnPacketReceived` | SenderId, Data, Channel |
| `OnConnectionEstablished` | RemoteUserId |
| `OnConnectionClosed` | RemoteUserId, Reason |
| `OnNATTypeQueried` | NATType |

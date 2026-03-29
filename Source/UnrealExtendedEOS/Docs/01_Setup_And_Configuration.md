# 01 — Setup & Configuration

## Build Module Dependencies

```csharp
// UnrealExtendedEOS.Build.cs
PublicDependencyModuleNames: Core, Engine, InputCore, OnlineSubsystem, OnlineSubsystemUtils, DeveloperSettings
PrivateDependencyModuleNames: CoreUObject, Slate, SlateCore, Json, JsonUtilities, Sockets, OnlineSubsystemEOS, EOSShared, VoiceChat, UnrealExtendedFramework
```

## Developer Settings (Project Settings → Plugins → Extended EOS)

### Credentials
| Property | Type | Description |
|---|---|---|
| `ProductId` | FString | Product ID from EOS DevPortal |
| `SandboxId` | FString | Sandbox/environment ID |
| `DeploymentId` | FString | Deployment ID |
| `ClientId` | FString | Application Client ID |
| `ClientSecret` | FString | Application Client Secret |
| `EncryptionKey` | FString | Data encryption key (64 hex chars) |

### Auth
| Property | Default | Description |
|---|---|---|
| `DefaultLoginType` | AccountPortal | Login type for `LoginWithDefaults()` |
| `bAutoLoginOnStart` | false | Auto-login on subsystem init |

### Sessions
| Property | Default | Description |
|---|---|---|
| `DefaultMaxPlayers` | 4 | Max players per session (2-64) |
| `DefaultSessionName` | "DefaultGame" | Default session name |
| `bPublicSessionsByDefault` | true | Public session advertising |
| `bUseLobbiesByDefault` | false | Use lobbies instead of sessions |
| `PreferredRegion` | NoSelection | Region preference |

### Voice
| Property | Default | Description |
|---|---|---|
| `DefaultOutputVolume` | 1.0 | Speaker volume (0.0-1.0) |
| `DefaultInputVolume` | 1.0 | Mic volume (0.0-1.0) |
| `bStartMuted` | false | Mic starts muted |

### P2P
| Property | Default | Description |
|---|---|---|
| `DefaultRelayMode` | "AllowRelays" | NoRelays / AllowRelays / ForceRelays |

### Feature Toggles
| Property | Default |
|---|---|
| `bEnableAntiCheat` | false |
| `bEnableVoiceChat` | false |
| `bEnableP2P` | false |
| `bEnableLeaderboards` | true |
| `bEnableAchievements` | true |

### Developer/Debug
| Property | Default | Description |
|---|---|---|
| `DevAuthToolAddress` | localhost:6547 | DevAuth Tool host |
| `DevAuthCredentialName` | (empty) | DevAuth credential |
| `bEnableVerboseLogging` | false | Verbose SDK logging |

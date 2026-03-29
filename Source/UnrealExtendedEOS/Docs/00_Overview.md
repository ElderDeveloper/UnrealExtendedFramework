# Epic Online Services — Architecture Overview

> **EOS SDK Version**: 1.19.0  
> **Module**: `UnrealExtendedEOS`  
> **Wrapper**: UnrealExtendedFramework Plugin

---

## What is EOS?

**Epic Online Services (EOS)** is a free, cross-platform service suite provided by Epic Games. It provides backend infrastructure for multiplayer games regardless of engine or store. Key offerings include:

- **Identity & Authentication** — Epic Account login, cross-platform Connect identity
- **Multiplayer** — Sessions, lobbies, matchmaking, P2P networking
- **Social** — Friends lists, presence, voice chat
- **Game Services** — Stats, leaderboards, achievements, cloud storage
- **Trust & Safety** — Anti-cheat, player reports, sanctions

EOS has two identity layers:

| Layer | ID Type | Purpose |
|-------|---------|---------|
| **Epic Account Services (EAS)** | `EpicAccountId` | Login via Epic, social features, overlay |
| **Game Services (Connect)** | `ProductUserId` | Cross-platform game services (stats, lobbies, etc.) |

Most game features use the **Product User ID** from the Connect interface.

---

## Architecture

### Module Structure

The `UnrealExtendedEOS` module wraps EOS through Unreal's `OnlineSubsystem` layer. Every feature is exposed as a **GameInstance Subsystem** — automatically created per game instance, accessible from anywhere via `GetGameInstance()->GetSubsystem<T>()`.

```
UnrealExtendedEOS/
├── Shared/                      ← Base classes, settings, shared types
│   ├── EEOSSubsystem.h/cpp      ← Abstract base for all EOS subsystems
│   ├── EEOSSettings.h/cpp       ← Developer settings (Project Settings UI)
│   └── EEOSTypes.h              ← Shared structs, enums, delegates
│
├── Auth/                        ← Authentication
│   ├── EEOSAuthSubsystem        ← Epic Account login/logout
│   └── EEOSConnectSubsystem     ← Cross-platform Product User ID
│
├── Sessions/                    ← Multiplayer sessions
│   ├── EEOSSessionSubsystem     ← Game sessions (create/find/join/destroy)
│   └── EEOSLobbySubsystem       ← Lobbies with member management
│
├── Matchmaking/                 ← Queue-based matchmaking
│   └── EEOSMatchmakingSubsystem
│
├── Social/                      ← Social features
│   ├── EEOSFriendsSubsystem     ← Friends list & invites
│   └── EEOSPresenceSubsystem    ← Online status & rich presence
│
├── Stats/                       ← Player statistics
│   └── EEOSStatsSubsystem
│
├── Leaderboards/                ← Leaderboard queries
│   └── EEOSLeaderboardSubsystem
│
├── Achievements/                ← Achievement tracking
│   └── EEOSAchievementSubsystem
│
├── Storage/                     ← Cloud storage
│   ├── EEOSPlayerStorageSubsystem  ← Per-player cloud files
│   └── EEOSTitleStorageSubsystem   ← Game-wide read-only files
│
├── P2P/                         ← Peer-to-peer networking
│   └── UEEOSP2PSubsystem
│
├── Voice/                       ← Voice chat
│   └── UEEOSVoiceSubsystem
│
├── AntiCheat/                   ← Anti-cheat protection
│   └── UEEOSAntiCheatSubsystem
│
└── Sanctions/                   ← Reports & sanctions
    └── UEEOSSanctionsSubsystem
```

### Base Class: `UEEOSSubsystem`

All subsystems inherit from `UEEOSSubsystem` which extends `UGameInstanceSubsystem`. It provides:

- `IsEOSAvailable()` — Blueprint-callable check if EOS is ready
- `GetEOSOnlineSubsystem()` — Internal access to the `IOnlineSubsystem` (EOS)
- `GetEOSSettings()` — Access to `UEEOSSettings` configuration
- `LogEOSUnavailable()` — Standardized warning logging

### Async Pattern

All EOS operations are **asynchronous**. The pattern is consistent across every subsystem:

1. **Call an Action** (e.g., `Login()`, `CreateSession()`, `QueryStats()`)
2. **Bind a Delegate** to receive the result
3. **Handle the Callback** with success/failure info

```cpp
// C++ example
auto* Auth = GetGameInstance()->GetSubsystem<UEEOSAuthSubsystem>();
Auth->OnLoginComplete.AddDynamic(this, &AMyActor::HandleLoginResult);
Auth->Login(EEOSLoginType::AccountPortal);
```

```
// Blueprint equivalent
Get Subsystem (EEOSAuthSubsystem)
  → Bind Event to OnLoginComplete
  → Call Login
```

### OnlineSubsystem Delegate Pattern

When working directly with `IOnlineSession`, `IOnlineIdentity`, etc., use this pattern to avoid dangling delegates:

```cpp
// 1. Create delegate (usually in constructor)
CreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(
    this, &ThisClass::OnCreateSessionComplete);

// 2. Add to interface → store the handle
CreateSessionCompleteDelegateHandle = 
    SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

// 3. Clear in callback (or on failure)
void OnCreateSessionComplete(FName SessionName, bool bWasSuccess)
{
    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
        CreateSessionCompleteDelegateHandle);
    // ... handle result ...
}
```

> **Important**: Always clear delegate handles after use. Leaving them bound can cause callbacks to fire on destroyed objects.

### Shared Types

All data structures are in `EEOSTypes.h`:

| Type | Purpose |
|------|---------|
| `FEEOSResult` | Generic success/failure with error info |
| `FEEOSSessionSearchResult` | Session search result data |
| `FEEOSLeaderboardEntry` | Leaderboard rank entry |
| `FEEOSAchievement` | Achievement with progress |
| `FEEOSFriendInfo` | Friend with online status |
| `FEEOSPresenceInfo` | User presence data |
| `FEEOSStat` | Player statistic entry |
| `FEEOSSanction` | Active sanction info |

### Module Dependencies

```
UnrealExtendedEOS
├── OnlineSubsystem          ← Unreal's online subsystem interface
├── OnlineSubsystemEOS       ← Epic's EOS Online Subsystem plugin
├── EOSShared                ← Shared EOS utilities
├── DeveloperSettings        ← For UEEOSSettings (Project Settings UI)
└── UnrealExtendedFramework  ← Parent plugin utilities
```

---

## Feature Enable Flags

Some features require explicit enabling in **Project Settings → Plugins → Extended EOS**:

| Setting | Default | Controls |
|---------|---------|----------|
| `bEnableAntiCheat` | `false` | Anti-Cheat subsystem activation |
| `bEnableVoiceChat` | `false` | Voice Chat subsystem activation |
| `bEnableP2P` | `false` | P2P Networking subsystem activation |

---

## Quick-Start Flow

```
1. Configure credentials in Project Settings → Plugins → Extended EOS
2. Configure DefaultEngine.ini for OnlineSubsystemEOS
3. Login: UEEOSAuthSubsystem → Login()
4. Connect: UEEOSConnectSubsystem → ConnectLogin()
5. Use game services (sessions, stats, etc.)
```

See [01_Setup_And_Configuration.md](01_Setup_And_Configuration.md) for detailed setup instructions.

---

## Document Index

| Guide | Topic |
|-------|-------|
| [00_Overview](00_Overview.md) | This document — architecture overview |
| [01_Setup_And_Configuration](01_Setup_And_Configuration.md) | Project setup, credentials, engine config |
| [02_Authentication](02_Authentication.md) | Auth + Connect subsystems |
| [03_Sessions_And_Lobbies](03_Sessions_And_Lobbies.md) | Sessions + Lobbies |
| [04_Matchmaking](04_Matchmaking.md) | Queue-based matchmaking |
| [05_Social](05_Social.md) | Friends + Presence |
| [06_Stats_And_Leaderboards](06_Stats_And_Leaderboards.md) | Stats + Leaderboards |
| [07_Achievements](07_Achievements.md) | Achievement tracking |
| [08_Storage](08_Storage.md) | Player Data + Title Storage |
| [09_P2P_Networking](09_P2P_Networking.md) | P2P connections |
| [10_Voice_Chat](10_Voice_Chat.md) | Voice chat rooms |
| [11_AntiCheat](11_AntiCheat.md) | Anti-cheat protection |
| [12_Sanctions_And_Reports](12_Sanctions_And_Reports.md) | Reports + Sanctions |
| [13_EOS_SDK_Reference](13_EOS_SDK_Reference.md) | SDK header quick-reference |
| [14_Error_Codes](14_Error_Codes.md) | ~200 error codes by category |

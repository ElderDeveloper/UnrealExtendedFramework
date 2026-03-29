# Sessions & Lobbies

> Two multiplayer systems: **Sessions** for traditional server-browser gameplay, **Lobbies** for pre-game gathering with member sync.

---

## Concepts

| Feature | Sessions | Lobbies |
|---------|----------|---------|
| **Purpose** | Game matches (dedicated/listen server) | Pre-game gathering rooms |
| **Data Sync** | Settings on creation | Real-time attribute sync to all members |
| **Member Events** | No built-in member tracking | Join/leave notifications per member |
| **Discovery** | Search by settings | Search by attributes |
| **Typical Flow** | Find → Join → Play | Create → Gather → Start Match |

---

## UEEOSSessionSubsystem

**Header**: `Sessions/EEOSSessionSubsystem.h`  
**Category**: `EOS|Sessions`

### Actions

| Function | Parameters | Description |
|----------|-----------|-------------|
| `CreateSession()` | `MaxPlayers=4`, `bIsLAN=false`, `bIsPresence=true`, `SessionName="GameSession"` | Create a new game session |
| `FindSessions()` | `MaxResults=20` | Search for available sessions |
| `JoinSession()` | `SearchResultIndex`, `SessionName="GameSession"` | Join by search result index |
| `DestroySession()` | `SessionName="GameSession"` | Destroy the current session |

### Queries

| Function | Returns | Description |
|----------|---------|-------------|
| `GetSearchResults()` | `TArray<FEEOSSessionSearchResult>` | Cached results from last `FindSessions` |
| `IsInSession()` | `bool` | Whether currently in a session |

### Delegates

| Delegate | Parameters | Fires When |
|----------|-----------|------------|
| `OnSessionCreated` | `bSuccess`, `SessionName` | Session creation completes |
| `OnSessionsFound` | `Results[]` | Session search finishes |
| `OnSessionJoined` | `bSuccess`, `SessionName` | Join attempt completes |
| `OnSessionDestroyed` | `bSuccess`, `SessionName` | Session destruction completes |

### Session Search Result

```cpp
struct FEEOSSessionSearchResult
{
    FString SessionId;
    FString OwnerName;
    int32   CurrentPlayers;
    int32   MaxPlayers;
    int32   Ping;
    TMap<FName, FString> Settings;  // Custom key-value data
};
```

### C++ Example: Host & Join Flow

```cpp
// ── HOST ─────────────────────────────────────
auto* Sessions = GetGameInstance()->GetSubsystem<UEEOSSessionSubsystem>();
Sessions->OnSessionCreated.AddDynamic(this, &AMyGM::OnCreated);
Sessions->CreateSession(4, false, true, TEXT("GameSession"));

void AMyGM::OnCreated(bool bSuccess, const FString& SessionName)
{
    if (bSuccess)
    {
        // Session is live — clients can now find and join
        GetWorld()->ServerTravel(TEXT("/Game/Maps/GameMap?listen"));
    }
}

// ── CLIENT ───────────────────────────────────
auto* Sessions = GetGameInstance()->GetSubsystem<UEEOSSessionSubsystem>();
Sessions->OnSessionsFound.AddDynamic(this, &AMyPC::OnFound);
Sessions->FindSessions(20);

void AMyPC::OnFound(const TArray<FEEOSSessionSearchResult>& Results)
{
    if (Results.Num() > 0)
    {
        // Show server browser UI, then join selected
        Sessions->OnSessionJoined.AddDynamic(this, &AMyPC::OnJoined);
        Sessions->JoinSession(0);  // Join first result
    }
}

void AMyPC::OnJoined(bool bSuccess, const FString& SessionName)
{
    if (bSuccess)
    {
        // Travel to the host's map
        // The session system handles the connection URL
    }
}
```

### FOnlineSessionSettings — Full Configuration

> From EOSGoUltimate — the complete set of session settings available when creating sessions.

| Setting | Type | Description |
|---------|------|-------------|
| `bIsDedicated` | `bool` | Whether this is a dedicated server session |
| `bIsLANMatch` | `bool` | LAN-only session (no EOS backend) |
| `NumPublicConnections` | `int32` | Public player slots |
| `NumPrivateConnections` | `int32` | Private/reserved player slots |
| `bUsesPresence` | `bool` | Show in friends' presence info |
| `bAllowJoinViaPresence` | `bool` | Friends can click "Join" from presence |
| `bAllowJoinViaPresenceFriendsOnly` | `bool` | Restrict presence-join to friends only |
| `bAllowInvites` | `bool` | Allow sending session invites |
| `bAllowJoinInProgress` | `bool` | Allow joining after session starts |
| `bUseLobbiesIfAvailable` | `bool` | Use lobby backend instead of sessions |
| `bUseLobbiesVoiceChatIfAvailable` | `bool` | Enable built-in voice chat via lobby |
| `bShouldAdvertise` | `bool` | Advertise to the matchmaking service |
| `bUsesStats` | `bool` | Track stats for this session |

### Custom Attributes (Advertised Data)

Use `EOnlineDataAdvertisementType::ViaOnlineService` to make attributes searchable:

```cpp
SessionSettings->Set(FName("OWNER_NAME"), OwnerDisplayName, 
    EOnlineDataAdvertisementType::ViaOnlineService);
SessionSettings->Set(FName("MATCH_TYPE"), TEXT("FreeForAll"), 
    EOnlineDataAdvertisementType::ViaOnlineService);
SessionSettings->Set(FName("IS_SESSION_PRIVATE"), false, 
    EOnlineDataAdvertisementType::ViaOnlineService);
```

### Private Sessions with Join IDs

Implement private sessions using a secret join code:

```cpp
// Host: set a private join ID
int32 PrivateCode = FMath::RandRange(100000, 999999);
SessionSettings->Set(FName("SERVER_JOIN_ID"), PrivateCode, 
    EOnlineDataAdvertisementType::ViaOnlineService);

// Client: search by join code
SearchSettings->QuerySettings.SearchParams.Add(
    FName("SERVER_JOIN_ID"), 
    FOnlineSessionSearchParam(PrivateCode, EOnlineComparisonOp::Equals)
);
```

### Travel Patterns

After session create/join, you need to travel players to the game map:

```cpp
// ── HOST: Server Travel ────────────────────────
// Called after session creation succeeds
if (UWorld* World = GetWorld())
{
    // ?listen makes the server accept client connections
    World->ServerTravel(TEXT("/Game/Maps/GameMap?listen"));
}

// ── CLIENT: Client Travel ──────────────────────
// Called after session join succeeds
FString ConnectionInfo;
SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectionInfo);

if (!ConnectionInfo.IsEmpty())
{
    APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
    PC->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
}

// ── RETURNING TO MENU ──────────────────────────
// Clean disconnect with user-facing reason
PlayerController->ClientReturnToMainMenuWithTextReason(
    FText::FromString(TEXT("Session ended"))
);
```

### Destroy-and-Recreate Pattern

If a session already exists when creating a new one, destroy it first:

```cpp
auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
if (ExistingSession != nullptr)
{
    // Save all creation params for re-creation after destroy
    bCreateSessionOnDestroy = true;
    // ... cache all settings ...
    GoDestroySession();  // Will auto-recreate in OnDestroyComplete
}
```

---

## UEEOSLobbySubsystem

**Header**: `Sessions/EEOSLobbySubsystem.h`  
**Category**: `EOS|Lobbies`

### Actions

| Function | Parameters | Description |
|----------|-----------|-------------|
| `CreateLobby()` | `MaxMembers=4`, `bIsPublic=true` | Create a new lobby |
| `FindLobbies()` | `MaxResults=20` | Search for available lobbies |
| `JoinLobby()` | `SearchResultIndex` | Join a lobby from results |
| `LeaveLobby()` | *(none)* | Leave the current lobby |
| `SetLobbyAttribute()` | `Key`, `Value` | Set synced key-value data |

### Queries

| Function | Returns | Description |
|----------|---------|-------------|
| `IsInLobby()` | `bool` | Whether currently in a lobby |
| `GetCurrentLobbyId()` | `FString` | ID of the current lobby |

### Delegates

| Delegate | Parameters | Fires When |
|----------|-----------|------------|
| `OnLobbyCreated` | `bSuccess`, `LobbyId` | Lobby creation completes |
| `OnLobbiesFound` | `Results[]` | Lobby search finishes |
| `OnLobbyJoined` | `bSuccess`, `LobbyId` | Join attempt completes |
| `OnLobbyMemberJoined` | `MemberId` | A player joins the lobby |
| `OnLobbyMemberLeft` | `MemberId` | A player leaves the lobby |

### C++ Example: Lobby-Based Party

```cpp
auto* Lobby = GetGameInstance()->GetSubsystem<UEEOSLobbySubsystem>();

// Create a party lobby
Lobby->OnLobbyCreated.AddDynamic(this, &AMyPC::OnLobbyReady);
Lobby->OnLobbyMemberJoined.AddDynamic(this, &AMyPC::OnMemberJoined);
Lobby->OnLobbyMemberLeft.AddDynamic(this, &AMyPC::OnMemberLeft);
Lobby->CreateLobby(4, true);

void AMyPC::OnLobbyReady(bool bSuccess, const FString& LobbyId)
{
    if (bSuccess)
    {
        // Set lobby metadata
        Lobby->SetLobbyAttribute(TEXT("MapName"), TEXT("Forest"));
        Lobby->SetLobbyAttribute(TEXT("GameMode"), TEXT("PvP"));
    }
}

void AMyPC::OnMemberJoined(const FString& MemberId)
{
    UE_LOG(LogTemp, Log, TEXT("Player joined: %s"), *MemberId);
    // Update party UI
}
```

---

## Sessions vs Lobbies: When to Use

| Scenario | Recommended |
|----------|-------------|
| Server browser | **Sessions** |
| Dedicated server matchmaking | **Sessions** |
| Party/group gathering pre-match | **Lobbies** |
| Real-time settings sync (map vote, ready-up) | **Lobbies** |
| Quick play with friends | **Lobbies** → start **Session** |

### Combined Flow

```
1. Players form a party in a Lobby
2. Party leader starts matchmaking or creates a Session
3. All lobby members join the Session
4. Lobby can stay alive for post-match regrouping
```

---

## Session Lifecycle States

> From EOS SDK — sessions progress through these states:

```
NoSession → Creating → Pending → Starting → InProgress → Ending → Ended → Destroying
```

| State | Description |
|-------|-------------|
| `NoSession` | No session exists yet |
| `Creating` | Session is being created |
| `Pending` | Created but not started (pre-match lobby) |
| `Starting` | Start requested (may take time for backend) |
| `InProgress` | Session is active. **Join-in-progress disabled sessions are no longer joinable** |
| `Ending` | Session is closing (post-match lobby) |
| `Ended` | Session is closed, stats committed |
| `Destroying` | Session is being destroyed |

---

## Session Permission Levels

| Level | Description |
|-------|-------------|
| `PublicAdvertised` | Anyone can find this session (as long as it isn't full) |
| `JoinViaPresence` | Only players with presence access can see this session |
| `InviteOnly` | Only players with explicit invites can see this session |

---

## Attribute Types & Search Operators

> Used by both Sessions and Lobbies to store and search custom key-value data.

### Attribute Data Types

| Type | C++ Type | Example |
|------|----------|---------|
| `Boolean` | `bool` | `IsRanked = true` |
| `Int64` | `int64` | `MinLevel = 10` |
| `Double` | `double` | `SkillRating = 1500.0` |
| `String` | `FString` | `MapName = "Forest"` |

### Search Comparison Operators

| Operator | Description |
|----------|-------------|
| `EQUAL` | Value must exactly match |
| `NOTEQUAL` | Value must not match |
| `GREATERTHAN` | Strictly greater |
| `GREATERTHANOREQUAL` | Greater or equal |
| `LESSTHAN` | Strictly less |
| `LESSTHANOREQUAL` | Less or equal |
| `DISTANCE` | Prefer nearest value (`abs(Search - Stored)` closest to 0) |
| `ANYOF` | Value must be in specified list |
| `NOTANYOF` | Value must NOT be in specified list |

### Attribute Advertisement

| Setting | Description |
|---------|-------------|
| `DontAdvertise` | Attribute is local only — not visible in search |
| `Advertise` | Attribute is visible when other players search |

---

## Session / Lobby Error Codes

| Error Code | Meaning |
|-----------|---------|
| `EOS_Sessions_SessionInProgress` | Session already started |
| `EOS_Sessions_TooManyPlayers` | Player limit exceeded |
| `EOS_Sessions_NoPermission` | Client lacks access |
| `EOS_Sessions_SessionAlreadyExists` | Duplicate session name |
| `EOS_Sessions_InvalidSession` | Invalid session reference |
| `EOS_Sessions_InviteFailed` | Invite could not be sent |
| `EOS_Sessions_OutOfSync` | Local data out of sync with backend |
| `EOS_Sessions_PlayerSanctioned` | Player is sanctioned — blocked from joining |
| `EOS_Lobby_NotOwner` | Only the owner can modify the lobby |
| `EOS_Lobby_TooManyPlayers` | Lobby member limit exceeded |
| `EOS_Lobby_VoiceNotEnabled` | Operation requires voice-enabled lobby |
| `EOS_Lobby_PlatformNotAllowed` | Client platform not in allowed list |

### Sanctions Integration

Sessions support built-in sanctions checking via `bSanctionsEnabled`. When enabled, sanctioned players are automatically blocked from joining, and `RegisterPlayers()` returns a `SanctionedPlayers` array identifying who was rejected.

---

## Region-Based Sessions

> From EOSIntegrationKit — filter sessions by geographic region.

| Region | Description |
|--------|-------------|
| `NoSelection` | Any region (default) |
| `Asia` | Asia Pacific |
| `NorthAmerica` | US/Canada |
| `SouthAmerica` | Latin America |
| `Africa` | Africa |
| `Europe` | Europe |
| `Australia` | Oceania |

```cpp
// Create a session in a specific region
CreateSession(/*...*/, ERegionInfo::RE_Europe);

// Search for sessions in a specific region
FindSessions(/*...*/, ERegionInfo::RE_NorthAmerica);
```

---

## Dedicated Server Settings

```cpp
struct FDedicatedServerSettings
{
    bool bIsDedicatedServer = false;
    int32 PortInfo = 7777;   // Server listen port
};
```

### Session Creation Extra Settings

```cpp
struct FCreateSessionExtraSettings
{
    bool bIsLanMatch = false;
    int32 NumberOfPrivateConnections = 0;
    bool bShouldAdvertise = true;
    bool bAllowJoinInProgress = true;
    ERegionInfo Region = ERegionInfo::RE_NoSelection;
    bool bUsePresence = false;
    bool bAllowJoinViaPresence = false;
    bool bAllowJoinViaPresenceFriendsOnly = false;
    bool bEnforceSanctions = false;   // Auto-reject sanctioned players
};
```

---

## Session Codes

Use generated alphanumeric codes for private session sharing:

```cpp
// Generate a random session join code (default: 9 chars)
FString Code = GenerateSessionCode(6);  // e.g., "A3BX7K"

// Host stores code as a session attribute
SessionSettings->Set(FName("SESSION_CODE"), Code,
    EOnlineDataAdvertisementType::ViaOnlineService);

// Joining player searches by code
SearchSettings->QuerySettings.SearchParams.Add(
    FName("SESSION_CODE"),
    FOnlineSessionSearchParam(Code, EOnlineComparisonOp::Equals)
);
```

---

## Host Migration

When the session host disconnects, EOS can promote another player:

```cpp
// Bind host migration callback
OnHostMigrated.BindDynamic(this, &AMyGM::HandleHostMigration);

void AMyGM::HandleHostMigration(bool bIsLocalHost, 
    const FString& PromotedMember, const FString& JoinAddress)
{
    if (bIsLocalHost)
    {
        // This client is the new host — start listening
        GetWorld()->ServerTravel(JoinAddress + TEXT("?listen"));
    }
    else
    {
        // Travel to the new host
        PC->ClientTravel(JoinAddress, TRAVEL_Absolute);
    }
}
```

---

## Session Invite Handling

```cpp
// Listen for incoming session invites
OnSessionUserInviteAccepted.AddDynamic(this, &AMyPC::OnInviteAccepted);

void AMyPC::OnInviteAccepted(bool bWasSuccessful, 
    const FBlueprintSessionResult& AcceptedSession)
{
    if (bWasSuccessful)
    {
        // Auto-join the session from the invite
        JoinSession(AcceptedSession);
    }
}

// Programmatically accept/reject invites
AcceptSessionInvite(InviteId, LocalUserId, InviterUserId);
RejectSessionInvite(InviteId, LocalUserId);
```

---

## Current Session Info Query

Get full info about the active session at any time:

```cpp
FEIK_CurrentSessionInfo Info = GetCurrentSessionInfo(
    this, bIsSessionPresent, NAME_GameSession);

if (bIsSessionPresent)
{
    // Joinability
    Info.bPublicJoinable;     // Anyone can join
    Info.bFriendJoinable;     // Friends can join
    Info.bInviteOnly;         // Invite-only
    Info.bAllowInvites;       // Invites enabled

    // Capacity
    Info.NumOpenPublicConnections;     // Available public slots
    Info.MaxPublicConnections;        // Total public slots
    Info.NumOpenPrivateConnections;    // Available private slots

    // State
    Info.SessionState;                // NoSession → Destroying
    Info.SessionIdString;             // Backend session ID
    Info.SessionOwner;                // Host user ID
    Info.RegisteredPlayers;           // All registered players
}
```

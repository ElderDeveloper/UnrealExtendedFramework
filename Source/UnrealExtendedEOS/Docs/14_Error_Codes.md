# EOS Error Codes Reference

> Complete reference of EOS result codes organized by category.  
> Source: `EOSResult` enum from the EOS SDK (via EOSCore mapping).

---

## General Errors

| Code | Description |
|------|-------------|
| `EOS_Success` | Operation succeeded |
| `EOS_NoConnection` | No network connectivity |
| `EOS_InvalidCredentials` | Wrong email/password or invalid token |
| `EOS_InvalidUser` | User not found |
| `EOS_InvalidAuth` | Missing or invalid authentication token |
| `EOS_AccessDenied` | Access denied |
| `EOS_MissingPermissions` | Client lacks the required permission |
| `EOS_TooManyRequests` | Rate limited — wait and retry |
| `EOS_AlreadyPending` | Async request was already in progress |
| `EOS_InvalidParameters` | Invalid parameters specified |
| `EOS_InvalidRequest` | Invalid request |
| `EOS_IncompatibleVersion` | Client incompatible with backend |
| `EOS_NotConfigured` | SDK not properly configured |
| `EOS_AlreadyConfigured` | SDK already configured |
| `EOS_NotImplemented` | Feature not available |
| `EOS_Canceled` | Operation was canceled (usually by user) |
| `EOS_NotFound` | Requested item not found |
| `EOS_OperationWillRetry` | Error occurred, SDK will retry automatically |
| `EOS_NoChange` | Request had no effect |
| `EOS_LimitExceeded` | Client-side limit exceeded |
| `EOS_Disabled` | Feature or client ID disabled |
| `EOS_DuplicateNotAllowed` | Duplicate entry not allowed |
| `EOS_InvalidSandboxId` | Sandbox ID is invalid |
| `EOS_TimedOut` | Request timed out |
| `EOS_PartialResult` | Query returned some but not all results |
| `EOS_InvalidState` | Resource in an invalid state |
| `EOS_RequestInProgress` | Request already in progress |
| `EOS_ApplicationSuspended` | App is suspended |
| `EOS_NetworkDisconnected` | Network disconnected |
| `EOS_ServiceFailure` | Backend service failure |
| `EOS_UnexpectedError` | Unknown error |

---

## Auth Interface Errors

| Code | Description |
|------|-------------|
| `EOS_Auth_AccountLocked` | Account locked due to login failures |
| `EOS_Auth_AccountLockedForUpdate` | Account locked by update operation |
| `EOS_Auth_InvalidRefreshToken` | Refresh token was invalid |
| `EOS_Auth_InvalidToken` | Invalid access token (common when switching environments) |
| `EOS_Auth_AuthenticationFailure` | Invalid bearer token |
| `EOS_Auth_InvalidPlatformToken` | Invalid platform token |
| `EOS_Auth_WrongAccount` | Auth params don't match this account |
| `EOS_Auth_WrongClient` | Auth params don't match this client |
| `EOS_Auth_FullAccountRequired` | Full Epic Account required |
| `EOS_Auth_HeadlessAccountRequired` | Headless account required |
| `EOS_Auth_PasswordResetRequired` | User must reset password |
| `EOS_Auth_PasswordCannotBeReused` | Password was previously used |
| `EOS_Auth_Expired` | Auth code has expired |
| `EOS_Auth_ScopeConsentRequired` | User consent not given |
| `EOS_Auth_ApplicationNotFound` | Application not found in backend |
| `EOS_Auth_ScopeNotFound` | Requested consent not found |
| `EOS_Auth_AccountFeatureRestricted` | Account access denied |
| `EOS_Auth_AccountPortalLoadError` | Overlay failed to load Account Portal |
| `EOS_Auth_CorrectiveActionRequired` | User must fix their account |
| `EOS_Auth_PinGrantCode` | PIN grant flow initiated |
| `EOS_Auth_PinGrantExpired` | PIN grant code expired |
| `EOS_Auth_PinGrantPending` | PIN grant code pending approval |
| `EOS_Auth_ExternalAuthNotLinked` | External account not linked to Epic |
| `EOS_Auth_ExternalAuthRevoked` | External auth was revoked |
| `EOS_Auth_ExternalAuthInvalid` | External auth token unreadable |
| `EOS_Auth_ExternalAuthRestricted` | External auth cannot be linked due to restrictions |
| `EOS_Auth_ExternalAuthCannotLogin` | External auth cannot be used for login |
| `EOS_Auth_ExternalAuthExpired` | External auth token expired |
| `EOS_Auth_ExternalAuthIsLastLoginType` | Cannot remove — it's the last login method |
| `EOS_Auth_ExchangeCodeNotFound` | Exchange code not found or already used |
| `EOS_Auth_OriginatingExchangeCodeSessionExpired` | Source exchange code session expired |
| `EOS_Auth_AccountNotActive` | Account has been disabled |
| `EOS_Auth_MFARequired` | Multi-factor authentication required |
| `EOS_Auth_ParentalControls` | Blocked by parental controls |
| `EOS_Auth_NoRealId` | Korea real ID association required |
| `EOS_Auth_UserInterfaceRequired` | Silent login failed — user interaction needed |

---

## Connect Interface Errors

| Code | Description |
|------|-------------|
| `EOS_Connect_ExternalTokenValidationFailed` | External platform token is invalid |
| `EOS_Connect_UserAlreadyExists` | Trying to create a duplicate user |
| `EOS_Connect_AuthExpired` | Connect auth has expired |
| `EOS_Connect_InvalidToken` | Invalid connect token |
| `EOS_Connect_UnsupportedTokenType` | Token type not supported |
| `EOS_Connect_LinkAccountFailed` | Account linking failed |
| `EOS_Connect_ExternalServiceUnavailable` | Platform validation service is down |
| `EOS_Connect_ExternalServiceConfigurationFailure` | Dev Portal configuration issue |

---

## Friends Errors

| Code | Description |
|------|-------------|
| `EOS_Friends_InviteAwaitingAcceptance` | Outgoing invite already pending |
| `EOS_Friends_NoInvitation` | No invite to accept/reject |
| `EOS_Friends_AlreadyFriends` | Already friends |
| `EOS_Friends_NotFriends` | Not friends |
| `EOS_Friends_TargetUserTooManyInvites` | Remote user has too many invites |
| `EOS_Friends_LocalUserTooManyInvites` | Local user has too many invites |
| `EOS_Friends_TargetUserFriendLimitExceeded` | Remote user's friend list is full |
| `EOS_Friends_LocalUserFriendLimitExceeded` | Local user's friend list is full |

---

## Presence Errors

| Code | Description |
|------|-------------|
| `EOS_Presence_DataInvalid` | Request data was null or invalid |
| `EOS_Presence_DataLengthInvalid` | Too many/few data items, or would overflow max |
| `EOS_Presence_DataKeyInvalid` | Invalid key in data |
| `EOS_Presence_DataKeyLengthInvalid` | Key too long or too short |
| `EOS_Presence_DataValueInvalid` | Invalid value in data |
| `EOS_Presence_DataValueLengthInvalid` | Value too long |
| `EOS_Presence_RichTextInvalid` | Invalid rich text string |
| `EOS_Presence_RichTextLengthInvalid` | Rich text too long |
| `EOS_Presence_StatusInvalid` | Invalid status state |

---

## Sessions Errors

| Code | Description |
|------|-------------|
| `EOS_Sessions_SessionInProgress` | Session already started |
| `EOS_Sessions_TooManyPlayers` | Too many players to register |
| `EOS_Sessions_NoPermission` | No permission to access session |
| `EOS_Sessions_SessionAlreadyExists` | Session already exists |
| `EOS_Sessions_InvalidLock` | Session lock required |
| `EOS_Sessions_InvalidSession` | Invalid session reference |
| `EOS_Sessions_SandboxNotAllowed` | Sandbox ID mismatch |
| `EOS_Sessions_InviteFailed` | Invite failed to send |
| `EOS_Sessions_InviteNotFound` | Invite not found |
| `EOS_Sessions_UpsertNotAllowed` | Client may not modify session |
| `EOS_Sessions_HostAtCapacity` | Backend node at capacity |
| `EOS_Sessions_OutOfSync` | Local data out of sync with backend |
| `EOS_Sessions_TooManyInvites` | User has too many invites |
| `EOS_Sessions_PlayerSanctioned` | Player is sanctioned |

---

## Lobby Errors

| Code | Description |
|------|-------------|
| `EOS_Lobby_NotOwner` | Only owner can modify the lobby |
| `EOS_Lobby_LobbyAlreadyExists` | Lobby already exists |
| `EOS_Lobby_TooManyPlayers` | Too many members |
| `EOS_Lobby_NoPermission` | No permission |
| `EOS_Lobby_InviteFailed` | Invite failed |
| `EOS_Lobby_MemberUpdateOnly` | After reconnect — only local member data updated |
| `EOS_Lobby_PresenceLobbyExists` | Presence lobby already exists |
| `EOS_Lobby_VoiceNotEnabled` | Voice not enabled for this lobby |
| `EOS_Lobby_PlatformNotAllowed` | Client platform not in allowed list |

---

## Player Data Storage Errors

| Code | Description |
|------|-------------|
| `EOS_PlayerDataStorage_FilenameInvalid` | Invalid filename |
| `EOS_PlayerDataStorage_FilenameLengthInvalid` | Filename too long |
| `EOS_PlayerDataStorage_FilenameInvalidChars` | Filename has invalid characters |
| `EOS_PlayerDataStorage_FileSizeTooLarge` | File would be too large |
| `EOS_PlayerDataStorage_EncryptionKeyNotSet` | **Encryption key not set** — set during SDK init |
| `EOS_PlayerDataStorage_UserThrottled` | User is rate-limited |
| `EOS_PlayerDataStorage_FileCorrupted` | File is corrupted (retry may fix) |
| `EOS_PlayerDataStorage_FileHeaderHasNewerVersion` | File from newer SDK — update required |

---

## Anti-Cheat Errors

| Code | Description |
|------|-------------|
| `EOS_AntiCheat_ClientProtectionNotAvailable` | Anti-cheat bootstrapper not used |
| `EOS_AntiCheat_InvalidMode` | Wrong anti-cheat mode for this API |
| `EOS_AntiCheat_ClientProductIdMismatch` | Product ID mismatch in helper |
| `EOS_AntiCheat_ClientSandboxIdMismatch` | Sandbox ID mismatch in helper |
| `EOS_AntiCheat_PeerAlreadyRegistered` | Peer already registered |
| `EOS_AntiCheat_PeerNotFound` | Peer does not exist |
| `EOS_AntiCheat_DeviceIdAuthIsNotSupported` | Device ID auth not supported |

---

## RTC (Voice) Errors

| Code | Description |
|------|-------------|
| `EOS_RTC_TooManyParticipants` | Room cannot accept more participants |
| `EOS_RTC_RoomAlreadyExists` | Room already exists |
| `EOS_RTC_UserKicked` | User was kicked from room |
| `EOS_RTC_UserBanned` | User is banned from room |
| `EOS_RTC_RoomWasLeft` | Room was left successfully |
| `EOS_RTC_ReconnectionTimegateExpired` | Connection dropped — reconnect timeout |
| `EOS_RTC_UserIsInBlocklist` | User is in the block list |

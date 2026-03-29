# EOS SDK C Header Reference

> Quick reference for the raw EOS C SDK headers located in `Thirdparty/EOS/SDK/Include/`.  
> **SDK Version**: 1.19.0

---

## Overview

The EOS SDK is a **C API** — all functions use C-style calling conventions with handle-based interfaces. The `UnrealExtendedEOS` module wraps these through Unreal's `OnlineSubsystem`, so you typically don't call these directly. This reference is for understanding the underlying API surface.

---

## SDK Initialization

| Header | Purpose |
|--------|---------|
| `eos_sdk.h` | Platform create/destroy, all interface getters |
| `eos_init.h` | SDK initialization options |
| `eos_types.h` | Core types: handles, options structs |
| `eos_common.h` | Common definitions, account ID handling |
| `eos_base.h` | Base macros and declarations |
| `eos_version.h` | SDK version (1.19.0) |
| `eos_result.h` | All EOS result codes (~200+ error codes) |
| `eos_logging.h` | Logging configuration |
| `eos_logging_categories.h` | Log category definitions |
| `eos_platform_prereqs.h` | Platform prerequisites |

---

## Authentication

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_auth.h` | Auth Interface | `EOS_Auth_Login`, `EOS_Auth_Logout`, `EOS_Auth_CopyUserAuthToken` |
| `eos_auth_types.h` | Auth Types | `EOS_Auth_Credentials`, `EOS_Auth_LoginOptions`, login types |
| `eos_connect.h` | Connect Interface | `EOS_Connect_Login`, `EOS_Connect_CreateDeviceId`, `EOS_Connect_LinkAccount` |
| `eos_connect_types.h` | Connect Types | `EOS_Connect_Credentials`, `EOS_ProductUserId` |

---

## Sessions & Lobbies

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_sessions.h` | Sessions Interface | `EOS_Sessions_CreateSessionModification`, `EOS_Sessions_UpdateSession`, `EOS_Sessions_StartSession`, `EOS_Sessions_DestroySession` |
| `eos_sessions_types.h` | Sessions Types | Session search, modification options, attribute types |
| `eos_lobby.h` | Lobby Interface | `EOS_Lobby_CreateLobby`, `EOS_Lobby_JoinLobby`, `EOS_Lobby_LeaveLobby`, `EOS_Lobby_UpdateLobbyModification` |
| `eos_lobby_types.h` | Lobby Types | Lobby attributes, member info, search parameters |

---

## Social

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_friends.h` | Friends Interface | `EOS_Friends_QueryFriends`, `EOS_Friends_SendInvite`, `EOS_Friends_AcceptInvite` |
| `eos_friends_types.h` | Friends Types | Friend status enum, invite options |
| `eos_presence.h` | Presence Interface | `EOS_Presence_SetPresence`, `EOS_Presence_QueryPresence`, `EOS_Presence_CopyPresence` |
| `eos_presence_types.h` | Presence Types | Status enum (Online, Away, etc.), rich text data |
| `eos_userinfo.h` | User Info Interface | `EOS_UserInfo_QueryUserInfo`, `EOS_UserInfo_CopyExternalUserInfoByIndex` |
| `eos_userinfo_types.h` | User Info Types | User display info, external account info |

---

## Game Services

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_stats.h` | Stats Interface | `EOS_Stats_IngestStat`, `EOS_Stats_QueryStats` |
| `eos_stats_types.h` | Stats Types | Stat options, aggregation methods |
| `eos_leaderboards.h` | Leaderboards Interface | `EOS_Leaderboards_QueryLeaderboardDefinitions`, `EOS_Leaderboards_QueryLeaderboardRanks` |
| `eos_leaderboards_types.h` | Leaderboards Types | Rank entries, definition data |
| `eos_achievements.h` | Achievements Interface | `EOS_Achievements_QueryDefinitions`, `EOS_Achievements_QueryPlayerAchievements`, `EOS_Achievements_UnlockAchievements` |
| `eos_achievements_types.h` | Achievements Types | Achievement definitions, progress data |
| `eos_progressionsnapshot.h` | Progression | `EOS_ProgressionSnapshot_BeginSnapshot`, `EOS_ProgressionSnapshot_SubmitSnapshot` |
| `eos_progressionsnapshot_types.h` | Progression Types | Snapshot data |

---

## Cloud Storage

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_playerdatastorage.h` | Player Storage | `EOS_PlayerDataStorage_QueryFile`, `EOS_PlayerDataStorage_ReadFile`, `EOS_PlayerDataStorage_WriteFile`, `EOS_PlayerDataStorage_DeleteFile` |
| `eos_playerdatastorage_types.h` | Player Storage Types | File metadata, transfer callbacks |
| `eos_titlestorage.h` | Title Storage | `EOS_TitleStorage_QueryFile`, `EOS_TitleStorage_ReadFile` |
| `eos_titlestorage_types.h` | Title Storage Types | File metadata, read options |

---

## Communication

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_p2p.h` | P2P Interface | `EOS_P2P_SendPacket`, `EOS_P2P_ReceivePacket`, `EOS_P2P_AcceptConnection`, `EOS_P2P_CloseConnection`, `EOS_P2P_QueryNATType` |
| `eos_p2p_types.h` | P2P Types | Socket ID, packet types, NAT type enum |
| `eos_rtc.h` | RTC Interface | `EOS_RTC_JoinRoom`, `EOS_RTC_LeaveRoom` |
| `eos_rtc_types.h` | RTC Types | Room options, join/leave data |
| `eos_rtc_audio.h` | RTC Audio | `EOS_RTCAudio_SendAudio`, `EOS_RTCAudio_AddNotifyParticipantUpdated` |
| `eos_rtc_audio_types.h` | RTC Audio Types | Audio buffer, participant info |
| `eos_rtc_data.h` | RTC Data | Data channel for RTC rooms |
| `eos_rtc_data_types.h` | RTC Data Types | Data message types |
| `eos_rtc_admin.h` | RTC Admin | Admin tools for voice rooms |
| `eos_rtc_admin_types.h` | RTC Admin Types | Admin options |

---

## Trust & Safety

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_anticheatclient.h` | Anti-Cheat Client | `EOS_AntiCheatClient_BeginSession`, `EOS_AntiCheatClient_EndSession` |
| `eos_anticheatclient_types.h` | Client Types | Session options, action callbacks |
| `eos_anticheatserver.h` | Anti-Cheat Server | `EOS_AntiCheatServer_RegisterClient`, `EOS_AntiCheatServer_UnregisterClient` |
| `eos_anticheatserver_types.h` | Server Types | Client info, action types |
| `eos_anticheatcommon_types.h` | Common AC Types | Shared anti-cheat enums |
| `eos_sanctions.h` | Sanctions Interface | `EOS_Sanctions_QueryActivePlayerSanctions`, `EOS_Sanctions_CopyPlayerSanctionByIndex` |
| `eos_sanctions_types.h` | Sanctions Types | Sanction data, query options |
| `eos_reports.h` | Reports Interface | `EOS_Reports_SendPlayerBehaviorReport` |
| `eos_reports_types.h` | Reports Types | Report categories, options |
| `eos_kws.h` | Kids Web Services | Parental controls |
| `eos_kws_types.h` | KWS Types | Permission data |

---

## Commerce & Store

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_ecom.h` | E-Commerce | `EOS_Ecom_QueryOffers`, `EOS_Ecom_Checkout`, `EOS_Ecom_QueryEntitlements` |
| `eos_ecom_types.h` | E-Commerce Types | Offers, entitlements, catalog items |

---

## UI & Overlay

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_ui.h` | UI Interface | `EOS_UI_ShowFriends`, `EOS_UI_SetDisplayPreference` |
| `eos_ui_types.h` | UI Types | Notification placement, overlay settings |
| `eos_ui_keys.h` | UI Keys | Key binding definitions for overlay toggle |
| `eos_ui_buttons.h` | UI Buttons | Button mappings |

---

## Other

| Header | API | Key Functions |
|--------|-----|--------------|
| `eos_metrics.h` | Metrics | `EOS_Metrics_BeginPlayerSession`, `EOS_Metrics_EndPlayerSession` |
| `eos_metrics_types.h` | Metrics Types | Account source, display name |
| `eos_mods.h` | Mods Interface | `EOS_Mods_EnumerateMods`, `EOS_Mods_InstallMod` |
| `eos_mods_types.h` | Mods Types | Mod identifiers, enumeration data |
| `eos_custominvites.h` | Custom Invites | `EOS_CustomInvites_SendCustomInvite`, `EOS_CustomInvites_AddNotifyCustomInviteReceived` |
| `eos_custominvites_types.h` | Custom Invite Types | Invite payload data |
| `eos_integratedplatform.h` | Integrated Platform | Platform integration options |
| `eos_integratedplatform_types.h` | IP Types | Platform-specific settings |

---

## Key SDK Constants

From `eos_version.h`:

```c
#define EOS_MAJOR_VERSION  1
#define EOS_MINOR_VERSION  19
#define EOS_PATCH_VERSION  0
#define EOS_HOTFIX_VERSION 3
```

---

## Platform Libraries

Located in `Thirdparty/EOS/SDK/`:

```
SDK/
├── Bin/        ← Runtime DLLs (ship with game)
├── Include/    ← C headers (77 files)
├── Lib/        ← Link libraries
└── Tools/      ← Dev Auth Tool, anti-cheat tools
```

| Folder | Contents |
|--------|----------|
| `Bin/` | `EOSSDK-Win64-Shipping.dll` and variants |
| `Lib/` | `EOSSDK-Win64-Shipping.lib` |
| `Include/` | All headers documented above |
| `Tools/` | Developer Auth Tool executable |

# 20 — Blueprint Utilities

## Library: `UEEOSBlueprintLibrary`

Static utility functions accessible from **any Blueprint** — no subsystem reference needed.

### EOS Status
| Function | Returns |
|---|---|
| `IsEOSInitialized()` | bool — is EOS SDK ready |
| `IsLoggedIn(LocalUserNum)` | bool — is user logged in |

### Local User Info
| Function | Returns |
|---|---|
| `GetLocalEpicAccountId()` | FString — Epic Account ID |
| `GetLocalDisplayName()` | FString — player name |
| `GetLocalProductUserId()` | FString — Product User ID |
| `GetLoginStatus()` | FString — "LoggedIn", "NotLoggedIn", etc. |

### ID Validation
| Function | Returns |
|---|---|
| `IsValidEpicAccountId(Id)` | bool — 32-char hex check |
| `IsValidProductUserId(Id)` | bool — 16-64 char hex check |

### String Conversions
| Function | Use Case |
|---|---|
| `ByteArrayToHexString(Data)` | Debug/display binary data |
| `HexStringToByteArray(Hex)` | Parse hex strings |
| `StringToBytes(Text)` | UTF-8 encode for P2P/Chat |
| `BytesToString(Data)` | UTF-8 decode received data |

### Platform Info
| Function | Returns |
|---|---|
| `GetEOSSubsystemName()` | "EOS", "EOSPlus", etc. |
| `GetNumLocalUsers()` | int32 — logged-in user count |
| `GetAuthToken()` | FString — auth token |

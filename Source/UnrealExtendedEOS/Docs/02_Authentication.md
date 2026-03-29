# 02 — Authentication

## Subsystem: `UEEOSAuthSubsystem`

### Login Types (`EEOSLoginType`)
| Type | Credentials.Type | Notes |
|---|---|---|
| Password | `password` | Id + Token required |
| ExchangeCode | `exchangecode` | Token only |
| PersistentAuth | `persistentauth` | No credentials needed (stored token) |
| DeviceCode | `devicecode` | No credentials needed |
| Developer | `developer` | Uses DevAuth Tool (Id=address, Token=credential) |
| AccountPortal | `accountportal` | Browser-based OAuth |
| ExternalAuth | `externalauth` | Token from external provider |

### Login Status (`EEOSLoginStatus`)
`NotLoggedIn` → `LoggingIn` → `LoggedIn` / `Failed`

### Actions
| Function | Description |
|---|---|
| `Login(LoginType, Id, Token)` | Login with specified credential type |
| `LoginWithDefaults()` | Login using `DefaultLoginType` from Settings |
| `Logout()` | Logout current user |
| `RefreshAuthToken()` | Silent re-auth via PersistentAuth |
| `DeletePersistentAuth()` | Clear stored credentials |

### Queries
| Function | Returns |
|---|---|
| `GetLoginStatus()` | Current `EEOSLoginStatus` |
| `GetLoggedInUserId()` | Epic Account ID string |
| `GetDisplayName()` | Player nickname |
| `IsLoggedIn()` | bool |
| `GetAuthToken()` | Current auth token string |
| `GetCurrentLoginType()` | `EEOSLoginType` used for login |

### Delegates
| Delegate | Params | Description |
|---|---|---|
| `OnLoginComplete` | bSuccess, ErrorMessage | Login attempt result |
| `OnLogoutComplete` | (none) | Logout completed |
| `OnLoginStatusChanged` | NewStatus | Status enum changed |
| `OnAuthTokenRefreshed` | bSuccess, NewToken | Token refresh result |

### Auto-Login
Set `bAutoLoginOnStart = true` in Settings → calls `LoginWithDefaults()` on subsystem init.

### Developer Auth Flow
1. Run Epic's DevAuth Tool
2. Set `DefaultLoginType = Developer`
3. Configure `DevAuthToolAddress` and `DevAuthCredentialName`
4. Call `Login(Developer)` or `LoginWithDefaults()`

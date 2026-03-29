# 16 — User Info

## Subsystem: `UEEOSUserInfoSubsystem`

User lookup, search by display name, batch queries, and external account mapping.

### User Lookup
| Function | Description |
|---|---|
| `QueryUserInfo(EpicAccountId)` | Query a single user by Epic Account ID |
| `FindUserByDisplayName(DisplayName)` | Search for a user by display name |
| `QueryUserInfoBatch(EpicAccountIds)` | Query multiple users at once |

### External Account Mapping
| Function | Description |
|---|---|
| `QueryExternalAccountMappings(ExternalIds, AccountType)` | Map external IDs (Steam/PSN/Xbox) to EOS IDs |

### Queries
| Function | Returns |
|---|---|
| `GetCachedUserInfo()` | `FEEOSUserInfo` — last single query |
| `GetCachedSearchResults()` | `TArray<FEEOSUserInfo>` — batch/search results |
| `GetCachedMappings()` | `TArray<FEEOSExternalAccountMapping>` |

### Delegates
| Delegate | Params |
|---|---|
| `OnUserInfoQueried` | bSuccess, UserInfo |
| `OnUserSearchComplete` | bSuccess, Results array |
| `OnExternalMappingsQueried` | bSuccess, Mappings array |

### Types

**`FEEOSUserInfo`**: EpicAccountId, DisplayName, Nickname, Country, PreferredLanguage

**`FEEOSExternalAccountMapping`**: ProductUserId, ExternalAccountId, ExternalAccountType, DisplayName

### Flow
```
FindUserByDisplayName("PlayerName") → OnUserSearchComplete(true, [{EpicAccountId, DisplayName}])
QueryUserInfo("000123...") → OnUserInfoQueried(true, UserInfo)
QueryUserInfoBatch(["id1", "id2"]) → OnUserSearchComplete(true, [Info1, Info2])
```

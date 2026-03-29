# 17 — UI Overlay

## Subsystem: `UEEOSUISubsystem`

Wraps `IOnlineExternalUI` for EOS Social Overlay and platform UI control.

### Friends & Invites
| Function | IOnlineExternalUI API |
|---|---|
| `ShowFriendsUI()` | `ShowFriendsUI(0)` |
| `ShowInviteUI(SessionName)` | `ShowInviteUI(0, SessionName)` |

### Player Profile
| Function | Description |
|---|---|
| `ShowProfileUI(TargetUserId)` | Show a player's profile card |

### Achievements / Leaderboards
| Function | API |
|---|---|
| `ShowAchievementsUI()` | `ShowAchievementsUI(0)` |
| `ShowLeaderboardUI(Name)` | `ShowLeaderboardUI(Name)` |

### Store & Web
| Function | Description |
|---|---|
| `ShowStoreUI(ProductId)` | Open platform store, optionally to a product |
| `ShowWebURL(URL)` | Open URL in overlay browser |
| `CloseWebURL()` | Close overlay browser |

### Login & Account
| Function | Description |
|---|---|
| `ShowLoginUI()` | Platform login prompt |
| `ShowAccountUpgradeUI()` | PS+, Xbox Gold upgrade prompt |

### Overlay State
| Function | Returns |
|---|---|
| `IsOverlayVisible()` | bool — current overlay state |

### Delegates
`OnOverlayStateChanged(bIsVisible)`, `OnProfileClosed(bSuccess)`

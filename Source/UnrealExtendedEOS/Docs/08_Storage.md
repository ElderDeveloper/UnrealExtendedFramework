# 08 — Cloud Storage

## Player Storage: `UEEOSPlayerStorageSubsystem`

Uses `IOnlineUserCloud` for per-user read/write cloud files.

### Actions
| Function | Description |
|---|---|
| `WritePlayerData(FileName, Data)` | Write a `TArray<uint8>` to cloud |
| `ReadPlayerData(FileName)` | Read a cloud file |
| `DeletePlayerData(FileName)` | Delete a cloud file |
| `QueryPlayerFiles()` | Enumerate user's cloud files |

### Queries
| Function | Returns |
|---|---|
| `GetPlayerFileList()` | `TArray<FString>` |

### Delegates
| Delegate | Params |
|---|---|
| `OnPlayerDataWritten` | bSuccess, FileName |
| `OnPlayerDataRead` | bSuccess, FileName, Data |
| `OnPlayerFilesQueried` | FileNames |

---

## Title Storage: `UEEOSTitleStorageSubsystem`

Uses `IOnlineTitleFile` for game-wide read-only cloud files (configured in DevPortal).

### Actions
| Function | Description |
|---|---|
| `ReadTitleFile(FileName)` | Read a title storage file → `OnTitleFileRead` |
| `ReadTitleFileAsString(FileName)` | Same as ReadTitleFile (convert bytes via BPL) |
| `QueryTitleFiles()` | Enumerate available title files → `OnTitleFilesQueried` |

### Queries
| Function | Returns |
|---|---|
| `GetTitleFileList()` | `TArray<FString>` — cached file names |
| `HasTitleFile(FileName)` | bool — exists in cache |

### Delegates
| Delegate | Params |
|---|---|
| `OnTitleFileRead` | bSuccess, FileName, Data (`TArray<uint8>`) |
| `OnTitleFilesQueried` | FileNames (`TArray<FString>`) |

### Implementation Details
- `QueryTitleFiles()` → `IOnlineTitleFile::EnumerateFiles()` → callback populates cache via `GetFileList(FCloudFileHeader)`
- `ReadTitleFile()` → `IOnlineTitleFile::ReadFile()` → callback retrieves data via `GetFileContents()`
- Delegate cleanup in `Deinitialize()`

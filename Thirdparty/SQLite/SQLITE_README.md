# SQLite Amalgamation

**Required version:** 3.35.0+ (for ALTER TABLE DROP COLUMN support)
**Recommended version:** 3.53.0 (latest stable as of April 2026)

## Setup

1. Download the **amalgamation** from https://www.sqlite.org/download.html
2. Extract `sqlite3.c` and `sqlite3.h` into THIS directory
3. The UnrealExtendedSQL module compiles them inline — no separate build step

## Files needed

```
Thirdparty/SQLite/
├── sqlite3.c      ← SQLite amalgamation source (~250K lines)
├── sqlite3.h      ← SQLite amalgamation header
└── SQLITE_README.md  ← This file
```

## Compile definitions (set by Build.cs)

- `SQLITE_THREADSAFE=1` — Serialized threading mode
- `SQLITE_ENABLE_FTS5=1` — Full-text search
- `SQLITE_ENABLE_JSON1=1` — JSON functions
- `SQLITE_ENABLE_COLUMN_METADATA=1` — Column metadata API
- `SQLITE_CORE` — Compiled as part of the application

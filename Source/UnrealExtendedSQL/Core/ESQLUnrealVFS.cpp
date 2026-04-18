// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLUnrealVFS.h"
#include "UnrealExtendedSQL.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#include "sqlite3.h"
THIRD_PARTY_INCLUDES_END


// ── Static state ─────────────────────────────────────────────────────────────

bool FESQLUnrealVFS::bRegistered = false;

static const char* ESQL_VFS_NAME = "unreal_vfs";


// ── File handle wrapper ──────────────────────────────────────────────────────

/**
 * Extended sqlite3_file that holds an IFileHandle pointer.
 * SQLite allocates this struct for us — we just fill the pMethods.
 */
struct FESQLVFSFile
{
	sqlite3_file Base;            // Must be first member
	IFileHandle* FileHandle;      // Unreal file handle
	FString FilePath;             // For logging / reopening
};


// ── VFS I/O Methods (static C callbacks) ─────────────────────────────────────

static int ESQLVFSClose(sqlite3_file* pFile)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (File->FileHandle)
	{
		delete File->FileHandle;
		File->FileHandle = nullptr;
	}
	File->FilePath.Empty();
	return SQLITE_OK;
}

static int ESQLVFSRead(sqlite3_file* pFile, void* zBuf, int iAmt, sqlite3_int64 iOfst)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (!File->FileHandle)
	{
		return SQLITE_IOERR_READ;
	}

	// Check file size first — reading at or past EOF must return SHORT_READ per SQLite VFS spec
	const int64 FileSize = File->FileHandle->Size();
	if (iOfst >= FileSize)
	{
		// Entire read is past EOF — zero-fill and return short read
		FMemory::Memzero(zBuf, iAmt);
		return SQLITE_IOERR_SHORT_READ;
	}

	if (!File->FileHandle->Seek(iOfst))
	{
		return SQLITE_IOERR_READ;
	}

	// Check if the read extends past EOF (partial read)
	const int64 Available = FileSize - iOfst;
	if (Available < iAmt)
	{
		// Partial read — read what's available, zero-fill the rest
		if (!File->FileHandle->Read(reinterpret_cast<uint8*>(zBuf), static_cast<int32>(Available)))
		{
			return SQLITE_IOERR_READ;
		}
		FMemory::Memzero(reinterpret_cast<uint8*>(zBuf) + Available, iAmt - Available);
		return SQLITE_IOERR_SHORT_READ;
	}

	// Full read
	if (!File->FileHandle->Read(reinterpret_cast<uint8*>(zBuf), iAmt))
	{
		return SQLITE_IOERR_READ;
	}

	return SQLITE_OK;
}

static int ESQLVFSWrite(sqlite3_file* pFile, const void* zBuf, int iAmt, sqlite3_int64 iOfst)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (!File->FileHandle)
	{
		return SQLITE_IOERR_WRITE;
	}

	if (!File->FileHandle->Seek(iOfst))
	{
		return SQLITE_IOERR_WRITE;
	}

	if (!File->FileHandle->Write(reinterpret_cast<const uint8*>(zBuf), iAmt))
	{
		return SQLITE_IOERR_WRITE;
	}

	return SQLITE_OK;
}

static int ESQLVFSTruncate(sqlite3_file* pFile, sqlite3_int64 nByte)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (!File->FileHandle)
	{
		return SQLITE_IOERR_TRUNCATE;
	}

	if (!File->FileHandle->Truncate(nByte))
	{
		return SQLITE_IOERR_TRUNCATE;
	}

	return SQLITE_OK;
}

static int ESQLVFSSync(sqlite3_file* pFile, int /*flags*/)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (!File->FileHandle)
	{
		return SQLITE_IOERR_FSYNC;
	}

	if (!File->FileHandle->Flush())
	{
		return SQLITE_IOERR_FSYNC;
	}

	return SQLITE_OK;
}

static int ESQLVFSFileSize(sqlite3_file* pFile, sqlite3_int64* pSize)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	if (!File->FileHandle)
	{
		return SQLITE_IOERR_FSTAT;
	}

	*pSize = File->FileHandle->Size();
	return SQLITE_OK;
}

// Locking — UE's file handles don't support advisory locks, so we use no-ops.
// Thread safety is handled by FCriticalSection in FESQLDatabase.
static int ESQLVFSLock(sqlite3_file* /*pFile*/, int /*eLock*/) { return SQLITE_OK; }
static int ESQLVFSUnlock(sqlite3_file* /*pFile*/, int /*eLock*/) { return SQLITE_OK; }
static int ESQLVFSCheckReservedLock(sqlite3_file* /*pFile*/, int* pResOut) { *pResOut = 0; return SQLITE_OK; }

static int ESQLVFSFileControl(sqlite3_file* /*pFile*/, int /*op*/, void* /*pArg*/) { return SQLITE_NOTFOUND; }
static int ESQLVFSSectorSize(sqlite3_file* /*pFile*/) { return 4096; }
static int ESQLVFSDeviceCharacteristics(sqlite3_file* /*pFile*/) { return 0; }


// ── I/O methods table ────────────────────────────────────────────────────────

static const sqlite3_io_methods GESQLVFSIOMethods = {
	1,                              // iVersion
	ESQLVFSClose,
	ESQLVFSRead,
	ESQLVFSWrite,
	ESQLVFSTruncate,
	ESQLVFSSync,
	ESQLVFSFileSize,
	ESQLVFSLock,
	ESQLVFSUnlock,
	ESQLVFSCheckReservedLock,
	ESQLVFSFileControl,
	ESQLVFSSectorSize,
	ESQLVFSDeviceCharacteristics,
	// v2/v3 methods not needed
	nullptr, // xShmMap
	nullptr, // xShmLock
	nullptr, // xShmBarrier
	nullptr, // xShmUnmap
	nullptr, // xFetch
	nullptr, // xUnfetch
};


#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

/**
 * Minimal IFileHandle around a Windows HANDLE with controlled share mode.
 * This gives us FILE_SHARE_READ | FILE_SHARE_WRITE which UE's OpenWrite
 * doesn't guarantee, enabling concurrent access (editor + picker).
 */
class FESQLWindowsFileHandle : public IFileHandle
{
	HANDLE WinHandle;
public:
	FESQLWindowsFileHandle(HANDLE InHandle) : WinHandle(InHandle) {}
	virtual ~FESQLWindowsFileHandle() override
	{
		if (WinHandle != INVALID_HANDLE_VALUE) { CloseHandle(WinHandle); WinHandle = INVALID_HANDLE_VALUE; }
	}
	virtual int64 Tell() override
	{
		LARGE_INTEGER Zero = {}; LARGE_INTEGER Pos;
		SetFilePointerEx(WinHandle, Zero, &Pos, FILE_CURRENT);
		return Pos.QuadPart;
	}
	virtual bool Seek(int64 NewPosition) override
	{
		LARGE_INTEGER Pos; Pos.QuadPart = NewPosition;
		return SetFilePointerEx(WinHandle, Pos, nullptr, FILE_BEGIN) != 0;
	}
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override
	{
		LARGE_INTEGER Pos; Pos.QuadPart = NewPositionRelativeToEnd;
		return SetFilePointerEx(WinHandle, Pos, nullptr, FILE_END) != 0;
	}
	virtual bool Read(uint8* Dest, int64 BytesToRead) override
	{
		while (BytesToRead > 0)
		{
			DWORD ToRead = (DWORD)FMath::Min(BytesToRead, (int64)MAXDWORD);
			DWORD BytesRead = 0;
			if (!ReadFile(WinHandle, Dest, ToRead, &BytesRead, nullptr) || BytesRead == 0) return false;
			Dest += BytesRead;
			BytesToRead -= BytesRead;
		}
		return true;
	}
	virtual bool Write(const uint8* Src, int64 BytesToWrite) override
	{
		while (BytesToWrite > 0)
		{
			DWORD ToWrite = (DWORD)FMath::Min(BytesToWrite, (int64)MAXDWORD);
			DWORD Written = 0;
			if (!WriteFile(WinHandle, Src, ToWrite, &Written, nullptr)) return false;
			Src += Written;
			BytesToWrite -= Written;
		}
		return true;
	}
	virtual bool Flush(const bool bFullFlush = false) override
	{
		return FlushFileBuffers(WinHandle) != 0;
	}
	virtual bool Truncate(int64 NewSize) override
	{
		LARGE_INTEGER Pos; Pos.QuadPart = NewSize;
		return SetFilePointerEx(WinHandle, Pos, nullptr, FILE_BEGIN) && SetEndOfFile(WinHandle);
	}
	virtual int64 Size() override
	{
		LARGE_INTEGER FileSize;
		GetFileSizeEx(WinHandle, &FileSize);
		return FileSize.QuadPart;
	}
	virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override
	{
		if (!Seek(Offset)) return false;
		return Read(Destination, BytesToRead);
	}
};
#endif // PLATFORM_WINDOWS

// ── VFS-level Methods ────────────────────────────────────────────────────────

static int ESQLVFSOpen(sqlite3_vfs* /*pVfs*/, const char* zName, sqlite3_file* pFile, int flags, int* pOutFlags)
{
	FESQLVFSFile* File = reinterpret_cast<FESQLVFSFile*>(pFile);
	File->Base.pMethods = nullptr;
	File->FileHandle = nullptr;

	// In-memory or temp databases — let SQLite use the default VFS
	if (zName == nullptr)
	{
		return SQLITE_CANTOPEN;
	}

	const FString Path = UTF8_TO_TCHAR(zName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Ensure directory exists
	const FString Directory = FPaths::GetPath(Path);
	if (!Directory.IsEmpty() && !PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// Determine open mode
	const bool bCreate = (flags & SQLITE_OPEN_CREATE) != 0;
	const bool bReadWrite = (flags & SQLITE_OPEN_READWRITE) != 0;

#if PLATFORM_WINDOWS
	// Use CreateFileW directly with FILE_SHARE_READ | FILE_SHARE_WRITE
	// so that multiple connections can access the same .db file concurrently.
	{
		DWORD DesiredAccess = GENERIC_READ;
		DWORD CreationDisposition = OPEN_EXISTING;

		if (bReadWrite || bCreate)
		{
			DesiredAccess |= GENERIC_WRITE;
			CreationDisposition = bCreate ? OPEN_ALWAYS : OPEN_EXISTING;
		}

		HANDLE WinHandle = CreateFileW(
			*Path,
			DesiredAccess,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			CreationDisposition,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (WinHandle != INVALID_HANDLE_VALUE)
		{
			File->FileHandle = new FESQLWindowsFileHandle(WinHandle);
		}
	}
#else
	if (bReadWrite || bCreate)
	{
		File->FileHandle = PlatformFile.OpenWrite(*Path, /*bAppend=*/true, /*bAllowRead=*/true);

		if (!File->FileHandle && bCreate && !PlatformFile.FileExists(*Path))
		{
			IFileHandle* TempHandle = PlatformFile.OpenWrite(*Path, /*bAppend=*/false, /*bAllowRead=*/false);
			if (TempHandle) { delete TempHandle; }
			File->FileHandle = PlatformFile.OpenWrite(*Path, /*bAppend=*/true, /*bAllowRead=*/true);
		}
	}
	else
	{
		File->FileHandle = PlatformFile.OpenRead(*Path);
	}
#endif

	if (!File->FileHandle)
	{
		UE_LOG(LogExtendedSQL, Warning, TEXT("VFS: Failed to open file: %s"), *Path);
		return SQLITE_CANTOPEN;
	}

	File->FilePath = Path;
	File->Base.pMethods = &GESQLVFSIOMethods;

	if (pOutFlags)
	{
		*pOutFlags = flags;
	}

	return SQLITE_OK;
}

static int ESQLVFSDelete(sqlite3_vfs* /*pVfs*/, const char* zName, int /*syncDir*/)
{
	const FString Path = UTF8_TO_TCHAR(zName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*Path))
	{
		return PlatformFile.DeleteFile(*Path) ? SQLITE_OK : SQLITE_IOERR_DELETE;
	}

	return SQLITE_OK;
}

static int ESQLVFSAccess(sqlite3_vfs* /*pVfs*/, const char* zName, int flags, int* pResOut)
{
	const FString Path = UTF8_TO_TCHAR(zName);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	switch (flags)
	{
	case SQLITE_ACCESS_EXISTS:
		*pResOut = PlatformFile.FileExists(*Path) ? 1 : 0;
		break;
	case SQLITE_ACCESS_READWRITE:
		*pResOut = (PlatformFile.FileExists(*Path) && !PlatformFile.IsReadOnly(*Path)) ? 1 : 0;
		break;
	case SQLITE_ACCESS_READ:
		*pResOut = PlatformFile.FileExists(*Path) ? 1 : 0;
		break;
	default:
		*pResOut = 0;
		break;
	}

	return SQLITE_OK;
}

static int ESQLVFSFullPathname(sqlite3_vfs* /*pVfs*/, const char* zName, int nOut, char* zOut)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(UTF8_TO_TCHAR(zName));
	const FTCHARToUTF8 Utf8(*FullPath);
	const int32 Len = FCStringAnsi::Strlen(Utf8.Get());

	if (Len >= nOut)
	{
		return SQLITE_CANTOPEN;
	}

	FCStringAnsi::Strncpy(zOut, Utf8.Get(), nOut);
	return SQLITE_OK;
}

// Randomness, sleep, current time — delegate to defaults
static int ESQLVFSRandomness(sqlite3_vfs* /*pVfs*/, int nByte, char* zOut)
{
	for (int i = 0; i < nByte; ++i)
	{
		zOut[i] = static_cast<char>(FMath::RandRange(0, 255));
	}
	return nByte;
}

static int ESQLVFSSleep(sqlite3_vfs* /*pVfs*/, int microseconds)
{
	FPlatformProcess::SleepNoStats(microseconds / 1000000.0f);
	return microseconds;
}

static int ESQLVFSCurrentTimeInt64(sqlite3_vfs* /*pVfs*/, sqlite3_int64* pTimeOut)
{
	// SQLite wants time as Julian day number * 86400000 (ms since Julian epoch)
	const FDateTime Now = FDateTime::UtcNow();
	// Unix epoch in Julian days: 2440587.5
	// Julian day number = Unix timestamp / 86400 + 2440587.5
	const int64 UnixTimestamp = (Now - FDateTime(1970, 1, 1)).GetTotalMilliseconds();
	*pTimeOut = UnixTimestamp + static_cast<sqlite3_int64>(2440587.5 * 86400000.0);
	return SQLITE_OK;
}

static int ESQLVFSCurrentTime(sqlite3_vfs* pVfs, double* pTimeOut)
{
	sqlite3_int64 TimeInt64 = 0;
	ESQLVFSCurrentTimeInt64(pVfs, &TimeInt64);
	*pTimeOut = TimeInt64 / 86400000.0;
	return SQLITE_OK;
}


// ── VFS Instance ─────────────────────────────────────────────────────────────

static sqlite3_vfs GESQLUnrealVFS = {
	3,                              // iVersion
	sizeof(FESQLVFSFile),           // szOsFile
	512,                            // mxPathname (SQLITE_MAX_PATHNAME default)
	nullptr,                        // pNext
	ESQL_VFS_NAME,                  // zName
	nullptr,                        // pAppData
	ESQLVFSOpen,
	ESQLVFSDelete,
	ESQLVFSAccess,
	ESQLVFSFullPathname,
	nullptr, // xDlOpen
	nullptr, // xDlError
	nullptr, // xDlSym
	nullptr, // xDlClose
	ESQLVFSRandomness,
	ESQLVFSSleep,
	ESQLVFSCurrentTime,
	nullptr, // xGetLastError
	ESQLVFSCurrentTimeInt64,
	// v3 methods
	nullptr, // xSetSystemCall
	nullptr, // xGetSystemCall
	nullptr, // xNextSystemCall
};


// ── Public API ───────────────────────────────────────────────────────────────

bool FESQLUnrealVFS::Register()
{
	if (bRegistered)
	{
		return true;
	}

	const int Result = sqlite3_vfs_register(&GESQLUnrealVFS, /*makeDflt=*/0);
	if (Result != SQLITE_OK)
	{
		UE_LOG(LogExtendedSQL, Error, TEXT("Failed to register SQLite VFS '%hs': error %d"), ESQL_VFS_NAME, Result);
		return false;
	}

	bRegistered = true;
	UE_LOG(LogExtendedSQL, Log, TEXT("Registered custom SQLite VFS: %hs"), ESQL_VFS_NAME);
	return true;
}

void FESQLUnrealVFS::Unregister()
{
	if (!bRegistered)
	{
		return;
	}

	sqlite3_vfs_unregister(&GESQLUnrealVFS);
	bRegistered = false;
	UE_LOG(LogExtendedSQL, Log, TEXT("Unregistered custom SQLite VFS: %hs"), ESQL_VFS_NAME);
}

const char* FESQLUnrealVFS::GetVFSName()
{
	return ESQL_VFS_NAME;
}

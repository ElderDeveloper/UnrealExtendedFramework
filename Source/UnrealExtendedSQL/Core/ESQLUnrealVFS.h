// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Custom SQLite VFS (Virtual File System) backed by FPlatformFileManager.
 *
 * SQLite normally uses raw C fopen/fread/fwrite for file I/O. This doesn't
 * work on all UE platforms (consoles have sandboxed file systems, mobile
 * has specific path restrictions).
 *
 * This VFS replaces all file I/O with Unreal's FPlatformFileManager,
 * ensuring cross-platform compatibility (PC, PS5, Xbox GDK, Switch, iOS, Android).
 *
 * Usage:
 *   Call FESQLUnrealVFS::Register() once at module startup.
 *   FESQLDatabase::Open() uses the registered VFS name automatically.
 */
class UNREALEXTENDEDSQL_API FESQLUnrealVFS
{
public:

	/** Register the custom VFS with SQLite. Call once at module startup.
	    Returns true on success. */
	static bool Register();

	/** Unregister the custom VFS. Call at module shutdown. */
	static void Unregister();

	/** Returns the VFS name used for sqlite3_open_v2(). */
	static const char* GetVFSName();

private:

	/** Whether the VFS has been registered. */
	static bool bRegistered;
};

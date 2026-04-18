// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedSQL.h"
#include "Core/ESQLUnrealVFS.h"
#include "sqlite3.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedSQLModule"

DEFINE_LOG_CATEGORY(LogExtendedSQL);

void FUnrealExtendedSQLModule::StartupModule()
{
	UE_LOG(LogExtendedSQL, Log, TEXT("UnrealExtendedSQL module started — SQLite version: %hs"), sqlite3_libversion());

	// Register custom VFS backed by FPlatformFileManager (cross-platform I/O)
	if (!FESQLUnrealVFS::Register())
	{
		UE_LOG(LogExtendedSQL, Error, TEXT("Failed to register custom SQLite VFS — file-backed databases will not work!"));
	}
}

void FUnrealExtendedSQLModule::ShutdownModule()
{
	// Unregister VFS
	FESQLUnrealVFS::Unregister();

	UE_LOG(LogExtendedSQL, Log, TEXT("UnrealExtendedSQL module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedSQLModule, UnrealExtendedSQL)

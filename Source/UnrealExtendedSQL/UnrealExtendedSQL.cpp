// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedSQL.h"
#include "Private/ThirdParty/ESQLVendorSqlite.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedSQLModule"

DEFINE_LOG_CATEGORY(LogExtendedSQL);

void FUnrealExtendedSQLModule::StartupModule()
{
	UE_LOG(LogExtendedSQL, Log, TEXT("UnrealExtendedSQL module started — SQLite version: %hs"), sqlite3_libversion());
}

void FUnrealExtendedSQLModule::ShutdownModule()
{
	UE_LOG(LogExtendedSQL, Log, TEXT("UnrealExtendedSQL module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedSQLModule, UnrealExtendedSQL)

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Private/Paths/ESQLContentExtractor.h"
#include "Private/Paths/ESQLPathResolver.h"
#include "Shared/ESQLSettings.h"

namespace
{
void CleanupDatabaseArtifacts(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*Path))
	{
		PlatformFile.DeleteFile(*Path);
	}

	PlatformFile.DeleteFile(*(Path + TEXT("-wal")));
	PlatformFile.DeleteFile(*(Path + TEXT("-shm")));
	PlatformFile.DeleteFile(*(Path + TEXT(".tmp")));
	}

void CleanupDirectoryIfEmpty(const FString& Directory)
{
	if (Directory.IsEmpty())
	{
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		return;
	}

	TArray<FString> Entries;
	PlatformFile.FindFiles(Entries, *(Directory / TEXT("*")), nullptr);
	if (Entries.Num() == 0)
	{
		PlatformFile.DeleteDirectory(*Directory);
	}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLLocalDatabaseRoundTripTest, "ExtendedSQL.PhaseB.LocalRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLLocalDatabaseRoundTripTest::RunTest(const FString& Parameters)
{
	const FString DatabasePath = FESQLPathResolver::ResolveDatabasePath(TEXT("PhaseBLocalRoundTrip.db"), EESQLDatabaseScope::Local);
	CleanupDatabaseArtifacts(DatabasePath);

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(DatabasePath);
	TSharedPtr<FESQLDatabase> Database = OpenResult ? OpenResult.GetValue() : nullptr;
	TestTrue(TEXT("Open local database"), Database.IsValid());
	if (!OpenResult)
	{
		AddError(OpenResult.GetErrorMessage());
	}

	if (Database)
	{
		FESQLQueryResult Result = Database->Execute(TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\"Id\" TEXT PRIMARY KEY, \"Quantity\" INTEGER NOT NULL)"));
		TestTrue(TEXT("Create table"), Result.bSuccess);

		Result = Database->Execute(
			TEXT("INSERT INTO \"Items\" (\"Id\", \"Quantity\") VALUES (?1, ?2)"),
			{ FESQLBindingValue::FromText(TEXT("apple")), FESQLBindingValue::FromInteger(7) });
		TestTrue(TEXT("Insert row"), Result.bSuccess);
		Database->Close();
	}

	const FESQLDatabaseOpenResult ReopenResult = FESQLDatabase::OpenReadOnly(DatabasePath);
	Database = ReopenResult ? ReopenResult.GetValue() : nullptr;
	TestTrue(TEXT("Reopen local database read-only"), Database.IsValid());
	if (!ReopenResult)
	{
		AddError(ReopenResult.GetErrorMessage());
	}

	if (Database)
	{
		const FESQLQueryResult QueryResult = Database->Execute(
			TEXT("SELECT \"Quantity\" FROM \"Items\" WHERE \"Id\" = ?1"),
			{ FESQLBindingValue::FromText(TEXT("apple")) });
		TestTrue(TEXT("Query reopened database"), QueryResult.bSuccess);
		TestEqual(TEXT("Reopened row count"), QueryResult.Rows.Num(), 1);

		int64 Quantity = 0;
		const bool bReadQuantity = QueryResult.Rows.Num() == 1
			&& QueryResult.Rows[0].TryGetInt64(TEXT("Quantity"), Quantity);
		TestTrue(TEXT("Read inserted quantity"), bReadQuantity);
		TestEqual(TEXT("Inserted quantity value"), Quantity, static_cast<int64>(7));
		Database->Close();
	}

	CleanupDatabaseArtifacts(DatabasePath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLReadonlyContentExtractionTest, "ExtendedSQL.PhaseB.ReadonlyExtraction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLReadonlyContentExtractionTest::RunTest(const FString& Parameters)
{
	const FString DatabaseName = TEXT("PhaseBReadonlyExtraction");
	const FString SourceDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Database"));
	const FString SourcePath = FPaths::Combine(SourceDirectory, DatabaseName + TEXT(".db"));
	const FString LocalDatabasePath = UESQLSettings::ResolveDatabaseFilePath(DatabaseName);

	CleanupDatabaseArtifacts(SourcePath);
	CleanupDatabaseArtifacts(LocalDatabasePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*SourceDirectory);

	const FESQLDatabaseOpenResult OpenSourceResult = FESQLDatabase::Open(SourcePath);
	TSharedPtr<FESQLDatabase> Database = OpenSourceResult ? OpenSourceResult.GetValue() : nullptr;
	TestTrue(TEXT("Create readonly source database"), Database.IsValid());
	if (!OpenSourceResult)
	{
		AddError(OpenSourceResult.GetErrorMessage());
	}

	if (Database)
	{
		FESQLQueryResult Result = Database->Execute(TEXT("CREATE TABLE IF NOT EXISTS \"Catalog\" (\"Id\" TEXT PRIMARY KEY, \"Name\" TEXT NOT NULL)"));
		TestTrue(TEXT("Create readonly source table"), Result.bSuccess);

		Result = Database->Execute(
			TEXT("INSERT INTO \"Catalog\" (\"Id\", \"Name\") VALUES (?1, ?2)"),
			{ FESQLBindingValue::FromText(TEXT("sword")), FESQLBindingValue::FromText(TEXT("Iron Sword")) });
		TestTrue(TEXT("Insert readonly source row"), Result.bSuccess);
		Database->Close();
	}

	const FESQLResolvedPath ResolvedPath = FESQLPathResolver::ResolveExistingDatabasePathInfo(DatabaseName, true);
	TestTrue(TEXT("Resolve readonly database path"), ResolvedPath.IsValid());
	TestTrue(TEXT("Resolved readonly flag"), ResolvedPath.bReadOnly);
	TestTrue(TEXT("Resolved readonly kind"), ResolvedPath.Kind == EESQLResolvedDatabaseKind::Readonly);
	TestTrue(TEXT("Resolved path exists"), PlatformFile.FileExists(*ResolvedPath.AbsolutePath));
	TestTrue(TEXT("Resolved path extracted into content cache"), FPaths::IsUnderDirectory(ResolvedPath.AbsolutePath, FESQLContentExtractor::GetContentCacheRoot()));

	const FESQLDatabaseOpenResult OpenReadonlyResult = FESQLDatabase::OpenReadOnly(ResolvedPath.AbsolutePath);
	Database = OpenReadonlyResult ? OpenReadonlyResult.GetValue() : nullptr;
	TestTrue(TEXT("Open extracted readonly database"), Database.IsValid());
	if (!OpenReadonlyResult)
	{
		AddError(OpenReadonlyResult.GetErrorMessage());
	}

	if (Database)
	{
		const FESQLQueryResult QueryResult = Database->Execute(
			TEXT("SELECT \"Name\" FROM \"Catalog\" WHERE \"Id\" = ?1"),
			{ FESQLBindingValue::FromText(TEXT("sword")) });
		TestTrue(TEXT("Query extracted readonly database"), QueryResult.bSuccess);
		TestEqual(TEXT("Extracted row count"), QueryResult.Rows.Num(), 1);

		FString Name;
		const bool bReadName = QueryResult.Rows.Num() == 1
			&& QueryResult.Rows[0].TryGetString(TEXT("Name"), Name);
		TestTrue(TEXT("Read extracted row name"), bReadName);
		TestEqual(TEXT("Extracted row name value"), Name, FString(TEXT("Iron Sword")));
		Database->Close();
	}

	CleanupDatabaseArtifacts(SourcePath);
	CleanupDatabaseArtifacts(LocalDatabasePath);
	CleanupDatabaseArtifacts(ResolvedPath.AbsolutePath);
	CleanupDirectoryIfEmpty(FPaths::GetPath(ResolvedPath.AbsolutePath));
	return true;
}

#endif
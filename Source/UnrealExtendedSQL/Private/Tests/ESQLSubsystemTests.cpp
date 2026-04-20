// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/ParallelFor.h"
#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Private/Paths/ESQLPathResolver.h"
#include "Subsystem/ESQLSubsystem.h"

namespace
{
void CleanupDatabaseArtifacts(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteFile(*Path);
	PlatformFile.DeleteFile(*(Path + TEXT("-wal")));
	PlatformFile.DeleteFile(*(Path + TEXT("-shm")));
}

UESQLSubsystem* CreateTestSubsystem()
{
	UESQLSubsystem* Subsystem = NewObject<UESQLSubsystem>();
	Subsystem->AddToRoot();
	return Subsystem;
}

void DestroyTestSubsystem(UESQLSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return;
	}

	Subsystem->CloseAllDatabases();
	Subsystem->RemoveFromRoot();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLPhaseDScopeGateTest, "ExtendedSQL.PhaseD.ScopeGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLPhaseDScopeGateTest::RunTest(const FString& Parameters)
{
	const FString SharedPath = FESQLPathResolver::ResolveDatabasePath(TEXT("PhaseDSharedGate.db"), EESQLDatabaseScope::Shared);
	CleanupDatabaseArtifacts(SharedPath);

	const FString ReadonlyContentPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Database"), TEXT("PhaseDReadonlyGate.db"));
	CleanupDatabaseArtifacts(ReadonlyContentPath);
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(ReadonlyContentPath));

	if (const FESQLDatabaseOpenResult SeedOpenResult = FESQLDatabase::Open(ReadonlyContentPath))
	{
		TSharedPtr<FESQLDatabase> SeedDatabase = SeedOpenResult.GetValue();
		SeedDatabase->Execute(TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\"Id\" TEXT PRIMARY KEY)"));
		SeedDatabase->Close();
	}

	UESQLSubsystem* Subsystem = CreateTestSubsystem();
	Subsystem->SetCachedNetModeForTesting(NM_Client);

	const FESQLQueryResult SharedOpenResult = Subsystem->OpenDatabase(TEXT("PhaseDSharedGate"), EESQLDatabaseScope::Shared);
	TestFalse(TEXT("Clients cannot open shared databases"), SharedOpenResult.bSuccess);
	TestEqual(TEXT("Shared open fails with authority violation"), SharedOpenResult.Error.Code, EESQLErrorCode::AuthorityViolation);

	Subsystem->SetCachedNetModeForTesting(NM_Standalone);
	const FESQLQueryResult ReadonlyOpenResult = Subsystem->OpenDatabase(TEXT("PhaseDReadonlyGate"), EESQLDatabaseScope::Readonly);
	TestTrue(TEXT("Readonly content database opens"), ReadonlyOpenResult.bSuccess);

	const FESQLQueryResult ReadonlyWriteResult = Subsystem->ExecuteSQL(TEXT("PhaseDReadonlyGate"), TEXT("CREATE TABLE \"ShouldFail\" (\"Id\" TEXT)"));
	TestFalse(TEXT("Readonly database rejects writes"), ReadonlyWriteResult.bSuccess);
	TestEqual(TEXT("Readonly write fails with readonly violation"), ReadonlyWriteResult.Error.Code, EESQLErrorCode::ReadonlyViolation);

	const FString ExtractedReadonlyPath = Subsystem->GetDatabaseFilePath(TEXT("PhaseDReadonlyGate"));
	DestroyTestSubsystem(Subsystem);

	CleanupDatabaseArtifacts(SharedPath);
	CleanupDatabaseArtifacts(ReadonlyContentPath);
	CleanupDatabaseArtifacts(ExtractedReadonlyPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLPhaseDConcurrencyTest, "ExtendedSQL.PhaseD.Concurrency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLPhaseDConcurrencyTest::RunTest(const FString& Parameters)
{
	const FString DatabaseName = TEXT("PhaseDConcurrency");
	const FString DatabasePath = FESQLPathResolver::ResolveDatabasePath(DatabaseName + TEXT(".db"), EESQLDatabaseScope::Local);
	CleanupDatabaseArtifacts(DatabasePath);

	UESQLSubsystem* Subsystem = CreateTestSubsystem();
	Subsystem->SetCachedNetModeForTesting(NM_Standalone);

	FESQLQueryResult Result = Subsystem->OpenDatabase(DatabaseName, EESQLDatabaseScope::Local);
	TestTrue(TEXT("Open concurrency database"), Result.bSuccess);

	Result = Subsystem->ExecuteSQL(DatabaseName, TEXT("CREATE TABLE IF NOT EXISTS \"Events\" (\"Id\" INTEGER PRIMARY KEY, \"Value\" TEXT NOT NULL)"));
	TestTrue(TEXT("Create concurrency table"), Result.bSuccess);

	TAtomic<int32> FailureCount = 0;
	ParallelFor(1000, [Subsystem, &FailureCount, DatabaseName](const int32 Index)
	{
		if ((Index % 2) == 0)
		{
			const FESQLQueryResult WriteResult = Subsystem->ExecuteSQLWithValues(
				DatabaseName,
				TEXT("INSERT INTO \"Events\" (\"Id\", \"Value\") VALUES (?1, ?2) ON CONFLICT(\"Id\") DO UPDATE SET \"Value\"=excluded.\"Value\""),
				{
					FESQLBindingValue::FromInteger(Index / 2),
					FESQLBindingValue::FromText(FString::Printf(TEXT("Value_%d"), Index))
				});

			if (!WriteResult.bSuccess)
			{
				FailureCount.IncrementExchange();
			}
		}
		else
		{
			const FESQLQueryResult ReadResult = Subsystem->QuerySQL(
				DatabaseName,
				TEXT("SELECT COUNT(*) AS RowCount FROM \"Events\""),
				{});

			if (!ReadResult.bSuccess)
			{
				FailureCount.IncrementExchange();
			}
		}
	});

	TestEqual(TEXT("Mixed concurrent operations succeed"), FailureCount.Load(), 0);

	const FESQLQueryResult CountResult = Subsystem->QuerySQL(DatabaseName, TEXT("SELECT COUNT(*) AS RowCount FROM \"Events\""), {});
	TestTrue(TEXT("Count query after concurrent traffic succeeds"), CountResult.bSuccess);

	int64 RowCount = 0;
	if (const FESQLRow* Row = CountResult.GetFirstRow())
	{
		Row->TryGetInt64(TEXT("RowCount"), RowCount);
	}

	TestEqual(TEXT("Expected number of unique event rows"), RowCount, static_cast<int64>(500));

	DestroyTestSubsystem(Subsystem);
	CleanupDatabaseArtifacts(DatabasePath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLPhaseDTransactionNestingTest, "ExtendedSQL.PhaseD.TransactionNesting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLPhaseDTransactionNestingTest::RunTest(const FString& Parameters)
{
	UESQLSubsystem* Subsystem = CreateTestSubsystem();
	Subsystem->SetCachedNetModeForTesting(NM_Standalone);

	FESQLQueryResult Result = Subsystem->OpenDatabase(TEXT("PhaseDTransactionNesting"), EESQLDatabaseScope::Local, EESQLDatabasePersistence::Session);
	TestTrue(TEXT("Open session database for transaction nesting"), Result.bSuccess);

	Result = Subsystem->ExecuteSQL(TEXT("PhaseDTransactionNesting"), TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\"Id\" TEXT PRIMARY KEY)"));
	TestTrue(TEXT("Create transaction table"), Result.bSuccess);

	FESQLTransactionHandle OuterHandle;
	Result = Subsystem->BeginTransaction(TEXT("PhaseDTransactionNesting"), OuterHandle);
	TestTrue(TEXT("Begin outer transaction"), Result.bSuccess);

	Result = Subsystem->ExecuteSQLWithValues(
		TEXT("PhaseDTransactionNesting"),
		TEXT("INSERT INTO \"Items\" (\"Id\") VALUES (?1)"),
		{ FESQLBindingValue::FromText(TEXT("outer")) });
	TestTrue(TEXT("Insert outer row inside outer transaction"), Result.bSuccess);

	FESQLTransactionHandle InnerHandle;
	Result = Subsystem->BeginTransaction(TEXT("PhaseDTransactionNesting"), InnerHandle);
	TestTrue(TEXT("Begin inner transaction"), Result.bSuccess);

	Result = Subsystem->ExecuteSQLWithValues(
		TEXT("PhaseDTransactionNesting"),
		TEXT("INSERT INTO \"Items\" (\"Id\") VALUES (?1)"),
		{ FESQLBindingValue::FromText(TEXT("inner")) });
	TestTrue(TEXT("Insert inner row inside nested transaction"), Result.bSuccess);

	Result = Subsystem->RollbackTransaction(InnerHandle);
	TestTrue(TEXT("Rollback inner transaction"), Result.bSuccess);

	Result = Subsystem->CommitTransaction(OuterHandle);
	TestTrue(TEXT("Commit outer transaction"), Result.bSuccess);

	const FESQLQueryResult QueryResult = Subsystem->QuerySQL(
		TEXT("PhaseDTransactionNesting"),
		TEXT("SELECT \"Id\" FROM \"Items\" ORDER BY \"Id\" ASC"),
		{});
	TestTrue(TEXT("Query rows after nested transaction sequence"), QueryResult.bSuccess);
	TestEqual(TEXT("Only outer row survives nested rollback"), QueryResult.Rows.Num(), 1);

	if (QueryResult.Rows.Num() == 1)
	{
		FString RowId;
		QueryResult.Rows[0].TryGetString(TEXT("Id"), RowId);
		TestEqual(TEXT("Remaining row id"), RowId, FString(TEXT("outer")));
	}

	DestroyTestSubsystem(Subsystem);
	return true;
}

#endif
// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Private/Paths/ESQLPathResolver.h"

namespace
{
void CleanupCoreTestDatabase(const FString& Path)
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
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLPhaseCPragmaAndErrorTest, "ExtendedSQL.PhaseC.PragmasAndErrors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLPhaseCPragmaAndErrorTest::RunTest(const FString& Parameters)
{
	const FString DatabasePath = FESQLPathResolver::ResolveDatabasePath(TEXT("PhaseCPragmas.db"), EESQLDatabaseScope::Local);
	CleanupCoreTestDatabase(DatabasePath);

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::Open(DatabasePath);
	TSharedPtr<FESQLDatabase> Database = OpenResult ? OpenResult.GetValue() : nullptr;
	TestTrue(TEXT("Open phase C database"), Database.IsValid());
	if (!OpenResult)
	{
		AddError(OpenResult.GetErrorMessage());
	}

	if (Database)
	{
		const FESQLQueryResult JournalModeResult = Database->Execute(TEXT("PRAGMA journal_mode"));
		TestTrue(TEXT("Read journal mode"), JournalModeResult.bSuccess);
		TestEqual(TEXT("Journal mode row count"), JournalModeResult.Rows.Num(), 1);

		FString JournalMode;
		const bool bReadJournalMode = JournalModeResult.Rows.Num() == 1
			&& JournalModeResult.Rows[0].TryGetString(TEXT("journal_mode"), JournalMode);
		TestTrue(TEXT("Read journal mode value"), bReadJournalMode);
		TestEqual(TEXT("Journal mode value"), JournalMode, FString(TEXT("wal")));

		const FESQLQueryResult ForeignKeyResult = Database->Execute(TEXT("PRAGMA foreign_keys"));
		TestTrue(TEXT("Read foreign key pragma"), ForeignKeyResult.bSuccess);
		TestEqual(TEXT("Foreign key row count"), ForeignKeyResult.Rows.Num(), 1);

		int64 ForeignKeysEnabled = 0;
		const bool bReadForeignKeys = ForeignKeyResult.Rows.Num() == 1
			&& ForeignKeyResult.Rows[0].TryGetInt64(TEXT("foreign_keys"), ForeignKeysEnabled);
		TestTrue(TEXT("Read foreign key value"), bReadForeignKeys);
		TestEqual(TEXT("Foreign keys enabled"), ForeignKeysEnabled, static_cast<int64>(1));

		const FESQLQueryResult FailureResult = Database->Execute(TEXT("SELECT FROM"));
		TestFalse(TEXT("Malformed SQL fails"), FailureResult.bSuccess);
		TestFalse(TEXT("Malformed SQL returns message"), FailureResult.ErrorMessage.IsEmpty());
		TestEqual(TEXT("Malformed SQL error code"), FailureResult.Error.Code, EESQLErrorCode::PrepareFailed);
		TestEqual(TEXT("Malformed SQL error fragment"), FailureResult.Error.SqlFragment, FString(TEXT("SELECT FROM")));

		Database->Close();
	}

	CleanupCoreTestDatabase(DatabasePath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLStatementEnumerateRowsTest, "ExtendedSQL.PhaseC.StatementEnumerateRows", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLStatementEnumerateRowsTest::RunTest(const FString& Parameters)
{
	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::OpenInMemory();
	TSharedPtr<FESQLDatabase> Database = OpenResult ? OpenResult.GetValue() : nullptr;
	TestTrue(TEXT("Open in-memory database"), Database.IsValid());
	if (!OpenResult)
	{
		AddError(OpenResult.GetErrorMessage());
	}

	if (Database)
	{
		FESQLQueryResult Result = Database->Execute(TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\"Id\" TEXT PRIMARY KEY, \"Quantity\" INTEGER NOT NULL)"));
		TestTrue(TEXT("Create table for enumeration"), Result.bSuccess);

		Result = Database->Execute(
			TEXT("INSERT INTO \"Items\" (\"Id\", \"Quantity\") VALUES (?1, ?2), (?3, ?4)"),
			{
				FESQLBindingValue::FromText(TEXT("apple")),
				FESQLBindingValue::FromInteger(7),
				FESQLBindingValue::FromText(TEXT("pear")),
				FESQLBindingValue::FromInteger(3)
			});
		TestTrue(TEXT("Insert rows for enumeration"), Result.bSuccess);

		const FESQLStatementPrepareResult PrepareResult = Database->Prepare(TEXT("SELECT \"Id\", \"Quantity\" FROM \"Items\" ORDER BY \"Id\""));
		TSharedPtr<FESQLStatement> Statement = PrepareResult ? PrepareResult.GetValue() : nullptr;
		TestTrue(TEXT("Prepare enumeration statement"), Statement.IsValid());
		if (!PrepareResult)
		{
			AddError(PrepareResult.GetErrorMessage());
		}

		if (Statement)
		{
			TArray<FString> Ids;
			TArray<int64> Quantities;
			FESQLError EnumerateError;
			bool bVisitorReadSucceeded = true;
			const bool bEnumerated = Statement->EnumerateRows(
				[&Ids, &Quantities, &bVisitorReadSucceeded](const FESQLRow& Row)
				{
					FString Id;
					int64 Quantity = 0;
					if (!Row.TryGetString(TEXT("Id"), Id) || !Row.TryGetInt64(TEXT("Quantity"), Quantity))
					{
						bVisitorReadSucceeded = false;
						return true;
					}

					Ids.Add(Id);
					Quantities.Add(Quantity);
					return true;
				},
				&EnumerateError);

			TestTrue(TEXT("Enumerate rows succeeds"), bEnumerated);
			TestTrue(TEXT("Enumerated rows read expected fields"), bVisitorReadSucceeded);
			if (EnumerateError.HasError())
			{
				AddError(EnumerateError.Message);
			}

			TestEqual(TEXT("Enumerated row count"), Ids.Num(), 2);
			if (Ids.Num() == 2)
			{
				TestEqual(TEXT("First enumerated id"), Ids[0], FString(TEXT("apple")));
				TestEqual(TEXT("Second enumerated id"), Ids[1], FString(TEXT("pear")));
			}

			if (Quantities.Num() == 2)
			{
				TestEqual(TEXT("First enumerated quantity"), Quantities[0], static_cast<int64>(7));
				TestEqual(TEXT("Second enumerated quantity"), Quantities[1], static_cast<int64>(3));
			}
		}

		Database->Close();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLStatementBindStepResetTest, "ExtendedSQL.PhaseC.StatementBindStepReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLStatementBindStepResetTest::RunTest(const FString& Parameters)
{
	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::OpenInMemory();
	TSharedPtr<FESQLDatabase> Database = OpenResult ? OpenResult.GetValue() : nullptr;
	TestTrue(TEXT("Open in-memory database for bind/step test"), Database.IsValid());
	if (!OpenResult)
	{
		AddError(OpenResult.GetErrorMessage());
	}

	if (Database)
	{
		FESQLQueryResult Result = Database->Execute(TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\"Id\" TEXT PRIMARY KEY, \"Quantity\" INTEGER NOT NULL)"));
		TestTrue(TEXT("Create table for bind/step test"), Result.bSuccess);

		const FESQLStatementPrepareResult InsertPrepareResult = Database->Prepare(
			TEXT("INSERT INTO \"Items\" (\"Id\", \"Quantity\") VALUES (?1, ?2)"));
		TSharedPtr<FESQLStatement> InsertStatement = InsertPrepareResult ? InsertPrepareResult.GetValue() : nullptr;
		TestTrue(TEXT("Prepare insert statement"), InsertStatement.IsValid());
		if (!InsertPrepareResult)
		{
			AddError(InsertPrepareResult.GetErrorMessage());
		}

		if (InsertStatement)
		{
			TestTrue(TEXT("Bind insert id"), InsertStatement->BindText(1, TEXT("apple")));
			TestTrue(TEXT("Bind insert quantity"), InsertStatement->BindInt(2, 7));
			TestFalse(TEXT("Insert statement reaches SQLITE_DONE"), InsertStatement->Step());
			TestTrue(TEXT("Insert statement done state"), InsertStatement->IsDone());
			InsertStatement->Reset();
			TestFalse(TEXT("Insert statement reset clears done state"), InsertStatement->IsDone());
			TestTrue(TEXT("Insert statement clear bindings"), InsertStatement->ClearBindings());
		}

		const FESQLStatementPrepareResult SelectPrepareResult = Database->Prepare(
			TEXT("SELECT \"Id\", \"Quantity\" FROM \"Items\" WHERE \"Id\" = ?1"));
		TSharedPtr<FESQLStatement> SelectStatement = SelectPrepareResult ? SelectPrepareResult.GetValue() : nullptr;
		TestTrue(TEXT("Prepare select statement"), SelectStatement.IsValid());
		if (!SelectPrepareResult)
		{
			AddError(SelectPrepareResult.GetErrorMessage());
		}

		if (SelectStatement)
		{
			TestTrue(TEXT("Bind select id"), SelectStatement->BindText(1, TEXT("apple")));
			TestTrue(TEXT("Select statement produces row"), SelectStatement->Step());
			TestEqual(TEXT("Selected id"), SelectStatement->GetColumnText(0), FString(TEXT("apple")));
			TestEqual(TEXT("Selected quantity"), SelectStatement->GetColumnInt(1), static_cast<int64>(7));
			TestFalse(TEXT("Select statement done after row"), SelectStatement->IsDone());
			TestFalse(TEXT("Select statement reaches done after final step"), SelectStatement->Step());
			TestTrue(TEXT("Select statement done state"), SelectStatement->IsDone());
			SelectStatement->Reset();
			TestTrue(TEXT("Select statement clear bindings"), SelectStatement->ClearBindings());
		}

		Database->Close();
	}

	return true;
}

#endif
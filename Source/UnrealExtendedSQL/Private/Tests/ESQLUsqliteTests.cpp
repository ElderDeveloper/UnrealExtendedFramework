// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/ESQLDatabase.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Private/USqlite/ESQLUsqliteBuilder.h"
#include "Private/USqlite/ESQLUsqliteSerializer.h"

namespace
{
FESQLUsqliteProject MakeSampleProject()
{
	FESQLUsqliteProject Project;
	Project.ProjectName = TEXT("SampleProject");

	FESQLUsqliteTableSchema Table;
	Table.TableName = TEXT("Items");
	Table.PrimaryKeyColumn = TEXT("RowName");
	Table.LabelColumn = TEXT("DisplayName");
	Table.Columns = {
		{ TEXT("RowName"), TEXT("TEXT"), false, true, FString() },
		{ TEXT("DisplayName"), TEXT("TEXT"), true, false, FString() },
		{ TEXT("Price"), TEXT("INTEGER"), true, false, FString() }
	};
	Project.Tables.Add(Table);

	FESQLUsqliteRow FirstRow;
	FirstRow.Values.Add(TEXT("RowName"), FESQLBindingValue::FromText(TEXT("item_001")));
	FirstRow.Values.Add(TEXT("DisplayName"), FESQLBindingValue::FromText(TEXT("Iron Sword")));
	FirstRow.Values.Add(TEXT("Price"), FESQLBindingValue::FromInteger(100));

	FESQLUsqliteRow SecondRow;
	SecondRow.Values.Add(TEXT("RowName"), FESQLBindingValue::FromText(TEXT("item_002")));
	SecondRow.Values.Add(TEXT("DisplayName"), FESQLBindingValue::FromText(TEXT("Steel Shield")));
	SecondRow.Values.Add(TEXT("Price"), FESQLBindingValue::FromInteger(250));

	Project.Data.Add(TEXT("Items"), { FirstRow, SecondRow });

	FESQLUsqliteMigration Migration;
	Migration.Sequence = 1;
	Migration.Slug = TEXT("initial");
	Migration.FileName = TEXT("0001_initial.sql");
	Migration.RelativePath = TEXT("migrations/0001_initial.sql");
	Migration.Sql =
		TEXT("CREATE TABLE IF NOT EXISTS \"Items\" (\n")
		TEXT("  \"RowName\" TEXT PRIMARY KEY,\n")
		TEXT("  \"DisplayName\" TEXT,\n")
		TEXT("  \"Price\" INTEGER\n")
		TEXT(");\n");
	Project.Migrations.Add(Migration);

	return Project;
}

FString MakeTestRoot(const FString& TestName)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedSQLTests"), TestName + TEXT(".usqlite"));
}

void CleanupPath(const FString& Path)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*Path))
	{
		PlatformFile.DeleteDirectoryRecursively(*Path);
	}

	if (PlatformFile.FileExists(*Path))
	{
		PlatformFile.DeleteFile(*Path);
	}

	PlatformFile.DeleteFile(*(Path + TEXT("-wal")));
	PlatformFile.DeleteFile(*(Path + TEXT("-shm")));
}

void LoadManagedFiles(const FString& Root, TMap<FString, FString>& OutFiles)
{
	OutFiles.Empty();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> AbsoluteFiles;
	PlatformFile.FindFilesRecursively(AbsoluteFiles, *Root, TEXT("*"));
	for (const FString& AbsoluteFile : AbsoluteFiles)
	{
		if (PlatformFile.DirectoryExists(*AbsoluteFile))
		{
			continue;
		}

		FString Content;
		if (FFileHelper::LoadFileToString(Content, *AbsoluteFile))
		{
			FString RelativePath = AbsoluteFile;
			RelativePath.RemoveFromStart(Root);
			RelativePath.RemoveFromStart(TEXT("\\"));
			RelativePath.RemoveFromStart(TEXT("/"));
			RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
			OutFiles.Add(RelativePath, Content);
		}
	}
}

bool CompareFileMaps(FAutomationTestBase& Test, const TMap<FString, FString>& Left, const TMap<FString, FString>& Right)
{
	bool bMatch = true;
	Test.TestEqual(TEXT("Managed file count"), Left.Num(), Right.Num());

	for (const TPair<FString, FString>& Pair : Left)
	{
		const FString* RightContent = Right.Find(Pair.Key);
		if (!RightContent)
		{
			Test.AddError(FString::Printf(TEXT("Missing file in comparison set: %s"), *Pair.Key));
			bMatch = false;
			continue;
		}

		if (*RightContent != Pair.Value)
		{
			Test.AddError(FString::Printf(TEXT("File contents differ for %s"), *Pair.Key));
			bMatch = false;
		}
	}

	return bMatch;
	}

bool FileHasUtf8Bom(const FString& AbsolutePath)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath) || Bytes.Num() < 3)
	{
		return false;
	}

	return Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteRoundTripTest, "ExtendedSQL.Usqlite.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteRoundTripTest::RunTest(const FString& Parameters)
{
	const FString RootA = MakeTestRoot(TEXT("RoundTripA"));
	const FString RootB = MakeTestRoot(TEXT("RoundTripB"));
	CleanupPath(RootA);
	CleanupPath(RootB);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save initial project"), FESQLUsqliteSerializer::SaveProject(Project, RootA, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FESQLUsqliteProject LoadedProject;
	Error.Reset();
	TestTrue(TEXT("Load saved project"), FESQLUsqliteSerializer::LoadProject(RootA, LoadedProject, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	Error.Reset();
	TestTrue(TEXT("Save loaded project"), FESQLUsqliteSerializer::SaveProject(LoadedProject, RootB, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TMap<FString, FString> FilesA;
	TMap<FString, FString> FilesB;
	LoadManagedFiles(RootA, FilesA);
	LoadManagedFiles(RootB, FilesB);
	CompareFileMaps(*this, FilesA, FilesB);

	CleanupPath(RootA);
	CleanupPath(RootB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteBuilderDeterminismTest, "ExtendedSQL.Usqlite.BuilderDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteBuilderDeterminismTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("BuilderDeterminism"));
	const FString DbPathA = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedSQLTests"), TEXT("BuilderA.db"));
	const FString DbPathB = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedSQLTests"), TEXT("BuilderB.db"));
	CleanupPath(Root);
	CleanupPath(DbPathA);
	CleanupPath(DbPathB);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project for builder"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FString ResolvedPathA;
	Error.Reset();
	TestTrue(TEXT("Build database A"), FESQLUsqliteBuilder::BuildProjectRoot(Root, DbPathA, ResolvedPathA, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FString ResolvedPathB;
	Error.Reset();
	TestTrue(TEXT("Build database B"), FESQLUsqliteBuilder::BuildProjectRoot(Root, DbPathB, ResolvedPathB, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TArray<uint8> BytesA;
	TArray<uint8> BytesB;
	TestTrue(TEXT("Load database A bytes"), FFileHelper::LoadFileToArray(BytesA, *ResolvedPathA));
	TestTrue(TEXT("Load database B bytes"), FFileHelper::LoadFileToArray(BytesB, *ResolvedPathB));
	TestEqual(TEXT("Built database byte size"), BytesA.Num(), BytesB.Num());
	TestTrue(TEXT("Built database bytes match"), BytesA == BytesB);

	const FESQLDatabaseOpenResult OpenResult = FESQLDatabase::OpenReadOnly(ResolvedPathA);
	TSharedPtr<FESQLDatabase> Database = OpenResult ? OpenResult.GetValue() : nullptr;
	TestTrue(TEXT("Open built database"), Database.IsValid());
	if (!OpenResult)
	{
		AddError(OpenResult.GetErrorMessage());
	}
	if (Database)
	{
		const FESQLQueryResult QueryResult = Database->Execute(TEXT("SELECT \"RowName\", \"Price\" FROM \"Items\" ORDER BY \"RowName\""));
		TestTrue(TEXT("Query built database"), QueryResult.bSuccess);
		TestEqual(TEXT("Built row count"), QueryResult.Rows.Num(), 2);
		Database->Close();
	}

	CleanupPath(Root);
	CleanupPath(DbPathA);
	CleanupPath(DbPathB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteBuilderUsesDerivedPathTest, "ExtendedSQL.Usqlite.BuilderUsesDerivedPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteBuilderUsesDerivedPathTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("BuilderDerivedPath"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project for derived build"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FString ProjectHash;
	Error.Reset();
	TestTrue(TEXT("Compute project hash for derived build"), FESQLUsqliteSerializer::ComputeProjectHash(Project, ProjectHash, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	const FString ExpectedDerivedPath = FESQLUsqliteSerializer::GetDerivedDatabasePath(Root, ProjectHash);
	CleanupPath(ExpectedDerivedPath);

	FString ResolvedPath;
	Error.Reset();
	TestTrue(TEXT("Build project using derived path"), FESQLUsqliteBuilder::BuildProjectRoot(Root, FString(), ResolvedPath, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TestEqual(TEXT("Derived build path matches project hash path"), ResolvedPath, ExpectedDerivedPath);
	TestTrue(TEXT("Derived database file exists"), IFileManager::Get().FileExists(*ResolvedPath));

	CleanupPath(Root);
	CleanupPath(ExpectedDerivedPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteBuilderReusesDerivedCacheTest, "ExtendedSQL.Usqlite.BuilderReusesDerivedCache", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteBuilderReusesDerivedCacheTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("BuilderReusesDerivedCache"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project for cache reuse"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FString FirstResolvedPath;
	Error.Reset();
	TestTrue(TEXT("Build derived database first time"), FESQLUsqliteBuilder::BuildProjectRoot(Root, FString(), FirstResolvedPath, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TestTrue(TEXT("Mark derived database read-only"), PlatformFile.SetReadOnly(*FirstResolvedPath, true));

	FString SecondResolvedPath;
	Error.Reset();
	const bool bReusedCache = FESQLUsqliteBuilder::BuildProjectRoot(Root, FString(), SecondResolvedPath, Error);
	PlatformFile.SetReadOnly(*FirstResolvedPath, false);

	TestTrue(TEXT("Reuse derived cache without rebuilding"), bReusedCache);
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	TestEqual(TEXT("Reused cache path matches original derived path"), SecondResolvedPath, FirstResolvedPath);

	CleanupPath(Root);
	CleanupPath(FirstResolvedPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqlitePrettyJsonFormattingTest, "ExtendedSQL.Usqlite.PrettyJsonFormatting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqlitePrettyJsonFormattingTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("PrettyJsonFormatting"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project for formatting"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	const FString ProjectJsonPath = FPaths::Combine(Root, TEXT("project.json"));
	const FString SchemaJsonPath = FPaths::Combine(Root, TEXT("schema/Items.json"));
	const FString LockJsonPath = FPaths::Combine(Root, TEXT(".usqlite-lock.json"));

	FString ProjectJson;
	FString SchemaJson;
	FString LockJson;
	TestTrue(TEXT("Load project.json for formatting"), FFileHelper::LoadFileToString(ProjectJson, *ProjectJsonPath));
	TestTrue(TEXT("Load schema json for formatting"), FFileHelper::LoadFileToString(SchemaJson, *SchemaJsonPath));
	TestTrue(TEXT("Load lock json for formatting"), FFileHelper::LoadFileToString(LockJson, *LockJsonPath));

	TestFalse(TEXT("project.json uses tabs"), ProjectJson.Contains(TEXT("\t")));
	TestFalse(TEXT("schema json uses tabs"), SchemaJson.Contains(TEXT("\t")));
	TestFalse(TEXT("lock json uses tabs"), LockJson.Contains(TEXT("\t")));
	TestTrue(TEXT("project.json has two-space indent"), ProjectJson.Contains(TEXT("\n  \"tables\"")));
	TestFalse(TEXT("project.json has no UTF-8 BOM"), FileHasUtf8Bom(ProjectJsonPath));
	TestFalse(TEXT("schema json has no UTF-8 BOM"), FileHasUtf8Bom(SchemaJsonPath));
	TestFalse(TEXT("lock json has no UTF-8 BOM"), FileHasUtf8Bom(LockJsonPath));

	CleanupPath(Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteValidatorMutationTest, "ExtendedSQL.Usqlite.ValidatorMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteValidatorMutationTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("ValidatorMutation"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save initial validator project"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));

	FESQLUsqliteProject LoadedProject;
	Error.Reset();
	TestTrue(TEXT("Load validator project"), FESQLUsqliteSerializer::LoadProject(Root, LoadedProject, Error));

	LoadedProject.Migrations[0].Sql += TEXT("-- mutated\n");
	Error.Reset();
	TestFalse(TEXT("Reject migration mutation"), FESQLUsqliteSerializer::SaveProject(LoadedProject, Root, Error));
	TestTrue(TEXT("Mutation error mentions append-only"), Error.Contains(TEXT("append-only")));

	CleanupPath(Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteValidatorDuplicateMigrationSequenceTest, "ExtendedSQL.Usqlite.ValidatorDuplicateMigrationSequence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteValidatorDuplicateMigrationSequenceTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("DuplicateMigrationSequence"));
	CleanupPath(Root);

	FESQLUsqliteProject Project = MakeSampleProject();
	FESQLUsqliteMigration DuplicateMigration;
	DuplicateMigration.Sequence = 1;
	DuplicateMigration.Slug = TEXT("duplicate");
	DuplicateMigration.FileName = TEXT("0001_duplicate.sql");
	DuplicateMigration.RelativePath = TEXT("migrations/0001_duplicate.sql");
	DuplicateMigration.Sql = TEXT("CREATE TABLE IF NOT EXISTS \"Duplicate\" (\"Id\" TEXT PRIMARY KEY);\n");
	Project.Migrations.Add(DuplicateMigration);

	FString Error;
	TestFalse(TEXT("Reject duplicate migration sequence"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	TestTrue(TEXT("Duplicate sequence error mentions duplicate migration"), Error.Contains(TEXT("Duplicate migration sequence")));

	CleanupPath(Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteLockHashAlgorithmTest, "ExtendedSQL.Usqlite.LockHashAlgorithm", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteLockHashAlgorithmTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("LockHashAlgorithm"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project for lock hash"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	FESQLUsqliteProject LoadedProject;
	Error.Reset();
	TestTrue(TEXT("Load project for lock hash"), FESQLUsqliteSerializer::LoadProject(Root, LoadedProject, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	TestEqual(TEXT("Lock hash algorithm is SHA256"), LoadedProject.Lock.HashAlgorithm, FString(TEXT("sha256")));

	CleanupPath(Root);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteBuilderRejectsTamperedMigrationTest, "ExtendedSQL.Usqlite.BuilderRejectsTamperedMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteBuilderRejectsTamperedMigrationTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("TamperedMigration"));
	const FString DbPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedSQLTests"), TEXT("TamperedMigration.db"));
	CleanupPath(Root);
	CleanupPath(DbPath);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save project before tamper"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}

	const FString MigrationPath = FPaths::Combine(Root, TEXT("migrations/0001_initial.sql"));
	FString MigrationContent;
	TestTrue(TEXT("Load migration for tamper"), FFileHelper::LoadFileToString(MigrationContent, *MigrationPath));
	MigrationContent += TEXT("-- tampered\n");
	TestTrue(TEXT("Write tampered migration"), FFileHelper::SaveStringToFile(MigrationContent, *MigrationPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FString ResolvedPath;
	Error.Reset();
	TestFalse(TEXT("Reject tampered migration during build"), FESQLUsqliteBuilder::BuildProjectRoot(Root, DbPath, ResolvedPath, Error));
	TestTrue(TEXT("Tamper error mentions append-only"), Error.Contains(TEXT("append-only")) || Error.Contains(TEXT("changed")));

	CleanupPath(Root);
	CleanupPath(DbPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FESQLUsqliteValidatorSchemaDriftTest, "ExtendedSQL.Usqlite.ValidatorSchemaDrift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FESQLUsqliteValidatorSchemaDriftTest::RunTest(const FString& Parameters)
{
	const FString Root = MakeTestRoot(TEXT("ValidatorSchemaDrift"));
	CleanupPath(Root);

	const FESQLUsqliteProject Project = MakeSampleProject();
	FString Error;
	TestTrue(TEXT("Save initial schema project"), FESQLUsqliteSerializer::SaveProject(Project, Root, Error));

	FESQLUsqliteProject LoadedProject;
	Error.Reset();
	TestTrue(TEXT("Load schema project"), FESQLUsqliteSerializer::LoadProject(Root, LoadedProject, Error));

	LoadedProject.Tables[0].Columns.Add({ TEXT("Rarity"), TEXT("TEXT"), true, false, FString() });
	Error.Reset();
	TestFalse(TEXT("Reject schema drift without migration"), FESQLUsqliteSerializer::SaveProject(LoadedProject, Root, Error));
	TestTrue(TEXT("Schema drift error mentions migration"), Error.Contains(TEXT("migration")));

	CleanupPath(Root);
	return true;
}

#endif
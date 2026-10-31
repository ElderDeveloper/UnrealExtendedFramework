// Copyright Moon Punch Games. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Libraries/File/EFFileAccessSettings.h"
#include "Libraries/File/EFFileLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

namespace EFFileLibraryTests
{
	/** Puts the file access settings back as they were when it goes out of scope. */
	class FScopedFileAccessSettings
	{
	public:
		FScopedFileAccessSettings()
			: AccessSettings(GetMutableDefault<UEFFileAccessSettings>())
			, SavedMode(AccessSettings->RestrictionMode)
			, SavedDirectories(AccessSettings->AdditionalAllowedDirectories)
		{
		}

		~FScopedFileAccessSettings()
		{
			AccessSettings->RestrictionMode = SavedMode;
			AccessSettings->AdditionalAllowedDirectories = SavedDirectories;
		}

		FScopedFileAccessSettings(const FScopedFileAccessSettings&) = delete;
		FScopedFileAccessSettings& operator=(const FScopedFileAccessSettings&) = delete;

		/** Sets the mode and replaces the allowed directories, so the project's own config cannot change the results. */
		void Apply(EEFFileAccessRestriction Mode, const TArray<FString>& AllowedDirectories = TArray<FString>())
		{
			AccessSettings->RestrictionMode = Mode;
			AccessSettings->AdditionalAllowedDirectories = AllowedDirectories;
		}

	private:
		UEFFileAccessSettings* AccessSettings;
		EEFFileAccessRestriction SavedMode;
		TArray<FString> SavedDirectories;
	};

	/** A uniquely named folder under BaseDirectory, deleted with everything in it when this goes out of scope. */
	class FScopedTestFolder
	{
	public:
		explicit FScopedTestFolder(const FString& BaseDirectory)
			: FolderPath(FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseDirectory,
				FString::Printf(TEXT("EFFileLibraryTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)))))
		{
			IFileManager::Get().MakeDirectory(*FolderPath, true);
		}

		~FScopedTestFolder()
		{
			IFileManager::Get().DeleteDirectory(*FolderPath, false, true);
		}

		FScopedTestFolder(const FScopedTestFolder&) = delete;
		FScopedTestFolder& operator=(const FScopedTestFolder&) = delete;

		const FString& GetPath() const { return FolderPath; }

		FString PathOf(const TCHAR* RelativePath) const { return FPaths::Combine(FolderPath, FString(RelativePath)); }

	private:
		FString FolderPath;
	};

	/** The root of the file system the project is on ("/" or "C:/"). No allowed directory contains it. */
	static FString FileSystemRoot()
	{
		const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		int32 SlashIndex = INDEX_NONE;
		return ProjectPath.FindChar(TEXT('/'), SlashIndex) ? ProjectPath.Left(SlashIndex + 1) : FString(TEXT("/"));
	}

	/** An absolute path in a uniquely named folder at the file system root, outside every allowed directory. */
	static FString OutsidePath(const TCHAR* FileName)
	{
		return FileSystemRoot() + FString::Printf(TEXT("EFFileLibraryOutside_%s/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits), FileName);
	}

	/** A path that starts in Saved and climbs above the file system root with "..". */
	static FString EscapeAboveRootFromSaved()
	{
		FString EscapePath = FPaths::ProjectSavedDir();
		for (int32 Level = 0; Level < 64; ++Level)
		{
			EscapePath += TEXT("../");
		}
		return EscapePath + TEXT("EFFileLibraryEscape.txt");
	}

	/** True when Directory is a real folder below the file system root, not the root itself. */
	static bool IsBelowFileSystemRoot(const FString& Directory)
	{
		FString FullDirectory = FPaths::ConvertRelativePathToFull(Directory);
		FullDirectory.RemoveFromEnd(TEXT("/"));
		return FullDirectory.Contains(TEXT("/"));
	}

	/** Expects IsPathAllowed to answer bExpectRead for reading and bExpectWrite for writing CheckedPath. */
	static void TestAccess(FAutomationTestBase& AutomationTest, const TCHAR* What, const FString& CheckedPath, bool bExpectRead, bool bExpectWrite)
	{
		AutomationTest.TestTrue(*FString::Printf(TEXT("%s: reading is %s"), What, bExpectRead ? TEXT("allowed") : TEXT("refused")),
			UEFFileLibrary::IsPathAllowed(CheckedPath, false) == bExpectRead);
		AutomationTest.TestTrue(*FString::Printf(TEXT("%s: writing is %s"), What, bExpectWrite ? TEXT("allowed") : TEXT("refused")),
			UEFFileLibrary::IsPathAllowed(CheckedPath, true) == bExpectWrite);
	}

	/** Expects Actual to hold exactly the strings of Expected, in order and with the same case. */
	static void TestSameStrings(FAutomationTestBase& AutomationTest, const TCHAR* What, const TArray<FString>& Actual, const TArray<FString>& Expected)
	{
		if (AutomationTest.TestEqual(*FString::Printf(TEXT("%s: count"), What), Actual.Num(), Expected.Num()))
		{
			for (int32 EntryIndex = 0; EntryIndex < Expected.Num(); ++EntryIndex)
			{
				AutomationTest.TestEqual(*FString::Printf(TEXT("%s: entry %d"), What, EntryIndex), Actual[EntryIndex], Expected[EntryIndex]);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryAccessOffTest, "UnrealExtendedFramework.FileLibrary.Access.Off",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryAccessOffTest::RunTest(const FString& Parameters)
{
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::Off);

	TestFalse(TEXT("Off never restricts"), GetDefault<UEFFileAccessSettings>()->IsRestrictionActive());

	EFFileLibraryTests::TestAccess(*this, TEXT("Off: a file outside the project"), EFFileLibraryTests::OutsidePath(TEXT("Secret.txt")), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("Off: project content"), FPaths::ProjectContentDir() / TEXT("Asset.uasset"), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("Off: a .. above the root"), EFFileLibraryTests::EscapeAboveRootFromSaved(), true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryAccessShippingOnlyTest, "UnrealExtendedFramework.FileLibrary.Access.ShippingOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryAccessShippingOnlyTest::RunTest(const FString& Parameters)
{
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::ShippingOnly);

	// Automation tests do not run in Shipping builds, but say what Shipping expects anyway.
	const bool bRestricted = UE_BUILD_SHIPPING != 0;
	TestTrue(TEXT("ShippingOnly restricts Shipping builds only"), GetDefault<UEFFileAccessSettings>()->IsRestrictionActive() == bRestricted);

	EFFileLibraryTests::TestAccess(*this, TEXT("ShippingOnly: a file outside the project"), EFFileLibraryTests::OutsidePath(TEXT("Secret.txt")), !bRestricted, !bRestricted);
	EFFileLibraryTests::TestAccess(*this, TEXT("ShippingOnly: project content"), FPaths::ProjectContentDir() / TEXT("Asset.uasset"), true, !bRestricted);
	EFFileLibraryTests::TestAccess(*this, TEXT("ShippingOnly: a file in Saved"), FPaths::ProjectSavedDir() / TEXT("EFFileLibrary/File.txt"), true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryAccessAlwaysTest, "UnrealExtendedFramework.FileLibrary.Access.Always",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryAccessAlwaysTest::RunTest(const FString& Parameters)
{
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString FsRoot = EFFileLibraryTests::FileSystemRoot();
	const FString RelativeAllowed = FString::Printf(TEXT("EFFileLibraryAllowed_%s"), *UniqueSuffix);
	const FString AbsoluteAllowed = FsRoot + FString::Printf(TEXT("EFFileLibraryAllowedAbs_%s"), *UniqueSuffix);

	TArray<FString> AllowedDirectories;
	AllowedDirectories.Add(RelativeAllowed);
	AllowedDirectories.Add(AbsoluteAllowed + TEXT("/"));
	AllowedDirectories.Add(FsRoot);       // a file system root is ignored
	AllowedDirectories.Add(TEXT("   "));  // so is a blank entry

	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::Always, AllowedDirectories);
	TestTrue(TEXT("Always restricts"), GetDefault<UEFFileAccessSettings>()->IsRestrictionActive());

	const FString SavedDir = FPaths::ProjectSavedDir();
	FString SavedFull = FPaths::ConvertRelativePathToFull(SavedDir);
	SavedFull.RemoveFromEnd(TEXT("/"));

	// Saved: read and write.
	EFFileLibraryTests::TestAccess(*this, TEXT("a file in Saved"), SavedDir / TEXT("EFFileLibrary/File.txt"), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("the Saved directory itself"), SavedDir, true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("a .. that stays inside Saved"), SavedDir / TEXT("EFFileLibrary/Sub/../File.txt"), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("backslashes inside Saved"), SavedFull + TEXT("\\EFFileLibrary\\File.txt"), true, true);

	// The rest of the project: read only.
	EFFileLibraryTests::TestAccess(*this, TEXT("project content"), FPaths::ProjectContentDir() / TEXT("Asset.uasset"), true, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("the project directory itself"), FPaths::ProjectDir(), true, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("a .. from Saved into the project config"), SavedDir / TEXT("../Config/DefaultGame.ini"), true, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("backslashes and .. from Saved into content"), SavedFull + TEXT("\\Sub\\..\\..\\Content\\Asset.uasset"), true, false);
	TestFalse(TEXT("a sibling whose name starts with Saved's cannot be written"), UEFFileLibrary::IsPathAllowed(SavedFull + TEXT("Evil/File.txt"), true));

	// The platform user directories: read only.
	const FString UserSettingsDirectory = FPlatformProcess::UserSettingsDir();
	if (!UserSettingsDirectory.IsEmpty() && EFFileLibraryTests::IsBelowFileSystemRoot(UserSettingsDirectory))
	{
		EFFileLibraryTests::TestAccess(*this, TEXT("the user settings directory"), UserSettingsDirectory / TEXT("EFFileLibrary.ini"), true, false);
	}
	const FString UserDirectory = FPlatformProcess::UserDir();
	if (!UserDirectory.IsEmpty() && EFFileLibraryTests::IsBelowFileSystemRoot(UserDirectory))
	{
		EFFileLibraryTests::TestAccess(*this, TEXT("the user directory"), UserDirectory / TEXT("EFFileLibrary.txt"), true, false);
	}

	// Outside every allowed directory, or not a safe path: refused.
	EFFileLibraryTests::TestAccess(*this, TEXT("an absolute path outside the allowed directories"), EFFileLibraryTests::OutsidePath(TEXT("Secret.txt")), false, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("a .. from Saved above the root"), EFFileLibraryTests::EscapeAboveRootFromSaved(), false, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("the file system root"), FsRoot, false, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("an empty path"), FString(), false, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("a segment of only dots"), SavedDir / TEXT(".../File.txt"), false, false);

	// The additional allowed directories: read and write, and nothing next to them.
	EFFileLibraryTests::TestAccess(*this, TEXT("a relative allowed directory"), FPaths::ProjectDir() / RelativeAllowed / TEXT("File.txt"), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("an absolute allowed directory"), AbsoluteAllowed / TEXT("File.txt"), true, true);
	EFFileLibraryTests::TestAccess(*this, TEXT("a sibling whose name starts with the allowed directory's"), AbsoluteAllowed + TEXT("Evil/File.txt"), false, false);
	EFFileLibraryTests::TestAccess(*this, TEXT("a .. out of the absolute allowed directory"), AbsoluteAllowed / TEXT("../Other.txt"), false, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryAccessRefusedCallsTest, "UnrealExtendedFramework.FileLibrary.Access.RefusedCalls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryAccessRefusedCallsTest::RunTest(const FString& Parameters)
{
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	EFFileLibraryTests::FScopedTestFolder SavedFolder(FPaths::AutomationTransientDir());
	// Readable but not writable while restricted: inside the project directory, outside Saved.
	EFFileLibraryTests::FScopedTestFolder ProjectFolder(FPaths::ProjectIntermediateDir());

	const FString OriginalText = TEXT("original");
	const FString SourceText = TEXT("source");
	const FString Victim = ProjectFolder.PathOf(TEXT("Victim.txt"));
	const FString SavedSource = SavedFolder.PathOf(TEXT("Source.txt"));
	if (!TestTrue(TEXT("the fixtures are written"), FFileHelper::SaveStringToFile(OriginalText, *Victim) && FFileHelper::SaveStringToFile(SourceText, *SavedSource)))
	{
		return false;
	}

	const FString FsRoot = EFFileLibraryTests::FileSystemRoot();
	AccessGuard.Apply(EEFFileAccessRestriction::Off);
	TestTrue(TEXT("unrestricted, the file system root is a directory"), UEFFileLibrary::IsDirectory(FsRoot));

	AccessGuard.Apply(EEFFileAccessRestriction::Always);
	AddExpectedMessage(TEXT("Extended File Access"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0, false);

	// Reading, listing and querying inside the project directory still work.
	FString ReadBack;
	TestTrue(TEXT("ReadText inside the project directory"), UEFFileLibrary::ReadText(Victim, ReadBack));
	TestEqual(TEXT("ReadText returns the file"), ReadBack, OriginalText);
	TestTrue(TEXT("IsFile inside the project directory"), UEFFileLibrary::IsFile(Victim));
	TestTrue(TEXT("GetFileSize inside the project directory"), UEFFileLibrary::GetFileSize(Victim) == OriginalText.Len());
	TArray<FString> ListedNodes;
	TestTrue(TEXT("ListDirectory inside the project directory"), UEFFileLibrary::ListDirectory(ProjectFolder.GetPath(), FString(), ListedNodes, true, false, false));
	TestTrue(TEXT("ListDirectory finds the file"), ListedNodes.ContainsByPredicate([](const FString& Node) { return Node.EndsWith(TEXT("Victim.txt")); }));

	// Every kind of write outside Saved is refused.
	FString WriteError;
	TestFalse(TEXT("SaveText outside Saved"), UEFFileLibrary::SaveText(Victim, TEXT("changed"), WriteError, false, true));
	TestFalse(TEXT("SaveText reports the refusal"), WriteError.IsEmpty());
	const TArray<FString> NewLines = { TEXT("changed") };
	TestFalse(TEXT("SaveLine outside Saved"), UEFFileLibrary::SaveLine(Victim, NewLines, WriteError, false, true));
	const TArray<uint8> NewBytes = { 42 };
	TestFalse(TEXT("SaveByte outside Saved"), UEFFileLibrary::SaveByte(Victim, NewBytes, WriteError, true, true));
	TestFalse(TEXT("SetReadOnlyFlag outside Saved"), UEFFileLibrary::SetReadOnlyFlag(Victim, true));
	TestFalse(TEXT("RemoveFile outside Saved"), UEFFileLibrary::RemoveFile(Victim));
	TestFalse(TEXT("RenameFile outside Saved"), UEFFileLibrary::RenameFile(Victim, TEXT("Renamed.txt")));
	TestFalse(TEXT("MoveFile must be able to delete its source"), UEFFileLibrary::MoveFile(Victim, SavedFolder.PathOf(TEXT("Moved.txt")), true));
	TestFalse(TEXT("CopyFile into the project directory"), UEFFileLibrary::CopyFile(SavedSource, ProjectFolder.PathOf(TEXT("Copy.txt")), true));
	TestTrue(TEXT("CopyFile from the project directory into Saved"), UEFFileLibrary::CopyFile(Victim, SavedFolder.PathOf(TEXT("Copy.txt")), true));
	TestFalse(TEXT("MakeDirectory outside Saved"), UEFFileLibrary::MakeDirectory(ProjectFolder.PathOf(TEXT("NewDirectory")), true));
	TestFalse(TEXT("RemoveDirectory outside Saved"), UEFFileLibrary::RemoveDirectory(ProjectFolder.GetPath(), true));
	TestFalse(TEXT("CopyDirectory into the project directory"), UEFFileLibrary::CopyDirectory(SavedFolder.GetPath(), ProjectFolder.GetPath()));
	TestFalse(TEXT("MoveDirectory out of the project directory"), UEFFileLibrary::MoveDirectory(ProjectFolder.GetPath(), SavedFolder.PathOf(TEXT("MovedDirectory"))));
	const TArray<FString> CsvHeaders = { TEXT("Name") };
	const TArray<FString> CsvData = { TEXT("Value") };
	int32 CsvTotal = 0;
	TestFalse(TEXT("SaveCSV outside Saved"), UEFFileLibrary::SaveCSV(ProjectFolder.PathOf(TEXT("Table.csv")), CsvHeaders, CsvData, CsvTotal, true));
	FStrProperty* StringProperty = FindFProperty<FStrProperty>(FProjectPath::StaticStruct(), TEXT("Directory"));
	if (TestTrue(TEXT("a string property to write config with"), StringProperty != nullptr))
	{
		FString ConfigValue = TEXT("value");
		TestFalse(TEXT("WriteConfig outside Saved"), UEFFileLibrary::WriteConfigFile(ProjectFolder.PathOf(TEXT("Test.ini")), TEXT("EFFileLibraryTest"), TEXT("Key"), StringProperty, &ConfigValue, false));
	}
	TestFalse(TEXT("RemoveConfig outside Saved"), UEFFileLibrary::RemoveConfig(ProjectFolder.PathOf(TEXT("Test.ini")), TEXT("EFFileLibraryTest"), TEXT("Key")));

	// The refused calls changed nothing.
	FString VictimContents;
	TestTrue(TEXT("the file is still there"), FFileHelper::LoadFileToString(VictimContents, *Victim));
	TestEqual(TEXT("the file is unchanged"), VictimContents, OriginalText);
	TestFalse(TEXT("the file is not read-only"), IFileManager::Get().IsReadOnly(*Victim));
	TestFalse(TEXT("the refused move created nothing"), IFileManager::Get().FileExists(*SavedFolder.PathOf(TEXT("Moved.txt"))));
	TestFalse(TEXT("the refused copy created nothing"), IFileManager::Get().FileExists(*ProjectFolder.PathOf(TEXT("Copy.txt"))));
	TestFalse(TEXT("the refused CSV was not written"), IFileManager::Get().FileExists(*ProjectFolder.PathOf(TEXT("Table.csv"))));
	TestFalse(TEXT("the refused directory was not made"), IFileManager::Get().DirectoryExists(*ProjectFolder.PathOf(TEXT("NewDirectory"))));
	TestTrue(TEXT("the refused removal left the directory"), IFileManager::Get().DirectoryExists(*ProjectFolder.GetPath()));

	// Reading outside the readable directories is refused, even where the path exists.
	TestFalse(TEXT("IsDirectory outside the readable directories"), UEFFileLibrary::IsDirectory(FsRoot));
	TArray<FString> RootNodes;
	TestFalse(TEXT("ListDirectory outside the readable directories"), UEFFileLibrary::ListDirectory(FsRoot, FString(), RootNodes, true, true, false));
	TestEqual(TEXT("a refused listing returns nothing"), RootNodes.Num(), 0);
	FCustomNodeStat RootStats;
	TestFalse(TEXT("NodeStats outside the readable directories"), UEFFileLibrary::NodeStats(FsRoot, RootStats));
	bool bLoaded = true;
	TestTrue(TEXT("LoadScreenshot outside the readable directories"), UEFFileLibrary::LoadScreenshot(EFFileLibraryTests::OutsidePath(TEXT("Shot.png")), bLoaded) == nullptr && !bLoaded);
	bool bHasTime = true;
	UEFFileLibrary::GetFileModificationTime(EFFileLibraryTests::OutsidePath(TEXT("Secret.txt")), bHasTime);
	TestFalse(TEXT("GetFileModificationTime outside the readable directories"), bHasTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryTextRoundTripTest, "UnrealExtendedFramework.FileLibrary.RoundTrip.Text",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryTextRoundTripTest::RunTest(const FString& Parameters)
{
	// Restricted, as in a shipped game: everything here stays inside Saved.
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::Always);
	EFFileLibraryTests::FScopedTestFolder TempFolder(FPaths::AutomationTransientDir());

	// Text
	const FString TextFile = TempFolder.PathOf(TEXT("Text.txt"));
	const FString FirstText = TEXT("First line\nSecond line with \"quotes\", commas and \\ backslashes");
	FString SaveError;
	TestTrue(TEXT("SaveText writes a new file"), UEFFileLibrary::SaveText(TextFile, FirstText, SaveError));
	FString ReadBack;
	TestTrue(TEXT("ReadText reads it"), UEFFileLibrary::ReadText(TextFile, ReadBack));
	TestEqual(TEXT("text round trip"), ReadBack, FirstText);

	TestFalse(TEXT("SaveText keeps an existing file without Force"), UEFFileLibrary::SaveText(TextFile, TEXT("other"), SaveError));
	TestEqual(TEXT("SaveText says why"), SaveError, FString(TEXT("File already exists")));

	const FString MoreText = TEXT("\nappended");
	TestTrue(TEXT("SaveText appends"), UEFFileLibrary::SaveText(TextFile, MoreText, SaveError, true));
	ReadBack.Reset();
	TestTrue(TEXT("ReadText reads the appended file"), UEFFileLibrary::ReadText(TextFile, ReadBack));
	TestEqual(TEXT("appended text round trip"), ReadBack, FirstText + MoreText);

	const FString ForcedText = TEXT("forced");
	TestTrue(TEXT("SaveText overwrites with Force"), UEFFileLibrary::SaveText(TextFile, ForcedText, SaveError, false, true));
	ReadBack.Reset();
	TestTrue(TEXT("ReadText reads the overwritten file"), UEFFileLibrary::ReadText(TextFile, ReadBack));
	TestEqual(TEXT("overwritten text round trip"), ReadBack, ForcedText);

	// Lines
	const FString LinesFile = TempFolder.PathOf(TEXT("Lines.txt"));
	const TArray<FString> WrittenLines = { TEXT("alpha"), TEXT("beta"), TEXT("gamma") };
	TestTrue(TEXT("SaveLine writes lines"), UEFFileLibrary::SaveLine(LinesFile, WrittenLines, SaveError));
	TArray<FString> LinesBack;
	TestTrue(TEXT("ReadLine reads them"), UEFFileLibrary::ReadLine(LinesFile, FString(), LinesBack));
	// A trailing line terminator may come back as one empty last line.
	if (LinesBack.Num() == WrittenLines.Num() + 1 && LinesBack.Last().IsEmpty())
	{
		LinesBack.RemoveAt(LinesBack.Num() - 1);
	}
	EFFileLibraryTests::TestSameStrings(*this, TEXT("line round trip"), LinesBack, WrittenLines);
	TArray<FString> MatchedLines;
	TestTrue(TEXT("ReadLine filters by pattern"), UEFFileLibrary::ReadLine(LinesFile, TEXT("^b"), MatchedLines));
	const TArray<FString> ExpectedMatches = { TEXT("beta") };
	EFFileLibraryTests::TestSameStrings(*this, TEXT("only the matching line"), MatchedLines, ExpectedMatches);

	// Bytes
	const FString BytesFile = TempFolder.PathOf(TEXT("Bytes.bin"));
	TArray<uint8> WrittenBytes;
	for (int32 ByteValue = 0; ByteValue < 256; ++ByteValue)
	{
		WrittenBytes.Add(static_cast<uint8>(ByteValue));
	}
	TestTrue(TEXT("SaveByte writes bytes"), UEFFileLibrary::SaveByte(BytesFile, WrittenBytes, SaveError));
	TArray<uint8> BytesBack;
	TestTrue(TEXT("ReadByte reads them"), UEFFileLibrary::ReadByte(BytesFile, BytesBack));
	TestTrue(TEXT("byte round trip"), BytesBack == WrittenBytes);

	// Base64
	const FString PlainText = TEXT("Extended file library");
	FString DecodedText;
	TestTrue(TEXT("string Base64 decodes"), UEFFileLibrary::StringFromBase64(UEFFileLibrary::StringToBase64(PlainText), DecodedText));
	TestEqual(TEXT("string Base64 round trip"), DecodedText, PlainText);
	TArray<uint8> DecodedBytes;
	TestTrue(TEXT("byte Base64 decodes"), UEFFileLibrary::BytesFromBase64(UEFFileLibrary::BytesToBase64(WrittenBytes), DecodedBytes));
	TestTrue(TEXT("byte Base64 round trip"), DecodedBytes == WrittenBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryCsvRoundTripTest, "UnrealExtendedFramework.FileLibrary.RoundTrip.Csv",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryCsvRoundTripTest::RunTest(const FString& Parameters)
{
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::Always);
	EFFileLibraryTests::FScopedTestFolder TempFolder(FPaths::AutomationTransientDir());

	const TArray<FString> CsvHeaders = { TEXT("Name"), TEXT("Quote"), TEXT("List") };
	const TArray<FString> CsvData = {
		TEXT("Alpha"), TEXT("He said \"hi\""), TEXT("a,b,c"),
		TEXT("Beta"), TEXT(""), TEXT("plain")
	};

	// File
	const FString CsvFile = TempFolder.PathOf(TEXT("Table.csv"));
	int32 WrittenTotal = 0;
	TestTrue(TEXT("SaveCSV writes the table"), UEFFileLibrary::SaveCSV(CsvFile, CsvHeaders, CsvData, WrittenTotal));
	TestEqual(TEXT("SaveCSV counts the header and two rows"), WrittenTotal, 3);

	TArray<FString> HeadersBack;
	TArray<FString> DataBack;
	int32 ReadTotal = 0;
	TestTrue(TEXT("ReadCSV reads the table"), UEFFileLibrary::ReadCSV(CsvFile, HeadersBack, DataBack, ReadTotal, true));
	TestEqual(TEXT("ReadCSV counts the header and two rows"), ReadTotal, 3);
	EFFileLibraryTests::TestSameStrings(*this, TEXT("CSV file headers"), HeadersBack, CsvHeaders);
	EFFileLibraryTests::TestSameStrings(*this, TEXT("CSV file cells (quotes, commas and empty cells)"), DataBack, CsvData);

	// String
	FString CsvText;
	int32 StringTotal = 0;
	TestTrue(TEXT("CSVToString converts the table"), UEFFileLibrary::CSVToString(CsvText, CsvHeaders, CsvData, StringTotal));
	TArray<FString> ParsedHeaders;
	TArray<FString> ParsedData;
	int32 ParsedTotal = 0;
	TestTrue(TEXT("StringToCSV parses it"), UEFFileLibrary::StringToCSV(CsvText, ParsedHeaders, ParsedData, ParsedTotal, true));
	EFFileLibraryTests::TestSameStrings(*this, TEXT("CSV string headers"), ParsedHeaders, CsvHeaders);
	EFFileLibraryTests::TestSameStrings(*this, TEXT("CSV string cells"), ParsedData, CsvData);

	// A row that does not fill every column is refused before anything is written.
	const TArray<FString> RaggedData = { TEXT("Alpha"), TEXT("Beta") };
	int32 RaggedTotal = 0;
	TestFalse(TEXT("SaveCSV refuses a partial row"), UEFFileLibrary::SaveCSV(TempFolder.PathOf(TEXT("Ragged.csv")), CsvHeaders, RaggedData, RaggedTotal));
	TestFalse(TEXT("the partial table was not written"), IFileManager::Get().FileExists(*TempFolder.PathOf(TEXT("Ragged.csv"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFFileLibraryStructJsonRoundTripTest, "UnrealExtendedFramework.FileLibrary.RoundTrip.StructJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFFileLibraryStructJsonRoundTripTest::RunTest(const FString& Parameters)
{
	EFFileLibraryTests::FScopedFileAccessSettings AccessGuard;
	AccessGuard.Apply(EEFFileAccessRestriction::Always);
	EFFileLibraryTests::FScopedTestFolder TempFolder(FPaths::AutomationTransientDir());

	// The wildcard nodes receive the struct's property from the Blueprint VM. Take the same kind of
	// property from the library's own functions: GetProjectDirectories returns an FProjectPath and
	// NodeStats fills an FCustomNodeStat.
	UFunction* DirectoriesFunction = UEFFileLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UEFFileLibrary, GetProjectDirectories));
	FStructProperty* PathsProperty = DirectoriesFunction ? FindFProperty<FStructProperty>(DirectoriesFunction, TEXT("ReturnValue")) : nullptr;
	UFunction* StatsFunction = UEFFileLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UEFFileLibrary, NodeStats));
	FStructProperty* StatsProperty = StatsFunction ? FindFProperty<FStructProperty>(StatsFunction, TEXT("Stats")) : nullptr;
	if (!TestTrue(TEXT("the struct properties are found"), PathsProperty != nullptr && StatsProperty != nullptr))
	{
		return false;
	}

	// Strings that need escaping, through a file.
	FProjectPath OriginalPaths;
	OriginalPaths.Directory = TEXT("C:\\Games\\\"Quoted\"\\");
	OriginalPaths.Config = TEXT("Line one\nLine two\ttabbed");
	OriginalPaths.Content = TEXT("{\"looks\": [\"like\", \"json\"]}");
	OriginalPaths.Saved = TEXT("caf\u00E9 <xml> & more");
	OriginalPaths.Log = TEXT("/Game/Some/Path");
	OriginalPaths.User = FString();

	FString PathsJson;
	TestTrue(TEXT("the struct converts to JSON"), UEFFileLibrary::AnyStructToJsonString(PathsProperty, &OriginalPaths, PathsJson));
	const FString JsonFile = TempFolder.PathOf(TEXT("Paths.json"));
	FString SaveError;
	TestTrue(TEXT("the JSON is saved"), UEFFileLibrary::SaveText(JsonFile, PathsJson, SaveError));
	FString JsonBack;
	TestTrue(TEXT("the JSON is read back"), UEFFileLibrary::ReadText(JsonFile, JsonBack));
	FProjectPath ParsedPaths;
	TestTrue(TEXT("the JSON converts back to the struct"), UEFFileLibrary::JsonStringToAnyStruct(PathsProperty, &ParsedPaths, JsonBack));
	TestEqual(TEXT("Directory round trip"), ParsedPaths.Directory, OriginalPaths.Directory);
	TestEqual(TEXT("Config round trip"), ParsedPaths.Config, OriginalPaths.Config);
	TestEqual(TEXT("Content round trip"), ParsedPaths.Content, OriginalPaths.Content);
	TestEqual(TEXT("Saved round trip"), ParsedPaths.Saved, OriginalPaths.Saved);
	TestEqual(TEXT("Log round trip"), ParsedPaths.Log, OriginalPaths.Log);
	TestEqual(TEXT("User round trip"), ParsedPaths.User, OriginalPaths.User);

	// Booleans and a 64-bit integer.
	FCustomNodeStat OriginalStats;
	OriginalStats.IsDirectory = true;
	OriginalStats.IsReadOnly = false;
	OriginalStats.FileSize = 1234567890123LL;
	FString StatsJson;
	TestTrue(TEXT("the stats convert to JSON"), UEFFileLibrary::AnyStructToJsonString(StatsProperty, &OriginalStats, StatsJson));
	FCustomNodeStat ParsedStats;
	TestTrue(TEXT("the JSON converts back to the stats"), UEFFileLibrary::JsonStringToAnyStruct(StatsProperty, &ParsedStats, StatsJson));
	TestTrue(TEXT("bool fields round trip"), ParsedStats.IsDirectory && !ParsedStats.IsReadOnly);
	TestTrue(TEXT("int64 field round trips"), ParsedStats.FileSize == OriginalStats.FileSize);

	// Hand-written JSON fills only the fields it names.
	FProjectPath FromText;
	TestTrue(TEXT("hand-written JSON converts"), UEFFileLibrary::JsonStringToAnyStruct(PathsProperty, &FromText, TEXT("{\"Directory\": \"D\", \"Saved\": \"S\"}")));
	TestEqual(TEXT("named field Directory"), FromText.Directory, FString(TEXT("D")));
	TestEqual(TEXT("named field Saved"), FromText.Saved, FString(TEXT("S")));
	TestTrue(TEXT("unnamed fields stay empty"), FromText.Config.IsEmpty() && FromText.Content.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

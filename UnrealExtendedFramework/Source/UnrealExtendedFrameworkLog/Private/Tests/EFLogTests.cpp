// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && EF_LOG_ENABLED

#include "../EFLogSessionFolder.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

// These tests write real category files (EFLogTest*.log) into Saved/Logs/Extended. Each run tags
// its lines with a fresh token and only reads those back, so earlier runs never interfere.

namespace EFLogTestsPrivate
{
	static constexpr int32 WorkerThreadCount = 8;
	static constexpr int32 LinesPerThread = 2000;

	static FString NewToken()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	/** Flushes, then reads the category's file. The writer keeps it open, hence AllowWrite. */
	static bool LoadCategoryLines(FAutomationTestBase& Test, FName Category, TArray<FString>& OutLines)
	{
		EFLog::Flush();

		const FString Path = EFLog::GetCategoryFilePath(Category);
		if (!Test.TestFalse(TEXT("The category has a file path"), Path.IsEmpty()))
		{
			return false;
		}

		FString Contents;
		if (!Test.TestTrue(FString::Printf(TEXT("%s can be read"), *Path), FFileHelper::LoadFileToString(Contents, *Path, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite)))
		{
			return false;
		}

		Contents.ParseIntoArrayLines(OutLines, false);
		return true;
	}

	static int32 CountLinesContaining(const TArray<FString>& Lines, const FString& Needle)
	{
		int32 Count = 0;
		for (const FString& Line : Lines)
		{
			if (Line.Contains(Needle))
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogThreadStressTest, "ExtendedFramework.Log.ThreadStress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogThreadStressTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	const FString Token = NewToken();

	// Eight dedicated threads plus the game thread (numbered WorkerThreadCount) write at once.
	TArray<TFuture<void>> Workers;
	for (int32 Worker = 0; Worker < WorkerThreadCount; ++Worker)
	{
		Workers.Add(Async(EAsyncExecution::Thread, [Token, Worker]()
		{
			for (int32 Index = 0; Index < LinesPerThread; ++Index)
			{
				EF_LOG(EFLogTestStress, Log, TEXT("%s T%d #%d"), *Token, Worker, Index);
			}
		}));
	}

	for (int32 Index = 0; Index < LinesPerThread; ++Index)
	{
		EF_LOG(EFLogTestStress, Log, TEXT("%s T%d #%d"), *Token, WorkerThreadCount, Index);
	}

	for (TFuture<void>& Worker : Workers)
	{
		Worker.Wait();
	}

	TArray<FString> Lines;
	if (!LoadCategoryLines(*this, TEXT("EFLogTestStress"), Lines))
	{
		return false;
	}

	// Each thread's lines must all be there, and in the order that thread wrote them.
	TArray<int32> NextExpected;
	NextExpected.Init(0, WorkerThreadCount + 1);
	int32 Found = 0;
	bool bInOrder = true;

	for (const FString& Line : Lines)
	{
		const int32 TokenAt = Line.Find(Token);
		if (TokenAt == INDEX_NONE)
		{
			continue;
		}

		// "... <Token> T<thread> #<index>"
		FString ThreadPart;
		FString IndexPart;
		if (!Line.RightChop(TokenAt + Token.Len()).TrimStartAndEnd().Split(TEXT(" #"), &ThreadPart, &IndexPart))
		{
			AddError(FString::Printf(TEXT("Unparseable line: %s"), *Line));
			return false;
		}

		const int32 Thread = FCString::Atoi(*ThreadPart.RightChop(1));
		const int32 Index = FCString::Atoi(*IndexPart);
		if (!NextExpected.IsValidIndex(Thread))
		{
			AddError(FString::Printf(TEXT("Line from an unknown thread: %s"), *Line));
			return false;
		}

		if (Index != NextExpected[Thread])
		{
			bInOrder = false;
		}
		NextExpected[Thread] = Index + 1;
		++Found;
	}

	TestEqual(TEXT("Every line from every thread reached the file"), Found, (WorkerThreadCount + 1) * LinesPerThread);
	TestTrue(TEXT("Each thread's lines are in the order it wrote them"), bInOrder);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogLineFormatTest, "ExtendedFramework.Log.LineFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogLineFormatTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	const FString Token = NewToken();
	EF_LOG(EFLogTestFormat, Warning, TEXT("%s first line\nsecond line [not a field]\n"), *Token);

	TArray<FString> Lines;
	if (!LoadCategoryLines(*this, TEXT("EFLogTestFormat"), Lines))
	{
		return false;
	}

	TestTrue(TEXT("The file starts with the EFLog header"),
		Lines.Num() > 0 && Lines[0].StartsWith(TEXT("# EFLog v1 | Category=EFLogTestFormat | Session=")));

	// "| UtcOffset=+03:00 |": sign, hours, minutes.
	const int32 OffsetField = Lines.Num() > 0 ? Lines[0].Find(TEXT(" | UtcOffset=")) : INDEX_NONE;
	const FString Offset = OffsetField != INDEX_NONE ? Lines[0].Mid(OffsetField + 13, 6) : FString();
	TestTrue(TEXT("The header carries the UTC offset as +HH:MM"),
		Offset.Len() == 6 && (Offset[0] == TEXT('+') || Offset[0] == TEXT('-')) && Offset[3] == TEXT(':'));

	const int32 LineIndex = Lines.IndexOfByPredicate([&Token](const FString& Line) { return Line.Contains(Token); });
	if (!TestTrue(TEXT("The line was written"), LineIndex != INDEX_NONE))
	{
		return false;
	}

	// [2026.09.24-14.03.11.482][f123456][-][Warning][Plugins/.../EFLogTests.cpp:NN] <Token> first line
	const FString& Line = Lines[LineIndex];
	TestTrue(TEXT("Starts with a [yyyy.MM.dd-HH.mm.ss.mmm] timestamp"), Line.Len() > 25 && Line[0] == TEXT('[') && Line[24] == TEXT(']'));
	TestTrue(TEXT("Carries the frame"), Line.Contains(TEXT("][f")));
	TestTrue(TEXT("Carries the level"), Line.Contains(TEXT("][Warning][")));
	TestTrue(TEXT("Carries the source location"), Line.Contains(TEXT("EFLogTests.cpp:")));
	TestTrue(TEXT("The message follows the fields"), Line.Contains(FString::Printf(TEXT("] %s first line"), *Token)));

	TestTrue(TEXT("A multi-line message continues on an indented line"),
		Lines.IsValidIndex(LineIndex + 1) && Lines[LineIndex + 1] == TEXT("    second line [not a field]"));
	TestTrue(TEXT("A trailing newline does not leave an empty continuation line"),
		!Lines.IsValidIndex(LineIndex + 2) || Lines[LineIndex + 2] != TEXT("    "));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogVerbosityTest, "ExtendedFramework.Log.Verbosity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogVerbosityTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	const FString Token = NewToken();

	// Set before the category's first line: the threshold can be configured up front.
	EFLog::SetCategoryVerbosity(TEXT("EFLogTestVerbosity"), EEFLogVerbosity::Warning);

	EF_LOG(EFLogTestVerbosity, Log, TEXT("%s filtered-by-level"), *Token);
	EF_LOG(EFLogTestVerbosity, Warning, TEXT("%s kept-warning"), *Token);
	EF_CLOG(false, EFLogTestVerbosity, Error, TEXT("%s filtered-by-condition"), *Token);
	EF_CLOG(true, EFLogTestVerbosity, Error, TEXT("%s kept-condition"), *Token);

	EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;
	TestTrue(TEXT("The threshold can be read back"), EFLog::GetCategoryVerbosity(TEXT("EFLogTestVerbosity"), Verbosity));
	TestTrue(TEXT("The threshold is the one set"), Verbosity == EEFLogVerbosity::Warning);

	EFLog::SetCategoryVerbosity(TEXT("EFLogTestVerbosity"), EEFLogVerbosity::Log);

	TArray<FString> Lines;
	if (!LoadCategoryLines(*this, TEXT("EFLogTestVerbosity"), Lines))
	{
		return false;
	}

	TestEqual(TEXT("A level above the threshold is not written"), CountLinesContaining(Lines, Token + TEXT(" filtered-by-level")), 0);
	TestEqual(TEXT("A level within the threshold is written"), CountLinesContaining(Lines, Token + TEXT(" kept-warning")), 1);
	TestEqual(TEXT("EF_CLOG with a false condition writes nothing"), CountLinesContaining(Lines, Token + TEXT(" filtered-by-condition")), 0);
	TestEqual(TEXT("EF_CLOG with a true condition writes"), CountLinesContaining(Lines, Token + TEXT(" kept-condition")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogCategoryNameTest, "ExtendedFramework.Log.CategoryNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogCategoryNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("An identifier is kept as written"), EFLog::SanitizeCategoryName(TEXT("Araf")), FString(TEXT("Araf")));
	TestEqual(TEXT("Spaces are trimmed and replaced"), EFLog::SanitizeCategoryName(TEXT(" Hostage Follow ")), FString(TEXT("Hostage_Follow")));
	TestEqual(TEXT("Dots are replaced"), EFLog::SanitizeCategoryName(TEXT("Online.PlayFab")), FString(TEXT("Online_PlayFab")));
	TestEqual(TEXT("A reserved Windows device name gets a suffix"), EFLog::SanitizeCategoryName(TEXT("con")), FString(TEXT("con_")));
	TestEqual(TEXT("An empty name still names a file"), EFLog::SanitizeCategoryName(TEXT("   ")), FString(TEXT("Unnamed")));

	// Category names ignore case, so two spellings share one category and one file.
	EFLog::Private::FindOrAddCategory(TEXT("EFLogTestCase"));
	const FString FirstSpelling = EFLog::GetCategoryFilePath(TEXT("EFLogTestCase"));
	const FString OtherSpelling = EFLog::GetCategoryFilePath(TEXT("efLOGtestCASE"));
	TestFalse(TEXT("The category has a file path"), FirstSpelling.IsEmpty());
	TestEqual(TEXT("Different casing resolves to the same file"), OtherSpelling, FirstSpelling);
	TestTrue(TEXT("The file keeps the first spelling"), FirstSpelling.EndsWith(TEXT("/EFLogTestCase.log"), ESearchCase::CaseSensitive));

	// A threshold set up front (from the console, say) must not decide the file name: the EF_LOG
	// call site's spelling does.
	EFLog::SetCategoryVerbosity(TEXT("eflogtestspelling"), EEFLogVerbosity::Log);
	EF_LOG(EFLogTestSpelling, Log, TEXT("spelling check"));
	TestTrue(TEXT("The call site names the file"), EFLog::GetCategoryFilePath(TEXT("EFLogTestSpelling")).EndsWith(TEXT("/EFLogTestSpelling.log"), ESearchCase::CaseSensitive));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogDynamicCategoryTest, "ExtendedFramework.Log.DynamicCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogDynamicCategoryTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	const FString Token = NewToken();

	// The same category reached three ways: an FString, an FName and a literal.
	const FString NameAsString = TEXT("EFLogTestDynamic");
	const FName NameAsName(TEXT("EFLogTestDynamic"));
	EF_LOG_DYNAMIC(NameAsString, Log, TEXT("%s from-string"), *Token);
	EF_LOG_DYNAMIC(NameAsName, Log, TEXT("%s from-name"), *Token);
	EF_LOG_DYNAMIC(TEXT("EFLogTestDynamic"), Log, TEXT("%s from-literal"), *Token);

	TArray<FString> Lines;
	if (!LoadCategoryLines(*this, TEXT("EFLogTestDynamic"), Lines))
	{
		return false;
	}

	TestEqual(TEXT("An FString name writes into the category's file"), CountLinesContaining(Lines, Token + TEXT(" from-string")), 1);
	TestEqual(TEXT("An FName name writes into the same file"), CountLinesContaining(Lines, Token + TEXT(" from-name")), 1);
	TestEqual(TEXT("A literal name writes into the same file"), CountLinesContaining(Lines, Token + TEXT(" from-literal")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogSeparatorTest, "ExtendedFramework.Log.Separator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogSeparatorTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	const FString Token = NewToken();

	// A plain separator, not a run marker: this also lands in every other open category file, and
	// a run marker there would change what "current PIE run only" shows in the window.
	EF_LOG(EFLogTestSeparator, Log, TEXT("%s before"), *Token);
	EFLog::WriteSeparator(FString::Printf(TEXT("EFLog automation test %s"), *Token), EEFLogSeparatorKind::Plain);
	EF_LOG(EFLogTestSeparator, Log, TEXT("%s after"), *Token);

	TArray<FString> Lines;
	if (!LoadCategoryLines(*this, TEXT("EFLogTestSeparator"), Lines))
	{
		return false;
	}

	const int32 Before = Lines.IndexOfByPredicate([&Token](const FString& Line) { return Line.Contains(Token + TEXT(" before")); });
	// [2026.09.24-14.03.11.482] ==== text ====
	const FString SeparatorEnd = FString::Printf(TEXT("] ==== EFLog automation test %s ===="), *Token);
	const int32 Separator = Lines.IndexOfByPredicate([&SeparatorEnd](const FString& Line)
	{
		// "[" + the 23-character stamp, then SeparatorEnd, which starts with the closing "]".
		return Line.StartsWith(TEXT("[")) && Line.Len() == 24 + SeparatorEnd.Len() && Line.EndsWith(SeparatorEnd);
	});
	const int32 After = Lines.IndexOfByPredicate([&Token](const FString& Line) { return Line.Contains(Token + TEXT(" after")); });

	TestTrue(TEXT("The separator line was written as [time] ==== text ===="), Separator != INDEX_NONE);
	TestTrue(TEXT("The separator sits between the lines around it"), Before != INDEX_NONE && Before < Separator && Separator < After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogParseVerbosityTest, "ExtendedFramework.Log.ParseVerbosity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogParseVerbosityTest::RunTest(const FString& Parameters)
{
	EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;

	TestTrue(TEXT("Parses a level ignoring case"), EFLog::ParseVerbosity(TEXT("warning"), Verbosity) && Verbosity == EEFLogVerbosity::Warning);
	TestTrue(TEXT("Parses VeryVerbose"), EFLog::ParseVerbosity(TEXT(" VeryVerbose "), Verbosity) && Verbosity == EEFLogVerbosity::VeryVerbose);
	TestFalse(TEXT("Refuses an unknown level"), EFLog::ParseVerbosity(TEXT("Fatal"), Verbosity));

	// Every level round-trips through its written name, which is what the window parses back.
	for (uint8 Value = static_cast<uint8>(EEFLogVerbosity::Error); Value <= static_cast<uint8>(EEFLogVerbosity::VeryVerbose); ++Value)
	{
		const EEFLogVerbosity Expected = static_cast<EEFLogVerbosity>(Value);
		EEFLogVerbosity Parsed = EEFLogVerbosity::Log;
		TestTrue(FString::Printf(TEXT("%s round-trips"), LexToString(Expected)), EFLog::ParseVerbosity(LexToString(Expected), Parsed) && Parsed == Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEFLogSessionFolderTest, "ExtendedFramework.Log.SessionFolder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEFLogSessionFolderTest::RunTest(const FString& Parameters)
{
	using namespace EFLogTestsPrivate;

	IFileManager& FileManager = IFileManager::Get();

	// A throwaway root beside the real one, so the running session's files are never touched.
	const FString FolderName = FString::Printf(TEXT("EFLogTest_%s"), *NewToken());
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / FolderName);
	FPaths::NormalizeDirectoryName(Root);
	const FString Archive = Root / TEXT("Archive");

	const auto WriteFile = [](const FString& Path, const FString& Text)
	{
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	};

	// What a previous session leaves behind: the owner's file, a dead process's folder, and more
	// archived sessions than are kept.
	WriteFile(Root / TEXT("Old.log"), TEXT("# EFLog v1 | Category=Old | Session=2020.01.01-00.00.00 | Process=UnrealEditor pid 1 | Project=Test\r\nline\r\n"));
	WriteFile(Root / TEXT("Game_999") / TEXT("Stale.log"), TEXT("# EFLog v1 | Category=Stale | Session=2020.01.02-00.00.00 | Process=UnrealEditor pid 999 | Project=Test\r\nline\r\n"));
	const TArray<FString> OldArchives = { TEXT("2019.01.01-00.00.00_Old"), TEXT("2019.01.02-00.00.00_Old"), TEXT("2019.01.03-00.00.00_Old") };
	for (const FString& OldArchive : OldArchives)
	{
		FileManager.MakeDirectory(*(Archive / OldArchive), true);
	}

	{
		FEFLogSessionFolder Owner;
		TestTrue(TEXT("The folder initializes"), Owner.Initialize(FolderName, 3));
		TestTrue(TEXT("The first process owns the root"), Owner.OwnsRoot());
		TestEqual(TEXT("The owner writes into the root"), Owner.GetWriteDirectory(), Root);

		TestFalse(TEXT("The previous session's file left the root"), FileManager.FileExists(*(Root / TEXT("Old.log"))));
		TestTrue(TEXT("It was archived under its session and process"), FileManager.FileExists(*(Archive / TEXT("2020.01.01-00.00.00_UnrealEditor") / TEXT("Old.log"))));
		TestTrue(TEXT("A dead process's file was archived under its folder name"), FileManager.FileExists(*(Archive / TEXT("2020.01.02-00.00.00_Game_999") / TEXT("Stale.log"))));
		TestFalse(TEXT("The dead process's folder is gone"), FileManager.DirectoryExists(*(Root / TEXT("Game_999"))));

		// Five archive folders, three kept: the two oldest go.
		TestFalse(TEXT("The oldest archive was pruned"), FileManager.DirectoryExists(*(Archive / OldArchives[0])));
		TestFalse(TEXT("The second oldest archive was pruned"), FileManager.DirectoryExists(*(Archive / OldArchives[1])));
		TestTrue(TEXT("The newer archives were kept"), FileManager.DirectoryExists(*(Archive / OldArchives[2])));

#if PLATFORM_WINDOWS
		// Ownership is an exclusive open, which Windows enforces even within one process, so a
		// second folder object stands in for a second process.
		FEFLogSessionFolder Other;
		TestTrue(TEXT("A second process initializes too"), Other.Initialize(FolderName, 3));
		TestFalse(TEXT("A second process does not own the root"), Other.OwnsRoot());
		TestTrue(TEXT("It writes into a folder of its own"),
			Other.GetWriteDirectory().StartsWith(Root + TEXT("/")) && Other.GetWriteDirectory() != Root);
		Other.Release();
#endif

		Owner.Release();
	}

	FileManager.DeleteDirectory(*Root, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && EF_LOG_ENABLED

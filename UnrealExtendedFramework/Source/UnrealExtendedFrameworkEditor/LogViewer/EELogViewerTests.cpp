// Copyright Moon Punch Games. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "EELogFileParser.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace EELogViewerTestsPrivate
{
	static void WriteBytes(const FString& Path, const FString& Text)
	{
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static FString MakeTempPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::AutomationTransientDir() / FString::Printf(TEXT("EELogTail_%s.log"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEELogLineParserTest, "ExtendedFramework.Log.Viewer.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEELogLineParserTest::RunTest(const FString& Parameters)
{
	// Everything the writer produces, plus a line from a file that is not EF_LOG's.
	const TArray<FString> Lines =
	{
		TEXT("# EFLog v1 | Category=Araf | Session=2026.09.24-14.03.11 | Process=UnrealEditor pid 12 | Project=Test"),
		TEXT("[2026.09.24-14.03.11.482][f123456][Client 1][Warning][Source/Game/Araf.cpp:212] Door [3] missing"),
		TEXT("    second line"),
		TEXT("==== PIE 3 started | L_Castle | listen server + 1 client ===="),
		TEXT("[2026.09.24-14.03.12.000][f5][-][Log][C:/Engine/Thing.cpp:7] Caf\u00E9 \u2713"),
		TEXT("LogTemp: Warning: not ours"),
	};

	FEELogLineParser Parser;
	TArray<TSharedPtr<FEELogRow>> Rows;
	Parser.Parse(Lines, Rows);

	if (!TestEqual(TEXT("The continuation line joins its entry"), Rows.Num(), 5))
	{
		return false;
	}

	TestTrue(TEXT("A header row"), Rows[0]->Kind == EEELogRowKind::Header);
	TestTrue(TEXT("The header reads as a sentence"), Rows[0]->Message.Contains(TEXT("2026.09.24-14.03.11")));

	const FEELogRow& Line = *Rows[1];
	TestTrue(TEXT("A line row"), Line.Kind == EEELogRowKind::Line);
	TestEqual(TEXT("Clock time"), Line.Time, FString(TEXT("14:03:11.482")));
	TestEqual(TEXT("Frame without its f"), Line.Frame, FString(TEXT("123456")));
	TestEqual(TEXT("Instance"), Line.Instance, FString(TEXT("Client 1")));
	TestTrue(TEXT("Level"), Line.Verbosity == EEFLogVerbosity::Warning);
	TestEqual(TEXT("Source path"), Line.SourcePath, FString(TEXT("Source/Game/Araf.cpp")));
	TestEqual(TEXT("Source line"), Line.SourceLine, 212);
	TestEqual(TEXT("A bracket inside the message is message text"), Line.Message, FString(TEXT("Door [3] missing\nsecond line")));
	TestEqual(TEXT("Extra line count"), Line.ExtraLineCount, 1);
	TestEqual(TEXT("First line"), Line.GetFirstLine(), FString(TEXT("Door [3] missing")));

	TestTrue(TEXT("A separator row"), Rows[2]->Kind == EEELogRowKind::Separator);
	TestEqual(TEXT("A run start begins run 1"), Rows[2]->RunIndex, 1);
	TestEqual(TEXT("The parser is in run 1"), Parser.GetRunIndex(), 1);

	const FEELogRow& Absolute = *Rows[3];
	TestEqual(TEXT("A full Windows path keeps its drive colon"), Absolute.SourcePath, FString(TEXT("C:/Engine/Thing.cpp")));
	TestEqual(TEXT("... and its line"), Absolute.SourceLine, 7);
	TestTrue(TEXT("'-' means no instance"), Absolute.Instance.IsEmpty());
	TestEqual(TEXT("Non-ASCII text survives"), Absolute.Message, FString(TEXT("Caf\u00E9 \u2713")));

	TestTrue(TEXT("A foreign line is raw"), Rows[4]->Kind == EEELogRowKind::Raw);
	TestTrue(TEXT("... and still coloured by its level"), Rows[4]->Verbosity == EEFLogVerbosity::Warning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEELogTimeAndMergeTest, "ExtendedFramework.Log.Viewer.TimeAndMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEELogTimeAndMergeTest::RunTest(const FString& Parameters)
{
	auto Utc = [](int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
	{
		return FDateTime(2026, 9, 24, Hour, Minute, Second, Millisecond).GetTicks();
	};

	// Two files of one session, one written three hours ahead of UTC, and a separate process on UTC.
	FEELogLineParser Araf;
	TArray<TSharedPtr<FEELogRow>> ArafRows;
	Araf.Parse({
		TEXT("# EFLog v1 | Category=Araf | Session=2026.09.24-14.03.00 | UtcOffset=+03:00 | Process=UnrealEditor pid 12 | Project=Test"),
		TEXT("[2026.09.24-14.03.11.482][f1][Server][Log][Source/A.cpp:1] first"),
		TEXT("[2026.09.24-14.03.12.000] ==== PIE 1 started | L_Castle | listen server ===="),
		TEXT("[2026.09.24-14.03.13.000][f9][Server][Log][Source/A.cpp:2] third"),
		TEXT("==== an older, untimed marker ===="),
	}, ArafRows);

	FEELogLineParser Client;
	TArray<TSharedPtr<FEELogRow>> ClientRows;
	Client.Parse({
		TEXT("# EFLog v1 | Category=Araf | Session=2026.09.24-11.03.05 | UtcOffset=+00:00 | Process=UnrealEditor pid 40 | Project=Test"),
		TEXT("[2026.09.24-11.03.12.500][f3][Client 1][Log][Source/A.cpp:1] second"),
	}, ClientRows);

	if (!TestEqual(TEXT("Every row parsed"), ArafRows.Num() + ClientRows.Num(), 7))
	{
		return false;
	}

	TestTrue(TEXT("The header names the offset"), ArafRows[0]->Message.Contains(TEXT("(UTC+03:00)")));
	TestEqual(TEXT("A header sits at its session's start, in UTC"), ArafRows[0]->SortTicks, Utc(11, 3, 0, 0));
	TestEqual(TEXT("A line's time turns into UTC"), ArafRows[1]->SortTicks, Utc(11, 3, 11, 482));
	TestEqual(TEXT("A line on a UTC machine stays as written"), ClientRows[1]->SortTicks, Utc(11, 3, 12, 500));

	const FEELogRow& Marker = *ArafRows[2];
	TestTrue(TEXT("A timed separator is a separator"), Marker.Kind == EEELogRowKind::Separator);
	TestEqual(TEXT("... without its time in the text"), Marker.Message, FString(TEXT("PIE 1 started | L_Castle | listen server")));
	TestEqual(TEXT("... with its time shown"), Marker.Time, FString(TEXT("14:03:12.000")));
	TestEqual(TEXT("... and placed by it"), Marker.SortTicks, Utc(11, 3, 12, 0));
	TestTrue(TEXT("... marking a run start"), Marker.bRunStart);
	TestTrue(TEXT("... keyed by its session, so other files' copies are recognised"), Marker.StructureKey.Contains(TEXT("2026.09.24-14.03.00")));

	TestEqual(TEXT("An untimed marker takes the last time its file saw"), ArafRows[4]->SortTicks, ArafRows[3]->SortTicks);
	TestTrue(TEXT("Headers of different processes differ"), ArafRows[0]->StructureKey != ClientRows[0]->StructureKey);

	// Merge the whole first file, then the client's, which flushed later but wrote in between.
	TArray<TSharedPtr<FEELogRow>> Merged;
	TestFalse(TEXT("The first batch just appends"), EELogRows::MergeByTime(Merged, TArray<TSharedPtr<FEELogRow>>(ArafRows)));
	TestTrue(TEXT("A late file lands among the rows already shown"), EELogRows::MergeByTime(Merged, TArray<TSharedPtr<FEELogRow>>(ClientRows)));

	TArray<FString> Order;
	for (const TSharedPtr<FEELogRow>& Row : Merged)
	{
		Order.Add(Row->Kind == EEELogRowKind::Line ? Row->Message : (Row->Kind == EEELogRowKind::Header ? TEXT("#") : TEXT("====")));
	}
	const TArray<FString> Expected = { TEXT("#"), TEXT("#"), TEXT("first"), TEXT("===="), TEXT("second"), TEXT("third"), TEXT("====") };
	TestTrue(FString::Printf(TEXT("Rows interleave by time (got %s)"), *FString::Join(Order, TEXT(", "))), Order == Expected);

	// Equal times keep arrival order: what was shown first stays first.
	TArray<TSharedPtr<FEELogRow>> Tie;
	const TSharedPtr<FEELogRow> Shown = MakeShared<FEELogRow>();
	Shown->SortTicks = 100;
	const TSharedPtr<FEELogRow> Late = MakeShared<FEELogRow>();
	Late->SortTicks = 100;
	const TSharedPtr<FEELogRow> After = MakeShared<FEELogRow>();
	After->SortTicks = 200;
	EELogRows::MergeByTime(Tie, { Shown, After });
	EELogRows::MergeByTime(Tie, { Late });
	TestTrue(TEXT("A tie goes after the row already shown"), Tie.Num() == 3 && Tie[0] == Shown && Tie[1] == Late && Tie[2] == After);

	FTimespan Offset;
	TestTrue(TEXT("A negative offset parses"), FEELogLineParser::ParseUtcOffset(TEXT("-05:30"), Offset) && Offset == FTimespan::FromMinutes(-330));
	TestFalse(TEXT("A malformed offset is refused"), FEELogLineParser::ParseUtcOffset(TEXT("+3:00"), Offset));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEELogFileTailTest, "ExtendedFramework.Log.Viewer.Tail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEELogFileTailTest::RunTest(const FString& Parameters)
{
	using namespace EELogViewerTestsPrivate;

	const FString Path = MakeTempPath();
	FEELogFileTail Tail(Path, 1024 * 1024);
	TArray<FString> Lines;

	TestTrue(TEXT("A file that does not exist yet is missing"), Tail.Read(Lines) == FEELogFileTail::EReadResult::Missing);

	// An unfinished last line waits for the rest of it.
	WriteBytes(Path, TEXT("# header\r\nline one\r\nline tw"));
	TestTrue(TEXT("First read"), Tail.Read(Lines) == FEELogFileTail::EReadResult::Appended);
	TestTrue(TEXT("Only complete lines"), Lines == TArray<FString>({ TEXT("# header"), TEXT("line one") }));

	Lines.Reset();
	WriteBytes(Path, TEXT("# header\r\nline one\r\nline two\r\ncaf\u00E9\r\n"));
	TestTrue(TEXT("Appended"), Tail.Read(Lines) == FEELogFileTail::EReadResult::Appended);
	TestTrue(TEXT("The finished line and the next, decoded as UTF-8"), Lines == TArray<FString>({ TEXT("line two"), TEXT("caf\u00E9") }));

	Lines.Reset();
	TestTrue(TEXT("Nothing new"), Tail.Read(Lines) == FEELogFileTail::EReadResult::NoChange);
	TestEqual(TEXT("No lines"), Lines.Num(), 0);

	// Rolled at its size cap: a new, shorter file.
	WriteBytes(Path, TEXT("# header\r\nfresh\r\n"));
	TestTrue(TEXT("A shorter file restarts"), Tail.Read(Lines) == FEELogFileTail::EReadResult::Restarted);
	TestTrue(TEXT("... from its top"), Lines == TArray<FString>({ TEXT("# header"), TEXT("fresh") }));

	// Replaced by a longer file with a different header: detected by the header, not the size.
	Lines.Reset();
	WriteBytes(Path, TEXT("# another session\r\nfirst of the new file\r\nsecond of the new file\r\n"));
	TestTrue(TEXT("A different first line restarts"), Tail.Read(Lines) == FEELogFileTail::EReadResult::Restarted);
	TestEqual(TEXT("... and reads the new file whole"), Lines.Num(), 3);

	IFileManager::Get().Delete(*Path, false, false, true);

	// A long file opens at its end, and "Load earlier" reaches back to its start without gaps.
	const FString LongPath = MakeTempPath();
	FString LongText;
	for (int32 Index = 0; Index < 4000; ++Index)
	{
		LongText += FString::Printf(TEXT("row %05d with some padding text\r\n"), Index);
	}
	WriteBytes(LongPath, LongText);

	FEELogFileTail LongTail(LongPath, 64 * 1024);
	TArray<FString> Tail64;
	LongTail.Read(Tail64);
	TestTrue(TEXT("Only the end was read"), Tail64.Num() > 0 && Tail64.Num() < 4000);
	TestTrue(TEXT("There is more before it"), LongTail.HasEarlier());
	TestEqual(TEXT("The last row is the file's last"), Tail64.Last(), FString(TEXT("row 03999 with some padding text")));

	TArray<FString> All = Tail64;
	while (LongTail.HasEarlier())
	{
		TArray<FString> Earlier;
		if (!LongTail.ReadEarlier(64 * 1024, Earlier))
		{
			break;
		}
		Earlier.Append(All);
		All = MoveTemp(Earlier);
	}

	TestEqual(TEXT("Every row is reached exactly once"), All.Num(), 4000);
	bool bContiguous = All.Num() == 4000;
	for (int32 Index = 0; bContiguous && Index < All.Num(); ++Index)
	{
		bContiguous = All[Index] == FString::Printf(TEXT("row %05d with some padding text"), Index);
	}
	TestTrue(TEXT("... in file order, without gaps"), bContiguous);

	IFileManager::Get().Delete(*LongPath, false, false, true);
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Copyright Moon Punch Games. All Rights Reserved.

#include "EELogFileParser.h"

#if WITH_EDITOR

#include "Algo/BinarySearch.h"
#include "Algo/StableSort.h"
#include "EFLog.h"
#include "HAL/FileManager.h"

namespace EELogFileParserPrivate
{
	static constexpr int32 FieldCount = 5;
	static constexpr int64 ReadChunkBytes = 4 * 1024 * 1024;
	static const TCHAR* ContinuationIndent = TEXT("    ");
}

FString FEELogRow::GetFirstLine() const
{
	int32 LineEnd = INDEX_NONE;
	return Message.FindChar(TEXT('\n'), LineEnd) ? Message.Left(LineEnd) : Message;
}

// ---------------------------------------------------------------------------------------------------
// FEELogLineParser
// ---------------------------------------------------------------------------------------------------

FEELogLineParser::FEELogLineParser()
{
	// Until a header says otherwise, the file is taken to be this machine's. Whole minutes, as the
	// writer rounds them.
	UtcOffset = FTimespan::FromMinutes(FMath::RoundToDouble((FDateTime::Now() - FDateTime::UtcNow()).GetTotalMinutes()));
}

void FEELogLineParser::Parse(const TArray<FString>& Lines, TArray<TSharedPtr<FEELogRow>>& OutNewRows)
{
	using namespace EELogFileParserPrivate;

	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		// "    more text": the rest of a multi-line message.
		if (LastRow.IsValid() && LastRow->Kind == EEELogRowKind::Line && (Line.StartsWith(ContinuationIndent) || Line[0] == TEXT('\t')))
		{
			LastRow->Message += TEXT("\n");
			LastRow->Message += Line.StartsWith(ContinuationIndent) ? Line.RightChop(4) : Line.RightChop(1);
			LastRow->RawText += TEXT("\n");
			LastRow->RawText += Line;
			++LastRow->ExtraLineCount;
			continue;
		}

		const TSharedPtr<FEELogRow> Row = MakeShared<FEELogRow>();
		Row->Sequence = NextSequence++;
		Row->RawText = Line;
		Row->SortTicks = LastSortTicks;

		FString SeparatorText;
		TOptional<FDateTime> SeparatorTime;
		FDateTime LineTime;

		if (ParseSeparator(Line, SeparatorText, SeparatorTime))
		{
			Row->Kind = EEELogRowKind::Separator;
			Row->Message = MoveTemp(SeparatorText);
			if (SeparatorTime.IsSet())
			{
				Row->Time = FormatClock(*SeparatorTime);
				Row->SortTicks = (*SeparatorTime - UtcOffset).GetTicks();
				LastSortTicks = Row->SortTicks;
			}

			// The editor writes "PIE <n> started | ..." / "Simulate <n> started | ..." at every run.
			if (Row->Message.Contains(TEXT(" started")))
			{
				++RunIndex;
				Row->bRunStart = true;
			}

			// Every file of the session carries the same marker; run numbers restart per session.
			Row->StructureKey = CurrentSession + TEXT("|") + Row->Message;
		}
		else if (Line.StartsWith(TEXT("#")))
		{
			Row->Kind = EEELogRowKind::Header;

			// "# EFLog v1 | Category=Araf | Session=... | UtcOffset=... | Process=... | Project=..." reads
			// better as a sentence.
			FString Session;
			FString Process;
			FString OffsetText;
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT(" | "));
			for (const FString& Field : Fields)
			{
				if (Field.StartsWith(TEXT("Session=")))
				{
					Session = Field.RightChop(8);
				}
				else if (Field.StartsWith(TEXT("UtcOffset=")))
				{
					OffsetText = Field.RightChop(10);
				}
				else if (Field.StartsWith(TEXT("Process=")))
				{
					Process = Field.RightChop(8);
				}
			}

			FTimespan Offset;
			const bool bHasOffset = ParseUtcOffset(OffsetText, Offset);
			if (bHasOffset)
			{
				UtcOffset = Offset;
			}

			if (Session.IsEmpty())
			{
				Row->Message = Line;
			}
			else
			{
				Row->Message = bHasOffset
					? FString::Printf(TEXT("Session %s (UTC%s), %s"), *Session, *OffsetText, *Process)
					: FString::Printf(TEXT("Session %s, %s"), *Session, *Process);
			}

			// Placed at the session's start, which comes before anything the session wrote.
			FDateTime SessionTime;
			if (ParseStamp(Session, SessionTime))
			{
				Row->SortTicks = (SessionTime - UtcOffset).GetTicks();
				LastSortTicks = FMath::Max(LastSortTicks, Row->SortTicks);
			}

			CurrentSession = Session + TEXT("|") + Process;
			Row->StructureKey = TEXT("#") + CurrentSession;
		}
		else if (ParseFields(Line, *Row, LineTime))
		{
			Row->Kind = EEELogRowKind::Line;
			if (LineTime.GetTicks() > 0)
			{
				Row->SortTicks = (LineTime - UtcOffset).GetTicks();
				LastSortTicks = Row->SortTicks;
			}
		}
		else
		{
			Row->Kind = EEELogRowKind::Raw;
			Row->Message = Line;

			// A foreign log (the engine's own, say) still gets its errors and warnings coloured.
			if (Line.Contains(TEXT(": Error: ")))
			{
				Row->Verbosity = EEFLogVerbosity::Error;
			}
			else if (Line.Contains(TEXT(": Warning: ")))
			{
				Row->Verbosity = EEFLogVerbosity::Warning;
			}
		}

		Row->RunIndex = RunIndex;
		LastRow = Row;
		OutNewRows.Add(Row);
	}
}

bool FEELogLineParser::ParseSeparator(const FString& Line, FString& OutText, TOptional<FDateTime>& OutTime)
{
	FString Rest;
	if (Line.StartsWith(TEXT("====")))
	{
		Rest = Line;
	}
	else if (Line.StartsWith(TEXT("[")))
	{
		// "[2026.09.24-14.03.11.482] ==== ...": a line row instead has its next field right after the ']'.
		const int32 Close = Line.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		if (Close == INDEX_NONE || (Close + 1 < Line.Len() && Line[Close + 1] == TEXT('[')))
		{
			return false;
		}

		Rest = Line.RightChop(Close + 1).TrimStart();
		if (!Rest.StartsWith(TEXT("====")))
		{
			return false;
		}

		FDateTime Time;
		if (ParseStamp(FStringView(*Line + 1, Close - 1), Time))
		{
			OutTime = Time;
		}
	}
	else
	{
		return false;
	}

	Rest.RemoveFromStart(TEXT("===="));
	Rest.RemoveFromEnd(TEXT("===="));
	OutText = Rest.TrimStartAndEnd();
	return true;
}

bool FEELogLineParser::ParseStamp(FStringView Text, FDateTime& OutTime)
{
	// yyyy.MM.dd-HH.mm.ss, optionally .mmm
	if (Text.Len() != 19 && Text.Len() != 23)
	{
		return false;
	}

	auto ReadNumber = [&Text](int32 Start, int32 Count, int32& OutValue)
	{
		OutValue = 0;
		for (int32 Index = Start; Index < Start + Count; ++Index)
		{
			if (!FChar::IsDigit(Text[Index]))
			{
				return false;
			}
			OutValue = OutValue * 10 + (Text[Index] - TEXT('0'));
		}
		return true;
	};

	int32 Year = 0, Month = 0, Day = 0, Hour = 0, Minute = 0, Second = 0, Millisecond = 0;
	if (!ReadNumber(0, 4, Year) || Text[4] != TEXT('.') || !ReadNumber(5, 2, Month) || Text[7] != TEXT('.')
		|| !ReadNumber(8, 2, Day) || Text[10] != TEXT('-') || !ReadNumber(11, 2, Hour) || Text[13] != TEXT('.')
		|| !ReadNumber(14, 2, Minute) || Text[16] != TEXT('.') || !ReadNumber(17, 2, Second))
	{
		return false;
	}
	if (Text.Len() == 23 && (Text[19] != TEXT('.') || !ReadNumber(20, 3, Millisecond)))
	{
		return false;
	}
	if (!FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		return false;
	}

	OutTime = FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
	return true;
}

bool FEELogLineParser::ParseUtcOffset(FStringView Text, FTimespan& OutOffset)
{
	if (Text.Len() != 6 || (Text[0] != TEXT('+') && Text[0] != TEXT('-')) || Text[3] != TEXT(':')
		|| !FChar::IsDigit(Text[1]) || !FChar::IsDigit(Text[2]) || !FChar::IsDigit(Text[4]) || !FChar::IsDigit(Text[5]))
	{
		return false;
	}

	const int32 Hours = (Text[1] - TEXT('0')) * 10 + (Text[2] - TEXT('0'));
	const int32 Minutes = (Text[4] - TEXT('0')) * 10 + (Text[5] - TEXT('0'));
	if (Hours > 14 || Minutes > 59)
	{
		return false;
	}

	const int32 Sign = Text[0] == TEXT('-') ? -1 : 1;
	OutOffset = FTimespan::FromMinutes(Sign * (Hours * 60 + Minutes));
	return true;
}

FString FEELogLineParser::FormatClock(const FDateTime& Time)
{
	return FString::Printf(TEXT("%02d:%02d:%02d.%03d"), Time.GetHour(), Time.GetMinute(), Time.GetSecond(), Time.GetMillisecond());
}

bool FEELogLineParser::ParseFields(const FString& Line, FEELogRow& OutRow, FDateTime& OutLocalTime)
{
	using namespace EELogFileParserPrivate;

	// Five bracketed fields in a row, then the message. The fields are fixed, so a '[' inside the
	// message can never be mistaken for one.
	FString Fields[FieldCount];
	int32 Position = 0;
	for (int32 FieldIndex = 0; FieldIndex < FieldCount; ++FieldIndex)
	{
		if (Position >= Line.Len() || Line[Position] != TEXT('['))
		{
			return false;
		}

		const int32 Close = Line.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Position + 1);
		if (Close == INDEX_NONE)
		{
			return false;
		}

		Fields[FieldIndex] = Line.Mid(Position + 1, Close - Position - 1);
		Position = Close + 1;
	}

	if (Position < Line.Len() && Line[Position] == TEXT(' '))
	{
		++Position;
	}
	OutRow.Message = Line.RightChop(Position);

	// 2026.09.24-14.03.11.482 -> 14:03:11.482
	if (ParseStamp(Fields[0], OutLocalTime))
	{
		OutRow.Time = FormatClock(OutLocalTime);
	}
	else
	{
		OutLocalTime = FDateTime();
		OutRow.Time = Fields[0];
	}

	OutRow.Frame = Fields[1].StartsWith(TEXT("f")) ? Fields[1].RightChop(1) : Fields[1];
	OutRow.Instance = Fields[2] == TEXT("-") ? FString() : Fields[2];

	if (!EFLog::ParseVerbosity(Fields[3], OutRow.Verbosity))
	{
		OutRow.Verbosity = EEFLogVerbosity::Log;
	}

	// "Source/Game/Araf.cpp:212"; the last ':' splits, since a full Windows path has one of its own.
	const int32 LineColon = Fields[4].Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (LineColon != INDEX_NONE && Fields[4].RightChop(LineColon + 1).IsNumeric())
	{
		OutRow.SourcePath = Fields[4].Left(LineColon);
		OutRow.SourceLine = FCString::Atoi(*Fields[4].RightChop(LineColon + 1));
	}
	else
	{
		OutRow.SourcePath = Fields[4];
	}

	return true;
}

// ---------------------------------------------------------------------------------------------------
// FEELogFileTail
// ---------------------------------------------------------------------------------------------------

FEELogFileTail::FEELogFileTail(const FString& InPath, int64 InInitialWindowBytes)
	: Path(InPath)
	, InitialWindowBytes(FMath::Max<int64>(InInitialWindowBytes, 64 * 1024))
{
}

FEELogFileTail::EReadResult FEELogFileTail::Read(TArray<FString>& OutLines)
{
	using namespace EELogFileParserPrivate;

	// AllowWrite: the writer holds the file open for writing while this reads it.
	const TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path, FILEREAD_AllowWrite | FILEREAD_Silent));
	if (!Reader)
	{
		return EReadResult::Missing;
	}

	const int64 Size = Reader->TotalSize();
	EReadResult Result = EReadResult::NoChange;

	if (!bStarted)
	{
		bStarted = true;
		FirstLine = ReadFirstLine(*Reader);

		// A long file opens at its end; "Load earlier" reaches back from there.
		if (Size > InitialWindowBytes)
		{
			Offset = Size - InitialWindowBytes;
			EarliestOffset = Offset;
			bDiscardUntilNewline = true;
		}
	}
	else if (Size < Offset || (!FirstLine.IsEmpty() && ReadFirstLine(*Reader) != FirstLine))
	{
		// Rolled at the size cap (or truncated): a new file with a new header. Read it from the top;
		// the rows already shown stay.
		Offset = 0;
		EarliestOffset = 0;
		Carry.Reset();
		bDiscardUntilNewline = false;
		FirstLine = ReadFirstLine(*Reader);
		Result = EReadResult::Restarted;
	}
	else if (FirstLine.IsEmpty())
	{
		// The file was still empty the first time.
		FirstLine = ReadFirstLine(*Reader);
	}

	if (Size <= Offset)
	{
		return Result;
	}

	Reader->Seek(Offset);

	TArray<uint8> Buffer;
	int64 Position = Offset;
	while (Position < Size)
	{
		const int32 ChunkSize = static_cast<int32>(FMath::Min<int64>(Size - Position, ReadChunkBytes));
		Buffer.SetNumUninitialized(ChunkSize, EAllowShrinking::No);
		Reader->Serialize(Buffer.GetData(), ChunkSize);

		int32 Start = 0;
		if (bDiscardUntilNewline)
		{
			// Starting mid-file lands mid-line; the first complete line starts after the next newline.
			int32 NewlineIndex = INDEX_NONE;
			for (int32 Index = 0; Index < ChunkSize; ++Index)
			{
				if (Buffer[Index] == '\n')
				{
					NewlineIndex = Index;
					break;
				}
			}

			if (NewlineIndex == INDEX_NONE)
			{
				Position += ChunkSize;
				continue;
			}

			Start = NewlineIndex + 1;
			EarliestOffset = Position + Start;
			bDiscardUntilNewline = false;
		}

		AppendLines(Buffer.GetData() + Start, ChunkSize - Start, Carry, OutLines);
		Position += ChunkSize;
	}

	Offset = Size;
	return Result == EReadResult::Restarted ? Result : EReadResult::Appended;
}

bool FEELogFileTail::ReadEarlier(int64 ChunkBytes, TArray<FString>& OutLines)
{
	if (EarliestOffset <= 0)
	{
		return false;
	}

	const TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path, FILEREAD_AllowWrite | FILEREAD_Silent));
	if (!Reader)
	{
		return false;
	}

	const int64 Start = FMath::Max<int64>(0, EarliestOffset - FMath::Max<int64>(ChunkBytes, 64 * 1024));
	const int32 Length = static_cast<int32>(EarliestOffset - Start);

	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Length);
	Reader->Seek(Start);
	Reader->Serialize(Buffer.GetData(), Length);

	// Unless this reaches the top of the file, it starts mid-line: skip to the next complete one.
	int32 Begin = 0;
	if (Start > 0)
	{
		while (Begin < Length && Buffer[Begin] != '\n')
		{
			++Begin;
		}
		Begin = FMath::Min(Begin + 1, Length);
	}

	// The chunk ends exactly where the earliest shown line starts, so its last line is complete.
	TArray<uint8> LocalCarry;
	AppendLines(Buffer.GetData() + Begin, Length - Begin, LocalCarry, OutLines);
	if (LocalCarry.Num() > 0)
	{
		OutLines.Add(DecodeLine(LocalCarry.GetData(), LocalCarry.Num()));
	}

	EarliestOffset = Start + Begin;
	return true;
}

void FEELogFileTail::AppendLines(const uint8* Data, int64 Size, TArray<uint8>& InOutCarry, TArray<FString>& OutLines)
{
	int64 LineStart = 0;
	for (int64 Index = 0; Index < Size; ++Index)
	{
		if (Data[Index] != '\n')
		{
			continue;
		}

		if (InOutCarry.Num() > 0)
		{
			InOutCarry.Append(Data + LineStart, static_cast<int32>(Index - LineStart));
			OutLines.Add(DecodeLine(InOutCarry.GetData(), InOutCarry.Num()));
			InOutCarry.Reset();
		}
		else
		{
			OutLines.Add(DecodeLine(Data + LineStart, static_cast<int32>(Index - LineStart)));
		}
		LineStart = Index + 1;
	}

	// An unfinished last line waits for the rest of it.
	if (LineStart < Size)
	{
		InOutCarry.Append(Data + LineStart, static_cast<int32>(Size - LineStart));
	}
}

FString FEELogFileTail::DecodeLine(const uint8* Data, int32 Size)
{
	if (Size > 0 && Data[Size - 1] == '\r')
	{
		--Size;
	}

	// A UTF-8 byte order mark only ever appears at the start of a foreign file.
	if (Size >= 3 && Data[0] == 0xEF && Data[1] == 0xBB && Data[2] == 0xBF)
	{
		Data += 3;
		Size -= 3;
	}

	if (Size <= 0)
	{
		return FString();
	}

	const FUTF8ToTCHAR Converted(reinterpret_cast<const UTF8CHAR*>(Data), Size);
	return FString(Converted.Length(), Converted.Get());
}

FString FEELogFileTail::ReadFirstLine(FArchive& Reader)
{
	const int64 Size = Reader.TotalSize();
	if (Size <= 0)
	{
		return FString();
	}

	const int32 Length = static_cast<int32>(FMath::Min<int64>(Size, 512));
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Length);
	Reader.Seek(0);
	Reader.Serialize(Buffer.GetData(), Length);

	int32 LineEnd = 0;
	while (LineEnd < Length && Buffer[LineEnd] != '\n')
	{
		++LineEnd;
	}

	// Only a complete first line identifies the file; a half-written one would look like a new file.
	return LineEnd < Length ? DecodeLine(Buffer.GetData(), LineEnd) : FString();
}

// ---------------------------------------------------------------------------------------------------
// EELogRows
// ---------------------------------------------------------------------------------------------------

bool EELogRows::MergeByTime(TArray<TSharedPtr<FEELogRow>>& Rows, TArray<TSharedPtr<FEELogRow>>&& NewRows)
{
	if (NewRows.Num() == 0)
	{
		return false;
	}

	const auto ByTime = [](const TSharedPtr<FEELogRow>& Row) { return Row->SortTicks; };

	// Stable: a file's own rows keep their order where times tie.
	Algo::StableSortBy(NewRows, ByTime);

	// The common case: everything new is at least as late as everything shown.
	if (Rows.Num() == 0 || Rows.Last()->SortTicks <= NewRows[0]->SortTicks)
	{
		Rows.Append(MoveTemp(NewRows));
		return false;
	}

	// A file that flushed late: merge from the first shown row the new ones go before. On equal
	// times the row already shown stays first.
	const int32 Start = Algo::UpperBoundBy(Rows, NewRows[0]->SortTicks, ByTime);
	TArray<TSharedPtr<FEELogRow>> Later(Rows.GetData() + Start, Rows.Num() - Start);
	Rows.SetNum(Start, EAllowShrinking::No);
	Rows.Reserve(Start + Later.Num() + NewRows.Num());

	int32 LaterIndex = 0;
	int32 NewIndex = 0;
	while (LaterIndex < Later.Num() && NewIndex < NewRows.Num())
	{
		if (NewRows[NewIndex]->SortTicks < Later[LaterIndex]->SortTicks)
		{
			Rows.Add(MoveTemp(NewRows[NewIndex++]));
		}
		else
		{
			Rows.Add(MoveTemp(Later[LaterIndex++]));
		}
	}
	for (; LaterIndex < Later.Num(); ++LaterIndex)
	{
		Rows.Add(MoveTemp(Later[LaterIndex]));
	}
	for (; NewIndex < NewRows.Num(); ++NewIndex)
	{
		Rows.Add(MoveTemp(NewRows[NewIndex]));
	}
	return true;
}

#endif // WITH_EDITOR

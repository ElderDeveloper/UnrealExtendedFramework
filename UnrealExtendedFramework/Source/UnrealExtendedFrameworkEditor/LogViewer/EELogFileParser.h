// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "EFLogTypes.h"

enum class EEELogRowKind : uint8
{
	/** [time][frame][instance][level][source:line] message */
	Line,
	/** [time] ==== text ==== (files written before separators carried a time: ==== text ====) */
	Separator,
	/** # EFLog v1 | ... (a session starting in this file) */
	Header,
	/** Anything else: a .log that is not in EF_LOG's format still opens, one row per line. */
	Raw
};

/** One entry as the Extended Log window shows it. Continuation lines extend the row they belong to. */
struct FEELogRow
{
	/** Full message; multi-line messages keep their line breaks. */
	FString Message;

	/** The row's text exactly as in the file (every physical line), for copying. */
	FString RawText;

	FString Time;
	FString Frame;
	FString Instance;
	FString SourcePath;

	/**
	 * Separators and headers only: identifies the marker across files, so a view merging several
	 * files shows each once. A run marker is written into every file; a header repeats per file.
	 */
	FString StructureKey;

	/** Position in its tab, used by "Clear view". Rows loaded with "Load earlier" use 0. */
	uint64 Sequence = 0;

	/**
	 * When the row was written, as UTC ticks: what orders rows from several files. A row without a
	 * time of its own (a foreign line, an old untimed separator) takes the last time seen in its file.
	 */
	int64 SortTicks = 0;

	int32 SourceLine = 0;

	/** Run-start separators seen before (and including) this row; -1 for rows loaded with "Load earlier". */
	int32 RunIndex = 0;

	/** Physical lines after the first. */
	int32 ExtraLineCount = 0;

	/** Which of its tab's files the row came from (a merged tab reads several). */
	int32 SourceIndex = 0;

	EEELogRowKind Kind = EEELogRowKind::Raw;
	EEFLogVerbosity Verbosity = EEFLogVerbosity::Log;

	/** A "PIE <n> started" / "Simulate <n> started" separator. */
	bool bRunStart = false;

	/** The first line of Message: what the list shows. */
	FString GetFirstLine() const;
};

/**
 * Turns complete text lines into rows. Keeps just enough state to attach continuation lines, count
 * runs, and turn the file's local times into UTC (from its header's UtcOffset).
 */
class FEELogLineParser
{
public:
	FEELogLineParser();

	/** New rows go to OutNewRows; a continuation line extends the previous row instead, even one from an earlier call. */
	void Parse(const TArray<FString>& Lines, TArray<TSharedPtr<FEELogRow>>& OutNewRows);

	/** After the file was replaced, a first line must not be taken as the continuation of the old file's last one. */
	void ResetContinuation() { LastRow.Reset(); }

	int32 GetRunIndex() const { return RunIndex; }
	uint64 PeekNextSequence() const { return NextSequence; }

	/** "2026.09.24-14.03.11" or "2026.09.24-14.03.11.482", as written by EF_LOG. */
	static bool ParseStamp(FStringView Text, FDateTime& OutTime);

	/** "+03:00" / "-05:30". */
	static bool ParseUtcOffset(FStringView Text, FTimespan& OutOffset);

private:
	/** Fills everything but SortTicks; OutLocalTime is the line's own time. */
	static bool ParseFields(const FString& Line, FEELogRow& OutRow, FDateTime& OutLocalTime);

	/** "[time] ==== text ====" or the older "==== text ====". OutTime stays unset for the older form. */
	static bool ParseSeparator(const FString& Line, FString& OutText, TOptional<FDateTime>& OutTime);

	/** 14:03:11.482 */
	static FString FormatClock(const FDateTime& Time);

	TSharedPtr<FEELogRow> LastRow;

	/** The file's session and process, from its latest header: scopes run markers, which repeat across sessions. */
	FString CurrentSession;

	/** From the latest header; files without one (or older ones without the field) are assumed to be this machine's. */
	FTimespan UtcOffset;

	int64 LastSortTicks = 0;
	int32 RunIndex = 0;
	uint64 NextSequence = 1;
};

namespace EELogRows
{
	/**
	 * Adds NewRows to Rows, which is ordered by SortTicks, keeping it ordered: rows with equal times
	 * keep the order they arrived in, so each file's own order survives. Returns true when a row went
	 * anywhere but the end (a file that flushed late), since a filtered copy of Rows is then stale.
	 */
	bool MergeByTime(TArray<TSharedPtr<FEELogRow>>& Rows, TArray<TSharedPtr<FEELogRow>>&& NewRows);
}

/**
 * Follows one file as it grows. Every read opens, reads and closes the file: holding a handle open
 * would stop the writer from moving the file aside when it reaches its size cap.
 */
class FEELogFileTail
{
public:
	enum class EReadResult : uint8
	{
		NoChange,
		Appended,
		/** The file shrank or was replaced (rolled at its size cap); reading restarted at its beginning. */
		Restarted,
		Missing
	};

	FEELogFileTail(const FString& InPath, int64 InInitialWindowBytes);

	/** Complete lines appended since the last call. The first call reads only the last InitialWindowBytes. */
	EReadResult Read(TArray<FString>& OutLines);

	/** Up to ChunkBytes of complete lines before the earliest point read so far, in file order. */
	bool ReadEarlier(int64 ChunkBytes, TArray<FString>& OutLines);

	bool HasEarlier() const { return EarliestOffset > 0; }
	const FString& GetPath() const { return Path; }

private:
	static void AppendLines(const uint8* Data, int64 Size, TArray<uint8>& Carry, TArray<FString>& OutLines);
	static FString DecodeLine(const uint8* Data, int32 Size);
	static FString ReadFirstLine(FArchive& Reader);

	FString Path;
	FString FirstLine;
	TArray<uint8> Carry;
	int64 InitialWindowBytes = 0;
	int64 Offset = 0;
	int64 EarliestOffset = 0;
	bool bStarted = false;
	bool bDiscardUntilNewline = false;
};

#endif // WITH_EDITOR

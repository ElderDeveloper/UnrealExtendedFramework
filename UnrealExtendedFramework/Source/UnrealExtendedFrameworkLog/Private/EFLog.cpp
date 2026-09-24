// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLog.h"

#include "CoreGlobals.h"
#include "EFLogInstanceLabels.h"
#include "EFLogRegistry.h"
#include "EFLogWriter.h"
#include "HAL/PlatformTime.h"

const TCHAR* LexToString(EEFLogVerbosity Verbosity)
{
	switch (Verbosity)
	{
	case EEFLogVerbosity::Error:       return TEXT("Error");
	case EEFLogVerbosity::Warning:     return TEXT("Warning");
	case EEFLogVerbosity::Display:     return TEXT("Display");
	case EEFLogVerbosity::Log:         return TEXT("Log");
	case EEFLogVerbosity::Verbose:     return TEXT("Verbose");
	case EEFLogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
	}
	return TEXT("Log");
}

FEFLogCategory::FEFLogCategory(FName InName, const FString& InDisplayName, EEFLogVerbosity InVerbosity)
	: Name(InName)
	, DisplayName(InDisplayName)
	, Verbosity(static_cast<uint8>(InVerbosity))
{
}

namespace EFLog
{
	namespace Private
	{
		FEFLogCategory& FindOrAddCategory(const TCHAR* Name)
		{
			return FEFLogRegistry::Get().FindOrAdd(FStringView(Name));
		}

		FEFLogCategory& FindOrAddCategory(FStringView Name)
		{
			return FEFLogRegistry::Get().FindOrAdd(Name);
		}

		FEFLogCategory& FindOrAddCategory(const FString& Name)
		{
			return FEFLogRegistry::Get().FindOrAdd(FStringView(Name));
		}

		FEFLogCategory& FindOrAddCategory(FName Name)
		{
			return FEFLogRegistry::Get().FindOrAdd(Name.ToString());
		}

		void Submit(FEFLogCategory& Category, EEFLogVerbosity Verbosity, const ANSICHAR* File, int32 Line, FString&& Message)
		{
			FEFLogEntry Entry;
			Entry.Message = MoveTemp(Message);
			Entry.Category = &Category;
			Entry.SourceFile = File;
			Entry.SourceLine = Line;
			Entry.Verbosity = Verbosity;
			Entry.Seconds = FPlatformTime::Seconds();
			Entry.Frame = GFrameCounter;

			// GetPlayInEditorID() asserts on the async loading thread when the id was not forwarded
			// there, and a log line must never be able to assert. Loading-thread lines (and the game
			// thread while it acts as the loader) are simply "no instance"; worker threads already
			// report -1 by design.
			Entry.PIEInstance = IsInAsyncLoadingThread() ? INDEX_NONE : UE::GetPlayInEditorID();

			// A valid PIE id means the game thread, where world contexts can be read for the label.
			Entry.InstanceLabel = EFLogInstanceLabels::ForPIEInstance(Entry.PIEInstance);

			FEFLogWriter::Get().Submit(MoveTemp(Entry));
		}

		void SubmitFromWorld(FEFLogCategory& Category, EEFLogVerbosity Verbosity, const UWorld* World, FString&& SourceText, FString&& Message)
		{
			FEFLogEntry Entry;
			Entry.Message = MoveTemp(Message);
			Entry.SourceText = MoveTemp(SourceText);
			Entry.Category = &Category;
			Entry.Verbosity = Verbosity;
			Entry.Seconds = FPlatformTime::Seconds();
			Entry.Frame = GFrameCounter;
			Entry.InstanceLabel = EFLogInstanceLabels::ForWorld(World, Entry.PIEInstance);

			FEFLogWriter::Get().Submit(MoveTemp(Entry));
		}
	}

	FString SanitizeCategoryName(FStringView Name)
	{
		// Longer names still work; they are only cut to keep file paths sane.
		static constexpr int32 MaxLength = 64;

		FString Trimmed(Name);
		Trimmed.TrimStartAndEndInline();

		FString Result;
		Result.Reserve(FMath::Min(Trimmed.Len(), MaxLength));
		for (const TCHAR Character : Trimmed)
		{
			if (Result.Len() >= MaxLength)
			{
				break;
			}

			const bool bSafe = (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9'))
				|| Character == TEXT('_');
			Result.AppendChar(bSafe ? Character : TEXT('_'));
		}

		if (Result.IsEmpty())
		{
			return TEXT("Unnamed");
		}

		// Windows refuses these as file names whatever the extension ("Con.log" is the console).
		static const TCHAR* ReservedNames[] =
		{
			TEXT("CON"), TEXT("PRN"), TEXT("AUX"), TEXT("NUL"),
			TEXT("COM1"), TEXT("COM2"), TEXT("COM3"), TEXT("COM4"), TEXT("COM5"), TEXT("COM6"), TEXT("COM7"), TEXT("COM8"), TEXT("COM9"),
			TEXT("LPT1"), TEXT("LPT2"), TEXT("LPT3"), TEXT("LPT4"), TEXT("LPT5"), TEXT("LPT6"), TEXT("LPT7"), TEXT("LPT8"), TEXT("LPT9")
		};
		for (const TCHAR* Reserved : ReservedNames)
		{
			if (Result.Equals(Reserved, ESearchCase::IgnoreCase))
			{
				Result.AppendChar(TEXT('_'));
				break;
			}
		}

		return Result;
	}

	bool ParseVerbosity(FStringView Text, EEFLogVerbosity& OutVerbosity)
	{
		const FString Trimmed = FString(Text).TrimStartAndEnd();
		for (uint8 Value = static_cast<uint8>(EEFLogVerbosity::Error); Value <= static_cast<uint8>(EEFLogVerbosity::VeryVerbose); ++Value)
		{
			const EEFLogVerbosity Candidate = static_cast<EEFLogVerbosity>(Value);
			if (Trimmed.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutVerbosity = Candidate;
				return true;
			}
		}
		return false;
	}

	void SetCategoryVerbosity(FName CategoryName, EEFLogVerbosity Verbosity)
	{
		FEFLogRegistry::Get().SetCategoryVerbosity(CategoryName, Verbosity);
	}

	void SetAllCategoriesVerbosity(EEFLogVerbosity Verbosity)
	{
		FEFLogRegistry::Get().SetAllVerbosity(Verbosity);
	}

	bool GetCategoryVerbosity(FName CategoryName, EEFLogVerbosity& OutVerbosity)
	{
		if (const FEFLogCategory* Category = FEFLogRegistry::Get().Find(CategoryName))
		{
			OutVerbosity = Category->GetVerbosity();
			return true;
		}
		return false;
	}

	TArray<FString> GetCategoryNames()
	{
		TArray<FString> Names;
		FEFLogRegistry::Get().ForEach([&Names](FEFLogCategory& Category)
		{
			Names.Add(Category.GetDisplayName());
		});
		Names.Sort();
		return Names;
	}

	void WriteSeparator(const FString& Text, EEFLogSeparatorKind Kind)
	{
		FEFLogEntry Entry;
		Entry.Kind = EEFLogEntryKind::Separator;
		Entry.SeparatorKind = Kind;
		Entry.Message = Text;
		Entry.Seconds = FPlatformTime::Seconds();
		Entry.Frame = GFrameCounter;
		FEFLogWriter::Get().Submit(MoveTemp(Entry));
	}

	void Flush()
	{
		FEFLogWriter::Get().Flush();
	}

	FString GetRootDirectory()
	{
		return FEFLogWriter::Get().GetRootDirectory();
	}

	FString GetLogDirectory()
	{
		return FEFLogWriter::Get().GetWriteDirectory();
	}

	FString GetCategoryFilePath(FName CategoryName)
	{
		if (const FEFLogCategory* Category = FEFLogRegistry::Get().Find(CategoryName))
		{
			return FEFLogWriter::Get().GetCategoryFilePath(*Category);
		}
		return FString();
	}

	FDelegateHandle AddFilesFlushedListener(FEFLogFilesFlushed::FDelegate&& Listener)
	{
		return FEFLogWriter::Get().AddFilesFlushedListener(MoveTemp(Listener));
	}

	void RemoveFilesFlushedListener(FDelegateHandle Handle)
	{
		FEFLogWriter::Get().RemoveFilesFlushedListener(Handle);
	}
}

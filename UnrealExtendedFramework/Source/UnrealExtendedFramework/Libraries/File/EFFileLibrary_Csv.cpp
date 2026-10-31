// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: CSV files and CSV string conversion.

#include "EFFileLibrary.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/Csv/CsvParser.h"

bool UEFFileLibrary::SaveCSV(FString Path, TArray<FString> Headers, TArray<FString> Data, int32& Total, bool Force)
{
	FString Output;
	if (!UEFFileLibrary::CSVToString(Output, Headers, Data, Total))
	{
		return false;
	}
	FString Error;
	return UEFFileLibrary::SaveText(Path, Output, Error, false, Force);
}

bool UEFFileLibrary::ReadCSV(FString Path, TArray<FString>& Headers, TArray<FString>& Data, int32& Total,
                             bool HeaderFirst)
{
	Total = 0;
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	if (!file.FileExists(*Path))
	{
		return false;
	}
	FString Result;
	if (!FFileHelper::LoadFileToString(Result, *Path))
	{
		return false;
	}
	return UEFFileLibrary::StringToCSV(Result, Headers, Data, Total, HeaderFirst);
	// return UEFFileLibrary::StringArrayToCSV(Result, Headers, Data, Total, ",", HeaderFirst);
}

bool UEFFileLibrary::StringToCSV(FString Content, TArray<FString>& Headers, TArray<FString>& Data, int32& Total,
                                 bool HeaderFirst)
{
	FCsvParser Parser(Content);
	TArray<TArray<const TCHAR*>> Rows = Parser.GetRows();
	for (TArray<const TCHAR*> Row : Rows)
	{
		Total++;
		for (FString Col : Row)
		{
			if (Total == 1 && HeaderFirst)
			{
				Headers.Add(Col);
			}
			else
			{
				Data.Add(Col);
			}
		}
	}
	return true;
	// auto Result = SplitString(Content, LINE_TERMINATOR, ESearchCase::Type::IgnoreCase);
	// return UEFFileLibrary::StringArrayToCSV(Result, Headers, Data, Total, HeaderFirst);
}

bool UEFFileLibrary::CSVToString(FString& Output, TArray<FString> Headers, TArray<FString> Data, int32& Total)
{
	Total = 0;
	FString Delimiter = ",";
	if (Headers.Num() == 0)
	{
		return false;
	}
	if (Data.Num() % Headers.Num() != 0)
	{
		return false;
	}
	Output = TEXT("");
	// Header row
	for (FString Col : Headers)
	{
		if (Output.Len() > 0)
		{
			Output += Delimiter;
		}
		Output += (TEXT("\"") + Col.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\""));
	}
	Output += LINE_TERMINATOR;
	FString Row = TEXT("");
	int32 Count = 0;
	// Data row
	for (FString Col : Data)
	{
		Count++;
		if (Row.Len() > 0)
		{
			Row += Delimiter;
		}
		Row += (TEXT("\"") + Col.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\""));
		if (Count % Headers.Num() == 0)
		{
			Row += LINE_TERMINATOR;
			Output += Row;
			Row = "";
		}
	}
	Total = (Data.Num() / Headers.Num()) + 1;
	return true;
}

bool UEFFileLibrary::StringArrayToCSV(TArray<FString> Lines, TArray<FString>& Headers, TArray<FString>& Data,
                                      int32& Total, FString Delimiter, bool HeaderFirst)
{
	for (auto Line : Lines)
	{
		Total++;
		if (!Line.Contains(TEXT("\"") + Delimiter + TEXT("\"")))
		{
			continue;
		}
		if (Total == 1 && HeaderFirst)
		{
			for (FString Col : UEFFileLibrary::SplitString(Line, TEXT("\"") + Delimiter + TEXT("\""),
			                                               ESearchCase::CaseSensitive))
			{
				Col.TrimQuotesInline();
				Col.ReplaceInline(TEXT("\"\""), TEXT("\""));
				Headers.Add(Col);
			}
		}
		else
		{
			for (FString Col : UEFFileLibrary::SplitString(Line, TEXT("\"") + Delimiter + TEXT("\""),
			                                               ESearchCase::CaseSensitive))
			{
				Col.TrimQuotesInline();
				Col.ReplaceInline(TEXT("\"\""), TEXT("\""));
				Data.Add(Col);
			}
		}
	}
	return true;
}

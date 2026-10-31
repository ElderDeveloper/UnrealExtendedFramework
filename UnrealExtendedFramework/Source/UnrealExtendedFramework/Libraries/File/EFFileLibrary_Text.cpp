// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Text, line and byte files, Base64 and string splitting.

#include "EFFileLibrary.h"

#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"

bool UEFFileLibrary::ReadText(FString Path, FString& Output)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	if (file.FileExists(*Path))
	{
		return FFileHelper::LoadFileToString(Output, *Path);
	}
	return false;
}

bool UEFFileLibrary::SaveText(FString Path, FString Text, FString& Error, bool Append, bool Force)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	FText ErrorFilename;
	if (!FFileHelper::IsFilenameValidForSaving(Path, ErrorFilename))
	{
		Error = FString("Filename is not valid");
		return false;
	}
	if (!file.FileExists(*Path) || Append || Force)
	{
		return FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::AutoDetect,
		                                     &IFileManager::Get(), Append ? FILEWRITE_Append : FILEWRITE_None);
	}
	else
	{
		Error = FString("File already exists");
	}
	return false;
}

bool UEFFileLibrary::ReadLine(FString Path, FString Pattern, TArray<FString>& Lines)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	if (!file.FileExists(*Path))
	{
		return false;
	}
	if (!Pattern.IsEmpty())
	{
		FRegexPattern CustomPattern(Pattern);
		return FFileHelper::LoadFileToStringArrayWithPredicate(Lines, *Path, [CustomPattern](const FString Line)
		{
			FRegexMatcher CustomMatcher(CustomPattern, Line);
			return CustomMatcher.FindNext();
		});
	}
	else
	{
		return FFileHelper::LoadFileToStringArray(Lines, *Path);
	}
}

bool UEFFileLibrary::SaveLine(FString Path, const TArray<FString>& Text, FString& Error, bool Append, bool Force)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	FText ErrorFilename;
	if (!FFileHelper::IsFilenameValidForSaving(Path, ErrorFilename))
	{
		Error = FString("Filename is not valid");
		return false;
	}
	if (!file.FileExists(*Path) || Append || Force)
	{
		return FFileHelper::SaveStringArrayToFile(Text, *Path, FFileHelper::EEncodingOptions::AutoDetect,
		                                          &IFileManager::Get(), Append ? FILEWRITE_Append : FILEWRITE_None);
	}
	else
	{
		Error = FString("File already exists");
	}
	return false;
}

bool UEFFileLibrary::ReadByte(FString Path, TArray<uint8>& Bytes)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	if (!file.FileExists(*Path))
	{
		return false;
	}
	return FFileHelper::LoadFileToArray(Bytes, *Path);
}

bool UEFFileLibrary::SaveByte(FString Path, const TArray<uint8>& Bytes, FString& Error, bool Append, bool Force)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	FText ErrorFilename;
	if (!FFileHelper::IsFilenameValidForSaving(Path, ErrorFilename))
	{
		Error = FString("Filename is not valid");
		return false;
	}
	if (!file.FileExists(*Path) || Append || Force)
	{
		return FFileHelper::SaveArrayToFile(Bytes, *Path, &IFileManager::Get(),
		                                    Append ? FILEWRITE_Append : FILEWRITE_None);
	}
	else
	{
		Error = FString("File already exists");
	}
	return false;
}

FString UEFFileLibrary::StringToBase64(const FString& Source)
{
	return FBase64::Encode(Source);
}

bool UEFFileLibrary::StringFromBase64(const FString& Base64Str, FString& Result)
{
	return FBase64::Decode(Base64Str, Result);
}

FString UEFFileLibrary::BytesToBase64(const TArray<uint8>& Bytes)
{
	return FBase64::Encode(Bytes);
}

bool UEFFileLibrary::BytesFromBase64(const FString& Source, TArray<uint8>& Out)
{
	return FBase64::Decode(Source, Out);
}

TArray<FString> UEFFileLibrary::SplitString(FString String, FString Separator, ESearchCase::Type SearchCase)
{
	FString LeftString;
	FString RightString;
	TArray<FString> Array;
	bool Split = false;
	do
	{
		Split = String.Split(Separator, &LeftString, &RightString, SearchCase);
		if (Split)
		{
			Array.Add(LeftString);
		}
		else
		{
			Array.Add(String);
		}
		String = RightString;
	}
	while (Split);

	return Array;
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianDocumentStore.h"

#include "ExtendedAtlassianLog.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ExtendedAtlassianDocumentStorePrivate
{
	const TCHAR* Delimiter = TEXT("---");

	const TCHAR* KeyPageId = TEXT("confluence-id");
	const TCHAR* KeySpaceId = TEXT("confluence-space-id");
	const TCHAR* KeySpaceKey = TEXT("confluence-space-key");
	const TCHAR* KeyVersion = TEXT("confluence-version");
	const TCHAR* KeyTitle = TEXT("title");

	/** Makes a title safe for a filename without losing its readability. */
	FString SanitizeForFileName(const FString& Title)
	{
		FString Result;
		Result.Reserve(Title.Len());

		for (const TCHAR Char : Title)
		{
			const bool bIllegal =
				Char == TEXT('/') || Char == TEXT('\\') || Char == TEXT(':') || Char == TEXT('*') ||
				Char == TEXT('?') || Char == TEXT('"') || Char == TEXT('<') || Char == TEXT('>') ||
				Char == TEXT('|') || Char < 32;

			Result.AppendChar(bIllegal ? TEXT('-') : Char);
		}

		Result.TrimStartAndEndInline();

		// Long titles produce paths that overflow the Windows limit once combined with a deep
		// project directory.
		if (Result.Len() > 60)
		{
			Result.LeftInline(60);
			Result.TrimEndInline();
		}

		return Result.IsEmpty() ? TEXT("Untitled") : Result;
	}
}

FString FExtendedAtlassianDocumentStore::GetRootDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Documents"));
}

FString FExtendedAtlassianDocumentStore::MakeFilePath(const FString& SpaceKey, const FString& Title, const FString& PageId)
{
	using namespace ExtendedAtlassianDocumentStorePrivate;

	const FString Folder = SpaceKey.IsEmpty() ? TEXT("Unfiled") : SanitizeForFileName(SpaceKey);

	// The id is part of the filename so renaming a page in Confluence does not orphan its working
	// copy or create a duplicate on the next pull.
	const FString FileName = PageId.IsEmpty()
		? SanitizeForFileName(Title) + TEXT(".md")
		: FString::Printf(TEXT("%s-%s.md"), *SanitizeForFileName(Title), *PageId);

	return GetRootDirectory() / Folder / FileName;
}

FString FExtendedAtlassianDocumentStore::MakeFrontMatter(const FExtendedAtlassianDocumentFile& File)
{
	using namespace ExtendedAtlassianDocumentStorePrivate;

	TArray<FString> Lines;
	Lines.Add(Delimiter);
	Lines.Add(FString::Printf(TEXT("%s: %s"), KeyTitle, *File.Title));

	if (File.IsLinkedToConfluence())
	{
		Lines.Add(FString::Printf(TEXT("%s: %s"), KeyPageId, *File.PageId));
		Lines.Add(FString::Printf(TEXT("%s: %s"), KeySpaceId, *File.SpaceId));
		Lines.Add(FString::Printf(TEXT("%s: %s"), KeySpaceKey, *File.SpaceKey));
		Lines.Add(FString::Printf(TEXT("%s: %d"), KeyVersion, File.Version));
	}

	Lines.Add(Delimiter);
	Lines.Add(FString());

	return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
}

bool FExtendedAtlassianDocumentStore::ParseFrontMatter(const FString& Contents, FExtendedAtlassianDocumentFile& OutFile)
{
	using namespace ExtendedAtlassianDocumentStorePrivate;

	FString Normalized = Contents;
	Normalized.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

	if (!Normalized.StartsWith(TEXT("---")))
	{
		// A plain Markdown file with no front matter is perfectly valid; it is simply not linked to
		// any Confluence page.
		OutFile.Markdown = Normalized;
		return false;
	}

	TArray<FString> Lines;
	Normalized.ParseIntoArray(Lines, TEXT("\n"), false);

	int32 CloseIndex = INDEX_NONE;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		if (Lines[Index].TrimStartAndEnd() == Delimiter)
		{
			CloseIndex = Index;
			break;
		}
	}

	if (CloseIndex == INDEX_NONE)
	{
		// An unterminated block is malformed; treat the whole file as body rather than eating it.
		OutFile.Markdown = Normalized;
		return false;
	}

	for (int32 Index = 1; Index < CloseIndex; ++Index)
	{
		const FString& Line = Lines[Index];

		int32 Colon = INDEX_NONE;
		if (!Line.FindChar(TEXT(':'), Colon))
		{
			continue;
		}

		const FString Key = Line.Left(Colon).TrimStartAndEnd();
		const FString Value = Line.RightChop(Colon + 1).TrimStartAndEnd();

		if (Key == KeyPageId)         { OutFile.PageId = Value; }
		else if (Key == KeySpaceId)   { OutFile.SpaceId = Value; }
		else if (Key == KeySpaceKey)  { OutFile.SpaceKey = Value; }
		else if (Key == KeyTitle)     { OutFile.Title = Value; }
		else if (Key == KeyVersion)   { OutFile.Version = FCString::Atoi(*Value); }
	}

	TArray<FString> BodyLines;
	for (int32 Index = CloseIndex + 1; Index < Lines.Num(); ++Index)
	{
		BodyLines.Add(Lines[Index]);
	}

	OutFile.Markdown = FString::Join(BodyLines, TEXT("\n")).TrimStart();
	return true;
}

bool FExtendedAtlassianDocumentStore::Save(const FExtendedAtlassianPage& Page, const FString& SpaceKey, FString& OutPath)
{
	FExtendedAtlassianDocumentFile File;
	File.PageId = Page.Id;
	File.SpaceId = Page.SpaceId;
	File.SpaceKey = SpaceKey;
	File.Title = Page.Title;
	File.Version = Page.Version;
	File.Markdown = Page.Markdown;

	OutPath = MakeFilePath(SpaceKey, Page.Title, Page.Id);

	const FString Directory = FPaths::GetPath(OutPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not create document directory %s."), *Directory);
		return false;
	}

	const FString Contents = MakeFrontMatter(File) + File.Markdown;

	// UTF-8 without a BOM: these files are read by other tools, and a BOM trips several of them.
	if (!FFileHelper::SaveStringToFile(Contents, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogExtendedAtlassian, Error, TEXT("Could not write %s."), *OutPath);
		return false;
	}

	UE_LOG(LogExtendedAtlassian, Log, TEXT("Wrote working copy %s (version %d)."), *OutPath, Page.Version);
	return true;
}

bool FExtendedAtlassianDocumentStore::Load(const FString& Path, FExtendedAtlassianDocumentFile& OutFile)
{
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *Path))
	{
		return false;
	}

	OutFile = FExtendedAtlassianDocumentFile();
	OutFile.FilePath = Path;

	ParseFrontMatter(Contents, OutFile);

	if (OutFile.Title.IsEmpty())
	{
		OutFile.Title = FPaths::GetBaseFilename(Path);
	}

	return true;
}

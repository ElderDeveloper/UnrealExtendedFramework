// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

// Helpers shared by the EFFileLibrary_*.cpp files. Not part of the library's public API: include it
// only from this module's .cpp files. Everything here has a unique name or is defined inline, so the
// files can share a unity build.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Internationalization/Regex.h"
#include "Misc/Paths.h"

/**
 * Collects the files and/or directories UEFFileLibrary::ListDirectory visits, relative to BasePath,
 * keeping only those that match Filter when it is not empty.
 */
class FEFFileLibraryDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	FString BasePath;
	TArray<FString>& Nodes;
	FString Filter;
	FRegexPattern CustomPattern;
	bool bFile = true;
	bool bDirectory = true;

	FEFFileLibraryDirectoryVisitor(const FString& InBasePath, TArray<FString>& InNodes, const FString& InPattern, bool bInFile, bool bInDirectory)
		: BasePath(InBasePath)
		, Nodes(InNodes)
		, Filter(InPattern)
		, CustomPattern(InPattern)
		, bFile(bInFile)
		, bDirectory(bInDirectory)
	{
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
	{
		if ((bFile && !bIsDirectory) || (bDirectory && bIsDirectory))
		{
			FString RelativePath = FString(FilenameOrDirectory);
			FPaths::MakePathRelativeTo(RelativePath, *BasePath);
			if (!Filter.IsEmpty())
			{
				FRegexMatcher CustomMatcher(CustomPattern, RelativePath);
				if (CustomMatcher.FindNext())
				{
					Nodes.Add(RelativePath);
				}
			}
			else
			{
				Nodes.Add(RelativePath);
			}
		}
		return true;
	}
};

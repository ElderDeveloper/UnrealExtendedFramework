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
 * The checks behind UEFFileAccessSettings (EFFileLibrary_Access.cpp). Every Extended node that takes
 * a file path calls CheckRead or CheckWrite with the path before it touches the disk, and fails the
 * way it already reports failure when the check does.
 */
namespace EFFileLibraryAccess
{
	/**
	 * True when Path may be read, listed or queried (or, with bForWrite, written, moved, deleted or
	 * created) under the file access settings. Always true when the restriction does not apply to
	 * this build. Otherwise Path is made absolute and normalised first, so ".." cannot leave an
	 * allowed directory. When the path is refused and OutReason is given, it receives why.
	 */
	bool IsAllowed(const FString& Path, bool bForWrite, FString* OutReason = nullptr);

	/** IsAllowed for reading. When refused, logs one warning naming FunctionName and the reason. */
	bool CheckRead(const TCHAR* FunctionName, const FString& Path);

	/** IsAllowed for writing. When refused, logs one warning naming FunctionName and the reason. */
	bool CheckWrite(const TCHAR* FunctionName, const FString& Path);

	/**
	 * CheckRead or CheckWrite for the file argument of the config nodes. The names of GConfig's known
	 * config files ("Engine", "Game", "Input", "GameUserSettings" and the others it lists in
	 * ENUMERATE_KNOWN_INI_FILES) are not paths: GConfig writes them under the project's Saved/Config,
	 * which is always allowed. Anything else is checked as a path.
	 */
	bool CheckConfigFile(const TCHAR* FunctionName, const FString& Filename, bool bForWrite);
}

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

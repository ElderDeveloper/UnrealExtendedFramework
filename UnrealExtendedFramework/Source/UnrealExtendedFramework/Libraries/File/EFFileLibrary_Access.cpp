// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: the path checks behind UEFFileAccessSettings, and the Is Path Allowed node.

#include "EFFileLibrary.h"

#include "EFFileAccessSettings.h"
#include "EFFileLibraryPrivate.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "UnrealExtendedFramework.h"

namespace EFFileLibraryAccess
{
	/**
	 * True for a path segment made only of dots and spaces other than "." and "..", such as "..." or
	 * ".. ". File systems resolve those differently (Windows trims trailing dots and spaces), so such
	 * a path is refused rather than guessed at.
	 */
	static bool IsAmbiguousDotSegment(const FString& Segment)
	{
		if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT(".."))
		{
			return false;
		}
		for (int32 CharIndex = 0; CharIndex < Segment.Len(); ++CharIndex)
		{
			const TCHAR SegmentChar = Segment[CharIndex];
			if (SegmentChar != TEXT('.') && SegmentChar != TEXT(' '))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Makes InPath absolute, with '/' separators and every "." and ".." resolved. Returns false when
	 * the path is empty or cannot be compared safely: ".." above the root of an absolute path, or an
	 * ambiguous dot segment.
	 */
	static bool NormalizeForAccessCheck(const FString& InPath, FString& OutPath)
	{
		OutPath.Reset();
		if (InPath.TrimStartAndEnd().IsEmpty())
		{
			return false;
		}

		// ConvertRelativePathToFull rewrites some dot segments ("a/.../b" becomes "a/..b"), so look at
		// the segments as given first.
		FString GivenPath = InPath;
		GivenPath.ReplaceInline(TEXT("\\"), TEXT("/"));
		TArray<FString> GivenSegments;
		GivenPath.ParseIntoArray(GivenSegments, TEXT("/"), true);
		for (const FString& GivenSegment : GivenSegments)
		{
			if (IsAmbiguousDotSegment(GivenSegment))
			{
				return false;
			}
		}

		// ConvertRelativePathToFull makes the path absolute (a relative path is relative to the base
		// directory, as the platform file resolves it), uses '/' separators and collapses the ".." it
		// can. It leaves any other ".." in place, so the segments are resolved again below.
		const FString AbsolutePath = FPaths::ConvertRelativePathToFull(InPath);

		FString RootPrefix;
		if (AbsolutePath.StartsWith(TEXT("//")))
		{
			RootPrefix = TEXT("//");
		}
		else if (AbsolutePath.StartsWith(TEXT("/")))
		{
			RootPrefix = TEXT("/");
		}

		TArray<FString> AbsoluteSegments;
		AbsolutePath.ParseIntoArray(AbsoluteSegments, TEXT("/"), true);

		TArray<FString> ResolvedSegments;
		ResolvedSegments.Reserve(AbsoluteSegments.Num());
		int32 LockedSegments = 0;
		for (const FString& AbsoluteSegment : AbsoluteSegments)
		{
			if (AbsoluteSegment == TEXT("."))
			{
				continue;
			}
			if (AbsoluteSegment == TEXT(".."))
			{
				if (ResolvedSegments.Num() > LockedSegments && ResolvedSegments.Last() != TEXT(".."))
				{
					ResolvedSegments.RemoveAt(ResolvedSegments.Num() - 1);
				}
				else if (RootPrefix.IsEmpty() && LockedSegments == 0)
				{
					// Some platforms keep the project directory relative to the working directory
					// ("../../../MyGame/"). The allowed directories are normalised the same way, so a
					// leading ".." stays and still has to match theirs.
					ResolvedSegments.Add(AbsoluteSegment);
				}
				else
				{
					// ".." above the root of an absolute path.
					return false;
				}
				continue;
			}
			if (IsAmbiguousDotSegment(AbsoluteSegment))
			{
				return false;
			}
			if (ResolvedSegments.Num() == 0 && RootPrefix.IsEmpty() && AbsoluteSegment.EndsWith(TEXT(":")))
			{
				// A drive ("C:") that ".." must not remove.
				LockedSegments = 1;
			}
			ResolvedSegments.Add(AbsoluteSegment);
		}

		OutPath = RootPrefix + FString::Join(ResolvedSegments, TEXT("/"));
		return true;
	}

	/** True for a normalised path that names a whole file system: "", "/", "//" or a drive such as "C:". */
	static bool IsFileSystemRoot(const FString& NormalizedPath)
	{
		if (NormalizedPath.IsEmpty() || NormalizedPath == TEXT("/") || NormalizedPath == TEXT("//"))
		{
			return true;
		}
		int32 SlashIndex = INDEX_NONE;
		return !NormalizedPath.FindChar(TEXT('/'), SlashIndex) && NormalizedPath.EndsWith(TEXT(":"));
	}

	/** Adds Directory, normalised, unless it is empty, cannot be normalised or is a file system root. */
	static void AddAccessRoot(TArray<FString>& AccessRoots, const FString& Directory)
	{
		FString NormalizedDirectory;
		if (NormalizeForAccessCheck(Directory, NormalizedDirectory) && !IsFileSystemRoot(NormalizedDirectory))
		{
			AccessRoots.AddUnique(NormalizedDirectory);
		}
	}

	/** The normalised directories a path must be under: the writable ones, plus the readable ones unless bForWrite. */
	static TArray<FString> GatherAccessRoots(const UEFFileAccessSettings& AccessSettings, bool bForWrite)
	{
		TArray<FString> AccessRoots;
		AddAccessRoot(AccessRoots, FPaths::ProjectSavedDir());
		for (const FString& AllowedDirectory : AccessSettings.AdditionalAllowedDirectories)
		{
			const FString TrimmedDirectory = AllowedDirectory.TrimStartAndEnd();
			if (TrimmedDirectory.IsEmpty())
			{
				continue;
			}
			AddAccessRoot(AccessRoots, FPaths::IsRelative(TrimmedDirectory) ? FPaths::Combine(FPaths::ProjectDir(), TrimmedDirectory) : TrimmedDirectory);
		}

		if (!bForWrite)
		{
			AddAccessRoot(AccessRoots, FPaths::ProjectDir());
			AddAccessRoot(AccessRoots, FPaths::ProjectUserDir());
			AddAccessRoot(AccessRoots, FString(FPlatformProcess::UserDir()));
			AddAccessRoot(AccessRoots, FString(FPlatformProcess::UserSettingsDir()));
		}
		return AccessRoots;
	}

	/** True when NormalizedPath is NormalizedRoot or inside it. Both come from NormalizeForAccessCheck. */
	static bool IsUnderAccessRoot(const FString& NormalizedPath, const FString& NormalizedRoot)
	{
#if PLATFORM_WINDOWS
		const ESearchCase::Type RootSearchCase = ESearchCase::IgnoreCase;
#else
		const ESearchCase::Type RootSearchCase = ESearchCase::CaseSensitive;
#endif
		if (NormalizedRoot.IsEmpty() || !NormalizedPath.StartsWith(NormalizedRoot, RootSearchCase))
		{
			return false;
		}
		// "Saved" must not match "SavedGames".
		return NormalizedPath.Len() == NormalizedRoot.Len() || NormalizedPath[NormalizedRoot.Len()] == TEXT('/');
	}

	/** GConfig's known config files (ENUMERATE_KNOWN_INI_FILES), which it writes under the project's Saved/Config. */
	static bool IsKnownConfigName(const FString& Filename)
	{
		static const TCHAR* const KnownConfigNames[] =
		{
			TEXT("Engine"), TEXT("Game"), TEXT("Input"), TEXT("DeviceProfiles"), TEXT("GameUserSettings"),
			TEXT("Scalability"), TEXT("RuntimeOptions"), TEXT("InstallBundle"), TEXT("Hardware"), TEXT("GameplayTags")
		};
		for (const TCHAR* KnownConfigName : KnownConfigNames)
		{
			if (Filename.Equals(FString(KnownConfigName), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Path as it may appear in a log line: control characters replaced, very long paths shortened. */
	static FString MakeLoggablePath(const FString& Path)
	{
		FString LoggablePath = Path.Len() > 512 ? Path.Left(512) + TEXT("...") : Path;
		for (int32 CharIndex = 0; CharIndex < LoggablePath.Len(); ++CharIndex)
		{
			if (LoggablePath[CharIndex] < TEXT(' '))
			{
				LoggablePath[CharIndex] = TEXT('?');
			}
		}
		return LoggablePath;
	}

	/** IsAllowed, logging one warning (the function, the path and the reason, never file contents) when refused. */
	static bool CheckAccess(const TCHAR* FunctionName, const FString& Path, bool bForWrite)
	{
		FString RefusalReason;
		if (IsAllowed(Path, bForWrite, &RefusalReason))
		{
			return true;
		}
		UE_LOG(LogExtendedFramework, Warning, TEXT("%s refused to %s \"%s\": %s. See Project Settings > Extended Framework > Extended File Access."),
			FunctionName, bForWrite ? TEXT("write") : TEXT("read"), *MakeLoggablePath(Path), *RefusalReason);
		return false;
	}

	bool IsAllowed(const FString& Path, bool bForWrite, FString* OutReason)
	{
		const UEFFileAccessSettings* AccessSettings = GetDefault<UEFFileAccessSettings>();
		if (AccessSettings == nullptr || !AccessSettings->IsRestrictionActive())
		{
			return true;
		}

		FString NormalizedPath;
		if (!NormalizeForAccessCheck(Path, NormalizedPath))
		{
			if (OutReason)
			{
				if (Path.TrimStartAndEnd().IsEmpty())
				{
					*OutReason = TEXT("the path is empty");
				}
				else
				{
					*OutReason = TEXT("the path could not be normalised (\"..\" above the root, or a segment of only dots and spaces)");
				}
			}
			return false;
		}

		for (const FString& AccessRoot : GatherAccessRoots(*AccessSettings, bForWrite))
		{
			if (IsUnderAccessRoot(NormalizedPath, AccessRoot))
			{
				return true;
			}
		}

		if (OutReason)
		{
			if (bForWrite)
			{
				*OutReason = TEXT("writing is allowed only under the project Saved directory and the additional allowed directories");
			}
			else
			{
				*OutReason = TEXT("reading is allowed only under the project, Saved, user and user settings directories and the additional allowed directories");
			}
		}
		return false;
	}

	bool CheckRead(const TCHAR* FunctionName, const FString& Path)
	{
		return CheckAccess(FunctionName, Path, false);
	}

	bool CheckWrite(const TCHAR* FunctionName, const FString& Path)
	{
		return CheckAccess(FunctionName, Path, true);
	}

	bool CheckConfigFile(const TCHAR* FunctionName, const FString& Filename, bool bForWrite)
	{
		return IsKnownConfigName(Filename) || CheckAccess(FunctionName, Filename, bForWrite);
	}
}

bool UEFFileLibrary::IsPathAllowed(const FString& Path, bool bForWrite)
{
	return EFFileLibraryAccess::IsAllowed(Path, bForWrite);
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLSettings.h"

#include "Private/Paths/ESQLPathResolver.h"
#include "Misc/Paths.h"

namespace
{
FString ResolveConfiguredDatabaseDirectory(const FString& InDirectory)
{
	FString ConfiguredDirectory = InDirectory;
	ConfiguredDirectory.TrimStartAndEndInline();
	ConfiguredDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (ConfiguredDirectory.IsEmpty())
	{
		ConfiguredDirectory = TEXT("Databases");
	}

	const bool bLooksAbsolute = !FPaths::IsRelative(ConfiguredDirectory);
	const bool bLooksProjectRelative =
		ConfiguredDirectory.StartsWith(TEXT("Saved/"), ESearchCase::IgnoreCase)
		|| ConfiguredDirectory.StartsWith(TEXT("./"))
		|| ConfiguredDirectory.StartsWith(TEXT("../"));

	FString ResolvedDirectory;
	if (bLooksAbsolute)
	{
		ResolvedDirectory = ConfiguredDirectory;
	}
	else if (bLooksProjectRelative)
	{
		ResolvedDirectory = FPaths::Combine(FPaths::ProjectDir(), ConfiguredDirectory);
	}
	else
	{
		ResolvedDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), ConfiguredDirectory);
	}

	ResolvedDirectory = FPaths::ConvertRelativePathToFull(ResolvedDirectory);
	FPaths::NormalizeDirectoryName(ResolvedDirectory);
	return ResolvedDirectory;
}
}

UESQLSettings::UESQLSettings()
{
}

FString UESQLSettings::ResolveDatabaseDirectoryPath()
{
	const UESQLSettings* Settings = Get();
	return ResolveConfiguredDatabaseDirectory(
		Settings ? Settings->DefaultDatabaseDirectory : TEXT("Databases"));
}

FString UESQLSettings::ResolveDatabaseFilePath(const FString& DatabaseName)
{
	FString FileName = DatabaseName;
	if (!FileName.EndsWith(TEXT(".db"), ESearchCase::IgnoreCase))
	{
		FileName += TEXT(".db");
	}

	FString FullPath = FPaths::Combine(ResolveDatabaseDirectoryPath(), FileName);
	FullPath = FPaths::ConvertRelativePathToFull(FullPath);
	FPaths::NormalizeFilename(FullPath);
	return FullPath;
}

FString UESQLSettings::ResolveExistingDatabaseFilePath(const FString& DatabaseName, bool bAllowReadonlyContentFallback)
{
	return FESQLPathResolver::ResolveExistingDatabasePath(DatabaseName, bAllowReadonlyContentFallback);
}

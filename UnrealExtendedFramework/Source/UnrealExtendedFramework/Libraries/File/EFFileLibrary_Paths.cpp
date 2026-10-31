// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Project and engine directories, path parts and file name validation.

#include "EFFileLibrary.h"

#include "HAL/PlatformProperties.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString UEFFileLibrary::GetProjectDirectory(EFProjectDirectory DirectoryType)
{
	switch (DirectoryType)
	{
	case EFProjectDirectory::ProjectDir: return FPaths::ProjectDir();
	case EFProjectDirectory::ProjectConfigDir: return FPaths::ProjectConfigDir();
	case EFProjectDirectory::ProjectContentDir: return FPaths::ProjectContentDir();
	case EFProjectDirectory::ProjectIntermediateDir: return FPaths::ProjectIntermediateDir();
	case EFProjectDirectory::ProjectSavedDir: return FPaths::ProjectSavedDir();
	case EFProjectDirectory::ProjectPluginsDir: return FPaths::ProjectPluginsDir();
	case EFProjectDirectory::ProjectLogDir: return FPaths::ProjectLogDir();
	case EFProjectDirectory::ProjectModsDir: return FPaths::ProjectModsDir();
	default: return "";
	}
}

FEnginePath UEFFileLibrary::GetEngineDirectories()
{
	const FString PlatformName = FPlatformProperties::IniPlatformName();
	FEnginePath P;
	P.Directory = FPaths::EngineDir();
	P.Config = FPaths::EngineConfigDir();
	P.Content = FPaths::EngineContentDir();
	P.Intermediate = FPaths::EngineIntermediateDir();
	P.Plugins = FPaths::EnginePluginsDir();
	P.Saved = FPaths::EngineSavedDir();
	P.User = FPaths::EngineUserDir();
	P.DefaultLayout = FPaths::EngineDefaultLayoutDir();
	P.PlatformExtensions = FPaths::EnginePlatformExtensionDir(*PlatformName);
	P.UserLayout = FPaths::EngineUserLayoutDir();
	return P;
}

FProjectPath UEFFileLibrary::GetProjectDirectories()
{
	const FString PlatformName = FPlatformProperties::IniPlatformName();
	FProjectPath P;
	P.Directory = FPaths::ProjectDir();
	P.Config = FPaths::ProjectConfigDir();
	P.Content = FPaths::ProjectContentDir();
	P.Intermediate = FPaths::ProjectIntermediateDir();
	P.Log = FPaths::ProjectLogDir();
	P.Mods = FPaths::ProjectModsDir();
	P.Plugins = FPaths::ProjectPluginsDir();
	P.Saved = FPaths::ProjectSavedDir();
	P.User = FPaths::ProjectUserDir();
	P.PersistentDownload = FPaths::ProjectPersistentDownloadDir();
	P.PlatformExtensions = FPaths::ProjectPlatformExtensionDir(*PlatformName);
	return P;
}

bool UEFFileLibrary::IsValidFilename(FString Filename)
{
	FText Error;
	return FFileHelper::IsFilenameValidForSaving(*Filename, Error);
}

bool UEFFileLibrary::ValidateFilename(FString Filename, FString& ValidName)
{
	ValidName = FPaths::MakeValidFileName(Filename);
	return IsValidFilename(ValidName);
}

void UEFFileLibrary::GetPathParts(FString Path, FString& PathPart, FString& BasePart, FString& ExtensionPart,
                                  FString& FileName)
{
	PathPart = FPaths::GetPath(Path);
	BasePart = FPaths::GetBaseFilename(Path);
	ExtensionPart = FPaths::GetExtension(Path);
	FileName = FPaths::GetCleanFilename(Path);
}

FString UEFFileLibrary::GetFileExtension(FString Path)
{
	return FPaths::GetExtension(Path);
}

// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: File and directory queries and operations.

#include "EFFileLibrary.h"

#include "EFFileLibraryPrivate.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool UEFFileLibrary::IsFile(FString Path)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	return File.FileExists(*Path);
}

bool UEFFileLibrary::IsDirectory(FString Path)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	return File.DirectoryExists(*Path);
}

bool UEFFileLibrary::SetReadOnlyFlag(FString FilePath, bool Flag)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	return File.SetReadOnly(*FilePath, Flag);
}

bool UEFFileLibrary::GetReadOnlyFlag(FString FilePath)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	return File.IsReadOnly(*FilePath);
}

int64 UEFFileLibrary::GetFileSize(FString FilePath)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	return File.FileSize(*FilePath);
}

bool UEFFileLibrary::ListDirectory(FString Path, FString Pattern, TArray<FString>& Nodes, bool ShowFile,
                                   bool ShowDirectory, bool Recursive)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.DirectoryExists(*Path))
	{
		return false;
	}
	if (!ShowDirectory && !ShowFile)
	{
		return true;
	}
	FString BasePath = FPaths::Combine(Path, TEXT("/"));
	FEFFileLibraryDirectoryVisitor CustomFileVisitor(BasePath, Nodes, Pattern, ShowFile, ShowDirectory);
	if (Recursive)
	{
		return File.IterateDirectoryRecursively(*Path, CustomFileVisitor);
	}
	else
	{
		return File.IterateDirectory(*Path, CustomFileVisitor);
	}
}

bool UEFFileLibrary::MakeDirectory(FString Path, bool Recursive)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (File.DirectoryExists(*Path))
	{
		return true;
	}
	if (Recursive)
	{
		return File.CreateDirectoryTree(*Path);
	}
	else
	{
		return File.CreateDirectory(*Path);
	}
}

bool UEFFileLibrary::RemoveDirectory(FString Path, bool Recursive)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.DirectoryExists(*Path))
	{
		return true;
	}
	if (Recursive)
	{
		return File.DeleteDirectoryRecursively(*Path);
	}
	else
	{
		return File.DeleteDirectory(*Path);
	}
}

bool UEFFileLibrary::CopyDirectory(FString Source, FString Dest) // bool Force = false
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	FPaths::NormalizeDirectoryName(Dest);
	if (!File.DirectoryExists(*Source) && !File.FileExists(*Source))
	{
		return false;
	}
	if (!File.DirectoryExists(*Dest))
	{
		return false;
	}
	return File.CopyDirectoryTree(*Dest, *Source, true);
}

bool UEFFileLibrary::MoveDirectory(FString Source, FString Dest) // bool Force = false
{
	FPaths::NormalizeDirectoryName(Source);
	FPaths::NormalizeDirectoryName(Dest);
	if (Dest.Equals(Source))
	{
		return true;
	}
	if (!UEFFileLibrary::CopyDirectory(Source, Dest))
	{
		return false;
	}
	return UEFFileLibrary::RemoveDirectory(Source, true);
}

bool UEFFileLibrary::NodeStats(FString Path, FCustomNodeStat& Stats)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.DirectoryExists(*Path) && !File.FileExists(*Path))
	{
		return false;
	}
	auto Data = File.GetStatData(*Path);
	if (!Data.bIsValid)
	{
		return false;
	}
	Stats.CreationTime = Data.CreationTime;
	Stats.FileSize = Data.FileSize;
	Stats.IsDirectory = Data.bIsDirectory;
	Stats.IsReadOnly = Data.bIsReadOnly;
	Stats.LastAccessTime = Data.AccessTime;
	Stats.ModificationTime = Data.ModificationTime;
	return true;
}

bool UEFFileLibrary::RemoveFile(FString Path)
{
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (File.DirectoryExists(*Path))
	{
		return false;
	}
	if (!File.FileExists(*Path))
	{
		return true;
	}
	return File.DeleteFile(*Path);
}

bool UEFFileLibrary::CopyFile(FString Source, FString Dest, bool Force)
{
	FPaths::NormalizeFilename(Source);
	FPaths::NormalizeFilename(Dest);
	if (Dest.Equals(Source))
	{
		return true;
	}
	FText Error;
	if (!FFileHelper::IsFilenameValidForSaving(*Dest, Error))
	{
		return false;
	}
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.FileExists(*Source))
	{
		return false;
	}
	if (!Force && File.FileExists(*Dest))
	{
		return false;
	}
	UEFFileLibrary::RemoveFile(Dest);
	return File.CopyFile(*Dest, *Source);
}

bool UEFFileLibrary::MoveFile(FString Source, FString Dest, bool Force)
{
	FPaths::NormalizeFilename(Source);
	FPaths::NormalizeFilename(Dest);
	if (Dest.Equals(Source))
	{
		return true;
	}
	FText Error;
	if (!FFileHelper::IsFilenameValidForSaving(*Dest, Error))
	{
		return false;
	}
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.FileExists(*Source))
	{
		return false;
	}
	if (!Force && File.FileExists(*Dest))
	{
		return false;
	}
	UEFFileLibrary::RemoveFile(Dest);
	return File.MoveFile(*Dest, *Source);
}

bool UEFFileLibrary::RenameFile(FString Path, FString NewName)
{
	FText Error;
	if (!FFileHelper::IsFilenameValidForSaving(*NewName, Error))
	{
		return false;
	}
	IPlatformFile& File = FPlatformFileManager::Get().GetPlatformFile();
	if (!File.FileExists(*Path))
	{
		return false;
	}
	FString Clean = FPaths::GetCleanFilename(NewName);
	FString Base = FPaths::GetPath(Path);
	FString Output = FPaths::Combine(*Base, *Clean);
	if (File.FileExists(*Output) || File.DirectoryExists(*Output))
	{
		return false;
	}
	return File.MoveFile(*Output, *Path);
}

FDateTime UEFFileLibrary::GetFileModificationTime(FString FilePath, bool& Success)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*FilePath))
	{
		Success = true;
		return PlatformFile.GetTimeStamp(*FilePath);
	}
	Success = false;
	return FDateTime::MinValue();
}

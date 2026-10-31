// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogSessionFolder.h"

#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace EFLogSessionFolderPrivate
{
	static const TCHAR* LockFileName = TEXT(".owner.lock");
	static const TCHAR* ArchiveFolderName = TEXT("Archive");
	static const TCHAR* LogExtension = TEXT(".log");
	static const TCHAR* StampFormat = TEXT("%Y.%m.%d-%H.%M.%S");

	/**
	 * Opens a lock file with no sharing at all. On Windows a second process's open fails until
	 * this handle closes, which the OS also does when the holder dies, so a crash never leaves a
	 * folder locked for good.
	 */
	static FArchive* TryTakeLock(const FString& LockPath)
	{
		return IFileManager::Get().CreateFileWriter(*LockPath, FILEWRITE_Silent);
	}

	/** Names the folder a non-owner process writes into. */
	static FString GetProcessLabel()
	{
		if (IsRunningCommandlet())
		{
			return TEXT("Commandlet");
		}
		if (GIsEditor)
		{
			return TEXT("Editor");
		}
		if (IsRunningDedicatedServer())
		{
			return TEXT("Server");
		}
		if (IsRunningClientOnly())
		{
			return TEXT("Client");
		}
		return TEXT("Game");
	}

	/** Reads Session= and Process= from a log file's header line. False when the file has no header of ours. */
	static bool ReadHeader(const FString& FilePath, FString& OutSession, FString& OutProcess)
	{
		const TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath, FILEREAD_Silent | FILEREAD_AllowWrite));
		if (!Reader)
		{
			return false;
		}

		const int64 BytesToRead = FMath::Min<int64>(Reader->TotalSize(), 1024);
		if (BytesToRead <= 0)
		{
			return false;
		}

		TArray<uint8> Bytes;
		Bytes.SetNumUninitialized(static_cast<int32>(BytesToRead));
		Reader->Serialize(Bytes.GetData(), BytesToRead);

		FString Text;
		FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());

		int32 LineEnd = INDEX_NONE;
		if (Text.FindChar(TEXT('\n'), LineEnd))
		{
			Text.LeftInline(LineEnd);
		}
		Text.TrimEndInline();

		if (!Text.StartsWith(FEFLogSessionFolder::GetHeaderPrefix()))
		{
			return false;
		}

		TArray<FString> Fields;
		Text.ParseIntoArray(Fields, TEXT(" | "));
		for (const FString& Field : Fields)
		{
			if (Field.StartsWith(TEXT("Session=")))
			{
				OutSession = Field.RightChop(8).TrimStartAndEnd();
			}
			else if (Field.StartsWith(TEXT("Process=")))
			{
				// "Process=UnrealEditor pid 12345": the executable name is enough for a folder name.
				OutProcess = Field.RightChop(8).TrimStartAndEnd();
				int32 Space = INDEX_NONE;
				if (OutProcess.FindChar(TEXT(' '), Space))
				{
					OutProcess.LeftInline(Space);
				}
			}
		}

		return !OutSession.IsEmpty();
	}
}

const TCHAR* FEFLogSessionFolder::GetHeaderPrefix()
{
	return TEXT("# EFLog v1");
}

bool FEFLogSessionFolder::Initialize(const FString& FolderName, int32 ArchivesToKeep)
{
	using namespace EFLogSessionFolderPrivate;

	IFileManager& FileManager = IFileManager::Get();

	SessionStamp = FDateTime::Now().ToString(StampFormat);
	ProcessName = FPlatformProcess::ExecutableName(true);
	RootDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / FolderName);
	FPaths::NormalizeDirectoryName(RootDirectory);
	ArchiveDirectory = RootDirectory / ArchiveFolderName;

	FileManager.MakeDirectory(*RootDirectory, true);
	if (!FileManager.DirectoryExists(*RootDirectory))
	{
		return false;
	}

	LockFilePath = RootDirectory / LockFileName;
	LockFile.Reset(TryTakeLock(LockFilePath));
	bOwnsRoot = LockFile.IsValid();

	if (bOwnsRoot)
	{
		WriteDirectory = RootDirectory;

		// Earlier sessions move out before this one writes anything, so the root only ever holds
		// the running session's files.
		ArchiveStaleRootFiles();
		ArchiveStaleProcessFolders();
		PruneArchives(FMath::Max(1, ArchivesToKeep));
	}
	else
	{
		WriteFolderName = FString::Printf(TEXT("%s_%u"), *GetProcessLabel(), FPlatformProcess::GetCurrentProcessId());
		WriteDirectory = RootDirectory / WriteFolderName;
		FileManager.MakeDirectory(*WriteDirectory, true);
		if (!FileManager.DirectoryExists(*WriteDirectory))
		{
			return false;
		}

		// Holding a lock in our own folder is what tells a later owner this folder is still live.
		LockFilePath = WriteDirectory / LockFileName;
		LockFile.Reset(TryTakeLock(LockFilePath));
	}

	if (LockFile)
	{
		// Purely informational: which process holds the lock.
		const FString Owner = FString::Printf(TEXT("%s pid %u%s"), *ProcessName, FPlatformProcess::GetCurrentProcessId(), LINE_TERMINATOR);
		const FTCHARToUTF8 Utf8(*Owner);
		LockFile->Serialize((void*)Utf8.Get(), Utf8.Length());
		LockFile->Flush();
	}

	return true;
}

void FEFLogSessionFolder::Release()
{
	if (LockFile)
	{
		LockFile->Close();
		LockFile.Reset();
		IFileManager::Get().Delete(*LockFilePath, false, false, true);
	}
}

void FEFLogSessionFolder::ArchiveStaleRootFiles()
{
	using namespace EFLogSessionFolderPrivate;

	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *RootDirectory, LogExtension);

	for (const FString& FileName : FileNames)
	{
		ArchiveFile(RootDirectory / FileName, FString());
	}
}

void FEFLogSessionFolder::ArchiveStaleProcessFolders()
{
	using namespace EFLogSessionFolderPrivate;

	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> FolderNames;
	FileManager.FindFiles(FolderNames, *(RootDirectory / TEXT("*")), false, true);

	for (const FString& FolderName : FolderNames)
	{
		if (FolderName.Equals(ArchiveFolderName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString FolderPath = RootDirectory / FolderName;
		const FString FolderLockPath = FolderPath / LockFileName;

		// A folder whose lock we cannot take belongs to a process that is still running.
		TUniquePtr<FArchive> FolderLock(TryTakeLock(FolderLockPath));
		if (!FolderLock)
		{
			continue;
		}

		TArray<FString> FileNames;
		FileManager.FindFiles(FileNames, *FolderPath, LogExtension);

		bool bMovedEverything = true;
		for (const FString& FileName : FileNames)
		{
			bMovedEverything &= ArchiveFile(FolderPath / FileName, FolderName);
		}

		FolderLock->Close();
		FolderLock.Reset();
		FileManager.Delete(*FolderLockPath, false, false, true);

		// Non-recursive on purpose: anything we did not put there stops the delete.
		if (bMovedEverything)
		{
			FileManager.DeleteDirectory(*FolderPath, false, false);
		}
	}
}

FString FEFLogSessionFolder::MakeRollDestination(const FString& CategoryDisplayName) const
{
	using namespace EFLogSessionFolderPrivate;

	IFileManager& FileManager = IFileManager::Get();

	// Same folder name the next launch will archive this session's live files into.
	const FString Suffix = bOwnsRoot ? ProcessName : WriteFolderName;
	const FString DestinationFolder = ArchiveDirectory / FString::Printf(TEXT("%s_%s"), *SessionStamp, *Suffix);
	FileManager.MakeDirectory(*DestinationFolder, true);

	FString Destination;
	for (int32 Part = 1; ; ++Part)
	{
		Destination = DestinationFolder / FString::Printf(TEXT("%s.%d%s"), *CategoryDisplayName, Part, LogExtension);
		if (!FileManager.FileExists(*Destination))
		{
			return Destination;
		}
	}
}

void FEFLogSessionFolder::PruneArchives(int32 ArchivesToKeep)
{
	using namespace EFLogSessionFolderPrivate;

	IFileManager& FileManager = IFileManager::Get();

	TArray<FString> ArchiveNames;
	FileManager.FindFiles(ArchiveNames, *(ArchiveDirectory / TEXT("*")), false, true);

	// Names start with the session stamp (yyyy.MM.dd-HH.mm.ss), so they sort oldest first.
	ArchiveNames.Sort();

	const int32 Excess = ArchiveNames.Num() - ArchivesToKeep;
	for (int32 Index = 0; Index < Excess; ++Index)
	{
		FileManager.DeleteDirectory(*(ArchiveDirectory / ArchiveNames[Index]), false, true);
	}
}

bool FEFLogSessionFolder::ArchiveFile(const FString& FilePath, const FString& SourceFolderName)
{
	using namespace EFLogSessionFolderPrivate;

	IFileManager& FileManager = IFileManager::Get();

	FString Session;
	FString Process;
	if (!ReadHeader(FilePath, Session, Process))
	{
		// No header of ours: fall back to the file's own timestamp, in local time like real stamps.
		const FDateTime ModifiedUtc = FileManager.GetTimeStamp(*FilePath);
		const FDateTime ModifiedLocal = ModifiedUtc + (FDateTime::Now() - FDateTime::UtcNow());
		Session = ModifiedLocal.ToString(StampFormat);
		Process = TEXT("Unknown");
	}

	// Files from a process folder keep that folder's name (Game_20412), which already carries the pid.
	const FString Suffix = SourceFolderName.IsEmpty() ? Process : SourceFolderName;
	const FString DestinationFolder = ArchiveDirectory / FString::Printf(TEXT("%s_%s"), *Session, *Suffix);
	FileManager.MakeDirectory(*DestinationFolder, true);

	const FString BaseName = FPaths::GetBaseFilename(FilePath);
	FString Destination = DestinationFolder / (BaseName + LogExtension);
	for (int32 Attempt = 1; FileManager.FileExists(*Destination); ++Attempt)
	{
		Destination = DestinationFolder / FString::Printf(TEXT("%s_%d%s"), *BaseName, Attempt, LogExtension);
	}

	return FileManager.Move(*Destination, *FilePath, false, false, false, true);
}

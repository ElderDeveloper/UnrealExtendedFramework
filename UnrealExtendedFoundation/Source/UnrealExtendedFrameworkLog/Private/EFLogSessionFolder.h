// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Decides where this process writes its category files, and tidies up after earlier sessions.
 *
 *     Saved/Logs/Extended/                    root: the owning process writes here
 *       .owner.lock                           held open by the owner for its whole life
 *       Araf.log ...
 *       Game_20412/                           any other process on the same project
 *         .owner.lock                         held open by that process
 *         Araf.log ...
 *       Archive/<Session>_<Process>/          earlier sessions, newest MaxArchives kept
 *
 * Ownership is an exclusive open of the lock file: a second process's open fails with a sharing
 * violation until the owner exits (or dies, since the OS closes its handle). That exclusivity is a
 * Windows guarantee; on platforms without mandatory file locks every process believes it owns the
 * root, which is harmless only as long as one process writes there at a time.
 */
class FEFLogSessionFolder
{
public:
	/**
	 * Resolves Saved/Logs/<FolderName>/, takes the root if nobody holds it, and archives stale files
	 * (keeping the newest ArchivesToKeep sessions) when it does. False when no folder could be created.
	 */
	bool Initialize(const FString& FolderName, int32 ArchivesToKeep);

	/** Closes and deletes this process's lock file. */
	void Release();

	const FString& GetRootDirectory() const { return RootDirectory; }
	const FString& GetWriteDirectory() const { return WriteDirectory; }
	const FString& GetSessionStamp() const { return SessionStamp; }
	const FString& GetProcessName() const { return ProcessName; }
	bool OwnsRoot() const { return bOwnsRoot; }

	/**
	 * Where a full file goes when it reaches the size cap: this session's archive folder, as
	 * <Category>.<N>.log with the first unused N. The next launch archives the live file into the
	 * same folder, so a session's parts end up together.
	 */
	FString MakeRollDestination(const FString& CategoryDisplayName) const;

	/** Every file this system writes starts with this, followed by " | Key=Value" fields. */
	static const TCHAR* GetHeaderPrefix();

private:
	void ArchiveStaleRootFiles();
	void ArchiveStaleProcessFolders();
	void PruneArchives(int32 ArchivesToKeep);

	/** Moves one log file into Archive/<Session>_<Suffix>/. False if the file could not be moved (still open elsewhere). */
	bool ArchiveFile(const FString& FilePath, const FString& SourceFolderName);

	FString RootDirectory;
	FString ArchiveDirectory;
	FString WriteDirectory;
	FString WriteFolderName;
	FString SessionStamp;
	FString ProcessName;
	FString LockFilePath;
	TUniquePtr<FArchive> LockFile;
	bool bOwnsRoot = false;
};

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * IOnlineUserCloud backed by Steam Remote Storage (ISteamRemoteStorage).
 *
 * Local user only: Steam Cloud is the signed-in user's storage, so every call validates the
 * given id against the local CSteamID; other users fail through the matching failure delegate.
 *
 * Synchronous by design: the Steam client syncs cloud files to the local disk before the app
 * launches, so FileRead/FileWrite/FileDelete are local-disk operations. All work completes
 * inside the call and the completion delegates fire before the call returns (matching how the
 * engine treats platforms with locally-mirrored cloud storage). Consequently:
 *  - CancelWriteUserFile can never cancel anything and always reports failure.
 *  - "No async tasks outstanding" preconditions on the cache-clearing methods always hold.
 *
 * bCompressBeforeUpload is accepted and ignored: Steam owns the transport (and compresses on
 * the wire itself); the OSS contract explicitly allows platforms not to implement it.
 *
 * Steam filenames are case-insensitive (the SDK lowercases them), so the file cache is keyed
 * by the lowercased filename while delegates echo back the caller's original spelling.
 *
 * Game-thread only, like the rest of the subsystem.
 */
class FOnlineUserCloudExtendedSteam : public IOnlineUserCloud
{
public:
	explicit FOnlineUserCloudExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	virtual ~FOnlineUserCloudExtendedSteam() = default;

	//~ Begin IOnlineUserCloud
	virtual bool GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents) override;
	virtual bool ClearFiles(const FUniqueNetId& UserId) override;
	virtual bool ClearFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual void EnumerateUserFiles(const FUniqueNetId& UserId) override;
	virtual void GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles) override;
	virtual bool ReadUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents, bool bCompressBeforeUpload = false) override;
	virtual void CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName) override;
	virtual bool DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete) override;
	virtual bool RequestUsageInfo(const FUniqueNetId& UserId) override;
	virtual void DumpCloudState(const FUniqueNetId& UserId) override;
	virtual void DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName) override;
	//~ End IOnlineUserCloud

private:
	/** True when UserId is the local Steam user (byte-compares the SteamID64 against SteamUser()). */
	bool IsLocalSteamUser(const FUniqueNetId& UserId) const;

	/** Cache key for a filename: Steam filenames are case-insensitive, so key on the lowercase form. */
	static FString MakeCacheKey(const FString& FileName)
	{
		return FileName.ToLower();
	}

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** True once EnumerateUserFiles has completed successfully at least once. */
	bool bEnumeratedFiles = false;

	/** File metadata from the last successful EnumerateUserFiles (Hash empty, DLName == FileName). */
	TArray<FCloudFileHeader> EnumeratedFiles;

	/** Downloaded/written file payloads, keyed by lowercased filename (see MakeCacheKey). */
	TMap<FString, FCloudFile> FileCache;
};

typedef TSharedPtr<FOnlineUserCloudExtendedSteam, ESPMode::ThreadSafe> FOnlineUserCloudExtendedSteamPtr;

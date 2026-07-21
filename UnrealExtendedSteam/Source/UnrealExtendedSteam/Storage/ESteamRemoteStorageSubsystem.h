// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamRemoteStorageSubsystem.generated.h"

/** Fired when an async cloud file write completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamFileWriteComplete, bool, bSuccess, const FString&, FileName);

/** Fired when an async cloud file read completes; Data is empty on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamFileReadComplete, bool, bSuccess, const FString&, FileName, const TArray<uint8>&, Data);

/** Fired when a FileShare request completes; UGCHandle is 0 on failure. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamFileShared, bool, bSuccess, const FString&, FileName, int64, UGCHandle);

/**
 * Fired when a UGCDownload / UGCDownloadToLocation request completes. On success the file is
 * cached locally (read it with UGCRead, or it was written to the location passed to
 * UGCDownloadToLocation). UGCHandle echoes the requested handle; FileName/FileSize are empty/0 on failure.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamUGCDownloaded, bool, bSuccess, int64, UGCHandle, const FString&, FileName, int32, FileSize);

/**
 * Wraps ISteamRemoteStorage: Steam Cloud file read/write, streaming writes, quota and cloud
 * toggles, sync-platform flags, local-change tracking, shared-UGC download/read, and the async
 * write/read/share/UGC-download call results.
 *
 * Note: Steam Cloud filenames are case-insensitive and converted to lowercase by Steam.
 * Same-type async requests (write, read, share, UGC download) are serialized via an internal FIFO
 * queue; they complete in order, none are dropped. Starting a second one while the first is pending
 * enqueues it and it is issued automatically when the previous request of that type completes.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamRemoteStorageSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// --- Synchronous file operations ---

	/** Writes the bytes to a cloud file (created or overwritten). Synchronous; may block on large files. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWrite(const FString& FileName, const TArray<uint8>& Data);

	/** Writes a string to a cloud file as UTF-8. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWriteString(const FString& FileName, const FString& Contents);

	/** Reads a whole cloud file into OutData. Returns false (and an empty array) on failure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileRead(const FString& FileName, TArray<uint8>& OutData);

	/** Reads a whole cloud file as UTF-8 text. Returns false (and an empty string) on failure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileReadString(const FString& FileName, FString& OutContents);

	/** True when the cloud file exists. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool FileExists(const FString& FileName) const;

	/** Deletes the file locally and from the cloud. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileDelete(const FString& FileName);

	/** Removes the file from cloud sync but keeps it on disk (counts against quota until deleted). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileForget(const FString& FileName);

	/** True when the file is persisted in the Steam Cloud. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool FilePersisted(const FString& FileName) const;

	/** Size of the cloud file in bytes (0 when missing or Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int32 GetFileSize(const FString& FileName) const;

	/** Unix timestamp of the file's last write (0 when missing or Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int64 GetFileTimestamp(const FString& FileName) const;

	/** Number of files in the local user's cloud storage for this app. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int32 GetFileCount() const;

	/** Name and size of the file at Index (0..GetFileCount()-1). Returns false for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool GetFileNameAndSize(int32 Index, FString& OutName, int32& OutSize) const;

	// --- Streaming writes (build a cloud file from chunks without buffering it all in memory) ---

	/**
	 * Opens a streaming write to a cloud file and returns its stream handle (-1,
	 * k_UGCFileStreamHandleInvalid, on failure). Append data with FileWriteStreamWriteChunk, then
	 * commit with FileWriteStreamClose (or abandon with FileWriteStreamCancel). The handle is a
	 * UGCFileWriteStreamHandle_t (uint64) passed through Blueprint as int64 — treat it as opaque.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	int64 FileWriteStreamOpen(const FString& FileName);

	/** Appends a chunk to an open write stream. Returns false on failure (empty data is rejected). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWriteStreamWriteChunk(int64 StreamHandle, const TArray<uint8>& Data);

	/** Commits and closes a write stream, persisting the accumulated file. Returns false on failure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWriteStreamClose(int64 StreamHandle);

	/** Cancels a write stream, discarding everything written to it. Returns false on failure. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWriteStreamCancel(int64 StreamHandle);

	// --- Sync platforms ---

	/**
	 * Restricts which platforms a cloud file syncs to. PlatformFlags is an ERemoteStoragePlatform
	 * bitmask (None=0, Windows=1<<0, OSX=1<<1, PS3=1<<2, Linux=1<<3, Switch=1<<4, Android=1<<5,
	 * iOS=1<<6, All=0xffffffff). Returns false when Steam is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool SetSyncPlatforms(const FString& FileName, int32 PlatformFlags);

	/** Reads the ERemoteStoragePlatform sync bitmask of a cloud file (see SetSyncPlatforms). 0 when unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int32 GetSyncPlatforms(const FString& FileName) const;

	// --- Local change tracking (files updated/deleted by another device during this session) ---

	/** Number of local files changed by cloud sync during this session. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int32 GetLocalFileChangeCount() const;

	/**
	 * Details of the local change at Index (0..GetLocalFileChangeCount()-1). OutChangeType is an
	 * ERemoteStorageLocalFileChange (Invalid=0, FileUpdated=1, FileDeleted=2); OutFilePathType is an
	 * ERemoteStorageFilePathType (Invalid=0, Absolute=1, APIFilename=2). Returns false for invalid indices.
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool GetLocalFileChange(int32 Index, FString& OutName, int32& OutChangeType, int32& OutFilePathType) const;

	// --- File write batching (group related writes, e.g. a save that spans several files) ---

	/** Marks the start of a batch of local file writes so Steam can sync them together. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool BeginFileWriteBatch();

	/** Marks the end of a batch opened by BeginFileWriteBatch. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool EndFileWriteBatch();

	// --- Quota / cloud configuration ---

	/** Total and remaining cloud storage for this app, in bytes. Returns false when Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool GetQuota(int64& OutTotalBytes, int64& OutAvailableBytes) const;

	/** True when the user has cloud storage enabled account-wide. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool IsCloudEnabledForAccount() const;

	/** True when the user has cloud storage enabled for this app. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool IsCloudEnabledForApp() const;

	/** Toggles cloud storage for this app (only do this with explicit user consent). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	void SetCloudEnabledForApp(bool bEnabled);

	// --- Asynchronous operations (same-type requests are serialized via a FIFO queue) ---

	/** Starts an async cloud write; the result arrives on OnFileWriteComplete. While another async write is pending this is queued and issued when that write completes. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileWriteAsync(const FString& FileName, const TArray<uint8>& Data);

	/** Starts an async read of the whole file; the result arrives on OnFileReadComplete. While another async read is pending this is queued and issued when that read completes. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileReadAsync(const FString& FileName);

	/** Shares a cloud file publicly, producing a UGC handle on OnFileShared. While another share is pending this is queued and issued when that share completes. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool FileShare(const FString& FileName);

	// --- Shared UGC download / read (consume files shared via FileShare, e.g. by another user) ---

	/**
	 * Reads cached metadata of a shared UGC file. Available after the file has been downloaded
	 * (OnUGCDownloaded) — the same data the download result carried. UGCHandle is a UGCHandle_t
	 * (uint64) passed through Blueprint as int64. Returns false when no metadata is cached.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool GetUGCDetails(int64 UGCHandle, int32& OutAppId, FString& OutFileName, int32& OutFileSize, FESteamId& OutOwner) const;

	/**
	 * Starts downloading a shared UGC file into the local cache; the result arrives on OnUGCDownloaded.
	 * Priority 0 downloads immediately; higher values wait behind lower-priority downloads. While
	 * another UGC download is pending this is queued and issued when that download completes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool UGCDownload(int64 UGCHandle, int32 Priority);

	/**
	 * Like UGCDownload but writes the file straight to Location on disk instead of the memory cache.
	 * The result arrives on OnUGCDownloaded. Shares the same FIFO queue as UGCDownload.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	bool UGCDownloadToLocation(int64 UGCHandle, const FString& Location, int32 Priority);

	/**
	 * Reads BytesToRead bytes at Offset from a downloaded UGC file into OutData. Returns the number
	 * of bytes actually read (0 on failure / not yet downloaded). Large files (>100MB) must be read
	 * in chunks; reading the final byte implicitly closes the file (call UGCDownload again to reopen).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Storage")
	int32 UGCRead(int64 UGCHandle, int32 Offset, int32 BytesToRead, TArray<uint8>& OutData);

	/**
	 * Progress of an in-flight UGC download, in bytes. Returns false (and 0/0) before the transfer
	 * starts — guard against a zero OutBytesExpected before dividing to get a percentage.
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	bool GetUGCDownloadProgress(int64 UGCHandle, int32& OutBytesDownloaded, int32& OutBytesExpected) const;

	/** Number of downloaded UGC files that have finished downloading but not yet been read via UGCRead. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int32 GetCachedUGCCount() const;

	/** UGC handle of the cached, downloaded-but-unread file at Index (0..GetCachedUGCCount()-1). 0 when invalid. */
	UFUNCTION(BlueprintPure, Category = "Steam|Storage")
	int64 GetCachedUGCHandle(int32 Index) const;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Storage")
	FOnSteamFileWriteComplete OnFileWriteComplete;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Storage")
	FOnSteamFileReadComplete OnFileReadComplete;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Storage")
	FOnSteamFileShared OnFileShared;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Storage")
	FOnSteamUGCDownloaded OnUGCDownloaded;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamRemoteStorageCallbacks;
	TSharedPtr<class FESteamRemoteStorageCallbacks> Callbacks;
};

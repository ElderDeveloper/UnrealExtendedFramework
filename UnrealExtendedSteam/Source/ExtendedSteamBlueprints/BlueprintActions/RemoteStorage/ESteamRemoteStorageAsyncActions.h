// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Storage/ESteamRemoteStorageSubsystem.h"
#include "ESteamRemoteStorageAsyncActions.generated.h"

/** Completion pin for the async cloud write node (FileName echoes the request). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncFileWritePin, const FString&, FileName);

/**
 * Writes bytes to a Steam Cloud file asynchronously and completes when the write finishes.
 *
 * The subsystem serializes same-type async writes via a FIFO queue; this node matches the
 * completion to its own request by filename, so several write nodes for different files can run at
 * once without cross-completing.
 */
UCLASS()
class USteamAsyncWriteCloudFile : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Storage", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Write Cloud File Async"))
	static USteamAsyncWriteCloudFile* WriteCloudFileAsync(UObject* WorldContext, const FString& FileName, const TArray<uint8>& Data, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The write completed and the file is persisting to the cloud. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileWritePin OnSuccess;

	/** Steam is unavailable or the write failed. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileWritePin OnFailure;

private:
	UFUNCTION()
	void HandleWriteComplete(bool bSuccess, const FString& InFileName);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamRemoteStorageSubsystem> StorageSubsystem;
	FString FileName;
	TArray<uint8> Data;
};

/** Completion pin for the async cloud read node (Data is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncFileReadPin, const FString&, FileName, const TArray<uint8>&, Data);

/**
 * Reads a whole Steam Cloud file asynchronously and completes with its bytes.
 *
 * The subsystem serializes same-type async reads via a FIFO queue; this node matches the completion
 * to its own request by filename.
 */
UCLASS()
class USteamAsyncReadCloudFile : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Storage", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Read Cloud File Async"))
	static USteamAsyncReadCloudFile* ReadCloudFileAsync(UObject* WorldContext, const FString& FileName, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The read completed; Data holds the file contents. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileReadPin OnSuccess;

	/** Steam is unavailable, the file is missing, or the read failed; Data is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileReadPin OnFailure;

private:
	UFUNCTION()
	void HandleReadComplete(bool bSuccess, const FString& InFileName, const TArray<uint8>& InData);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<uint8>& InData);

	TWeakObjectPtr<UESteamRemoteStorageSubsystem> StorageSubsystem;
	FString FileName;
};

/** Completion pin for the file-share node (UGCHandle is 0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncFileSharePin, const FString&, FileName, int64, UGCHandle);

/**
 * Shares a Steam Cloud file publicly and completes with the resulting UGC handle (which can be
 * transmitted to other users and fed to Download UGC).
 *
 * The subsystem serializes same-type share requests via a FIFO queue; this node matches the
 * completion to its own request by filename.
 */
UCLASS()
class USteamAsyncShareCloudFile : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Storage", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Share Cloud File"))
	static USteamAsyncShareCloudFile* ShareCloudFile(UObject* WorldContext, const FString& FileName, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The file was shared; UGCHandle identifies it for download by other users. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileSharePin OnSuccess;

	/** Steam is unavailable or the share failed; UGCHandle is 0. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileSharePin OnFailure;

private:
	UFUNCTION()
	void HandleShared(bool bSuccess, const FString& InFileName, int64 InUGCHandle);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 InUGCHandle);

	TWeakObjectPtr<UESteamRemoteStorageSubsystem> StorageSubsystem;
	FString FileName;
};

/** Completion pin for the UGC download node (FileName/FileSize are empty/0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamAsyncUGCDownloadPin, int64, UGCHandle, const FString&, FileName, int32, FileSize);

/**
 * Downloads a shared UGC file into the local cache and completes when it is ready to read (with the
 * subsystem's UGCRead).
 *
 * The subsystem serializes UGC downloads via a FIFO queue; this node matches the completion to its
 * own request by UGC handle.
 */
UCLASS()
class USteamAsyncDownloadUGC : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Priority 0 downloads immediately; higher values wait behind lower-priority downloads.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Storage", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Download UGC"))
	static USteamAsyncDownloadUGC* DownloadUGC(UObject* WorldContext, int64 UGCHandle, int32 Priority, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The file finished downloading; read it with the subsystem's UGCRead. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUGCDownloadPin OnSuccess;

	/** Steam is unavailable or the download failed; FileName/FileSize are empty/0. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncUGCDownloadPin OnFailure;

private:
	UFUNCTION()
	void HandleDownloaded(bool bSuccess, int64 InUGCHandle, const FString& InFileName, int32 InFileSize);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FString& InFileName, int32 InFileSize);

	TWeakObjectPtr<UESteamRemoteStorageSubsystem> StorageSubsystem;
	int64 UGCHandle = 0;
	int32 Priority = 0;
};

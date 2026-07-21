// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSharedCloudInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;
class FOnlineSharedCloudExtendedSteamCallbacks;

/**
 * IOnlineSharedCloud backed by Steam Remote Storage UGC sharing.
 *
 * Shared content handles are Steam UGCHandle_t values (uint64), wrapped in the cpp-local
 * FSharedContentHandleExtendedSteam (string form: decimal uint64).
 *
 * WriteSharedFile = synchronous FileWrite (local user's cloud) followed by an async FileShare
 * call result that yields the UGC handle. ReadSharedFile = async UGCDownload call result
 * followed by UGCRead into the shared-file cache.
 *
 * Concurrency model: at most ONE in-flight operation per type (one write/share, one download)
 * — the Steam call results do not carry enough request context to demultiplex overlapping
 * calls safely, and the OSS callers in this project serialize cloud work anyway. A second
 * request while one is in flight fails immediately (delegate fires with failure).
 *
 * Game-thread only, like the rest of the subsystem. Steam call results are delivered by the
 * shared module's callback pump, also on the game thread.
 */
class FOnlineSharedCloudExtendedSteam : public IOnlineSharedCloud
{
public:
	explicit FOnlineSharedCloudExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineSharedCloudExtendedSteam();

	//~ Begin IOnlineSharedCloud
	virtual bool GetSharedFileContents(const FSharedContentHandle& SharedHandle, TArray<uint8>& FileContents) override;
	virtual bool ClearSharedFiles() override;
	virtual bool ClearSharedFile(const FSharedContentHandle& SharedHandle) override;
	virtual bool ReadSharedFile(const FSharedContentHandle& SharedHandle) override;
	virtual bool WriteSharedFile(const FUniqueNetId& UserId, const FString& Filename, TArray<uint8>& Contents) override;
	virtual void GetDummySharedHandlesForTest(TArray<TSharedRef<FSharedContentHandle>>& OutHandles) override;
	//~ End IOnlineSharedCloud

private:
	friend class FOnlineSharedCloudExtendedSteamCallbacks;

	/** True when UserId is the local Steam user (only the local user can share from their cloud). */
	bool IsLocalSteamUser(const FUniqueNetId& UserId) const;

	/** FileShare call result landed: caches the just-written payload under the new UGC handle and fires OnWriteSharedFileComplete. */
	void OnNativeShareComplete(bool bSuccess, uint64 SharedHandleValue);

	/** UGCDownload call result landed (payload already pulled via UGCRead): caches it and fires OnReadSharedFileComplete. */
	void OnNativeDownloadComplete(bool bSuccess, uint64 SharedHandleValue, TArray<uint8>&& Data);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Native Steam call-result listeners; allocated on demand while the SDK is available. */
	TSharedPtr<FOnlineSharedCloudExtendedSteamCallbacks> Callbacks;

	/** Downloaded/shared payloads keyed by UGC handle value. */
	TMap<uint64, FCloudFile> SharedFileCache;

	/** In-flight write/share state (one at a time, see class comment). */
	bool bShareInFlight = false;
	FUniqueNetIdPtr PendingShareUserId;
	FString PendingShareFileName;
	TArray<uint8> PendingShareData;

	/** In-flight download state (one at a time). 0 = idle. */
	uint64 PendingDownloadHandle = 0;
};

typedef TSharedPtr<FOnlineSharedCloudExtendedSteam, ESPMode::ThreadSafe> FOnlineSharedCloudExtendedSteamPtr;

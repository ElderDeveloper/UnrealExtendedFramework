// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Storage/ESteamRemoteStorageSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Parameters captured to (re-)issue a queued FileWriteAsync call. */
struct FESteamPendingFileWrite
{
	FString FileName;
	TArray<uint8> Data;
};

/** Parameters captured to (re-)issue a queued FileReadAsync call. */
struct FESteamPendingFileRead
{
	FString FileName;
};

/** Parameters captured to (re-)issue a queued FileShare call. */
struct FESteamPendingFileShare
{
	FString FileName;
};

/**
 * Parameters captured to (re-)issue a queued UGCDownload / UGCDownloadToLocation call. Both SDK
 * calls share one CCallResult<RemoteStorageDownloadUGCResult_t>, so they share one FIFO queue;
 * bToLocation selects which SDK call to (re-)issue and Location is only used when it is set.
 */
struct FESteamPendingUGCDownload
{
	UGCHandle_t Handle = k_UGCHandleInvalid;
	uint32 Priority = 0;
	bool bToLocation = false;
	FString Location;
};

/**
 * Native Steam call-result listeners; alive only while the Steam client API is initialized.
 *
 * Same-type async requests (write, read, share, UGC download) are serialized via a per-operation
 * FIFO queue: while a given op's CCallResult is in flight, further requests of that type are
 * enqueued and issued in order as each completion arrives, so none are dropped. The in-flight
 * request's filename is remembered separately (PendingWriteFileName/…) because the Steam result
 * structs do not all echo it back (the UGC-download result does, so no separate field is needed).
 * Queued-but-unissued requests hold no Steam handles, so tearing down this holder (Steam shutdown /
 * Deinitialize) abandons them cleanly.
 */
class FESteamRemoteStorageCallbacks
{
public:
	explicit FESteamRemoteStorageCallbacks(UESteamRemoteStorageSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the parameters. Returns true when the request was issued or queued; false only on the
	// immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueueFileWrite(const FESteamPendingFileWrite& Request)
	{
		if (bWriteBusy)
		{
			WriteQueue.Enqueue(Request);
			return true;
		}
		return IssueFileWrite(Request);
	}

	bool EnqueueFileRead(const FESteamPendingFileRead& Request)
	{
		if (bReadBusy)
		{
			ReadQueue.Enqueue(Request);
			return true;
		}
		return IssueFileRead(Request);
	}

	bool EnqueueFileShare(const FESteamPendingFileShare& Request)
	{
		if (bShareBusy)
		{
			ShareQueue.Enqueue(Request);
			return true;
		}
		return IssueFileShare(Request);
	}

	bool EnqueueUGCDownload(const FESteamPendingUGCDownload& Request)
	{
		if (bDownloadBusy)
		{
			DownloadQueue.Enqueue(Request);
			return true;
		}
		return IssueUGCDownload(Request);
	}

private:
	// ---- FileWriteAsync (serialized) ----

	bool IssueFileWrite(const FESteamPendingFileWrite& Request)
	{
		UESteamRemoteStorageSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamRemoteStorage())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamRemoteStorage()->FileWriteAsync(
			TCHAR_TO_UTF8(*Request.FileName), Request.Data.GetData(), static_cast<uint32>(Request.Data.Num()));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PendingWriteFileName = Request.FileName;
		WriteResult.Set(Call, this, &FESteamRemoteStorageCallbacks::HandleWriteComplete);
		bWriteBusy = true;
		return true;
	}

	void HandleWriteComplete(RemoteStorageFileWriteAsyncComplete_t* Data, bool bIOFailure)
	{
		const FString FileName = MoveTemp(PendingWriteFileName);
		PendingWriteFileName.Reset();

		if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnFileWriteComplete.Broadcast(bSuccess, FileName);
		}
		bWriteBusy = false;
		DrainWriteQueue();
	}

	void DrainWriteQueue()
	{
		while (!bWriteBusy)
		{
			FESteamPendingFileWrite Request;
			if (!WriteQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueFileWrite(Request))
			{
				// Steam went away while draining: fail this queued request instead of dropping it.
				if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnFileWriteComplete.Broadcast(false, Request.FileName);
				}
			}
		}
	}

	// ---- FileReadAsync (serialized) ----

	bool IssueFileRead(const FESteamPendingFileRead& Request)
	{
		UESteamRemoteStorageSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamRemoteStorage())
		{
			return false;
		}

		// The file size is (re)resolved here so a queued read sees current data.
		const FTCHARToUTF8 FileNameUtf8(*Request.FileName);
		const int32 FileSize = SteamRemoteStorage()->GetFileSize(FileNameUtf8.Get());
		if (FileSize <= 0)
		{
			return false;
		}

		const SteamAPICall_t Call = SteamRemoteStorage()->FileReadAsync(FileNameUtf8.Get(), 0, static_cast<uint32>(FileSize));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PendingReadFileName = Request.FileName;
		ReadResult.Set(Call, this, &FESteamRemoteStorageCallbacks::HandleReadComplete);
		bReadBusy = true;
		return true;
	}

	void HandleReadComplete(RemoteStorageFileReadAsyncComplete_t* Data, bool bIOFailure)
	{
		const FString FileName = MoveTemp(PendingReadFileName);
		PendingReadFileName.Reset();

		if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
		{
			TArray<uint8> Bytes;
			bool bSuccess = false;
			if (!bIOFailure && Data->m_eResult == k_EResultOK && Data->m_cubRead > 0 && SteamRemoteStorage())
			{
				Bytes.SetNumUninitialized(static_cast<int32>(Data->m_cubRead));
				bSuccess = SteamRemoteStorage()->FileReadAsyncComplete(Data->m_hFileReadAsync, Bytes.GetData(), Data->m_cubRead);
				if (!bSuccess)
				{
					Bytes.Reset();
				}
			}
			Subsystem->OnFileReadComplete.Broadcast(bSuccess, FileName, Bytes);
		}
		bReadBusy = false;
		DrainReadQueue();
	}

	void DrainReadQueue()
	{
		while (!bReadBusy)
		{
			FESteamPendingFileRead Request;
			if (!ReadQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueFileRead(Request))
			{
				if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
				{
					const TArray<uint8> EmptyData;
					Subsystem->OnFileReadComplete.Broadcast(false, Request.FileName, EmptyData);
				}
			}
		}
	}

	// ---- FileShare (serialized) ----

	bool IssueFileShare(const FESteamPendingFileShare& Request)
	{
		UESteamRemoteStorageSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamRemoteStorage())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamRemoteStorage()->FileShare(TCHAR_TO_UTF8(*Request.FileName));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		PendingShareFileName = Request.FileName;
		ShareResult.Set(Call, this, &FESteamRemoteStorageCallbacks::HandleShareComplete);
		bShareBusy = true;
		return true;
	}

	void HandleShareComplete(RemoteStorageFileShareResult_t* Data, bool bIOFailure)
	{
		const FString FileName = MoveTemp(PendingShareFileName);
		PendingShareFileName.Reset();

		if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnFileShared.Broadcast(bSuccess, FileName, bSuccess ? static_cast<int64>(Data->m_hFile) : 0);
		}
		bShareBusy = false;
		DrainShareQueue();
	}

	void DrainShareQueue()
	{
		while (!bShareBusy)
		{
			FESteamPendingFileShare Request;
			if (!ShareQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueFileShare(Request))
			{
				if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnFileShared.Broadcast(false, Request.FileName, 0);
				}
			}
		}
	}

	// ---- UGCDownload / UGCDownloadToLocation (serialized; shared CCallResult) ----

	bool IssueUGCDownload(const FESteamPendingUGCDownload& Request)
	{
		UESteamRemoteStorageSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamRemoteStorage())
		{
			return false;
		}

		const SteamAPICall_t Call = Request.bToLocation
			? SteamRemoteStorage()->UGCDownloadToLocation(Request.Handle, TCHAR_TO_UTF8(*Request.Location), Request.Priority)
			: SteamRemoteStorage()->UGCDownload(Request.Handle, Request.Priority);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		DownloadResult.Set(Call, this, &FESteamRemoteStorageCallbacks::HandleUGCDownloaded);
		bDownloadBusy = true;
		return true;
	}

	void HandleUGCDownloaded(RemoteStorageDownloadUGCResult_t* Data, bool bIOFailure)
	{
		if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			// The result struct echoes the handle, filename and size, so nothing needs remembering.
			Subsystem->OnUGCDownloaded.Broadcast(bSuccess,
				static_cast<int64>(Data->m_hFile),
				bSuccess ? FString(UTF8_TO_TCHAR(Data->m_pchFileName)) : FString(),
				bSuccess ? Data->m_nSizeInBytes : 0);
		}
		bDownloadBusy = false;
		DrainDownloadQueue();
	}

	void DrainDownloadQueue()
	{
		while (!bDownloadBusy)
		{
			FESteamPendingUGCDownload Request;
			if (!DownloadQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueUGCDownload(Request))
			{
				if (UESteamRemoteStorageSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnUGCDownloaded.Broadcast(false, static_cast<int64>(Request.Handle), FString(), 0);
				}
			}
		}
	}

	TWeakObjectPtr<UESteamRemoteStorageSubsystem> Owner;

	// The in-flight request's filename per op (the Steam result structs do not all echo it back).
	FString PendingWriteFileName;
	FString PendingReadFileName;
	FString PendingShareFileName;

	CCallResult<FESteamRemoteStorageCallbacks, RemoteStorageFileWriteAsyncComplete_t> WriteResult;
	CCallResult<FESteamRemoteStorageCallbacks, RemoteStorageFileReadAsyncComplete_t> ReadResult;
	CCallResult<FESteamRemoteStorageCallbacks, RemoteStorageFileShareResult_t> ShareResult;
	CCallResult<FESteamRemoteStorageCallbacks, RemoteStorageDownloadUGCResult_t> DownloadResult;

	// In-flight flags + FIFO queues, one per serialized operation type.
	bool bWriteBusy = false;
	bool bReadBusy = false;
	bool bShareBusy = false;
	bool bDownloadBusy = false;

	TQueue<FESteamPendingFileWrite> WriteQueue;
	TQueue<FESteamPendingFileRead> ReadQueue;
	TQueue<FESteamPendingFileShare> ShareQueue;
	TQueue<FESteamPendingUGCDownload> DownloadQueue;
};
#else
class FESteamRemoteStorageCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamRemoteStorageSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamRemoteStorageSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamRemoteStorageCallbacks>(this);
	}
#endif
}

void UESteamRemoteStorageSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamRemoteStorageSubsystem::FileWrite(const FString& FileName, const TArray<uint8>& Data)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWrite"));
		return false;
	}
	if (Data.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("FileWrite: refusing to write empty data to '%s'"), *FileName);
		return false;
	}
	return SteamRemoteStorage()->FileWrite(TCHAR_TO_UTF8(*FileName), Data.GetData(), Data.Num());
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileWriteString(const FString& FileName, const FString& Contents)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWriteString"));
		return false;
	}
	const FTCHARToUTF8 Utf8(*Contents);
	if (Utf8.Length() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("FileWriteString: refusing to write empty contents to '%s'"), *FileName);
		return false;
	}
	return SteamRemoteStorage()->FileWrite(TCHAR_TO_UTF8(*FileName), Utf8.Get(), Utf8.Length());
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileRead(const FString& FileName, TArray<uint8>& OutData)
{
	OutData.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileRead"));
		return false;
	}
	const FTCHARToUTF8 FileNameUtf8(*FileName);
	const int32 FileSize = SteamRemoteStorage()->GetFileSize(FileNameUtf8.Get());
	if (FileSize <= 0)
	{
		return false;
	}
	OutData.SetNumUninitialized(FileSize);
	const int32 BytesRead = SteamRemoteStorage()->FileRead(FileNameUtf8.Get(), OutData.GetData(), FileSize);
	if (BytesRead != FileSize)
	{
		OutData.Reset();
		return false;
	}
	return true;
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileReadString(const FString& FileName, FString& OutContents)
{
	OutContents.Reset();
	TArray<uint8> Bytes;
	if (!FileRead(FileName, Bytes))
	{
		return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	OutContents = FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
	return true;
}

bool UESteamRemoteStorageSubsystem::FileExists(const FString& FileName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamRemoteStorage() && SteamRemoteStorage()->FileExists(TCHAR_TO_UTF8(*FileName));
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileDelete(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileDelete"));
		return false;
	}
	return SteamRemoteStorage()->FileDelete(TCHAR_TO_UTF8(*FileName));
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileForget(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileForget"));
		return false;
	}
	return SteamRemoteStorage()->FileForget(TCHAR_TO_UTF8(*FileName));
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FilePersisted(const FString& FileName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamRemoteStorage() && SteamRemoteStorage()->FilePersisted(TCHAR_TO_UTF8(*FileName));
#else
	return false;
#endif
}

int32 UESteamRemoteStorageSubsystem::GetFileSize(const FString& FileName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return SteamRemoteStorage()->GetFileSize(TCHAR_TO_UTF8(*FileName));
	}
#endif
	return 0;
}

int64 UESteamRemoteStorageSubsystem::GetFileTimestamp(const FString& FileName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return SteamRemoteStorage()->GetFileTimestamp(TCHAR_TO_UTF8(*FileName));
	}
#endif
	return 0;
}

int32 UESteamRemoteStorageSubsystem::GetFileCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return SteamRemoteStorage()->GetFileCount();
	}
#endif
	return 0;
}

bool UESteamRemoteStorageSubsystem::GetFileNameAndSize(int32 Index, FString& OutName, int32& OutSize) const
{
	OutName.Reset();
	OutSize = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		return false;
	}
	const char* Name = SteamRemoteStorage()->GetFileNameAndSize(Index, &OutSize);
	if (!Name || Name[0] == '\0')
	{
		OutSize = 0;
		return false;
	}
	OutName = UTF8_TO_TCHAR(Name);
	return true;
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::GetQuota(int64& OutTotalBytes, int64& OutAvailableBytes) const
{
	OutTotalBytes = 0;
	OutAvailableBytes = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		return false;
	}
	uint64 TotalBytes = 0;
	uint64 AvailableBytes = 0;
	if (!SteamRemoteStorage()->GetQuota(&TotalBytes, &AvailableBytes))
	{
		return false;
	}
	OutTotalBytes = static_cast<int64>(TotalBytes);
	OutAvailableBytes = static_cast<int64>(AvailableBytes);
	return true;
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::IsCloudEnabledForAccount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamRemoteStorage() && SteamRemoteStorage()->IsCloudEnabledForAccount();
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::IsCloudEnabledForApp() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamRemoteStorage() && SteamRemoteStorage()->IsCloudEnabledForApp();
#else
	return false;
#endif
}

void UESteamRemoteStorageSubsystem::SetCloudEnabledForApp(bool bEnabled)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("SetCloudEnabledForApp"));
		return;
	}
	SteamRemoteStorage()->SetCloudEnabledForApp(bEnabled);
#endif
}

bool UESteamRemoteStorageSubsystem::FileWriteAsync(const FString& FileName, const TArray<uint8>& Data)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("FileWriteAsync"));
		return false;
	}
	if (Data.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("FileWriteAsync: refusing to write empty data to '%s'"), *FileName);
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight write (never dropped).
	FESteamPendingFileWrite Request;
	Request.FileName = FileName;
	Request.Data = Data;
	return Callbacks->EnqueueFileWrite(Request);
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileReadAsync(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("FileReadAsync"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight read (never dropped). The file
	// size is (re)resolved when the read is actually issued, so a queued read sees current data.
	FESteamPendingFileRead Request;
	Request.FileName = FileName;
	return Callbacks->EnqueueFileRead(Request);
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileShare(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("FileShare"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight share (never dropped).
	FESteamPendingFileShare Request;
	Request.FileName = FileName;
	return Callbacks->EnqueueFileShare(Request);
#else
	return false;
#endif
}

int64 UESteamRemoteStorageSubsystem::FileWriteStreamOpen(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWriteStreamOpen"));
		return static_cast<int64>(k_UGCFileStreamHandleInvalid);
	}
	// UGCFileWriteStreamHandle_t (uint64) -> int64: opaque bits fed back to the stream calls.
	return static_cast<int64>(SteamRemoteStorage()->FileWriteStreamOpen(TCHAR_TO_UTF8(*FileName)));
#else
	LogSteamUnavailable(TEXT("FileWriteStreamOpen"));
	return static_cast<int64>(0xffffffffffffffffull);
#endif
}

bool UESteamRemoteStorageSubsystem::FileWriteStreamWriteChunk(int64 StreamHandle, const TArray<uint8>& Data)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWriteStreamWriteChunk"));
		return false;
	}
	if (Data.Num() == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("FileWriteStreamWriteChunk: refusing to write an empty chunk"));
		return false;
	}
	return SteamRemoteStorage()->FileWriteStreamWriteChunk(
		static_cast<UGCFileWriteStreamHandle_t>(StreamHandle), Data.GetData(), Data.Num());
#else
	LogSteamUnavailable(TEXT("FileWriteStreamWriteChunk"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileWriteStreamClose(int64 StreamHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWriteStreamClose"));
		return false;
	}
	return SteamRemoteStorage()->FileWriteStreamClose(static_cast<UGCFileWriteStreamHandle_t>(StreamHandle));
#else
	LogSteamUnavailable(TEXT("FileWriteStreamClose"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::FileWriteStreamCancel(int64 StreamHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("FileWriteStreamCancel"));
		return false;
	}
	return SteamRemoteStorage()->FileWriteStreamCancel(static_cast<UGCFileWriteStreamHandle_t>(StreamHandle));
#else
	LogSteamUnavailable(TEXT("FileWriteStreamCancel"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::SetSyncPlatforms(const FString& FileName, int32 PlatformFlags)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("SetSyncPlatforms"));
		return false;
	}
	return SteamRemoteStorage()->SetSyncPlatforms(
		TCHAR_TO_UTF8(*FileName), static_cast<ERemoteStoragePlatform>(PlatformFlags));
#else
	LogSteamUnavailable(TEXT("SetSyncPlatforms"));
	return false;
#endif
}

int32 UESteamRemoteStorageSubsystem::GetSyncPlatforms(const FString& FileName) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return static_cast<int32>(SteamRemoteStorage()->GetSyncPlatforms(TCHAR_TO_UTF8(*FileName)));
	}
#endif
	return 0;
}

int32 UESteamRemoteStorageSubsystem::GetLocalFileChangeCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return SteamRemoteStorage()->GetLocalFileChangeCount();
	}
#endif
	return 0;
}

bool UESteamRemoteStorageSubsystem::GetLocalFileChange(int32 Index, FString& OutName, int32& OutChangeType, int32& OutFilePathType) const
{
	OutName.Reset();
	OutChangeType = 0;
	OutFilePathType = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		return false;
	}

	ERemoteStorageLocalFileChange ChangeType = k_ERemoteStorageLocalFileChange_Invalid;
	ERemoteStorageFilePathType PathType = k_ERemoteStorageFilePathType_Invalid;
	const char* Name = SteamRemoteStorage()->GetLocalFileChange(Index, &ChangeType, &PathType);
	if (!Name || Name[0] == '\0')
	{
		return false;
	}
	OutName = UTF8_TO_TCHAR(Name);
	OutChangeType = static_cast<int32>(ChangeType);
	OutFilePathType = static_cast<int32>(PathType);
	return true;
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::BeginFileWriteBatch()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("BeginFileWriteBatch"));
		return false;
	}
	return SteamRemoteStorage()->BeginFileWriteBatch();
#else
	LogSteamUnavailable(TEXT("BeginFileWriteBatch"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::EndFileWriteBatch()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("EndFileWriteBatch"));
		return false;
	}
	return SteamRemoteStorage()->EndFileWriteBatch();
#else
	LogSteamUnavailable(TEXT("EndFileWriteBatch"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::GetUGCDetails(int64 UGCHandle, int32& OutAppId, FString& OutFileName, int32& OutFileSize, FESteamId& OutOwner) const
{
	OutAppId = 0;
	OutFileName.Reset();
	OutFileSize = 0;
	OutOwner = FESteamId();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		return false;
	}

	AppId_t AppId = 0;
	char* Name = nullptr;
	int32 FileSize = 0;
	CSteamID Owner;
	// int64 -> UGCHandle_t (uint64): reinterprets the bits of a handle received from OnUGCDownloaded.
	if (!SteamRemoteStorage()->GetUGCDetails(static_cast<UGCHandle_t>(UGCHandle), &AppId, &Name, &FileSize, &Owner))
	{
		return false;
	}

	OutAppId = static_cast<int32>(AppId);
	OutFileName = Name ? FString(UTF8_TO_TCHAR(Name)) : FString();
	OutFileSize = FileSize;
	OutOwner = FESteamId(Owner.ConvertToUint64());
	return true;
#else
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::UGCDownload(int64 UGCHandle, int32 Priority)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("UGCDownload"));
		return false;
	}

	// Issued now if idle, otherwise queued behind the in-flight download (never dropped).
	FESteamPendingUGCDownload Request;
	Request.Handle = static_cast<UGCHandle_t>(UGCHandle);
	Request.Priority = static_cast<uint32>(FMath::Max(Priority, 0));
	Request.bToLocation = false;
	return Callbacks->EnqueueUGCDownload(Request);
#else
	LogSteamUnavailable(TEXT("UGCDownload"));
	return false;
#endif
}

bool UESteamRemoteStorageSubsystem::UGCDownloadToLocation(int64 UGCHandle, const FString& Location, int32 Priority)
{
	if (Location.IsEmpty())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("UGCDownloadToLocation: empty location"));
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("UGCDownloadToLocation"));
		return false;
	}

	// Shares the UGCDownload FIFO queue; issued now if idle, otherwise queued (never dropped).
	FESteamPendingUGCDownload Request;
	Request.Handle = static_cast<UGCHandle_t>(UGCHandle);
	Request.Priority = static_cast<uint32>(FMath::Max(Priority, 0));
	Request.bToLocation = true;
	Request.Location = Location;
	return Callbacks->EnqueueUGCDownload(Request);
#else
	LogSteamUnavailable(TEXT("UGCDownloadToLocation"));
	return false;
#endif
}

int32 UESteamRemoteStorageSubsystem::UGCRead(int64 UGCHandle, int32 Offset, int32 BytesToRead, TArray<uint8>& OutData)
{
	OutData.Reset();
	if (BytesToRead <= 0 || Offset < 0)
	{
		return 0;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		LogSteamUnavailable(TEXT("UGCRead"));
		return 0;
	}

	OutData.SetNumUninitialized(BytesToRead);
	// k_EUGCRead_ContinueReadingUntilFinished keeps the legacy behavior: the handle stays open until
	// the last byte is read, at which point Steam closes it (call UGCDownload again to reopen).
	const int32 BytesRead = SteamRemoteStorage()->UGCRead(static_cast<UGCHandle_t>(UGCHandle),
		OutData.GetData(), BytesToRead, static_cast<uint32>(Offset), k_EUGCRead_ContinueReadingUntilFinished);
	if (BytesRead <= 0)
	{
		OutData.Reset();
		return 0;
	}
	OutData.SetNum(BytesRead, EAllowShrinking::No);
	return BytesRead;
#else
	LogSteamUnavailable(TEXT("UGCRead"));
	return 0;
#endif
}

bool UESteamRemoteStorageSubsystem::GetUGCDownloadProgress(int64 UGCHandle, int32& OutBytesDownloaded, int32& OutBytesExpected) const
{
	OutBytesDownloaded = 0;
	OutBytesExpected = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemoteStorage())
	{
		return false;
	}
	return SteamRemoteStorage()->GetUGCDownloadProgress(
		static_cast<UGCHandle_t>(UGCHandle), &OutBytesDownloaded, &OutBytesExpected);
#else
	return false;
#endif
}

int32 UESteamRemoteStorageSubsystem::GetCachedUGCCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		return SteamRemoteStorage()->GetCachedUGCCount();
	}
#endif
	return 0;
}

int64 UESteamRemoteStorageSubsystem::GetCachedUGCHandle(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemoteStorage())
	{
		// UGCHandle_t (uint64) -> int64: opaque bits, valid for GetUGCDetails / UGCRead.
		return static_cast<int64>(SteamRemoteStorage()->GetCachedUGCHandle(Index));
	}
#endif
	return 0;
}

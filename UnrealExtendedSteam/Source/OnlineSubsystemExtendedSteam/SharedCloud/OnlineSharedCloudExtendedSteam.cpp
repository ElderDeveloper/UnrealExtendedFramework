// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SharedCloud/OnlineSharedCloudExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamSharedCloud
{
	/** Mirrors the SDK's k_UGCHandleInvalid so the handle type works in no-SDK builds too. */
	static constexpr uint64 InvalidUGCHandleValue = 0xffffffffffffffffull;

#if WITH_EXTENDEDSTEAM_SDK
	/** ISteamRemoteStorage accessor guarded by the shared module's client-API state; null when unavailable. */
	static ISteamRemoteStorage* GetRemoteStorage()
	{
		if (FExtendedSteamSharedModule::IsModuleAvailable() && FExtendedSteamSharedModule::Get().IsSteamClientInitialized())
		{
			return SteamRemoteStorage();
		}
		return nullptr;
	}
#endif
}

/**
 * Shared content handle wrapping a Steam UGCHandle_t (uint64).
 * String form is the decimal uint64; byte form is the raw 8-byte value.
 * Deliberately cpp-local: callers only ever see the opaque FSharedContentHandle base.
 */
class FSharedContentHandleExtendedSteam : public FSharedContentHandle
{
public:
	explicit FSharedContentHandleExtendedSteam(uint64 InHandleValue = 0)
		: HandleValue(InHandleValue)
	{
	}

	//~ Begin IOnlinePlatformData
	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8*>(&HandleValue);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(HandleValue);
	}

	virtual bool IsValid() const override
	{
		return HandleValue != 0 && HandleValue != ExtendedSteamSharedCloud::InvalidUGCHandleValue;
	}

	virtual FString ToString() const override
	{
		return LexToString(HandleValue);
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("SteamUGC:%llu"), HandleValue);
	}
	//~ End IOnlinePlatformData

	uint64 GetHandleValue() const
	{
		return HandleValue;
	}

private:
	uint64 HandleValue = 0;
};

namespace ExtendedSteamSharedCloud
{
	/** Extracts the uint64 UGC handle from any FSharedContentHandle via its opaque byte form. */
	static uint64 ExtractHandleValue(const FSharedContentHandle& SharedHandle)
	{
		if (SharedHandle.GetSize() == sizeof(uint64) && SharedHandle.GetBytes() != nullptr)
		{
			uint64 Value = 0;
			FMemory::Memcpy(&Value, SharedHandle.GetBytes(), sizeof(uint64));
			return Value;
		}
		return 0;
	}
}

#if WITH_EXTENDEDSTEAM_SDK
/**
 * Native Steam call-result listeners for the shared cloud. One CCallResult per operation type,
 * which is what enforces the one-in-flight-per-op rule (re-Set on an active CCallResult would
 * silently drop the earlier request, so the owner checks its in-flight flags before tracking).
 * Owned by FOnlineSharedCloudExtendedSteam; destruction cancels any registered call result.
 */
class FOnlineSharedCloudExtendedSteamCallbacks
{
public:
	explicit FOnlineSharedCloudExtendedSteamCallbacks(FOnlineSharedCloudExtendedSteam* InOwner)
		: Owner(InOwner)
	{
	}

	void TrackShare(SteamAPICall_t Call)
	{
		ShareResult.Set(Call, this, &FOnlineSharedCloudExtendedSteamCallbacks::HandleShareComplete);
	}

	void TrackDownload(SteamAPICall_t Call)
	{
		DownloadResult.Set(Call, this, &FOnlineSharedCloudExtendedSteamCallbacks::HandleDownloadComplete);
	}

private:
	void HandleShareComplete(RemoteStorageFileShareResult_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK && Data->m_hFile != k_UGCHandleInvalid;
		Owner->OnNativeShareComplete(bSuccess, bSuccess ? static_cast<uint64>(Data->m_hFile) : 0);
	}

	void HandleDownloadComplete(RemoteStorageDownloadUGCResult_t* Data, bool bIOFailure)
	{
		if (Data == nullptr)
		{
			return;
		}

		TArray<uint8> Bytes;
		bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK && Data->m_nSizeInBytes >= 0;
		if (bSuccess && Data->m_nSizeInBytes > 0)
		{
			bSuccess = false;
			if (ISteamRemoteStorage* RemoteStorage = ExtendedSteamSharedCloud::GetRemoteStorage())
			{
				// Single-shot read from offset 0; k_EUGCRead_Close frees the SDK-side file handle
				// (a re-read requires a fresh UGCDownload, but we serve re-reads from our cache).
				Bytes.SetNumUninitialized(Data->m_nSizeInBytes);
				const int32 BytesRead = RemoteStorage->UGCRead(Data->m_hFile, Bytes.GetData(), Data->m_nSizeInBytes, 0, k_EUGCRead_Close);
				bSuccess = BytesRead == Data->m_nSizeInBytes;
			}
			if (!bSuccess)
			{
				Bytes.Reset();
			}
		}
		Owner->OnNativeDownloadComplete(bSuccess, static_cast<uint64>(Data->m_hFile), MoveTemp(Bytes));
	}

	/** Owning interface (owns this object; outlives it). */
	FOnlineSharedCloudExtendedSteam* Owner = nullptr;

	CCallResult<FOnlineSharedCloudExtendedSteamCallbacks, RemoteStorageFileShareResult_t> ShareResult;
	CCallResult<FOnlineSharedCloudExtendedSteamCallbacks, RemoteStorageDownloadUGCResult_t> DownloadResult;
};
#else
class FOnlineSharedCloudExtendedSteamCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineSharedCloudExtendedSteam::FOnlineSharedCloudExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
}

// Defined here so TUniquePtr<FOnlineSharedCloudExtendedSteamCallbacks> sees the complete type.
FOnlineSharedCloudExtendedSteam::~FOnlineSharedCloudExtendedSteam() = default;

bool FOnlineSharedCloudExtendedSteam::IsLocalSteamUser(const FUniqueNetId& UserId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamSharedCloud::GetRemoteStorage() != nullptr && SteamUser() != nullptr)
	{
		const uint64 LocalSteamId64 = SteamUser()->GetSteamID().ConvertToUint64();
		return LocalSteamId64 != 0
			&& UserId.IsValid()
			&& UserId.GetType() == ESTEAM_SUBSYSTEM
			&& UserId.GetSize() == sizeof(uint64)
			&& FMemory::Memcmp(UserId.GetBytes(), &LocalSteamId64, sizeof(uint64)) == 0;
	}
#endif
	return false;
}

bool FOnlineSharedCloudExtendedSteam::GetSharedFileContents(const FSharedContentHandle& SharedHandle, TArray<uint8>& FileContents)
{
	FileContents.Reset();

	const uint64 HandleValue = ExtendedSteamSharedCloud::ExtractHandleValue(SharedHandle);
	const FCloudFile* CachedFile = SharedFileCache.Find(HandleValue);
	if (CachedFile != nullptr && CachedFile->AsyncState == EOnlineAsyncTaskState::Done)
	{
		FileContents = CachedFile->Data;
		return true;
	}
	return false;
}

bool FOnlineSharedCloudExtendedSteam::ClearSharedFiles()
{
	if (PendingDownloadHandle != 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ClearSharedFiles: a shared file download is in flight; not clearing"));
		return false;
	}
	SharedFileCache.Empty();
	return true;
}

bool FOnlineSharedCloudExtendedSteam::ClearSharedFile(const FSharedContentHandle& SharedHandle)
{
	const uint64 HandleValue = ExtendedSteamSharedCloud::ExtractHandleValue(SharedHandle);
	if (HandleValue != 0 && HandleValue == PendingDownloadHandle)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ClearSharedFile: %s is being downloaded; not clearing"), *SharedHandle.ToDebugString());
		return false;
	}
	// Not in flight: clearing succeeds whether or not the handle was cached.
	SharedFileCache.Remove(HandleValue);
	return true;
}

bool FOnlineSharedCloudExtendedSteam::ReadSharedFile(const FSharedContentHandle& SharedHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamSharedCloud::GetRemoteStorage();
	const uint64 HandleValue = ExtendedSteamSharedCloud::ExtractHandleValue(SharedHandle);

	if (RemoteStorage != nullptr && SharedHandle.IsValid() && HandleValue != 0
		&& HandleValue != ExtendedSteamSharedCloud::InvalidUGCHandleValue)
	{
		if (PendingDownloadHandle != 0)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("ReadSharedFile: a download is already in flight (one at a time); rejecting %s"), *SharedHandle.ToDebugString());
			TriggerOnReadSharedFileCompleteDelegates(false, SharedHandle);
			return false;
		}

		// Priority 0 = download immediately.
		const SteamAPICall_t Call = RemoteStorage->UGCDownload(static_cast<UGCHandle_t>(HandleValue), 0);
		if (Call != k_uAPICallInvalid)
		{
			if (!Callbacks.IsValid())
			{
				Callbacks = MakeShared<FOnlineSharedCloudExtendedSteamCallbacks>(this);
			}
			PendingDownloadHandle = HandleValue;
			Callbacks->TrackDownload(Call);
			return true;
		}
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadSharedFile: UGCDownload failed to start for %s"), *SharedHandle.ToDebugString());
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("ReadSharedFile: cannot read %s (Steam unavailable or invalid handle)"), *SharedHandle.ToDebugString());
	TriggerOnReadSharedFileCompleteDelegates(false, SharedHandle);
	return false;
}

bool FOnlineSharedCloudExtendedSteam::WriteSharedFile(const FUniqueNetId& UserId, const FString& Filename, TArray<uint8>& Contents)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamSharedCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId) && !Filename.IsEmpty()
		&& Contents.Num() > 0 && static_cast<uint32>(Contents.Num()) <= k_unMaxCloudFileChunkSize)
	{
		if (bShareInFlight)
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("WriteSharedFile: a write/share is already in flight (one at a time); rejecting '%s'"), *Filename);
			TriggerOnWriteSharedFileCompleteDelegates(false, UserId, Filename, MakeShared<FSharedContentHandleExtendedSteam>(0ull));
			return false;
		}

		// Step 1: synchronous FileWrite into the local user's cloud storage (local-disk write).
		if (!RemoteStorage->FileWrite(TCHAR_TO_UTF8(*Filename), Contents.GetData(), Contents.Num()))
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("WriteSharedFile: FileWrite failed for '%s' (%d bytes)"), *Filename, Contents.Num());
			TriggerOnWriteSharedFileCompleteDelegates(false, UserId, Filename, MakeShared<FSharedContentHandleExtendedSteam>(0ull));
			return false;
		}

		// Step 2: async FileShare; the call result carries the UGC handle for the shared file.
		const SteamAPICall_t Call = RemoteStorage->FileShare(TCHAR_TO_UTF8(*Filename));
		if (Call != k_uAPICallInvalid)
		{
			if (!Callbacks.IsValid())
			{
				Callbacks = MakeShared<FOnlineSharedCloudExtendedSteamCallbacks>(this);
			}
			bShareInFlight = true;
			PendingShareUserId = UserId.AsShared();
			PendingShareFileName = Filename;
			PendingShareData = Contents; // Cached under the UGC handle once the share completes.
			Callbacks->TrackShare(Call);
			return true;
		}
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteSharedFile: FileShare failed to start for '%s'"), *Filename);
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("WriteSharedFile: cannot share '%s' (%d bytes) (Steam unavailable, bad filename/size, or user %s is not the local Steam user)"),
		*Filename, Contents.Num(), *UserId.ToDebugString());
	TriggerOnWriteSharedFileCompleteDelegates(false, UserId, Filename, MakeShared<FSharedContentHandleExtendedSteam>(0ull));
	return false;
}

void FOnlineSharedCloudExtendedSteam::GetDummySharedHandlesForTest(TArray<TSharedRef<FSharedContentHandle>>& OutHandles)
{
	// A single zero-handle instance: well-formed object, IsValid() == false, never resolves.
	OutHandles.Add(MakeShared<FSharedContentHandleExtendedSteam>(0ull));
}

void FOnlineSharedCloudExtendedSteam::OnNativeShareComplete(bool bSuccess, uint64 SharedHandleValue)
{
	const FUniqueNetIdPtr UserId = PendingShareUserId;
	const FString Filename = PendingShareFileName;
	TArray<uint8> Data = MoveTemp(PendingShareData);

	bShareInFlight = false;
	PendingShareUserId.Reset();
	PendingShareFileName.Reset();
	PendingShareData.Reset();

	if (bSuccess)
	{
		// We already hold the payload we just wrote; cache it so GetSharedFileContents works
		// immediately without a redundant UGCDownload round trip.
		FCloudFile& CachedFile = SharedFileCache.FindOrAdd(SharedHandleValue, FCloudFile(Filename));
		CachedFile.Data = MoveTemp(Data);
		CachedFile.AsyncState = EOnlineAsyncTaskState::Done;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("WriteSharedFile: '%s' shared as UGC handle %llu"), *Filename, SharedHandleValue);
	}
	else
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("WriteSharedFile: FileShare call result failed for '%s'"), *Filename);
	}

	TriggerOnWriteSharedFileCompleteDelegates(bSuccess,
		UserId.IsValid() ? *UserId : *FUniqueNetIdExtendedSteam::EmptyId(),
		Filename,
		MakeShared<FSharedContentHandleExtendedSteam>(bSuccess ? SharedHandleValue : 0ull));
}

void FOnlineSharedCloudExtendedSteam::OnNativeDownloadComplete(bool bSuccess, uint64 SharedHandleValue, TArray<uint8>&& Data)
{
	// The delegate identity is the handle the caller requested; the result's echoed handle is a
	// sanity check only (with one download in flight they can only mismatch on an SDK IO failure).
	const uint64 HandleValue = PendingDownloadHandle;
	PendingDownloadHandle = 0;

	if (bSuccess && SharedHandleValue != HandleValue)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadSharedFile: result handle %llu does not match the requested handle %llu"), SharedHandleValue, HandleValue);
		bSuccess = false;
	}

	if (bSuccess)
	{
		FCloudFile& CachedFile = SharedFileCache.FindOrAdd(HandleValue, FCloudFile(LexToString(HandleValue)));
		CachedFile.Data = MoveTemp(Data);
		CachedFile.AsyncState = EOnlineAsyncTaskState::Done;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("ReadSharedFile: UGC handle %llu downloaded (%d bytes)"), HandleValue, CachedFile.Data.Num());
	}
	else
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadSharedFile: download failed for UGC handle %llu"), HandleValue);
	}

	const FSharedContentHandleExtendedSteam Handle(HandleValue);
	TriggerOnReadSharedFileCompleteDelegates(bSuccess, Handle);
}

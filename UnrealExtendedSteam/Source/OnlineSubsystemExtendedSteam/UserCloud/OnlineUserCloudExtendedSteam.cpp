// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UserCloud/OnlineUserCloudExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamUserCloud
{
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

bool FOnlineUserCloudExtendedSteam::IsLocalSteamUser(const FUniqueNetId& UserId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamUserCloud::GetRemoteStorage() != nullptr && SteamUser() != nullptr)
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

bool FOnlineUserCloudExtendedSteam::GetFileContents(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents)
{
	FileContents.Reset();

	if (!IsLocalSteamUser(UserId))
	{
		return false;
	}

	const FCloudFile* CachedFile = FileCache.Find(MakeCacheKey(FileName));
	if (CachedFile != nullptr && CachedFile->AsyncState == EOnlineAsyncTaskState::Done)
	{
		FileContents = CachedFile->Data;
		return true;
	}
	return false;
}

bool FOnlineUserCloudExtendedSteam::ClearFiles(const FUniqueNetId& UserId)
{
	if (!IsLocalSteamUser(UserId))
	{
		return false;
	}

	// All operations complete synchronously, so no download can be outstanding; always clearable.
	FileCache.Empty();
	return true;
}

bool FOnlineUserCloudExtendedSteam::ClearFile(const FUniqueNetId& UserId, const FString& FileName)
{
	if (!IsLocalSteamUser(UserId))
	{
		return false;
	}

	// Synchronous operations mean the file can never be "currently downloading"; clearing always
	// succeeds, whether or not the file was cached.
	FileCache.Remove(MakeCacheKey(FileName));
	return true;
}

void FOnlineUserCloudExtendedSteam::EnumerateUserFiles(const FUniqueNetId& UserId)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId))
	{
		EnumeratedFiles.Reset();

		const int32 FileCount = RemoteStorage->GetFileCount();
		EnumeratedFiles.Reserve(FileCount);
		for (int32 FileIndex = 0; FileIndex < FileCount; ++FileIndex)
		{
			int32 FileSizeInBytes = 0;
			const char* FileNameAnsi = RemoteStorage->GetFileNameAndSize(FileIndex, &FileSizeInBytes);
			if (FileNameAnsi == nullptr)
			{
				continue;
			}

			const FString FileName = UTF8_TO_TCHAR(FileNameAnsi);
			// Steam exposes no content hash for cloud files (Hash/HashType stay empty); the
			// download name equals the logical filename.
			EnumeratedFiles.Emplace(FileName, FileName, FileSizeInBytes);
		}

		bEnumeratedFiles = true;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("EnumerateUserFiles: %d cloud file(s)"), EnumeratedFiles.Num());
		TriggerOnEnumerateUserFilesCompleteDelegates(true, UserId);
		return;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("EnumerateUserFiles: Steam Remote Storage unavailable or user %s is not the local Steam user"), *UserId.ToDebugString());
	TriggerOnEnumerateUserFilesCompleteDelegates(false, UserId);
}

void FOnlineUserCloudExtendedSteam::GetUserFileList(const FUniqueNetId& UserId, TArray<FCloudFileHeader>& UserFiles)
{
	// Empty until EnumerateUserFiles has completed successfully (interface contract: this returns
	// "the list that was returned by the network store", never a fresh query).
	if (IsLocalSteamUser(UserId) && bEnumeratedFiles)
	{
		UserFiles = EnumeratedFiles;
	}
	else
	{
		UserFiles.Reset();
	}
}

bool FOnlineUserCloudExtendedSteam::ReadUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	// Synchronous read: Steam Cloud files are mirrored to the local disk by the Steam client, so
	// FileRead is a local-disk read; FileReadAsync would add latency and callback plumbing for no
	// benefit at save-game sizes. The completion delegate fires before this call returns.
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId) && !FileName.IsEmpty())
	{
		FCloudFile& CachedFile = FileCache.FindOrAdd(MakeCacheKey(FileName), FCloudFile(FileName));
		CachedFile.AsyncState = EOnlineAsyncTaskState::InProgress;
		CachedFile.Data.Reset();

		const FTCHARToUTF8 FileNameUtf8(*FileName);
		const int32 FileSize = RemoteStorage->GetFileSize(FileNameUtf8.Get());

		bool bSuccess = false;
		if (FileSize > 0)
		{
			CachedFile.Data.SetNumUninitialized(FileSize);
			const int32 BytesRead = RemoteStorage->FileRead(FileNameUtf8.Get(), CachedFile.Data.GetData(), FileSize);
			bSuccess = BytesRead == FileSize;
		}

		if (!bSuccess)
		{
			CachedFile.Data.Reset();
			UE_LOG(LogExtendedSteam, Warning, TEXT("ReadUserFile: failed to read '%s' (size %d)"), *FileName, FileSize);
		}
		CachedFile.AsyncState = bSuccess ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;

		TriggerOnReadUserFileCompleteDelegates(bSuccess, UserId, FileName);
		return bSuccess;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("ReadUserFile: cannot read '%s' (Steam unavailable, bad filename, or user %s is not the local Steam user)"), *FileName, *UserId.ToDebugString());
	TriggerOnReadUserFileCompleteDelegates(false, UserId, FileName);
	return false;
}

bool FOnlineUserCloudExtendedSteam::WriteUserFile(const FUniqueNetId& UserId, const FString& FileName, TArray<uint8>& FileContents, bool bCompressBeforeUpload)
{
	// bCompressBeforeUpload is deliberately ignored: Steam owns the cloud transport (and applies
	// its own wire compression); the OSS contract allows platforms not to implement this flag.
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId) && !FileName.IsEmpty()
		&& FileContents.Num() > 0 && static_cast<uint32>(FileContents.Num()) <= k_unMaxCloudFileChunkSize)
	{
		// Synchronous local-disk write; Steam uploads in the background afterwards. The completion
		// delegate fires before this call returns.
		const bool bSuccess = RemoteStorage->FileWrite(TCHAR_TO_UTF8(*FileName), FileContents.GetData(), FileContents.Num());
		if (bSuccess)
		{
			FCloudFile& CachedFile = FileCache.FindOrAdd(MakeCacheKey(FileName), FCloudFile(FileName));
			CachedFile.Data = FileContents;
			CachedFile.AsyncState = EOnlineAsyncTaskState::Done;

			// Keep the enumerated metadata coherent without forcing a re-enumeration.
			if (bEnumeratedFiles)
			{
				FCloudFileHeader* Header = EnumeratedFiles.FindByPredicate([&FileName](const FCloudFileHeader& Candidate)
				{
					return Candidate.FileName.Equals(FileName, ESearchCase::IgnoreCase);
				});
				if (Header != nullptr)
				{
					Header->FileSize = FileContents.Num();
				}
				else
				{
					EnumeratedFiles.Emplace(FileName, FileName, FileContents.Num());
				}
			}
		}
		else
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("WriteUserFile: FileWrite failed for '%s' (%d bytes). Is Steam Cloud enabled for the app and account?"), *FileName, FileContents.Num());
		}

		TriggerOnWriteUserFileCompleteDelegates(bSuccess, UserId, FileName);
		return bSuccess;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("WriteUserFile: cannot write '%s' (%d bytes) (Steam unavailable, bad filename/size, or user %s is not the local Steam user)"),
		*FileName, FileContents.Num(), *UserId.ToDebugString());
	TriggerOnWriteUserFileCompleteDelegates(false, UserId, FileName);
	return false;
}

void FOnlineUserCloudExtendedSteam::CancelWriteUserFile(const FUniqueNetId& UserId, const FString& FileName)
{
	// Writes are synchronous (see WriteUserFile): by the time anyone could ask for a cancel the
	// write has already completed, so there is never anything to cancel.
	UE_LOG(LogExtendedSteam, Verbose, TEXT("CancelWriteUserFile: writes are synchronous; nothing to cancel for '%s'"), *FileName);
	TriggerOnWriteUserFileCanceledDelegates(false, UserId, FileName);
}

bool FOnlineUserCloudExtendedSteam::DeleteUserFile(const FUniqueNetId& UserId, const FString& FileName, bool bShouldCloudDelete, bool bShouldLocallyDelete)
{
	// Flag mapping onto the two Steam primitives (verified against isteamremotestorage.h / Steam
	// Cloud docs: FileDelete removes the file locally AND propagates the delete to the cloud;
	// FileForget removes the file from cloud sync/quota but leaves the local copy on disk):
	//  - cloud + local  -> FileDelete
	//  - cloud only     -> FileForget
	//  - local only     -> unsupported (Steam has no "delete local, keep cloud" primitive; the
	//                      client would just re-sync the file anyway) -> failure
	//  - neither        -> nothing requested -> failure
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId) && !FileName.IsEmpty() && bShouldCloudDelete)
	{
		const FTCHARToUTF8 FileNameUtf8(*FileName);
		const bool bSuccess = bShouldLocallyDelete
			? RemoteStorage->FileDelete(FileNameUtf8.Get())
			: RemoteStorage->FileForget(FileNameUtf8.Get());

		if (bSuccess)
		{
			// Either way the file is gone from the cloud: drop it from both caches.
			FileCache.Remove(MakeCacheKey(FileName));
			EnumeratedFiles.RemoveAll([&FileName](const FCloudFileHeader& Candidate)
			{
				return Candidate.FileName.Equals(FileName, ESearchCase::IgnoreCase);
			});
		}
		else
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteUserFile: %s failed for '%s'"),
				bShouldLocallyDelete ? TEXT("FileDelete") : TEXT("FileForget"), *FileName);
		}

		TriggerOnDeleteUserFileCompleteDelegates(bSuccess, UserId, FileName);
		return bSuccess;
	}
#endif

	UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteUserFile: unsupported request for '%s' (cloud=%d local=%d) or Steam unavailable / user %s not local"),
		*FileName, bShouldCloudDelete ? 1 : 0, bShouldLocallyDelete ? 1 : 0, *UserId.ToDebugString());
	TriggerOnDeleteUserFileCompleteDelegates(false, UserId, FileName);
	return false;
}

bool FOnlineUserCloudExtendedSteam::RequestUsageInfo(const FUniqueNetId& UserId)
{
#if WITH_EXTENDEDSTEAM_SDK
	ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage();
	if (RemoteStorage != nullptr && IsLocalSteamUser(UserId))
	{
		uint64 TotalBytes = 0;
		uint64 AvailableBytes = 0;
		if (RemoteStorage->GetQuota(&TotalBytes, &AvailableBytes))
		{
			const int64 BytesUsed = static_cast<int64>(TotalBytes - AvailableBytes);
			TriggerOnRequestUsageInfoCompleteDelegates(true, UserId, BytesUsed, TOptional<int64>(static_cast<int64>(TotalBytes)));
			return true;
		}
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestUsageInfo: GetQuota failed"));
	}
#endif

	TriggerOnRequestUsageInfoCompleteDelegates(false, UserId, 0, TOptional<int64>());
	return false;
}

void FOnlineUserCloudExtendedSteam::DumpCloudState(const FUniqueNetId& UserId)
{
	UE_LOG(LogExtendedSteam, Log, TEXT("Steam user cloud state for %s:"), *UserId.ToDebugString());
	UE_LOG(LogExtendedSteam, Log, TEXT("  Local Steam user: %s"), IsLocalSteamUser(UserId) ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogExtendedSteam, Log, TEXT("  Enumerated: %s (%d file(s)), cached payloads: %d"),
		bEnumeratedFiles ? TEXT("yes") : TEXT("no"), EnumeratedFiles.Num(), FileCache.Num());

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage())
	{
		uint64 TotalBytes = 0;
		uint64 AvailableBytes = 0;
		if (RemoteStorage->GetQuota(&TotalBytes, &AvailableBytes))
		{
			UE_LOG(LogExtendedSteam, Log, TEXT("  Quota: %llu / %llu bytes used"), TotalBytes - AvailableBytes, TotalBytes);
		}
		UE_LOG(LogExtendedSteam, Log, TEXT("  Cloud enabled: app=%s account=%s"),
			RemoteStorage->IsCloudEnabledForApp() ? TEXT("yes") : TEXT("no"),
			RemoteStorage->IsCloudEnabledForAccount() ? TEXT("yes") : TEXT("no"));
	}
	else
#endif
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("  Steam Remote Storage unavailable"));
	}
}

void FOnlineUserCloudExtendedSteam::DumpCloudFileState(const FUniqueNetId& UserId, const FString& FileName)
{
	UE_LOG(LogExtendedSteam, Log, TEXT("Steam user cloud file state for '%s' (user %s):"), *FileName, *UserId.ToDebugString());

	const FCloudFile* CachedFile = FileCache.Find(MakeCacheKey(FileName));
	UE_LOG(LogExtendedSteam, Log, TEXT("  Cached: %s (state %s, %d bytes)"),
		CachedFile != nullptr ? TEXT("yes") : TEXT("no"),
		CachedFile != nullptr ? EOnlineAsyncTaskState::ToString(CachedFile->AsyncState) : TEXT("n/a"),
		CachedFile != nullptr ? CachedFile->Data.Num() : 0);

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamRemoteStorage* RemoteStorage = ExtendedSteamUserCloud::GetRemoteStorage())
	{
		const FTCHARToUTF8 FileNameUtf8(*FileName);
		UE_LOG(LogExtendedSteam, Log, TEXT("  Exists: %s, persisted: %s, size: %d, timestamp: %lld"),
			RemoteStorage->FileExists(FileNameUtf8.Get()) ? TEXT("yes") : TEXT("no"),
			RemoteStorage->FilePersisted(FileNameUtf8.Get()) ? TEXT("yes") : TEXT("no"),
			RemoteStorage->GetFileSize(FileNameUtf8.Get()),
			RemoteStorage->GetFileTimestamp(FileNameUtf8.Get()));
	}
	else
#endif
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("  Steam Remote Storage unavailable"));
	}
}

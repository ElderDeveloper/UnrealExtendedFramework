// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/RemoteStorage/ESteamRemoteStorageAsyncActions.h"

// ---- WriteCloudFileAsync ----

USteamAsyncWriteCloudFile* USteamAsyncWriteCloudFile::WriteCloudFileAsync(UObject* WorldContext, const FString& FileName, const TArray<uint8>& Data, float Timeout)
{
	USteamAsyncWriteCloudFile* Action = NewObject<USteamAsyncWriteCloudFile>();
	Action->FileName = FileName;
	Action->Data = Data;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncWriteCloudFile::ExecuteAction()
{
	UESteamRemoteStorageSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamRemoteStorageSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileWriteComplete.AddDynamic(this, &USteamAsyncWriteCloudFile::HandleWriteComplete);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->FileWriteAsync(FileName, Data); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncWriteCloudFile::HandleWriteComplete(bool bSuccess, const FString& InFileName)
{
	// Same-type writes are serialized and the delegate carries the filename; ignore completions for
	// other files (and other nodes' writes of the same file) so concurrent write nodes do not
	// cross-complete.
	if (!IsResultForThisCall() || !InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncWriteCloudFile::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FileName);
}

void USteamAsyncWriteCloudFile::CancelWork()
{
	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileWriteComplete.RemoveDynamic(this, &USteamAsyncWriteCloudFile::HandleWriteComplete);
	}
}

void USteamAsyncWriteCloudFile::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName);
	}
	else
	{
		OnFailure.Broadcast(FileName);
	}
}

// ---- ReadCloudFileAsync ----

USteamAsyncReadCloudFile* USteamAsyncReadCloudFile::ReadCloudFileAsync(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncReadCloudFile* Action = NewObject<USteamAsyncReadCloudFile>();
	Action->FileName = FileName;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncReadCloudFile::ExecuteAction()
{
	UESteamRemoteStorageSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamRemoteStorageSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileReadComplete.AddDynamic(this, &USteamAsyncReadCloudFile::HandleReadComplete);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->FileReadAsync(FileName); });
	if (!bStarted)
	{
		Complete(false, TArray<uint8>());
	}
}

void USteamAsyncReadCloudFile::HandleReadComplete(bool bSuccess, const FString& InFileName, const TArray<uint8>& InData)
{
	// Ignore completions for other files (same-type reads are serialized, delegate carries the name).
	if (!IsResultForThisCall() || !InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess, InData);
}

void USteamAsyncReadCloudFile::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FileName, TArray<uint8>());
}

void USteamAsyncReadCloudFile::CancelWork()
{
	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileReadComplete.RemoveDynamic(this, &USteamAsyncReadCloudFile::HandleReadComplete);
	}
}

void USteamAsyncReadCloudFile::Complete(bool bSuccess, const TArray<uint8>& InData)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName, InData);
	}
	else
	{
		OnFailure.Broadcast(FileName, InData);
	}
}

// ---- ShareCloudFile ----

USteamAsyncShareCloudFile* USteamAsyncShareCloudFile::ShareCloudFile(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncShareCloudFile* Action = NewObject<USteamAsyncShareCloudFile>();
	Action->FileName = FileName;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncShareCloudFile::ExecuteAction()
{
	UESteamRemoteStorageSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamRemoteStorageSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileShared.AddDynamic(this, &USteamAsyncShareCloudFile::HandleShared);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->FileShare(FileName); });
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncShareCloudFile::HandleShared(bool bSuccess, const FString& InFileName, int64 InUGCHandle)
{
	// Ignore completions for other files (same-type shares are serialized, delegate carries the name).
	if (!IsResultForThisCall() || !InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess, InUGCHandle);
}

void USteamAsyncShareCloudFile::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(FileName, 0);
}

void USteamAsyncShareCloudFile::CancelWork()
{
	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileShared.RemoveDynamic(this, &USteamAsyncShareCloudFile::HandleShared);
	}
}

void USteamAsyncShareCloudFile::Complete(bool bSuccess, int64 InUGCHandle)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName, InUGCHandle);
	}
	else
	{
		OnFailure.Broadcast(FileName, InUGCHandle);
	}
}

// ---- DownloadUGC ----

USteamAsyncDownloadUGC* USteamAsyncDownloadUGC::DownloadUGC(UObject* WorldContext, int64 UGCHandle, int32 Priority, float Timeout)
{
	USteamAsyncDownloadUGC* Action = NewObject<USteamAsyncDownloadUGC>();
	Action->UGCHandle = UGCHandle;
	Action->Priority = Priority;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncDownloadUGC::ExecuteAction()
{
	UESteamRemoteStorageSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamRemoteStorageSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnUGCDownloaded.AddDynamic(this, &USteamAsyncDownloadUGC::HandleDownloaded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->UGCDownload(UGCHandle, Priority); });
	if (!bStarted)
	{
		Complete(false, FString(), 0);
	}
}

void USteamAsyncDownloadUGC::HandleDownloaded(bool bSuccess, int64 InUGCHandle, const FString& InFileName, int32 InFileSize)
{
	// Ignore completions for other handles (downloads are serialized, delegate echoes the handle).
	if (!IsResultForThisCall() || InUGCHandle != UGCHandle)
	{
		return;
	}
	Complete(bSuccess, InFileName, InFileSize);
}

void USteamAsyncDownloadUGC::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(UGCHandle, FString(), 0);
}

void USteamAsyncDownloadUGC::CancelWork()
{
	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnUGCDownloaded.RemoveDynamic(this, &USteamAsyncDownloadUGC::HandleDownloaded);
	}
}

void USteamAsyncDownloadUGC::Complete(bool bSuccess, const FString& InFileName, int32 InFileSize)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(UGCHandle, InFileName, InFileSize);
	}
	else
	{
		OnFailure.Broadcast(UGCHandle, InFileName, InFileSize);
	}
}

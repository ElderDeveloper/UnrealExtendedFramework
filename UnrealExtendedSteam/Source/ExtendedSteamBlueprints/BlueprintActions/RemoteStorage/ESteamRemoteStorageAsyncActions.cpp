// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/RemoteStorage/ESteamRemoteStorageAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace ESteamRemoteStorageAsyncActionHelpers
{
	UESteamRemoteStorageSubsystem* GetStorageSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamRemoteStorageSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- WriteCloudFileAsync ----

USteamAsyncWriteCloudFile* USteamAsyncWriteCloudFile::WriteCloudFileAsync(UObject* WorldContext, const FString& FileName, const TArray<uint8>& Data, float Timeout)
{
	USteamAsyncWriteCloudFile* Action = NewObject<USteamAsyncWriteCloudFile>();
	Action->WorldContextObject = WorldContext;
	Action->FileName = FileName;
	Action->Data = Data;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncWriteCloudFile::Activate()
{
	UESteamRemoteStorageSubsystem* Subsystem = ESteamRemoteStorageAsyncActionHelpers::GetStorageSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false);
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileWriteComplete.AddDynamic(this, &USteamAsyncWriteCloudFile::HandleWriteComplete);

	if (!Subsystem->FileWriteAsync(FileName, Data))
	{
		Complete(false);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncWriteCloudFile::HandleWriteComplete(bool bSuccess, const FString& InFileName)
{
	// Same-type writes are serialized and the delegate carries the filename; ignore completions for
	// other files so concurrent write nodes do not cross-complete.
	if (!InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncWriteCloudFile::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncWriteCloudFile::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileWriteComplete.RemoveDynamic(this, &USteamAsyncWriteCloudFile::HandleWriteComplete);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName);
	}
	else
	{
		OnFailure.Broadcast(FileName);
	}

	SetReadyToDestroy();
}

// ---- ReadCloudFileAsync ----

USteamAsyncReadCloudFile* USteamAsyncReadCloudFile::ReadCloudFileAsync(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncReadCloudFile* Action = NewObject<USteamAsyncReadCloudFile>();
	Action->WorldContextObject = WorldContext;
	Action->FileName = FileName;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncReadCloudFile::Activate()
{
	UESteamRemoteStorageSubsystem* Subsystem = ESteamRemoteStorageAsyncActionHelpers::GetStorageSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<uint8>());
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileReadComplete.AddDynamic(this, &USteamAsyncReadCloudFile::HandleReadComplete);

	if (!Subsystem->FileReadAsync(FileName))
	{
		Complete(false, TArray<uint8>());
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncReadCloudFile::HandleReadComplete(bool bSuccess, const FString& InFileName, const TArray<uint8>& InData)
{
	// Ignore completions for other files (same-type reads are serialized, delegate carries the name).
	if (!InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess, InData);
}

void USteamAsyncReadCloudFile::OnTimeoutFailure()
{
	Complete(false, TArray<uint8>());
}

void USteamAsyncReadCloudFile::Complete(bool bSuccess, const TArray<uint8>& InData)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileReadComplete.RemoveDynamic(this, &USteamAsyncReadCloudFile::HandleReadComplete);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName, InData);
	}
	else
	{
		OnFailure.Broadcast(FileName, InData);
	}

	SetReadyToDestroy();
}

// ---- ShareCloudFile ----

USteamAsyncShareCloudFile* USteamAsyncShareCloudFile::ShareCloudFile(UObject* WorldContext, const FString& FileName, float Timeout)
{
	USteamAsyncShareCloudFile* Action = NewObject<USteamAsyncShareCloudFile>();
	Action->WorldContextObject = WorldContext;
	Action->FileName = FileName;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncShareCloudFile::Activate()
{
	UESteamRemoteStorageSubsystem* Subsystem = ESteamRemoteStorageAsyncActionHelpers::GetStorageSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, 0);
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnFileShared.AddDynamic(this, &USteamAsyncShareCloudFile::HandleShared);

	if (!Subsystem->FileShare(FileName))
	{
		Complete(false, 0);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncShareCloudFile::HandleShared(bool bSuccess, const FString& InFileName, int64 InUGCHandle)
{
	// Ignore completions for other files (same-type shares are serialized, delegate carries the name).
	if (!InFileName.Equals(FileName, ESearchCase::IgnoreCase))
	{
		return;
	}
	Complete(bSuccess, InUGCHandle);
}

void USteamAsyncShareCloudFile::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncShareCloudFile::Complete(bool bSuccess, int64 InUGCHandle)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnFileShared.RemoveDynamic(this, &USteamAsyncShareCloudFile::HandleShared);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(FileName, InUGCHandle);
	}
	else
	{
		OnFailure.Broadcast(FileName, InUGCHandle);
	}

	SetReadyToDestroy();
}

// ---- DownloadUGC ----

USteamAsyncDownloadUGC* USteamAsyncDownloadUGC::DownloadUGC(UObject* WorldContext, int64 UGCHandle, int32 Priority, float Timeout)
{
	USteamAsyncDownloadUGC* Action = NewObject<USteamAsyncDownloadUGC>();
	Action->WorldContextObject = WorldContext;
	Action->UGCHandle = UGCHandle;
	Action->Priority = Priority;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncDownloadUGC::Activate()
{
	UESteamRemoteStorageSubsystem* Subsystem = ESteamRemoteStorageAsyncActionHelpers::GetStorageSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, FString(), 0);
		return;
	}

	StorageSubsystem = Subsystem;
	Subsystem->OnUGCDownloaded.AddDynamic(this, &USteamAsyncDownloadUGC::HandleDownloaded);

	if (!Subsystem->UGCDownload(UGCHandle, Priority))
	{
		Complete(false, FString(), 0);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncDownloadUGC::HandleDownloaded(bool bSuccess, int64 InUGCHandle, const FString& InFileName, int32 InFileSize)
{
	// Ignore completions for other handles (downloads are serialized, delegate echoes the handle).
	if (InUGCHandle != UGCHandle)
	{
		return;
	}
	Complete(bSuccess, InFileName, InFileSize);
}

void USteamAsyncDownloadUGC::OnTimeoutFailure()
{
	Complete(false, FString(), 0);
}

void USteamAsyncDownloadUGC::Complete(bool bSuccess, const FString& InFileName, int32 InFileSize)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamRemoteStorageSubsystem* Subsystem = StorageSubsystem.Get())
	{
		Subsystem->OnUGCDownloaded.RemoveDynamic(this, &USteamAsyncDownloadUGC::HandleDownloaded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(UGCHandle, InFileName, InFileSize);
	}
	else
	{
		OnFailure.Broadcast(UGCHandle, InFileName, InFileSize);
	}

	SetReadyToDestroy();
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSPlayerStorageSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UnrealExtendedEOS.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"

void UEEOSPlayerStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSPlayerStorageSubsystem::Deinitialize()
{
	// Unbind cloud delegates (bound once on first use, kept until now)
	if (IsEOSAvailable())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
			if (CloudInterface.IsValid())
			{
				CloudInterface->ClearOnWriteUserFileCompleteDelegates(this);
				CloudInterface->ClearOnReadUserFileCompleteDelegates(this);
				CloudInterface->ClearOnEnumerateUserFilesCompleteDelegates(this);
				CloudInterface->ClearOnDeleteUserFileCompleteDelegates(this);
			}
		}
	}

	bCloudDelegatesBound = false;
	PendingWriteFiles.Empty();
	PendingReadFiles.Empty();
	PendingDeleteFiles.Empty();
	PendingSaveGameReads.Empty();
	bEnumerateInFlight = false;
	CachedFileList.Empty();
	Super::Deinitialize();
}

void UEEOSPlayerStorageSubsystem::EnsureCloudDelegatesBound(IOnlineUserCloud& CloudInterface)
{
	if (bCloudDelegatesBound)
	{
		return;
	}

	// Bind the interface-wide delegate lists exactly once; completions are correlated
	// to in-flight operations through the pending-file sets, so concurrent transfers
	// of different files each receive their own result.
	CloudInterface.OnWriteUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleWriteUserFileComplete);
	CloudInterface.OnReadUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleReadUserFileComplete);
	CloudInterface.OnEnumerateUserFilesCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleEnumerateUserFilesComplete);
	CloudInterface.OnDeleteUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleDeleteUserFileComplete);
	bCloudDelegatesBound = true;
}

bool UEEOSPlayerStorageSubsystem::WritePlayerData(const FString& FileName, const TArray<uint8>& Data)
{
	if (PendingWriteFiles.Contains(FileName))
	{
		// Log-only rejection: a failure broadcast here would carry the SAME file name as the
		// legitimate in-flight write and be indistinguishable from its real completion (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — Write already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("WritePlayerData"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — UserCloud interface not available"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return false;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerDataWritten.Broadcast(false, FileName);
		return false;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — No logged-in user"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return false;
	}

	EnsureCloudDelegatesBound(*CloudInterface);
	PendingWriteFiles.Add(FileName);
	TArray<uint8> MutableData = Data;
	if (!CloudInterface->WriteUserFile(*UserId, FileName, MutableData))
	{
		// Failed to start; if the completion delegate fired synchronously it already
		// removed the entry and broadcast, so only report if the file is still pending.
		if (PendingWriteFiles.Remove(FileName) > 0)
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — WriteUserFile failed to start for '%s'"), *FileName);
			OnPlayerDataWritten.Broadcast(false, FileName);
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — Writing %d bytes to '%s'"), Data.Num(), *FileName);
	return true;
}

bool UEEOSPlayerStorageSubsystem::WritePlayerString(const FString& FileName, const FString& Content)
{
	TArray<uint8> Data;
	FTCHARToUTF8 Converter(*Content);
	Data.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	return WritePlayerData(FileName, Data);
}

bool UEEOSPlayerStorageSubsystem::WriteSaveGame(const FString& FileName, USaveGame* SaveGameObject)
{
	if (!SaveGameObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — SaveGameObject is null"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return false;
	}

	TArray<uint8> SaveData;
	if (UGameplayStatics::SaveGameToMemory(SaveGameObject, SaveData))
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — Serialized %d bytes, writing to '%s'"), SaveData.Num(), *FileName);
		return WritePlayerData(FileName, SaveData);
	}

	UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — Failed to serialize SaveGame object"));
	OnPlayerDataWritten.Broadcast(false, FileName);
	return false;
}

bool UEEOSPlayerStorageSubsystem::ReadPlayerData(const FString& FileName)
{
	if (PendingReadFiles.Contains(FileName))
	{
		// Log-only rejection: a failure broadcast here would carry the SAME file name as the
		// legitimate in-flight read and be indistinguishable from its real completion — an
		// in-flight ReadSaveGame('X') would consume a rejected duplicate ReadPlayerData('X')
		// as its own failure and unbind while the real data is still on its way (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — Read already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadPlayerData"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — UserCloud interface not available"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — No logged-in user"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	EnsureCloudDelegatesBound(*CloudInterface);
	PendingReadFiles.Add(FileName);
	if (!CloudInterface->ReadUserFile(*UserId, FileName))
	{
		if (PendingReadFiles.Remove(FileName) > 0)
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — ReadUserFile failed to start for '%s'"), *FileName);
			OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — Reading '%s'"), *FileName);
	return true;
}

bool UEEOSPlayerStorageSubsystem::ReadSaveGame(const FString& FileName)
{
	if (PendingSaveGameReads.Contains(FileName))
	{
		// Log-only rejection — see ReadPlayerData's duplicate guard (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPlayerStorageSubsystem::ReadSaveGame — SaveGame read already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	// Track before starting the read: ReadPlayerData failure paths broadcast
	// synchronously and must route through HandleSaveGameDataRead.
	PendingSaveGameReads.Add(FileName);
	if (!OnPlayerDataRead.IsAlreadyBound(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead))
	{
		OnPlayerDataRead.AddDynamic(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead);
	}

	const bool bStarted = ReadPlayerData(FileName);
	if (!bStarted && PendingSaveGameReads.Remove(FileName) > 0)
	{
		// ReadPlayerData was rejected WITHOUT any broadcast (a raw read of the same file is
		// in flight) — a synchronous pre-flight failure would already have routed through
		// HandleSaveGameDataRead and removed the entry. Undo our tracking silently.
		if (PendingSaveGameReads.Num() == 0)
		{
			OnPlayerDataRead.RemoveDynamic(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead);
		}
	}
	return bStarted;
}

bool UEEOSPlayerStorageSubsystem::DeletePlayerData(const FString& FileName)
{
	if (PendingDeleteFiles.Contains(FileName))
	{
		// Log-only rejection — see ReadPlayerData's duplicate guard (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — Delete already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeletePlayerData"));
		OnPlayerDataDeleted.Broadcast(false, FileName);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — UserCloud interface not available"));
		OnPlayerDataDeleted.Broadcast(false, FileName);
		return false;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerDataDeleted.Broadcast(false, FileName);
		return false;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — No logged-in user"));
		OnPlayerDataDeleted.Broadcast(false, FileName);
		return false;
	}

	EnsureCloudDelegatesBound(*CloudInterface);
	PendingDeleteFiles.Add(FileName);
	if (!CloudInterface->DeleteUserFile(*UserId, FileName, true, true))
	{
		if (PendingDeleteFiles.Remove(FileName) > 0)
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — DeleteUserFile failed to start for '%s'"), *FileName);
			OnPlayerDataDeleted.Broadcast(false, FileName);
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — Deleting '%s'"), *FileName);
	return true;
}

bool UEEOSPlayerStorageSubsystem::QueryPlayerFiles()
{
	if (bEnumerateInFlight)
	{
		// Coalesce: the in-flight enumeration's completion broadcasts to all listeners,
		// so this caller still receives exactly one OnPlayerFilesQueried.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::QueryPlayerFiles — Enumeration already in flight; its completion will serve this request"));
		return true;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryPlayerFiles"));
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	EnsureCloudDelegatesBound(*CloudInterface);
	bEnumerateInFlight = true;
	CloudInterface->EnumerateUserFiles(*UserId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::QueryPlayerFiles — Enumerating player files..."));
	return true;
}

TArray<FString> UEEOSPlayerStorageSubsystem::GetPlayerFileList() const
{
	return CachedFileList;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSPlayerStorageSubsystem::HandleWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
	if (PendingWriteFiles.Remove(FileName) == 0)
	{
		// Not an operation we started (or already reported) — ignore
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Write '%s' %s"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnPlayerDataWritten.Broadcast(bWasSuccessful, FileName);
}

void UEEOSPlayerStorageSubsystem::HandleReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
	if (PendingReadFiles.Remove(FileName) == 0)
	{
		// Not an operation we started (or already reported) — ignore
		return;
	}

	TArray<uint8> FileData;

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
			{
				Cloud->GetFileContents(UserId, FileName, FileData);
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Read '%s' %s (%d bytes)"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), FileData.Num());
	OnPlayerDataRead.Broadcast(bWasSuccessful, FileName, FileData);
}

void UEEOSPlayerStorageSubsystem::HandleEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& UserId)
{
	if (!bEnumerateInFlight)
	{
		// Not an operation we started — ignore
		return;
	}
	bEnumerateInFlight = false;

	CachedFileList.Empty();

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
			{
				TArray<FCloudFileHeader> Headers;
				Cloud->GetUserFileList(UserId, Headers);
				for (const auto& Header : Headers)
				{
					CachedFileList.Add(Header.FileName);
				}
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Enumerated %d player files"), CachedFileList.Num());
	OnPlayerFilesQueried.Broadcast(CachedFileList);
}

void UEEOSPlayerStorageSubsystem::HandleDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
	if (PendingDeleteFiles.Remove(FileName) == 0)
	{
		// Not an operation we started (or already reported) — ignore
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Delete '%s' %s"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnPlayerDataDeleted.Broadcast(bWasSuccessful, FileName);
}

void UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	if (PendingSaveGameReads.Remove(FileName) == 0) return;

	if (PendingSaveGameReads.Num() == 0)
	{
		OnPlayerDataRead.RemoveDynamic(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead);
	}

	if (bSuccess && Data.Num() > 0)
	{
		USaveGame* LoadedGame = UGameplayStatics::LoadGameFromMemory(Data);
		if (LoadedGame)
		{
			UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Deserialized SaveGame from '%s'"), *FileName);
			OnSaveGameRead.Broadcast(true, FileName, LoadedGame);
		}
		else
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem: Failed to deserialize SaveGame from '%s'"), *FileName);
			OnSaveGameRead.Broadcast(false, FileName, nullptr);
		}
	}
	else
	{
		OnSaveGameRead.Broadcast(false, FileName, nullptr);
	}
}

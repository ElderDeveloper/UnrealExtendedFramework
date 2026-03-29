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
	// Unbind cloud delegates
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

	CachedFileList.Empty();
	Super::Deinitialize();
}

void UEEOSPlayerStorageSubsystem::WritePlayerData(const FString& FileName, const TArray<uint8>& Data)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("WritePlayerData"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — UserCloud interface not available"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerDataWritten.Broadcast(false, FileName);
		return;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — No logged-in user"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return;
	}

	CloudInterface->ClearOnWriteUserFileCompleteDelegates(this);
	CloudInterface->OnWriteUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleWriteUserFileComplete);
	TArray<uint8> MutableData = Data;
	CloudInterface->WriteUserFile(*UserId, FileName, MutableData);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::WritePlayerData — Writing %d bytes to '%s'"), Data.Num(), *FileName);
}

void UEEOSPlayerStorageSubsystem::WritePlayerString(const FString& FileName, const FString& Content)
{
	TArray<uint8> Data;
	FTCHARToUTF8 Converter(*Content);
	Data.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	WritePlayerData(FileName, Data);
}

void UEEOSPlayerStorageSubsystem::WriteSaveGame(const FString& FileName, USaveGame* SaveGameObject)
{
	if (!SaveGameObject)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — SaveGameObject is null"));
		OnPlayerDataWritten.Broadcast(false, FileName);
		return;
	}

	TArray<uint8> SaveData;
	if (UGameplayStatics::SaveGameToMemory(SaveGameObject, SaveData))
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — Serialized %d bytes, writing to '%s'"), SaveData.Num(), *FileName);
		WritePlayerData(FileName, SaveData);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::WriteSaveGame — Failed to serialize SaveGame object"));
		OnPlayerDataWritten.Broadcast(false, FileName);
	}
}

void UEEOSPlayerStorageSubsystem::ReadPlayerData(const FString& FileName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadPlayerData"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — UserCloud interface not available"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — No logged-in user"));
		OnPlayerDataRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	CloudInterface->ClearOnReadUserFileCompleteDelegates(this);
	CloudInterface->OnReadUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleReadUserFileComplete);
	CloudInterface->ReadUserFile(*UserId, FileName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::ReadPlayerData — Reading '%s'"), *FileName);
}

void UEEOSPlayerStorageSubsystem::ReadSaveGame(const FString& FileName)
{
	bPendingSaveGameRead = true;
	PendingSaveGameFileName = FileName;
	if (!OnPlayerDataRead.IsAlreadyBound(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead))
	{
		OnPlayerDataRead.AddDynamic(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead);
	}
	ReadPlayerData(FileName);
}

void UEEOSPlayerStorageSubsystem::DeletePlayerData(const FString& FileName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeletePlayerData"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid()) return;

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return;

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return;

	CloudInterface->ClearOnDeleteUserFileCompleteDelegates(this);
	CloudInterface->OnDeleteUserFileCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleDeleteUserFileComplete);
	CloudInterface->DeleteUserFile(*UserId, FileName, true, true);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::DeletePlayerData — Deleting '%s'"), *FileName);
}

void UEEOSPlayerStorageSubsystem::QueryPlayerFiles()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryPlayerFiles"));
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineUserCloudPtr CloudInterface = EOSSub->GetUserCloudInterface();
	if (!CloudInterface.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (!UserId.IsValid())
	{
		OnPlayerFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	CloudInterface->ClearOnEnumerateUserFilesCompleteDelegates(this);
	CloudInterface->OnEnumerateUserFilesCompleteDelegates.AddUObject(this, &UEEOSPlayerStorageSubsystem::HandleEnumerateUserFilesComplete);
	CloudInterface->EnumerateUserFiles(*UserId);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem::QueryPlayerFiles — Enumerating player files..."));
}

TArray<FString> UEEOSPlayerStorageSubsystem::GetPlayerFileList() const
{
	return CachedFileList;
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSPlayerStorageSubsystem::HandleWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Write '%s' %s"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
	OnPlayerDataWritten.Broadcast(bWasSuccessful, FileName);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
		{
			Cloud->ClearOnWriteUserFileCompleteDelegates(this);
		}
	}
}

void UEEOSPlayerStorageSubsystem::HandleReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
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

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
		{
			Cloud->ClearOnReadUserFileCompleteDelegates(this);
		}
	}
}

void UEEOSPlayerStorageSubsystem::HandleEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& UserId)
{
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

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
		{
			Cloud->ClearOnEnumerateUserFilesCompleteDelegates(this);
		}
	}
}

void UEEOSPlayerStorageSubsystem::HandleDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& UserId, const FString& FileName)
{
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSPlayerStorageSubsystem: Delete '%s' %s"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineUserCloudPtr Cloud = EOSSub->GetUserCloudInterface())
		{
			Cloud->ClearOnDeleteUserFileCompleteDelegates(this);
		}
	}
}

void UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	if (!bPendingSaveGameRead || FileName != PendingSaveGameFileName) return;

	bPendingSaveGameRead = false;
	OnPlayerDataRead.RemoveDynamic(this, &UEEOSPlayerStorageSubsystem::HandleSaveGameDataRead);

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

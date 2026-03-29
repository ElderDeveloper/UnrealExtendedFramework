// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSTitleStorageSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UnrealExtendedEOS.h"

void UEEOSTitleStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSTitleStorageSubsystem::Deinitialize()
{
	if (IsEOSAvailable())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
			if (TitleFileInterface.IsValid())
			{
				TitleFileInterface->ClearOnEnumerateFilesCompleteDelegates(this);
				TitleFileInterface->ClearOnReadFileCompleteDelegates(this);
			}
		}
	}
	CachedTitleFiles.Empty();
	Super::Deinitialize();
}

void UEEOSTitleStorageSubsystem::ReadTitleFile(const FString& FileName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadTitleFile"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
	if (!TitleFileInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — TitleFile interface not available"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		return;
	}

	TitleFileInterface->AddOnReadFileCompleteDelegate_Handle(
		FOnReadFileCompleteDelegate::CreateUObject(this, &UEEOSTitleStorageSubsystem::HandleReadTitleFileComplete));

	TitleFileInterface->ReadFile(FileName);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — Reading '%s'"), *FileName);
}

void UEEOSTitleStorageSubsystem::ReadTitleFileAsString(const FString& FileName)
{
	// Same as ReadTitleFile — caller converts bytes via BPL BytesToString
	ReadTitleFile(FileName);
}

void UEEOSTitleStorageSubsystem::QueryTitleFiles()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryTitleFiles"));
		OnTitleFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
	if (!TitleFileInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — TitleFile interface not available"));
		OnTitleFilesQueried.Broadcast(TArray<FString>());
		return;
	}

	TitleFileInterface->AddOnEnumerateFilesCompleteDelegate_Handle(
		FOnEnumerateFilesCompleteDelegate::CreateUObject(this, &UEEOSTitleStorageSubsystem::HandleEnumerateTitleFilesComplete));

	TitleFileInterface->EnumerateFiles();
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — Enumerating title files..."));
}

TArray<FString> UEEOSTitleStorageSubsystem::GetTitleFileList() const
{
	return CachedTitleFiles;
}

bool UEEOSTitleStorageSubsystem::HasTitleFile(const FString& FileName) const
{
	return CachedTitleFiles.Contains(FileName);
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void UEEOSTitleStorageSubsystem::HandleEnumerateTitleFilesComplete(bool bWasSuccessful, const FString& Error)
{
	CachedTitleFiles.Empty();

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
			if (TitleFileInterface.IsValid())
			{
				TArray<FCloudFileHeader> Headers;
				TitleFileInterface->GetFileList(Headers);
				for (const auto& Header : Headers)
				{
					CachedTitleFiles.Add(Header.FileName);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSTitleStorageSubsystem: Enumerate title files failed — %s"), *Error);
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem: Enumerated %d title files"), CachedTitleFiles.Num());
	OnTitleFilesQueried.Broadcast(CachedTitleFiles);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineTitleFilePtr TF = EOSSub->GetTitleFileInterface())
		{
			TF->ClearOnEnumerateFilesCompleteDelegates(this);
		}
	}
}

void UEEOSTitleStorageSubsystem::HandleReadTitleFileComplete(bool bWasSuccessful, const FString& FileName)
{
	TArray<uint8> FileData;

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
			if (TitleFileInterface.IsValid())
			{
				TitleFileInterface->GetFileContents(FileName, FileData);
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem: Read '%s' %s (%d bytes)"), *FileName, bWasSuccessful ? TEXT("succeeded") : TEXT("failed"), FileData.Num());
	OnTitleFileRead.Broadcast(bWasSuccessful, FileName, FileData);

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineTitleFilePtr TF = EOSSub->GetTitleFileInterface())
		{
			TF->ClearOnReadFileCompleteDelegates(this);
		}
	}
}

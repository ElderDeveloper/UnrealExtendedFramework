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
	bTitleFileDelegatesBound = false;
	PendingReadFiles.Empty();
	PendingAsStringFiles.Empty();
	bEnumerateInFlight = false;
	CachedTitleFiles.Empty();
	Super::Deinitialize();
}

void UEEOSTitleStorageSubsystem::EnsureTitleFileDelegatesBound(IOnlineTitleFile& TitleFileInterface)
{
	if (bTitleFileDelegatesBound)
	{
		return;
	}

	// Bind the interface-wide delegate lists exactly once; completions are correlated
	// to in-flight operations through the pending-file set / in-flight flag, so
	// concurrent transfers of different files each receive their own result.
	TitleFileInterface.AddOnReadFileCompleteDelegate_Handle(
		FOnReadFileCompleteDelegate::CreateUObject(this, &UEEOSTitleStorageSubsystem::HandleReadTitleFileComplete));
	TitleFileInterface.AddOnEnumerateFilesCompleteDelegate_Handle(
		FOnEnumerateFilesCompleteDelegate::CreateUObject(this, &UEEOSTitleStorageSubsystem::HandleEnumerateTitleFilesComplete));
	bTitleFileDelegatesBound = true;
}

bool UEEOSTitleStorageSubsystem::ReadTitleFile(const FString& FileName)
{
	if (PendingReadFiles.Contains(FileName))
	{
		// Log-only rejection: a failure broadcast here would carry the SAME file name as the
		// legitimate in-flight read and be indistinguishable from its real completion (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — Read already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadTitleFile"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
	if (!TitleFileInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — TitleFile interface not available"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		return false;
	}

	EnsureTitleFileDelegatesBound(*TitleFileInterface);
	PendingReadFiles.Add(FileName);
	if (!TitleFileInterface->ReadFile(FileName))
	{
		// Failed to start; if the completion delegate fired synchronously it already
		// removed the entry and broadcast, so only report if the file is still pending.
		if (PendingReadFiles.Remove(FileName) > 0)
		{
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — ReadFile failed to start for '%s'"), *FileName);
			OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::ReadTitleFile — Reading '%s'"), *FileName);
	return true;
}

bool UEEOSTitleStorageSubsystem::ReadTitleFileAsString(const FString& FileName)
{
	if (PendingReadFiles.Contains(FileName))
	{
		// Log-only rejection: a failure broadcast here would carry the SAME file name as the
		// legitimate in-flight read and be indistinguishable from its real completion (R1)
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSTitleStorageSubsystem::ReadTitleFileAsString — Read already in flight for '%s', rejecting duplicate request (no broadcast)"), *FileName);
		return false;
	}

	// Failure paths below fire BOTH delegates (byte-level first) — the dual-fire contract:
	// an as-string read always produces a symmetric event pair, success or failure, so
	// byte-level listeners never see an as-string read vanish.
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadTitleFileAsString"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		OnTitleFileReadAsString.Broadcast(false, FileName, FString());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
	if (!TitleFileInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::ReadTitleFileAsString — TitleFile interface not available"));
		OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
		OnTitleFileReadAsString.Broadcast(false, FileName, FString());
		return false;
	}

	EnsureTitleFileDelegatesBound(*TitleFileInterface);
	PendingReadFiles.Add(FileName);
	PendingAsStringFiles.Add(FileName);
	if (!TitleFileInterface->ReadFile(FileName))
	{
		// Failed to start; if the completion delegate fired synchronously it already
		// removed the entries and broadcast, so only report if the file is still pending.
		if (PendingReadFiles.Remove(FileName) > 0)
		{
			PendingAsStringFiles.Remove(FileName);
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::ReadTitleFileAsString — ReadFile failed to start for '%s'"), *FileName);
			OnTitleFileRead.Broadcast(false, FileName, TArray<uint8>());
			OnTitleFileReadAsString.Broadcast(false, FileName, FString());
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::ReadTitleFileAsString — Reading '%s'"), *FileName);
	return true;
}

bool UEEOSTitleStorageSubsystem::QueryTitleFiles()
{
	if (bEnumerateInFlight)
	{
		// Coalesce: the in-flight enumeration's completion broadcasts to all listeners,
		// so this caller still receives exactly one OnTitleFilesQueried.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — Enumeration already in flight; its completion will serve this request"));
		return true;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("QueryTitleFiles"));
		OnTitleFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineTitleFilePtr TitleFileInterface = EOSSub->GetTitleFileInterface();
	if (!TitleFileInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — TitleFile interface not available"));
		OnTitleFilesQueried.Broadcast(TArray<FString>());
		return false;
	}

	EnsureTitleFileDelegatesBound(*TitleFileInterface);
	bEnumerateInFlight = true;
	if (!TitleFileInterface->EnumerateFiles())
	{
		// Failed to start; if the completion delegate fired synchronously it already
		// reset the flag and broadcast, so only report if still marked in flight.
		if (bEnumerateInFlight)
		{
			bEnumerateInFlight = false;
			UE_LOG(LogExtendedEOS, Error, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — EnumerateFiles failed to start"));
			OnTitleFilesQueried.Broadcast(TArray<FString>());
		}
		return false;
	}
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSTitleStorageSubsystem::QueryTitleFiles — Enumerating title files..."));
	return true;
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
	if (!bEnumerateInFlight)
	{
		// Not an operation we started — ignore
		return;
	}
	bEnumerateInFlight = false;

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
}

void UEEOSTitleStorageSubsystem::HandleReadTitleFileComplete(bool bWasSuccessful, const FString& FileName)
{
	if (PendingReadFiles.Remove(FileName) == 0)
	{
		// Not an operation we started (or already reported) — ignore
		return;
	}
	const bool bRequestedAsString = PendingAsStringFiles.Remove(FileName) > 0;

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

	if (bRequestedAsString)
	{
		FString Content;
		if (bWasSuccessful && FileData.Num() > 0)
		{
			const uint8* Bytes = FileData.GetData();
			int32 NumBytes = FileData.Num();

			// Strip a UTF-8 BOM if present, then decode the payload as UTF-8
			if (NumBytes >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF)
			{
				Bytes += 3;
				NumBytes -= 3;
			}

			const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes), NumBytes);
			Content = FString::ConstructFromPtrSize(Converted.Get(), Converted.Length());
		}
		OnTitleFileReadAsString.Broadcast(bWasSuccessful, FileName, Content);
	}
}

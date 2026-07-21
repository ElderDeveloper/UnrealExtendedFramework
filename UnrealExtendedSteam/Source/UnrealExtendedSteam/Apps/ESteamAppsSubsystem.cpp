// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Apps/ESteamAppsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamAppsCallbacks
{
public:
	explicit FESteamAppsCallbacks(UESteamAppsSubsystem* InOwner)
		: Owner(InOwner)
		, DlcInstalled(this, &FESteamAppsCallbacks::HandleDlcInstalled)
		, NewUrlLaunchParameters(this, &FESteamAppsCallbacks::HandleNewUrlLaunchParameters)
		, TimedTrialStatus(this, &FESteamAppsCallbacks::HandleTimedTrialStatus)
	{
	}

	void TrackFileDetailsRequest(SteamAPICall_t Call)
	{
		FileDetailsResult.Set(Call, this, &FESteamAppsCallbacks::HandleFileDetailsResponse);
	}

	// Single-slot CallResult: a second GetFileDetails before the first completes would cancel it
	// (the first's callback would never fire). The call site checks this and rejects the overlap.
	bool IsFileDetailsBusy() const { return FileDetailsResult.IsActive(); }

private:
	void HandleDlcInstalled(DlcInstalled_t* Data)
	{
		if (UESteamAppsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnDlcInstalled.Broadcast(static_cast<int32>(Data->m_nAppID));
		}
	}

	void HandleNewUrlLaunchParameters(NewUrlLaunchParameters_t* Data)
	{
		if (UESteamAppsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnNewUrlLaunchParameters.Broadcast();
		}
	}

	void HandleTimedTrialStatus(TimedTrialStatus_t* Data)
	{
		if (UESteamAppsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnTimedTrialStatus.Broadcast(
				static_cast<int32>(Data->m_unAppID),
				Data->m_bIsOffline,
				static_cast<int32>(Data->m_unSecondsAllowed),
				static_cast<int32>(Data->m_unSecondsPlayed));
		}
	}

	void HandleFileDetailsResponse(FileDetailsResult_t* Data, bool bIOFailure)
	{
		UESteamAppsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem)
		{
			return;
		}

		const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
		if (!bSuccess)
		{
			Subsystem->OnFileDetails.Broadcast(false, 0, FString(), 0);
			return;
		}
		Subsystem->OnFileDetails.Broadcast(
			true,
			static_cast<int64>(Data->m_ulFileSize),
			BytesToHex(Data->m_FileSHA, sizeof(Data->m_FileSHA)),
			static_cast<int32>(Data->m_unFlags));
	}

	TWeakObjectPtr<UESteamAppsSubsystem> Owner;
	CCallback<FESteamAppsCallbacks, DlcInstalled_t> DlcInstalled;
	CCallback<FESteamAppsCallbacks, NewUrlLaunchParameters_t> NewUrlLaunchParameters;
	CCallback<FESteamAppsCallbacks, TimedTrialStatus_t> TimedTrialStatus;
	CCallResult<FESteamAppsCallbacks, FileDetailsResult_t> FileDetailsResult;
};
#else
class FESteamAppsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamAppsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamAppsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamAppsCallbacks>(this);
	}
#endif
}

void UESteamAppsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamAppsSubsystem::IsSubscribed() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsSubscribed();
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsSubscribedApp(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsSubscribedApp(static_cast<AppId_t>(AppId));
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsSubscribedFromFreeWeekend() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsSubscribedFromFreeWeekend();
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsSubscribedFromFamilySharing() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsSubscribedFromFamilySharing();
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsDlcInstalled(int32 DlcAppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsDlcInstalled(static_cast<AppId_t>(DlcAppId));
#else
	return false;
#endif
}

int32 UESteamAppsSubsystem::GetDlcCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return SteamApps()->GetDLCCount();
	}
#endif
	return 0;
}

bool UESteamAppsSubsystem::GetDlcDataByIndex(int32 Index, FESteamDlcData& OutDlc) const
{
	OutDlc = FESteamDlcData();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamApps())
	{
		return false;
	}

	AppId_t DlcAppId = 0;
	bool bAvailable = false;
	char NameBuffer[256] = {};
	if (!SteamApps()->BGetDLCDataByIndex(Index, &DlcAppId, &bAvailable, NameBuffer, sizeof(NameBuffer)))
	{
		return false;
	}

	OutDlc.AppId = static_cast<int32>(DlcAppId);
	OutDlc.bAvailable = bAvailable;
	OutDlc.Name = UTF8_TO_TCHAR(NameBuffer);
	return true;
#else
	return false;
#endif
}

void UESteamAppsSubsystem::InstallDlc(int32 DlcAppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamApps())
	{
		LogSteamUnavailable(TEXT("InstallDlc"));
		return;
	}
	SteamApps()->InstallDLC(static_cast<AppId_t>(DlcAppId));
#endif
}

void UESteamAppsSubsystem::UninstallDlc(int32 DlcAppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		SteamApps()->UninstallDLC(static_cast<AppId_t>(DlcAppId));
	}
#endif
}

bool UESteamAppsSubsystem::GetDlcDownloadProgress(int32 DlcAppId, int64& OutBytesDownloaded, int64& OutBytesTotal) const
{
	OutBytesDownloaded = 0;
	OutBytesTotal = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamApps())
	{
		return false;
	}

	uint64 Downloaded = 0;
	uint64 Total = 0;
	if (!SteamApps()->GetDlcDownloadProgress(static_cast<AppId_t>(DlcAppId), &Downloaded, &Total))
	{
		return false;
	}
	OutBytesDownloaded = static_cast<int64>(Downloaded);
	OutBytesTotal = static_cast<int64>(Total);
	return true;
#else
	return false;
#endif
}

FString UESteamAppsSubsystem::GetCurrentGameLanguage() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return UTF8_TO_TCHAR(SteamApps()->GetCurrentGameLanguage());
	}
#endif
	return FString();
}

FString UESteamAppsSubsystem::GetAvailableGameLanguages() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return UTF8_TO_TCHAR(SteamApps()->GetAvailableGameLanguages());
	}
#endif
	return FString();
}

bool UESteamAppsSubsystem::IsLowViolence() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsLowViolence();
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsCybercafe() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsCybercafe();
#else
	return false;
#endif
}

bool UESteamAppsSubsystem::IsVACBanned() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsVACBanned();
#else
	return false;
#endif
}

int64 UESteamAppsSubsystem::GetEarliestPurchaseTime(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return static_cast<int64>(SteamApps()->GetEarliestPurchaseUnixTime(static_cast<AppId_t>(AppId)));
	}
#endif
	return 0;
}

bool UESteamAppsSubsystem::IsAppInstalled(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamApps() && SteamApps()->BIsAppInstalled(static_cast<AppId_t>(AppId));
#else
	return false;
#endif
}

FString UESteamAppsSubsystem::GetAppInstallDir(int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		char PathBuffer[1024] = {};
		const uint32 Length = SteamApps()->GetAppInstallDir(static_cast<AppId_t>(AppId), PathBuffer, sizeof(PathBuffer));
		if (Length > 0)
		{
			return UTF8_TO_TCHAR(PathBuffer);
		}
	}
#endif
	return FString();
}

FESteamId UESteamAppsSubsystem::GetAppOwner() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return FESteamId(SteamApps()->GetAppOwner().ConvertToUint64());
	}
#endif
	return FESteamId();
}

FString UESteamAppsSubsystem::GetLaunchQueryParam(const FString& Key) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return UTF8_TO_TCHAR(SteamApps()->GetLaunchQueryParam(TCHAR_TO_UTF8(*Key)));
	}
#endif
	return FString();
}

FString UESteamAppsSubsystem::GetLaunchCommandLine() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		char CommandLineBuffer[1024] = {};
		const int Length = SteamApps()->GetLaunchCommandLine(CommandLineBuffer, sizeof(CommandLineBuffer));
		if (Length > 0)
		{
			return UTF8_TO_TCHAR(CommandLineBuffer);
		}
	}
#endif
	return FString();
}

bool UESteamAppsSubsystem::GetCurrentBetaName(FString& OutBetaName) const
{
	OutBetaName.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		char BetaBuffer[256] = {};
		if (SteamApps()->GetCurrentBetaName(BetaBuffer, sizeof(BetaBuffer)))
		{
			OutBetaName = UTF8_TO_TCHAR(BetaBuffer);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamAppsSubsystem::MarkContentCorrupt(bool bMissingFilesOnly)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return SteamApps()->MarkContentCorrupt(bMissingFilesOnly);
	}
#endif
	return false;
}

int32 UESteamAppsSubsystem::GetAppBuildId() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		return SteamApps()->GetAppBuildId();
	}
#endif
	return 0;
}

void UESteamAppsSubsystem::GetInstalledDepots(int32 AppId, TArray<int32>& OutDepotIds) const
{
	OutDepotIds.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		// GetInstalledDepots fills a caller-provided buffer and returns how many entries it wrote.
		// There is no separate count query, so a completely full buffer means "there may be more":
		// grow and retry until the result no longer fills the buffer (with a sanity cap).
		TArray<DepotId_t> Depots;
		uint32 Capacity = 64;
		uint32 Count = 0;
		for (;;)
		{
			Depots.SetNumUninitialized(static_cast<int32>(Capacity));
			Count = SteamApps()->GetInstalledDepots(static_cast<AppId_t>(AppId), Depots.GetData(), Capacity);
			if (Count < Capacity || Capacity >= 4096)
			{
				break;
			}
			Capacity *= 2;
		}

		OutDepotIds.Reserve(static_cast<int32>(Count));
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			OutDepotIds.Add(static_cast<int32>(Depots[Index]));
		}
	}
#endif
}

bool UESteamAppsSubsystem::SetDlcContext(int32 AppId)
{
#if WITH_EXTENDEDSTEAM_SDK && ESTEAM_SDK_AT_LEAST(154)
	if (IsSteamAvailable() && SteamApps())
	{
		return SteamApps()->SetDlcContext(static_cast<AppId_t>(AppId));
	}
#endif
	return false;
}

bool UESteamAppsSubsystem::IsTimedTrial(int32& OutSecondsAllowed, int32& OutSecondsPlayed) const
{
	OutSecondsAllowed = 0;
	OutSecondsPlayed = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamApps())
	{
		uint32 Allowed = 0;
		uint32 Played = 0;
		if (SteamApps()->BIsTimedTrial(&Allowed, &Played))
		{
			OutSecondsAllowed = static_cast<int32>(Allowed);
			OutSecondsPlayed = static_cast<int32>(Played);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamAppsSubsystem::GetFileDetails(const FString& FileName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamApps() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetFileDetails"));
		return false;
	}

	if (Callbacks->IsFileDetailsBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetFileDetails: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamApps()->GetFileDetails(TCHAR_TO_UTF8(*FileName));
	if (Call == k_uAPICallInvalid)
	{
		return false;
	}
	Callbacks->TrackFileDetailsRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("GetFileDetails"));
	return false;
#endif
}

int32 UESteamAppsSubsystem::GetNumBetas(int32& OutAvailable, int32& OutPrivate) const
{
	OutAvailable = 0;
	OutPrivate = 0;
	// GetNumBetas/GetBetaInfo/SetActiveBeta are newer beta-management APIs; gate them so older SDK
	// headers (which lack these methods) still compile. Offline-safe: returns 0.
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamApps())
	{
		int Available = 0;
		int Private = 0;
		const int Total = SteamApps()->GetNumBetas(&Available, &Private);
		OutAvailable = Available;
		OutPrivate = Private;
		return Total;
	}
#endif
	return 0;
}

bool UESteamAppsSubsystem::GetBetaInfo(int32 Index, int32& OutFlags, int32& OutBuildId, FString& OutName, FString& OutDescription) const
{
	OutFlags = 0;
	OutBuildId = 0;
	OutName.Reset();
	OutDescription.Reset();
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamApps())
	{
		uint32 Flags = 0;
		uint32 BuildId = 0;
		uint32 LastUpdated = 0;
		char NameBuffer[256] = {};
		char DescriptionBuffer[8192] = {};
		if (SteamApps()->GetBetaInfo(Index, &Flags, &BuildId, NameBuffer, sizeof(NameBuffer), DescriptionBuffer, sizeof(DescriptionBuffer), &LastUpdated))
		{
			OutFlags = static_cast<int32>(Flags);
			OutBuildId = static_cast<int32>(BuildId);
			OutName = UTF8_TO_TCHAR(NameBuffer);
			OutDescription = UTF8_TO_TCHAR(DescriptionBuffer);
			return true;
		}
	}
#endif
	return false;
}

bool UESteamAppsSubsystem::SetActiveBeta(const FString& BetaName)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamApps())
	{
		return SteamApps()->SetActiveBeta(TCHAR_TO_UTF8(*BetaName));
	}
#endif
	return false;
}

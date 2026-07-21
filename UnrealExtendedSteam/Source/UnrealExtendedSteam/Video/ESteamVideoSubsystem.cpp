// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Video/ESteamVideoSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Maps the Steamworks broadcast-upload-result enum to the Blueprint-facing enum. */
static EESteamBroadcastUploadResult ToESteamBroadcastResult(EBroadcastUploadResult Result)
{
	if (Result < k_EBroadcastUploadResultNone || Result > k_EBroadcastUploadResultAudioInitFailed)
	{
		return EESteamBroadcastUploadResult::None;
	}
	// The Blueprint enum mirrors EBroadcastUploadResult 0..23 one-to-one.
	return static_cast<EESteamBroadcastUploadResult>(static_cast<uint8>(Result));
}

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamVideoCallbacks
{
public:
	explicit FESteamVideoCallbacks(UESteamVideoSubsystem* InOwner)
		: Owner(InOwner)
		, VideoURLResult(this, &FESteamVideoCallbacks::HandleVideoURLResult)
		, OPFSettingsResult(this, &FESteamVideoCallbacks::HandleOPFSettingsResult)
		, BroadcastUploadStart(this, &FESteamVideoCallbacks::HandleBroadcastUploadStart)
		, BroadcastUploadStop(this, &FESteamVideoCallbacks::HandleBroadcastUploadStop)
	{
	}

private:
	void HandleVideoURLResult(GetVideoURLResult_t* Data)
	{
		if (UESteamVideoSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = Data->m_eResult == k_EResultOK;
			Subsystem->OnVideoURLReceived.Broadcast(
				bSuccess,
				static_cast<int32>(Data->m_unVideoAppID),
				bSuccess ? FString(UTF8_TO_TCHAR(Data->m_rgchURL)) : FString());
		}
	}

	void HandleOPFSettingsResult(GetOPFSettingsResult_t* Data)
	{
		if (UESteamVideoSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnOPFSettingsReceived.Broadcast(Data->m_eResult == k_EResultOK, static_cast<int32>(Data->m_unVideoAppID));
		}
	}

	void HandleBroadcastUploadStart(BroadcastUploadStart_t* Data)
	{
		if (UESteamVideoSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnBroadcastUploadStart.Broadcast(Data->m_bIsRTMP);
		}
	}

	void HandleBroadcastUploadStop(BroadcastUploadStop_t* Data)
	{
		if (UESteamVideoSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnBroadcastUploadStop.Broadcast(ToESteamBroadcastResult(Data->m_eResult));
		}
	}

	TWeakObjectPtr<UESteamVideoSubsystem> Owner;
	CCallback<FESteamVideoCallbacks, GetVideoURLResult_t> VideoURLResult;
	CCallback<FESteamVideoCallbacks, GetOPFSettingsResult_t> OPFSettingsResult;
	CCallback<FESteamVideoCallbacks, BroadcastUploadStart_t> BroadcastUploadStart;
	CCallback<FESteamVideoCallbacks, BroadcastUploadStop_t> BroadcastUploadStop;
};
#else
class FESteamVideoCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamVideoSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamVideoSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamVideoCallbacks>(this);
	}
#endif
}

void UESteamVideoSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

void UESteamVideoSubsystem::GetVideoURL(int32 VideoAppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamVideo() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetVideoURL"));
		OnVideoURLReceived.Broadcast(false, VideoAppId, FString());
		return;
	}
	SteamVideo()->GetVideoURL(static_cast<AppId_t>(VideoAppId));
#else
	LogSteamUnavailable(TEXT("GetVideoURL"));
	OnVideoURLReceived.Broadcast(false, VideoAppId, FString());
#endif
}

bool UESteamVideoSubsystem::IsBroadcasting(int32& OutNumViewers) const
{
	OutNumViewers = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamVideo())
	{
		return false;
	}
	int NumViewers = 0;
	const bool bBroadcasting = SteamVideo()->IsBroadcasting(&NumViewers);
	OutNumViewers = bBroadcasting ? NumViewers : 0;
	return bBroadcasting;
#else
	return false;
#endif
}

void UESteamVideoSubsystem::GetOPFSettings(int32 VideoAppId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamVideo() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetOPFSettings"));
		OnOPFSettingsReceived.Broadcast(false, VideoAppId);
		return;
	}
	SteamVideo()->GetOPFSettings(static_cast<AppId_t>(VideoAppId));
#else
	LogSteamUnavailable(TEXT("GetOPFSettings"));
	OnOPFSettingsReceived.Broadcast(false, VideoAppId);
#endif
}

bool UESteamVideoSubsystem::GetOPFStringForApp(int32 VideoAppId, FString& OutOPF)
{
	OutOPF.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamVideo())
	{
		LogSteamUnavailable(TEXT("GetOPFStringForApp"));
		return false;
	}

	// Two-call pattern: query the required size, then read into a buffer of that size. OPF
	// payloads can be large (~48KB); fall back to that when the sizing call yields nothing.
	int32 BufferSize = 0;
	SteamVideo()->GetOPFStringForApp(static_cast<AppId_t>(VideoAppId), nullptr, &BufferSize);
	if (BufferSize <= 0)
	{
		BufferSize = 48 * 1024;
	}

	TArray<char> Buffer;
	Buffer.SetNumZeroed(BufferSize);
	if (!SteamVideo()->GetOPFStringForApp(static_cast<AppId_t>(VideoAppId), Buffer.GetData(), &BufferSize))
	{
		return false;
	}

	OutOPF = UTF8_TO_TCHAR(Buffer.GetData());
	return true;
#else
	LogSteamUnavailable(TEXT("GetOPFStringForApp"));
	return false;
#endif
}

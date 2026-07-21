// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "RemotePlay/ESteamRemotePlaySubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	/** Maps the Steamworks device form-factor enum to the Blueprint-facing enum. */
	EESteamRemotePlayFormFactor ToESteamFormFactor(ESteamDeviceFormFactor FormFactor)
	{
		switch (FormFactor)
		{
		case k_ESteamDeviceFormFactorPhone:     return EESteamRemotePlayFormFactor::Phone;
		case k_ESteamDeviceFormFactorTablet:    return EESteamRemotePlayFormFactor::Tablet;
		case k_ESteamDeviceFormFactorComputer:  return EESteamRemotePlayFormFactor::Computer;
		case k_ESteamDeviceFormFactorTV:        return EESteamRemotePlayFormFactor::TV;
		case k_ESteamDeviceFormFactorVRHeadset: return EESteamRemotePlayFormFactor::VRHeadset;
		default:                                return EESteamRemotePlayFormFactor::Unknown;
		}
	}
}

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamRemotePlayCallbacks
{
public:
	explicit FESteamRemotePlayCallbacks(UESteamRemotePlaySubsystem* InOwner)
		: Owner(InOwner)
		, SessionConnected(this, &FESteamRemotePlayCallbacks::HandleSessionConnected)
		, SessionDisconnected(this, &FESteamRemotePlayCallbacks::HandleSessionDisconnected)
	{
	}

private:
	void HandleSessionConnected(SteamRemotePlaySessionConnected_t* Data)
	{
		if (UESteamRemotePlaySubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnRemotePlaySessionConnected.Broadcast(static_cast<int32>(Data->m_unSessionID));
		}
	}

	void HandleSessionDisconnected(SteamRemotePlaySessionDisconnected_t* Data)
	{
		if (UESteamRemotePlaySubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnRemotePlaySessionDisconnected.Broadcast(static_cast<int32>(Data->m_unSessionID));
		}
	}

	TWeakObjectPtr<UESteamRemotePlaySubsystem> Owner;
	CCallback<FESteamRemotePlayCallbacks, SteamRemotePlaySessionConnected_t> SessionConnected;
	CCallback<FESteamRemotePlayCallbacks, SteamRemotePlaySessionDisconnected_t> SessionDisconnected;
};
#else
class FESteamRemotePlayCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamRemotePlaySubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamRemotePlaySubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamRemotePlayCallbacks>(this);
	}
#endif
}

void UESteamRemotePlaySubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

int32 UESteamRemotePlaySubsystem::GetSessionCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemotePlay())
	{
		return static_cast<int32>(SteamRemotePlay()->GetSessionCount());
	}
#endif
	return 0;
}

int32 UESteamRemotePlaySubsystem::GetSessionId(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemotePlay() && Index >= 0)
	{
		return static_cast<int32>(SteamRemotePlay()->GetSessionID(Index));
	}
#endif
	return 0;
}

FString UESteamRemotePlaySubsystem::GetSessionClientName(int32 SessionId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemotePlay())
	{
		const char* Name = SteamRemotePlay()->GetSessionClientName(static_cast<RemotePlaySessionID_t>(SessionId));
		if (Name)
		{
			return UTF8_TO_TCHAR(Name);
		}
	}
#endif
	return FString();
}

FESteamId UESteamRemotePlaySubsystem::GetSessionSteamID(int32 SessionId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamRemotePlay())
	{
		return FESteamId(SteamRemotePlay()->GetSessionSteamID(static_cast<RemotePlaySessionID_t>(SessionId)).ConvertToUint64());
	}
#endif
	return FESteamId();
}

bool UESteamRemotePlaySubsystem::GetSessionInfo(int32 SessionId, FESteamRemotePlaySessionInfo& OutInfo) const
{
	OutInfo = FESteamRemotePlaySessionInfo();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemotePlay())
	{
		return false;
	}

	const RemotePlaySessionID_t Session = static_cast<RemotePlaySessionID_t>(SessionId);
	const char* Name = SteamRemotePlay()->GetSessionClientName(Session);
	if (!Name)
	{
		// GetSessionClientName is the documented invalid-session probe for this interface.
		return false;
	}

	OutInfo.SessionId = SessionId;
	OutInfo.UserId = FESteamId(SteamRemotePlay()->GetSessionSteamID(Session).ConvertToUint64());
	OutInfo.ClientName = UTF8_TO_TCHAR(Name);
	OutInfo.FormFactor = ToESteamFormFactor(SteamRemotePlay()->GetSessionClientFormFactor(Session));

	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	if (SteamRemotePlay()->BGetSessionClientResolution(Session, &ResolutionX, &ResolutionY))
	{
		OutInfo.Resolution = FIntPoint(ResolutionX, ResolutionY);
	}
	return true;
#else
	return false;
#endif
}

bool UESteamRemotePlaySubsystem::SendRemotePlayTogetherInvite(FESteamId Friend)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemotePlay())
	{
		LogSteamUnavailable(TEXT("SendRemotePlayTogetherInvite"));
		return false;
	}
	return SteamRemotePlay()->BSendRemotePlayTogetherInvite(CSteamID(Friend.Value));
#else
	LogSteamUnavailable(TEXT("SendRemotePlayTogetherInvite"));
	return false;
#endif
}

bool UESteamRemotePlaySubsystem::ShowRemotePlayTogetherUI()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamRemotePlay())
	{
		LogSteamUnavailable(TEXT("ShowRemotePlayTogetherUI"));
		return false;
	}
	return SteamRemotePlay()->ShowRemotePlayTogetherUI();
#else
	LogSteamUnavailable(TEXT("ShowRemotePlayTogetherUI"));
	return false;
#endif
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Music/ESteamMusicSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamMusicCallbacks
{
public:
	explicit FESteamMusicCallbacks(UESteamMusicSubsystem* InOwner)
		: Owner(InOwner)
		, PlaybackChanged(this, &FESteamMusicCallbacks::HandlePlaybackChanged)
		, VolumeChanged(this, &FESteamMusicCallbacks::HandleVolumeChanged)
	{
	}

private:
	void HandlePlaybackChanged(PlaybackStatusHasChanged_t* Data)
	{
		if (UESteamMusicSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPlaybackChanged.Broadcast();
		}
	}

	void HandleVolumeChanged(VolumeHasChanged_t* Data)
	{
		if (UESteamMusicSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnVolumeChanged.Broadcast(Data->m_flNewVolume);
		}
	}

	TWeakObjectPtr<UESteamMusicSubsystem> Owner;
	CCallback<FESteamMusicCallbacks, PlaybackStatusHasChanged_t> PlaybackChanged;
	CCallback<FESteamMusicCallbacks, VolumeHasChanged_t> VolumeChanged;
};
#else
class FESteamMusicCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamMusicSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamMusicSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamMusicCallbacks>(this);
	}
#endif
}

void UESteamMusicSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamMusicSubsystem::IsEnabled() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamMusic() && SteamMusic()->BIsEnabled();
#else
	return false;
#endif
}

bool UESteamMusicSubsystem::IsPlaying() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamMusic() && SteamMusic()->BIsPlaying();
#else
	return false;
#endif
}

EESteamMusicStatus UESteamMusicSubsystem::GetPlaybackStatus() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		switch (SteamMusic()->GetPlaybackStatus())
		{
		case AudioPlayback_Playing: return EESteamMusicStatus::Playing;
		case AudioPlayback_Paused:  return EESteamMusicStatus::Paused;
		case AudioPlayback_Idle:    return EESteamMusicStatus::Idle;
		case AudioPlayback_Undefined:
		default:                    return EESteamMusicStatus::Undefined;
		}
	}
#endif
	return EESteamMusicStatus::Undefined;
}

void UESteamMusicSubsystem::Play()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		SteamMusic()->Play();
	}
#endif
}

void UESteamMusicSubsystem::Pause()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		SteamMusic()->Pause();
	}
#endif
}

void UESteamMusicSubsystem::PlayPrevious()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		SteamMusic()->PlayPrevious();
	}
#endif
}

void UESteamMusicSubsystem::PlayNext()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		SteamMusic()->PlayNext();
	}
#endif
}

float UESteamMusicSubsystem::GetVolume() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		return SteamMusic()->GetVolume();
	}
#endif
	return 0.0f;
}

void UESteamMusicSubsystem::SetVolume(float Volume)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMusic())
	{
		SteamMusic()->SetVolume(FMath::Clamp(Volume, 0.0f, 1.0f));
	}
#endif
}

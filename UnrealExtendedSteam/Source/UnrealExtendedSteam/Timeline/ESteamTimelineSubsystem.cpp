// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Timeline/ESteamTimelineSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

// ISteamTimeline shipped its current API ("V004": SetTimelineTooltip, Add*TimelineEvent, game phases)
// in Steamworks SDK 1.62; earlier SDKs either lack the header or expose the incompatible
// SetTimelineStateDescription-era API, so everything is gated at 162.
// steam_api.h includes isteamtimeline.h itself in SDK 1.64 (and since 1.59), so no extra include is needed.
#if ESTEAM_SDK_AT_LEAST(162)
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	ETimelineGameMode ToSteamTimelineGameMode(EESteamTimelineGameMode Mode)
	{
		switch (Mode)
		{
		case EESteamTimelineGameMode::Playing:       return k_ETimelineGameMode_Playing;
		case EESteamTimelineGameMode::Staging:       return k_ETimelineGameMode_Staging;
		case EESteamTimelineGameMode::Menus:         return k_ETimelineGameMode_Menus;
		case EESteamTimelineGameMode::LoadingScreen: return k_ETimelineGameMode_LoadingScreen;
		default:                                     return k_ETimelineGameMode_Invalid;
		}
	}

	ETimelineEventClipPriority ToSteamTimelineClipPriority(EESteamTimelineClipPriority ClipPriority)
	{
		switch (ClipPriority)
		{
		case EESteamTimelineClipPriority::None:     return k_ETimelineEventClipPriority_None;
		case EESteamTimelineClipPriority::Standard: return k_ETimelineEventClipPriority_Standard;
		case EESteamTimelineClipPriority::Featured: return k_ETimelineEventClipPriority_Featured;
		default:                                    return k_ETimelineEventClipPriority_Invalid;
		}
	}

	uint32 ClampTimelinePriority(int32 Priority)
	{
		return static_cast<uint32>(FMath::Clamp<int32>(Priority, 0, static_cast<int32>(k_unMaxTimelinePriority)));
	}
}

/**
 * Native Steam call-result listener for DoesEventRecordingExist; alive only while the Steam
 * client API is initialized. Single in-flight query at a time (a new Set cancels the previous).
 */
class FESteamTimelineCallbacks
{
public:
	explicit FESteamTimelineCallbacks(UESteamTimelineSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

	void TrackDoesEventRecordingExist(SteamAPICall_t Call)
	{
		EventRecordingExists.Set(Call, this, &FESteamTimelineCallbacks::HandleEventRecordingExists);
	}

	// Single-slot CallResult: a second query before the first completes would cancel it (the first's
	// callback would never fire). The call site checks this and rejects the overlap.
	bool IsDoesEventRecordingExistBusy() const { return EventRecordingExists.IsActive(); }

private:
	void HandleEventRecordingExists(SteamTimelineEventRecordingExists_t* Data, bool bIOFailure)
	{
		if (UESteamTimelineSubsystem* Subsystem = Owner.Get())
		{
			const bool bExists = !bIOFailure && Data->m_bRecordingExists;
			Subsystem->OnEventRecordingExists.Broadcast(static_cast<int64>(Data->m_ulEventID), bExists);
		}
	}

	TWeakObjectPtr<UESteamTimelineSubsystem> Owner;
	CCallResult<FESteamTimelineCallbacks, SteamTimelineEventRecordingExists_t> EventRecordingExists;
};
#else
class FESteamTimelineCallbacks
{
};
#endif // ESTEAM_SDK_AT_LEAST(162)

void UESteamTimelineSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamTimelineSubsystem::HandleSteamClientInitialized()
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamTimelineCallbacks>(this);
	}
#endif
}

void UESteamTimelineSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

void UESteamTimelineSubsystem::LogTimelineUnavailable(const TCHAR* Context) const
{
	UE_LOG(LogExtendedSteam, Verbose,
		TEXT("%s: Steam Timeline is not available (requires Steamworks SDK 1.62+ and a recent Steam client); call is a no-op"),
		Context);
}

void UESteamTimelineSubsystem::SetTimelineTooltip(const FString& Description, float TimeDelta)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->SetTimelineTooltip(TCHAR_TO_UTF8(*Description), TimeDelta);
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("SetTimelineTooltip"));
}

void UESteamTimelineSubsystem::ClearTimelineTooltip(float TimeDelta)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->ClearTimelineTooltip(TimeDelta);
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("ClearTimelineTooltip"));
}

void UESteamTimelineSubsystem::SetTimelineGameMode(EESteamTimelineGameMode Mode)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->SetTimelineGameMode(ToSteamTimelineGameMode(Mode));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("SetTimelineGameMode"));
}

int64 UESteamTimelineSubsystem::AddInstantaneousTimelineEvent(const FString& Title, const FString& Description,
	const FString& Icon, int32 Priority, float StartOffsetSeconds, EESteamTimelineClipPriority ClipPriority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		const TimelineEventHandle_t Handle = SteamTimeline()->AddInstantaneousTimelineEvent(
			TCHAR_TO_UTF8(*Title),
			TCHAR_TO_UTF8(*Description),
			TCHAR_TO_UTF8(*Icon),
			ClampTimelinePriority(Priority),
			StartOffsetSeconds,
			ToSteamTimelineClipPriority(ClipPriority));
		return static_cast<int64>(Handle);
	}
#endif
	LogTimelineUnavailable(TEXT("AddInstantaneousTimelineEvent"));
	return 0;
}

int64 UESteamTimelineSubsystem::AddRangeTimelineEvent(const FString& Title, const FString& Description,
	const FString& Icon, int32 Priority, float StartOffsetSeconds, float Duration, EESteamTimelineClipPriority ClipPriority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		const TimelineEventHandle_t Handle = SteamTimeline()->AddRangeTimelineEvent(
			TCHAR_TO_UTF8(*Title),
			TCHAR_TO_UTF8(*Description),
			TCHAR_TO_UTF8(*Icon),
			ClampTimelinePriority(Priority),
			StartOffsetSeconds,
			FMath::Clamp(Duration, 0.f, k_flMaxTimelineEventDuration),
			ToSteamTimelineClipPriority(ClipPriority));
		return static_cast<int64>(Handle);
	}
#endif
	LogTimelineUnavailable(TEXT("AddRangeTimelineEvent"));
	return 0;
}

int64 UESteamTimelineSubsystem::StartRangeTimelineEvent(const FString& Title, const FString& Description,
	const FString& Icon, int32 Priority, float StartOffsetSeconds, EESteamTimelineClipPriority ClipPriority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		const TimelineEventHandle_t Handle = SteamTimeline()->StartRangeTimelineEvent(
			TCHAR_TO_UTF8(*Title),
			TCHAR_TO_UTF8(*Description),
			TCHAR_TO_UTF8(*Icon),
			ClampTimelinePriority(Priority),
			StartOffsetSeconds,
			ToSteamTimelineClipPriority(ClipPriority));
		return static_cast<int64>(Handle);
	}
#endif
	LogTimelineUnavailable(TEXT("StartRangeTimelineEvent"));
	return 0;
}

void UESteamTimelineSubsystem::UpdateRangeTimelineEvent(int64 EventHandle, const FString& Title, const FString& Description,
	const FString& Icon, int32 Priority, EESteamTimelineClipPriority ClipPriority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->UpdateRangeTimelineEvent(
			static_cast<TimelineEventHandle_t>(EventHandle),
			TCHAR_TO_UTF8(*Title),
			TCHAR_TO_UTF8(*Description),
			TCHAR_TO_UTF8(*Icon),
			ClampTimelinePriority(Priority),
			ToSteamTimelineClipPriority(ClipPriority));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("UpdateRangeTimelineEvent"));
}

void UESteamTimelineSubsystem::EndRangeTimelineEvent(int64 EventHandle, float EndOffsetSeconds)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->EndRangeTimelineEvent(static_cast<TimelineEventHandle_t>(EventHandle), EndOffsetSeconds);
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("EndRangeTimelineEvent"));
}

void UESteamTimelineSubsystem::RemoveTimelineEvent(int64 EventHandle)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->RemoveTimelineEvent(static_cast<TimelineEventHandle_t>(EventHandle));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("RemoveTimelineEvent"));
}

void UESteamTimelineSubsystem::DoesEventRecordingExist(int64 EventHandle)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline() && Callbacks)
	{
		if (Callbacks->IsDoesEventRecordingExistBusy())
		{
			UE_LOG(LogExtendedSteam, Warning, TEXT("DoesEventRecordingExist: a query is already in flight; ignoring the new one"));
			OnEventRecordingExists.Broadcast(EventHandle, false);
			return;
		}
		const SteamAPICall_t Call = SteamTimeline()->DoesEventRecordingExist(static_cast<TimelineEventHandle_t>(EventHandle));
		Callbacks->TrackDoesEventRecordingExist(Call);
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("DoesEventRecordingExist"));
	OnEventRecordingExists.Broadcast(EventHandle, false);
}

void UESteamTimelineSubsystem::StartGamePhase()
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->StartGamePhase();
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("StartGamePhase"));
}

void UESteamTimelineSubsystem::EndGamePhase()
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->EndGamePhase();
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("EndGamePhase"));
}

void UESteamTimelineSubsystem::SetGamePhaseId(const FString& PhaseId)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->SetGamePhaseID(TCHAR_TO_UTF8(*PhaseId.Left(static_cast<int32>(k_cchMaxPhaseIDLength) - 1)));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("SetGamePhaseId"));
}

void UESteamTimelineSubsystem::AddGamePhaseTag(const FString& TagName, const FString& TagIcon, const FString& TagGroup, int32 Priority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->AddGamePhaseTag(
			TCHAR_TO_UTF8(*TagName),
			TCHAR_TO_UTF8(*TagIcon),
			TCHAR_TO_UTF8(*TagGroup),
			ClampTimelinePriority(Priority));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("AddGamePhaseTag"));
}

void UESteamTimelineSubsystem::SetGamePhaseAttribute(const FString& AttributeGroup, const FString& AttributeValue, int32 Priority)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->SetGamePhaseAttribute(
			TCHAR_TO_UTF8(*AttributeGroup),
			TCHAR_TO_UTF8(*AttributeValue),
			ClampTimelinePriority(Priority));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("SetGamePhaseAttribute"));
}

void UESteamTimelineSubsystem::OpenOverlayToGamePhase(const FString& PhaseId)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->OpenOverlayToGamePhase(TCHAR_TO_UTF8(*PhaseId));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("OpenOverlayToGamePhase"));
}

void UESteamTimelineSubsystem::OpenOverlayToTimelineEvent(int64 EventHandle)
{
#if ESTEAM_SDK_AT_LEAST(162)
	if (IsSteamAvailable() && SteamTimeline())
	{
		SteamTimeline()->OpenOverlayToTimelineEvent(static_cast<TimelineEventHandle_t>(EventHandle));
		return;
	}
#endif
	LogTimelineUnavailable(TEXT("OpenOverlayToTimelineEvent"));
}

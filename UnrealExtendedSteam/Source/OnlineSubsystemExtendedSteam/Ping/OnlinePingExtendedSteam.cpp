// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Ping/OnlinePingExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/isteamnetworkingutils.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamPing
{
	/** True while the shared module has the Steam client API up (the OSS never initializes it itself here). */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
#else
		return false;
#endif
	}

#if WITH_EXTENDEDSTEAM_SDK
	/** SteamNetworkingUtils when the client API is up; nullptr otherwise. */
	static ISteamNetworkingUtils* GetNetworkingUtils()
	{
		return IsSteamClientUp() ? SteamNetworkingUtils() : nullptr;
	}
#endif
}

FOnlinePingExtendedSteam::FOnlinePingExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
}

bool FOnlinePingExtendedSteam::IsSteamAvailableForPing() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return ExtendedSteamPing::GetNetworkingUtils() != nullptr;
#else
	return false;
#endif
}

void FOnlinePingExtendedSteam::InitRelayNetworkAccess()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamNetworkingUtils* Utils = ExtendedSteamPing::GetNetworkingUtils())
	{
		Utils->InitRelayNetworkAccess();
	}
#endif
}

float FOnlinePingExtendedSteam::GetLocalPingLocation(FString& OutSerialized) const
{
	OutSerialized.Reset();

#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamNetworkingUtils* Utils = ExtendedSteamPing::GetNetworkingUtils())
	{
		SteamNetworkPingLocation_t Location;
		const float Age = Utils->GetLocalPingLocation(Location);
		if (Age >= 0.0f)
		{
			char Buffer[k_cchMaxSteamNetworkingPingLocationString] = {};
			Utils->ConvertPingLocationToString(Location, Buffer, sizeof(Buffer));
			OutSerialized = FString(UTF8_TO_TCHAR(Buffer));
			return Age;
		}
	}
#endif
	// Negative age: no location yet (Steam down or relay config not downloaded).
	return -1.0f;
}

int32 FOnlinePingExtendedSteam::EstimatePingBetweenLocations(const FString& LocationA, const FString& LocationB) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamNetworkingUtils* Utils = ExtendedSteamPing::GetNetworkingUtils())
	{
		SteamNetworkPingLocation_t ParsedA;
		SteamNetworkPingLocation_t ParsedB;
		if (Utils->ParsePingLocationString(TCHAR_TO_UTF8(*LocationA), ParsedA)
			&& Utils->ParsePingLocationString(TCHAR_TO_UTF8(*LocationB), ParsedB))
		{
			return static_cast<int32>(Utils->EstimatePingTimeBetweenTwoLocations(ParsedA, ParsedB));
		}
	}
#endif
	return PingFailed;
}

int32 FOnlinePingExtendedSteam::GetRelayNetworkStatus() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ISteamNetworkingUtils* Utils = ExtendedSteamPing::GetNetworkingUtils())
	{
		// Passing null asks only for the summary availability, not the detailed struct.
		return static_cast<int32>(Utils->GetRelayNetworkStatus(nullptr));
	}
	return static_cast<int32>(k_ESteamNetworkingAvailability_CannotTry);
#else
	return -102; // k_ESteamNetworkingAvailability_CannotTry
#endif
}

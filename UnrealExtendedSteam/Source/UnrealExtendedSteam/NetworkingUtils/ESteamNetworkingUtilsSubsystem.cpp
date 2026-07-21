// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "NetworkingUtils/ESteamNetworkingUtilsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	/** Maps the Steamworks availability enum to the simplified relay-status enum. */
	EESteamRelayNetworkStatus ToESteamRelayStatus(ESteamNetworkingAvailability Availability)
	{
		switch (Availability)
		{
		case k_ESteamNetworkingAvailability_Current:
			return EESteamRelayNetworkStatus::Available;
		case k_ESteamNetworkingAvailability_Attempting:
		case k_ESteamNetworkingAvailability_Waiting:
			return EESteamRelayNetworkStatus::Attempting;
		case k_ESteamNetworkingAvailability_Retrying:
		case k_ESteamNetworkingAvailability_Previously:
			return EESteamRelayNetworkStatus::Problem;
		case k_ESteamNetworkingAvailability_Failed:
		case k_ESteamNetworkingAvailability_CannotTry:
			return EESteamRelayNetworkStatus::Broken;
		case k_ESteamNetworkingAvailability_NeverTried:
		case k_ESteamNetworkingAvailability_Unknown:
		default:
			return EESteamRelayNetworkStatus::Unknown;
		}
	}
}
#endif

void UESteamNetworkingUtilsSubsystem::InitRelayNetworkAccess()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		SteamNetworkingUtils()->InitRelayNetworkAccess();
		return;
	}
#endif
	LogSteamUnavailable(TEXT("InitRelayNetworkAccess"));
}

EESteamRelayNetworkStatus UESteamNetworkingUtilsSubsystem::GetRelayNetworkStatusSimple() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return ToESteamRelayStatus(SteamNetworkingUtils()->GetRelayNetworkStatus(nullptr));
	}
#endif
	return EESteamRelayNetworkStatus::Unknown;
}

EESteamRelayNetworkStatus UESteamNetworkingUtilsSubsystem::GetRelayNetworkStatusDetailed(FString& OutDebugMsg, int32& OutAvailNetworkConfig, int32& OutAvailAnyRelay) const
{
	OutDebugMsg.Reset();
	OutAvailNetworkConfig = 0;
	OutAvailAnyRelay = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		SteamRelayNetworkStatus_t Details;
		FMemory::Memzero(Details);
		const ESteamNetworkingAvailability Availability = SteamNetworkingUtils()->GetRelayNetworkStatus(&Details);
		OutAvailNetworkConfig = static_cast<int32>(Details.m_eAvailNetworkConfig);
		OutAvailAnyRelay = static_cast<int32>(Details.m_eAvailAnyRelay);
		OutDebugMsg = UTF8_TO_TCHAR(Details.m_debugMsg);
		return ToESteamRelayStatus(Availability);
	}
#endif
	return EESteamRelayNetworkStatus::Unknown;
}

bool UESteamNetworkingUtilsSubsystem::CheckPingDataUpToDate(float MaxAgeSeconds)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return SteamNetworkingUtils()->CheckPingDataUpToDate(MaxAgeSeconds);
	}
#endif
	LogSteamUnavailable(TEXT("CheckPingDataUpToDate"));
	return false;
}

int64 UESteamNetworkingUtilsSubsystem::GetLocalTimestamp() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return static_cast<int64>(SteamNetworkingUtils()->GetLocalTimestamp());
	}
#endif
	return 0;
}

float UESteamNetworkingUtilsSubsystem::GetLocalPingLocation(FString& OutSerializedLocation) const
{
	OutSerializedLocation.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworkingUtils())
	{
		LogSteamUnavailable(TEXT("GetLocalPingLocation"));
		return -1.f;
	}

	SteamNetworkPingLocation_t Location;
	const float AgeSeconds = SteamNetworkingUtils()->GetLocalPingLocation(Location);
	if (AgeSeconds < 0.f)
	{
		return -1.f;
	}

	char Buffer[k_cchMaxSteamNetworkingPingLocationString];
	SteamNetworkingUtils()->ConvertPingLocationToString(Location, Buffer, static_cast<int>(sizeof(Buffer)));
	OutSerializedLocation = UTF8_TO_TCHAR(Buffer);
	return AgeSeconds;
#else
	LogSteamUnavailable(TEXT("GetLocalPingLocation"));
	return -1.f;
#endif
}

int32 UESteamNetworkingUtilsSubsystem::EstimatePingBetweenTwoLocations(const FString& LocationA, const FString& LocationB) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworkingUtils())
	{
		LogSteamUnavailable(TEXT("EstimatePingBetweenTwoLocations"));
		return -1;
	}

	SteamNetworkPingLocation_t ParsedA;
	SteamNetworkPingLocation_t ParsedB;
	if (!SteamNetworkingUtils()->ParsePingLocationString(TCHAR_TO_UTF8(*LocationA), ParsedA)
		|| !SteamNetworkingUtils()->ParsePingLocationString(TCHAR_TO_UTF8(*LocationB), ParsedB))
	{
		UE_LOG(LogExtendedSteam, Verbose, TEXT("EstimatePingBetweenTwoLocations: failed to parse a ping location string"));
		return -1;
	}

	const int32 PingMs = SteamNetworkingUtils()->EstimatePingTimeBetweenTwoLocations(ParsedA, ParsedB);
	return PingMs >= 0 ? PingMs : -1; // k_nSteamNetworkingPing_Failed / _Unknown -> -1
#else
	LogSteamUnavailable(TEXT("EstimatePingBetweenTwoLocations"));
	return -1;
#endif
}

bool UESteamNetworkingUtilsSubsystem::ParsePingLocationString(const FString& SerializedLocation, FString& OutNormalized) const
{
	OutNormalized.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworkingUtils())
	{
		LogSteamUnavailable(TEXT("ParsePingLocationString"));
		return false;
	}

	SteamNetworkPingLocation_t Parsed;
	if (!SteamNetworkingUtils()->ParsePingLocationString(TCHAR_TO_UTF8(*SerializedLocation), Parsed))
	{
		return false;
	}

	char Buffer[k_cchMaxSteamNetworkingPingLocationString];
	SteamNetworkingUtils()->ConvertPingLocationToString(Parsed, Buffer, static_cast<int>(sizeof(Buffer)));
	OutNormalized = UTF8_TO_TCHAR(Buffer);
	return true;
#else
	LogSteamUnavailable(TEXT("ParsePingLocationString"));
	return false;
#endif
}

int32 UESteamNetworkingUtilsSubsystem::EstimatePingTimeFromLocalHost(const FString& SerializedLocation) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworkingUtils())
	{
		LogSteamUnavailable(TEXT("EstimatePingTimeFromLocalHost"));
		return -1;
	}

	SteamNetworkPingLocation_t Parsed;
	if (!SteamNetworkingUtils()->ParsePingLocationString(TCHAR_TO_UTF8(*SerializedLocation), Parsed))
	{
		UE_LOG(LogExtendedSteam, Verbose, TEXT("EstimatePingTimeFromLocalHost: failed to parse the ping location string"));
		return -1;
	}

	const int32 PingMs = SteamNetworkingUtils()->EstimatePingTimeFromLocalHost(Parsed);
	return PingMs >= 0 ? PingMs : -1; // k_nSteamNetworkingPing_Failed / _Unknown -> -1
#else
	LogSteamUnavailable(TEXT("EstimatePingTimeFromLocalHost"));
	return -1;
#endif
}

int32 UESteamNetworkingUtilsSubsystem::GetPOPCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return SteamNetworkingUtils()->GetPOPCount();
	}
#endif
	return 0;
}

int32 UESteamNetworkingUtilsSubsystem::GetPOPList(TArray<int32>& OutPOPIds) const
{
	OutPOPIds.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		const int32 Count = SteamNetworkingUtils()->GetPOPCount();
		if (Count <= 0)
		{
			return 0;
		}

		TArray<SteamNetworkingPOPID> Ids;
		Ids.SetNumZeroed(Count);
		const int32 Filled = SteamNetworkingUtils()->GetPOPList(Ids.GetData(), Count);
		OutPOPIds.Reserve(Filled);
		for (int32 Index = 0; Index < Filled; ++Index)
		{
			OutPOPIds.Add(static_cast<int32>(Ids[Index]));
		}
		return Filled;
	}
#endif
	return 0;
}

int32 UESteamNetworkingUtilsSubsystem::GetPingToDataCenter(int32 POPId, int32& OutViaRelayPOP) const
{
	OutViaRelayPOP = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		SteamNetworkingPOPID ViaRelay = 0;
		const int32 PingMs = SteamNetworkingUtils()->GetPingToDataCenter(static_cast<SteamNetworkingPOPID>(POPId), &ViaRelay);
		OutViaRelayPOP = static_cast<int32>(ViaRelay);
		return PingMs >= 0 ? PingMs : -1;
	}
#endif
	return -1;
}

int32 UESteamNetworkingUtilsSubsystem::GetDirectPingToPOP(int32 POPId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		const int32 PingMs = SteamNetworkingUtils()->GetDirectPingToPOP(static_cast<SteamNetworkingPOPID>(POPId));
		return PingMs >= 0 ? PingMs : -1;
	}
#endif
	return -1;
}

bool UESteamNetworkingUtilsSubsystem::SetGlobalConfigValueInt32(int32 ConfigValue, int32 Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return SteamNetworkingUtils()->SetGlobalConfigValueInt32(static_cast<ESteamNetworkingConfigValue>(ConfigValue), Value);
	}
#endif
	LogSteamUnavailable(TEXT("SetGlobalConfigValueInt32"));
	return false;
}

bool UESteamNetworkingUtilsSubsystem::SetGlobalConfigValueFloat(int32 ConfigValue, float Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return SteamNetworkingUtils()->SetGlobalConfigValueFloat(static_cast<ESteamNetworkingConfigValue>(ConfigValue), Value);
	}
#endif
	LogSteamUnavailable(TEXT("SetGlobalConfigValueFloat"));
	return false;
}

bool UESteamNetworkingUtilsSubsystem::SetGlobalConfigValueString(int32 ConfigValue, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		return SteamNetworkingUtils()->SetGlobalConfigValueString(static_cast<ESteamNetworkingConfigValue>(ConfigValue), TCHAR_TO_UTF8(*Value));
	}
#endif
	LogSteamUnavailable(TEXT("SetGlobalConfigValueString"));
	return false;
}

bool UESteamNetworkingUtilsSubsystem::GetGlobalConfigValueInt32(int32 ConfigValue, int32& OutValue) const
{
	OutValue = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		int32 Result = 0;
		size_t ResultSize = sizeof(Result);
		ESteamNetworkingConfigDataType DataType = k_ESteamNetworkingConfig_Int32;
		const ESteamNetworkingGetConfigValueResult Outcome = SteamNetworkingUtils()->GetConfigValue(
			static_cast<ESteamNetworkingConfigValue>(ConfigValue), k_ESteamNetworkingConfig_Global, 0, &DataType, &Result, &ResultSize);
		if ((Outcome == k_ESteamNetworkingGetConfigValue_OK || Outcome == k_ESteamNetworkingGetConfigValue_OKInherited)
			&& DataType == k_ESteamNetworkingConfig_Int32)
		{
			OutValue = Result;
			return true;
		}
	}
#endif
	return false;
}

bool UESteamNetworkingUtilsSubsystem::GetGlobalConfigValueFloat(int32 ConfigValue, float& OutValue) const
{
	OutValue = 0.f;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		float Result = 0.f;
		size_t ResultSize = sizeof(Result);
		ESteamNetworkingConfigDataType DataType = k_ESteamNetworkingConfig_Float;
		const ESteamNetworkingGetConfigValueResult Outcome = SteamNetworkingUtils()->GetConfigValue(
			static_cast<ESteamNetworkingConfigValue>(ConfigValue), k_ESteamNetworkingConfig_Global, 0, &DataType, &Result, &ResultSize);
		if ((Outcome == k_ESteamNetworkingGetConfigValue_OK || Outcome == k_ESteamNetworkingGetConfigValue_OKInherited)
			&& DataType == k_ESteamNetworkingConfig_Float)
		{
			OutValue = Result;
			return true;
		}
	}
#endif
	return false;
}

bool UESteamNetworkingUtilsSubsystem::GetGlobalConfigValueString(int32 ConfigValue, FString& OutValue) const
{
	OutValue.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworkingUtils())
	{
		char Buffer[k_cchMaxSteamNetworkingErrMsg];
		FMemory::Memzero(Buffer, sizeof(Buffer));
		size_t ResultSize = sizeof(Buffer);
		ESteamNetworkingConfigDataType DataType = k_ESteamNetworkingConfig_String;
		const ESteamNetworkingGetConfigValueResult Outcome = SteamNetworkingUtils()->GetConfigValue(
			static_cast<ESteamNetworkingConfigValue>(ConfigValue), k_ESteamNetworkingConfig_Global, 0, &DataType, Buffer, &ResultSize);
		if ((Outcome == k_ESteamNetworkingGetConfigValue_OK || Outcome == k_ESteamNetworkingGetConfigValue_OKInherited)
			&& DataType == k_ESteamNetworkingConfig_String)
		{
			OutValue = UTF8_TO_TCHAR(Buffer);
			return true;
		}
	}
#endif
	return false;
}

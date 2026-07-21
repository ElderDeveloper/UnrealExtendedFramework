// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "User/ESteamUserSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Misc/StringBuilder.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ESteamUserHelpers
{
	/** Maps Steamworks EAuthSessionResponse onto the Blueprint-facing enum. */
	EESteamAuthSessionResponse MapAuthSessionResponse(EAuthSessionResponse Response)
	{
		switch (Response)
		{
		case k_EAuthSessionResponseOK:                          return EESteamAuthSessionResponse::Ok;
		case k_EAuthSessionResponseUserNotConnectedToSteam:     return EESteamAuthSessionResponse::UserNotConnectedToSteam;
		case k_EAuthSessionResponseNoLicenseOrExpired:          return EESteamAuthSessionResponse::NoLicenseOrExpired;
		case k_EAuthSessionResponseVACBanned:                   return EESteamAuthSessionResponse::VACBanned;
		case k_EAuthSessionResponseLoggedInElseWhere:           return EESteamAuthSessionResponse::LoggedInElseWhere;
		case k_EAuthSessionResponseVACCheckTimedOut:            return EESteamAuthSessionResponse::VACCheckTimedOut;
		case k_EAuthSessionResponseAuthTicketCanceled:          return EESteamAuthSessionResponse::AuthTicketCanceled;
		case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:return EESteamAuthSessionResponse::AuthTicketInvalidAlreadyUsed;
		case k_EAuthSessionResponseAuthTicketInvalid:           return EESteamAuthSessionResponse::AuthTicketInvalid;
		case k_EAuthSessionResponsePublisherIssuedBan:          return EESteamAuthSessionResponse::PublisherIssuedBan;
		default:                                                return EESteamAuthSessionResponse::Unknown;
		}
	}

	/** Parses a dotted IPv4 string into a host-order uint32 (as ISteamUser::AdvertiseGame expects). Returns 0 on failure/empty. */
	uint32 ParseIPv4HostOrder(const FString& InIp)
	{
		TArray<FString> Octets;
		InIp.ParseIntoArray(Octets, TEXT("."), true);
		if (Octets.Num() != 4)
		{
			return 0;
		}

		uint32 Result = 0;
		for (const FString& Octet : Octets)
		{
			const int32 Value = FCString::Atoi(*Octet);
			if (Value < 0 || Value > 255)
			{
				return 0;
			}
			Result = (Result << 8) | static_cast<uint32>(Value);
		}
		return Result;
	}
}

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamUserCallbacks
{
public:
	explicit FESteamUserCallbacks(UESteamUserSubsystem* InOwner)
		: Owner(InOwner)
		, AuthTicketResponse(this, &FESteamUserCallbacks::HandleAuthTicketResponse)
		, WebApiTicketResponse(this, &FESteamUserCallbacks::HandleWebApiTicketResponse)
		, ValidateTicketResponse(this, &FESteamUserCallbacks::HandleValidateTicketResponse)
		, ServersConnected(this, &FESteamUserCallbacks::HandleServersConnected)
		, ServersDisconnected(this, &FESteamUserCallbacks::HandleServersDisconnected)
		, ServerConnectFailure(this, &FESteamUserCallbacks::HandleServerConnectFailure)
		, DurationControlTimer(this, &FESteamUserCallbacks::HandleDurationControlTimer)
	{
	}

	void TrackEncryptedTicketRequest(SteamAPICall_t Call)
	{
		EncryptedTicketResult.Set(Call, this, &FESteamUserCallbacks::HandleEncryptedTicketResponse);
	}

	void TrackMarketEligibilityRequest(SteamAPICall_t Call)
	{
		MarketEligibilityResult.Set(Call, this, &FESteamUserCallbacks::HandleMarketEligibilityResponse);
	}

	void TrackStoreAuthURLRequest(SteamAPICall_t Call)
	{
		StoreAuthURLResult.Set(Call, this, &FESteamUserCallbacks::HandleStoreAuthURLResponse);
	}

	void TrackDurationControlRequest(SteamAPICall_t Call)
	{
		DurationControlResult.Set(Call, this, &FESteamUserCallbacks::HandleDurationControlResponse);
	}

	// Each of these single-slot CallResults tracks only one in-flight request. Issuing a second
	// before the first completes would cancel the first (its callback would never fire, so the
	// caller would wait forever). Call sites check these first and reject the overlapping request.
	bool IsEncryptedTicketBusy() const { return EncryptedTicketResult.IsActive(); }
	bool IsMarketEligibilityBusy() const { return MarketEligibilityResult.IsActive(); }
	bool IsStoreAuthURLBusy() const { return StoreAuthURLResult.IsActive(); }
	bool IsDurationControlBusy() const { return DurationControlResult.IsActive(); }

private:
	void HandleAuthTicketResponse(GetAuthSessionTicketResponse_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAuthTicketResponse.Broadcast(
				static_cast<int32>(Data->m_hAuthTicket), Data->m_eResult == k_EResultOK);
		}
	}

	void HandleWebApiTicketResponse(GetTicketForWebApiResponse_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			FString HexTicket;
			const bool bSuccess = Data->m_eResult == k_EResultOK && Data->m_cubTicket > 0;
			if (bSuccess)
			{
				HexTicket = BytesToHex(Data->m_rgubTicket, Data->m_cubTicket);
			}
			Subsystem->OnWebApiAuthTicketReady.Broadcast(bSuccess, static_cast<int32>(Data->m_hAuthTicket), HexTicket);
		}
	}

	void HandleValidateTicketResponse(ValidateAuthTicketResponse_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			const bool bAuthorized = Data->m_eAuthSessionResponse == k_EAuthSessionResponseOK;
			Subsystem->OnValidateAuthTicket.Broadcast(
				FESteamId(Data->m_SteamID.ConvertToUint64()),
				bAuthorized,
				ESteamUserHelpers::MapAuthSessionResponse(Data->m_eAuthSessionResponse),
				FESteamId(Data->m_OwnerSteamID.ConvertToUint64()));
		}
	}

	void HandleServersConnected(SteamServersConnected_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSteamServersConnected.Broadcast();
		}
	}

	void HandleServersDisconnected(SteamServersDisconnected_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSteamServersDisconnected.Broadcast(static_cast<int32>(Data->m_eResult));
		}
	}

	void HandleServerConnectFailure(SteamServerConnectFailure_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnSteamServerConnectFailure.Broadcast(static_cast<int32>(Data->m_eResult), Data->m_bStillRetrying);
		}
	}

	void HandleEncryptedTicketResponse(EncryptedAppTicketResponse_t* Data, bool bIOFailure)
	{
		UESteamUserSubsystem* Subsystem = Owner.Get();
		if (!Subsystem)
		{
			return;
		}

		FString HexTicket;
		bool bSuccess = false;
		if (!bIOFailure && Data->m_eResult == k_EResultOK && SteamUser())
		{
			uint8 Buffer[2048];
			uint32 TicketSize = 0;
			if (SteamUser()->GetEncryptedAppTicket(Buffer, sizeof(Buffer), &TicketSize) && TicketSize > 0)
			{
				HexTicket = BytesToHex(Buffer, TicketSize);
				bSuccess = true;
			}
		}
		Subsystem->OnEncryptedAppTicketReady.Broadcast(bSuccess, HexTicket);
	}

	void HandleMarketEligibilityResponse(MarketEligibilityResponse_t* Data, bool bIOFailure)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnMarketEligibility.Broadcast(!bIOFailure && Data->m_bAllowed);
		}
	}

	void HandleStoreAuthURLResponse(StoreAuthURLResponse_t* Data, bool bIOFailure)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			FString Url;
			const bool bSuccess = !bIOFailure;
			if (bSuccess)
			{
				Url = UTF8_TO_TCHAR(Data->m_szURL);
			}
			Subsystem->OnStoreAuthURL.Broadcast(bSuccess && !Url.IsEmpty(), Url);
		}
	}

	void HandleDurationControlResponse(DurationControl_t* Data, bool bIOFailure)
	{
		if (bIOFailure)
		{
			if (UESteamUserSubsystem* Subsystem = Owner.Get())
			{
				Subsystem->OnDurationControl.Broadcast(false, 0, 0, 0);
			}
			return;
		}
		BroadcastDurationControl(Data);
	}

	// DurationControl_t also arrives asynchronously on Steam's own playtime timers (not just GetDurationControl).
	void HandleDurationControlTimer(DurationControl_t* Data)
	{
		BroadcastDurationControl(Data);
	}

	void BroadcastDurationControl(DurationControl_t* Data)
	{
		if (UESteamUserSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnDurationControl.Broadcast(
				Data->m_bApplicable,
				static_cast<int32>(Data->m_csecsLast5h),
				static_cast<int32>(Data->m_csecsToday),
				static_cast<int32>(Data->m_csecsRemaining));
		}
	}

	TWeakObjectPtr<UESteamUserSubsystem> Owner;
	CCallback<FESteamUserCallbacks, GetAuthSessionTicketResponse_t> AuthTicketResponse;
	CCallback<FESteamUserCallbacks, GetTicketForWebApiResponse_t> WebApiTicketResponse;
	CCallback<FESteamUserCallbacks, ValidateAuthTicketResponse_t> ValidateTicketResponse;
	CCallback<FESteamUserCallbacks, SteamServersConnected_t> ServersConnected;
	CCallback<FESteamUserCallbacks, SteamServersDisconnected_t> ServersDisconnected;
	CCallback<FESteamUserCallbacks, SteamServerConnectFailure_t> ServerConnectFailure;
	CCallback<FESteamUserCallbacks, DurationControl_t> DurationControlTimer;
	CCallResult<FESteamUserCallbacks, EncryptedAppTicketResponse_t> EncryptedTicketResult;
	CCallResult<FESteamUserCallbacks, MarketEligibilityResponse_t> MarketEligibilityResult;
	CCallResult<FESteamUserCallbacks, StoreAuthURLResponse_t> StoreAuthURLResult;
	CCallResult<FESteamUserCallbacks, DurationControl_t> DurationControlResult;
};
#else
class FESteamUserCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamUserSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamUserSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamUserCallbacks>(this);
	}
#endif
}

void UESteamUserSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

FESteamId UESteamUserSubsystem::GetLocalSteamId() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		return FESteamId(SteamUser()->GetSteamID().ConvertToUint64());
	}
#endif
	return FESteamId();
}

bool UESteamUserSubsystem::IsLoggedOn() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BLoggedOn();
#else
	return false;
#endif
}

int32 UESteamUserSubsystem::GetPlayerSteamLevel() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		return SteamUser()->GetPlayerSteamLevel();
	}
#endif
	return 0;
}

int32 UESteamUserSubsystem::GetGameBadgeLevel(int32 Series, bool bFoil) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		return SteamUser()->GetGameBadgeLevel(Series, bFoil);
	}
#endif
	return 0;
}

bool UESteamUserSubsystem::IsBehindNAT() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BIsBehindNAT();
#else
	return false;
#endif
}

bool UESteamUserSubsystem::IsPhoneVerified() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BIsPhoneVerified();
#else
	return false;
#endif
}

bool UESteamUserSubsystem::IsTwoFactorEnabled() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BIsTwoFactorEnabled();
#else
	return false;
#endif
}

bool UESteamUserSubsystem::IsPhoneIdentifying() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BIsPhoneIdentifying();
#else
	return false;
#endif
}

bool UESteamUserSubsystem::IsPhoneRequiringVerification() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamUser() && SteamUser()->BIsPhoneRequiringVerification();
#else
	return false;
#endif
}

EESteamUserHasLicenseResult UESteamUserSubsystem::UserHasLicenseForApp(FESteamId SteamId, int32 AppId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		switch (SteamUser()->UserHasLicenseForApp(CSteamID(SteamId.Value), static_cast<AppId_t>(AppId)))
		{
		case k_EUserHasLicenseResultHasLicense:        return EESteamUserHasLicenseResult::HasLicense;
		case k_EUserHasLicenseResultDoesNotHaveLicense:return EESteamUserHasLicenseResult::DoesNotHaveLicense;
		case k_EUserHasLicenseResultNoAuth:            return EESteamUserHasLicenseResult::NoAuth;
		default:                                       return EESteamUserHasLicenseResult::NoAuth;
		}
	}
#endif
	return EESteamUserHasLicenseResult::NoAuth;
}

FString UESteamUserSubsystem::GetUserDataFolder() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		char PathBuffer[1024] = {};
		if (SteamUser()->GetUserDataFolder(PathBuffer, sizeof(PathBuffer)))
		{
			return UTF8_TO_TCHAR(PathBuffer);
		}
	}
#endif
	return FString();
}

void UESteamUserSubsystem::AdvertiseGame(FESteamId GameServerId, const FString& ServerIp, int32 ServerPort)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		const uint32 IPv4 = ESteamUserHelpers::ParseIPv4HostOrder(ServerIp);
		SteamUser()->AdvertiseGame(CSteamID(GameServerId.Value), IPv4, static_cast<uint16>(ServerPort));
	}
#endif
}

int32 UESteamUserSubsystem::GetAuthSessionTicket(FString& OutHexTicket)
{
	OutHexTicket.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser())
	{
		LogSteamUnavailable(TEXT("GetAuthSessionTicket"));
		return 0;
	}

	uint8 TicketBuffer[1024];
	uint32 TicketSize = 0;
#if ESTEAM_SDK_AT_LEAST(157)
	const HAuthTicket Handle = SteamUser()->GetAuthSessionTicket(TicketBuffer, sizeof(TicketBuffer), &TicketSize, nullptr);
#else
	const HAuthTicket Handle = SteamUser()->GetAuthSessionTicket(TicketBuffer, sizeof(TicketBuffer), &TicketSize);
#endif
	if (Handle == k_HAuthTicketInvalid || TicketSize == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthSessionTicket failed"));
		return 0;
	}

	OutHexTicket = BytesToHex(TicketBuffer, TicketSize);
	return static_cast<int32>(Handle);
#else
	return 0;
#endif
}

int32 UESteamUserSubsystem::RequestWebApiAuthTicket(const FString& RemoteServiceIdentity)
{
#if WITH_EXTENDEDSTEAM_SDK && ESTEAM_SDK_AT_LEAST(157)
	if (!IsSteamAvailable() || !SteamUser())
	{
		LogSteamUnavailable(TEXT("RequestWebApiAuthTicket"));
		return 0;
	}

	const HAuthTicket Handle = RemoteServiceIdentity.IsEmpty()
		? SteamUser()->GetAuthTicketForWebApi(nullptr)
		: SteamUser()->GetAuthTicketForWebApi(TCHAR_TO_UTF8(*RemoteServiceIdentity));

	if (Handle == k_HAuthTicketInvalid)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestWebApiAuthTicket failed"));
		return 0;
	}
	return static_cast<int32>(Handle);
#else
	LogSteamUnavailable(TEXT("RequestWebApiAuthTicket"));
	return 0;
#endif
}

void UESteamUserSubsystem::CancelAuthTicket(int32 TicketHandle)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser() && TicketHandle != 0)
	{
		SteamUser()->CancelAuthTicket(static_cast<HAuthTicket>(TicketHandle));
	}
#endif
}

bool UESteamUserSubsystem::RequestEncryptedAppTicket()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestEncryptedAppTicket"));
		return false;
	}

	if (Callbacks->IsEncryptedTicketBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestEncryptedAppTicket: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamUser()->RequestEncryptedAppTicket(nullptr, 0);
	if (Call == k_uAPICallInvalid)
	{
		return false;
	}
	Callbacks->TrackEncryptedTicketRequest(Call);
	return true;
#else
	return false;
#endif
}

EESteamBeginAuthResult UESteamUserSubsystem::BeginAuthSession(const FString& HexTicket, FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser())
	{
		return EESteamBeginAuthResult::SteamUnavailable;
	}

	// Validate before decoding: HexToBytes assumes clean, even-length hex. Odd-length or non-hex
	// input would otherwise produce a truncated/garbage ticket buffer and a misleading result.
	auto IsHexChar = [](TCHAR C)
	{
		return (C >= TEXT('0') && C <= TEXT('9'))
			|| (C >= TEXT('a') && C <= TEXT('f'))
			|| (C >= TEXT('A') && C <= TEXT('F'));
	};

	const int32 HexLen = HexTicket.Len();
	if (HexLen == 0 || (HexLen % 2) != 0)
	{
		return EESteamBeginAuthResult::InvalidTicket;
	}
	for (int32 Index = 0; Index < HexLen; ++Index)
	{
		if (!IsHexChar(HexTicket[Index]))
		{
			return EESteamBeginAuthResult::InvalidTicket;
		}
	}

	TArray<uint8> TicketBytes;
	TicketBytes.SetNumUninitialized(HexLen / 2);
	HexToBytes(HexTicket, TicketBytes.GetData());

	switch (SteamUser()->BeginAuthSession(TicketBytes.GetData(), TicketBytes.Num(), CSteamID(SteamId.Value)))
	{
	case k_EBeginAuthSessionResultOK:               return EESteamBeginAuthResult::Ok;
	case k_EBeginAuthSessionResultInvalidTicket:    return EESteamBeginAuthResult::InvalidTicket;
	case k_EBeginAuthSessionResultDuplicateRequest: return EESteamBeginAuthResult::DuplicateRequest;
	case k_EBeginAuthSessionResultInvalidVersion:   return EESteamBeginAuthResult::InvalidVersion;
	case k_EBeginAuthSessionResultGameMismatch:     return EESteamBeginAuthResult::GameMismatch;
	case k_EBeginAuthSessionResultExpiredTicket:    return EESteamBeginAuthResult::ExpiredTicket;
	default:                                        return EESteamBeginAuthResult::InvalidTicket;
	}
#else
	return EESteamBeginAuthResult::SteamUnavailable;
#endif
}

void UESteamUserSubsystem::EndAuthSession(FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser() && SteamId.IsValid())
	{
		SteamUser()->EndAuthSession(CSteamID(SteamId.Value));
	}
#endif
}

bool UESteamUserSubsystem::RequestStoreAuthURL(const FString& RedirectUrl)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestStoreAuthURL"));
		return false;
	}

	if (Callbacks->IsStoreAuthURLBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestStoreAuthURL: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamUser()->RequestStoreAuthURL(TCHAR_TO_UTF8(*RedirectUrl));
	if (Call == k_uAPICallInvalid)
	{
		return false;
	}
	Callbacks->TrackStoreAuthURLRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("RequestStoreAuthURL"));
	return false;
#endif
}

bool UESteamUserSubsystem::GetMarketEligibility()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetMarketEligibility"));
		return false;
	}

	if (Callbacks->IsMarketEligibilityBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetMarketEligibility: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamUser()->GetMarketEligibility();
	if (Call == k_uAPICallInvalid)
	{
		return false;
	}
	Callbacks->TrackMarketEligibilityRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("GetMarketEligibility"));
	return false;
#endif
}

bool UESteamUserSubsystem::GetDurationControl()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamUser() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetDurationControl"));
		return false;
	}

	if (Callbacks->IsDurationControlBusy())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetDurationControl: a request is already in flight; ignoring the new one"));
		return false;
	}

	const SteamAPICall_t Call = SteamUser()->GetDurationControl();
	if (Call == k_uAPICallInvalid)
	{
		return false;
	}
	Callbacks->TrackDurationControlRequest(Call);
	return true;
#else
	LogSteamUnavailable(TEXT("GetDurationControl"));
	return false;
#endif
}

bool UESteamUserSubsystem::SetDurationControlOnlineState(int32 NewState)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamUser())
	{
		return SteamUser()->BSetDurationControlOnlineState(static_cast<EDurationControlOnlineState>(NewState));
	}
#endif
	return false;
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Networking/ESteamNetworkingSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ESteamNetworkingPrivate
{
	EP2PSend ToSteamSendType(EESteamP2PSendType SendType)
	{
		switch (SendType)
		{
		case EESteamP2PSendType::UnreliableNoDelay:     return k_EP2PSendUnreliableNoDelay;
		case EESteamP2PSendType::Reliable:              return k_EP2PSendReliable;
		case EESteamP2PSendType::ReliableWithBuffering: return k_EP2PSendReliableWithBuffering;
		case EESteamP2PSendType::Unreliable:
		default:                                        return k_EP2PSendUnreliable;
		}
	}

	FString IpToString(uint32 Ip)
	{
		return FString::Printf(TEXT("%u.%u.%u.%u"), (Ip >> 24) & 0xFF, (Ip >> 16) & 0xFF, (Ip >> 8) & 0xFF, Ip & 0xFF);
	}

	/** Maps the raw EP2PSessionError code (stored as uint8 in P2PSessionState_t) to the Blueprint enum. */
	EESteamP2PSessionError ToESteamP2PSessionError(uint8 ErrorCode)
	{
		switch (ErrorCode)
		{
		case k_EP2PSessionErrorNotRunningApp_DELETED:          return EESteamP2PSessionError::NotRunningApp;
		case k_EP2PSessionErrorNoRightsToApp:                  return EESteamP2PSessionError::NoRightsToApp;
		case k_EP2PSessionErrorDestinationNotLoggedIn_DELETED: return EESteamP2PSessionError::DestinationNotLoggedIn;
		case k_EP2PSessionErrorTimeout:                        return EESteamP2PSessionError::Timeout;
		case k_EP2PSessionErrorNone:
		default:                                               return EESteamP2PSessionError::None;
		}
	}
}

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamNetworkingCallbacks
{
public:
	explicit FESteamNetworkingCallbacks(UESteamNetworkingSubsystem* InOwner)
		: Owner(InOwner)
		, SessionRequestCallback(this, &FESteamNetworkingCallbacks::HandleSessionRequest)
		, SessionConnectFailCallback(this, &FESteamNetworkingCallbacks::HandleSessionConnectFail)
	{
	}

private:
	void HandleSessionRequest(P2PSessionRequest_t* Data)
	{
		if (UESteamNetworkingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnP2PSessionRequested.Broadcast(FESteamId(Data->m_steamIDRemote.ConvertToUint64()));
		}
	}

	void HandleSessionConnectFail(P2PSessionConnectFail_t* Data)
	{
		if (UESteamNetworkingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnP2PSessionConnectFailed.Broadcast(
				FESteamId(Data->m_steamIDRemote.ConvertToUint64()),
				static_cast<int32>(Data->m_eP2PSessionError));
		}
	}

	TWeakObjectPtr<UESteamNetworkingSubsystem> Owner;
	CCallback<FESteamNetworkingCallbacks, P2PSessionRequest_t> SessionRequestCallback;
	CCallback<FESteamNetworkingCallbacks, P2PSessionConnectFail_t> SessionConnectFailCallback;
};
#else
class FESteamNetworkingCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamNetworkingSubsystem::Deinitialize()
{
	Super::Deinitialize();
	// HandleSteamClientShutdown already ran when Steam was up; this covers the Steam-down path.
	RemovePollTicker();
	Callbacks.Reset();
}

void UESteamNetworkingSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamNetworkingCallbacks>(this);
	}

	if (!PollTickerHandle.IsValid())
	{
		PollTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UESteamNetworkingSubsystem::TickPollPackets));
	}
#endif
}

void UESteamNetworkingSubsystem::HandleSteamClientShutdown()
{
	RemovePollTicker();
	Callbacks.Reset();
}

bool UESteamNetworkingSubsystem::TickPollPackets(float /*DeltaTime*/)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworking())
	{
		for (int32 Channel = 0; Channel < PolledChannelCount; ++Channel)
		{
			uint32 PacketSize = 0;
			while (SteamNetworking()->IsP2PPacketAvailable(&PacketSize, Channel))
			{
				TArray<uint8> Data;
				Data.SetNumUninitialized(static_cast<int32>(PacketSize));

				uint32 BytesRead = 0;
				CSteamID Sender;
				if (!SteamNetworking()->ReadP2PPacket(Data.GetData(), PacketSize, &BytesRead, &Sender, Channel))
				{
					break;
				}

				Data.SetNum(static_cast<int32>(BytesRead), EAllowShrinking::No);
				OnP2PPacketReceived.Broadcast(FESteamId(Sender.ConvertToUint64()), Data, Channel);
			}
		}
	}
#endif
	return true;
}

void UESteamNetworkingSubsystem::RemovePollTicker()
{
	if (PollTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollTickerHandle);
		PollTickerHandle.Reset();
	}
}

bool UESteamNetworkingSubsystem::SendP2PPacket(FESteamId Target, const TArray<uint8>& Data, EESteamP2PSendType SendType, int32 Channel)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworking())
	{
		LogSteamUnavailable(TEXT("SendP2PPacket"));
		return false;
	}

	if (!Target.IsValid() || Data.Num() == 0)
	{
		return false;
	}

	return SteamNetworking()->SendP2PPacket(
		CSteamID(Target.Value), Data.GetData(), static_cast<uint32>(Data.Num()),
		ESteamNetworkingPrivate::ToSteamSendType(SendType), Channel);
#else
	LogSteamUnavailable(TEXT("SendP2PPacket"));
	return false;
#endif
}

void UESteamNetworkingSubsystem::SetPolledChannelCount(int32 Count)
{
	PolledChannelCount = FMath::Max(1, Count);
}

bool UESteamNetworkingSubsystem::AcceptP2PSessionWithUser(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworking())
	{
		LogSteamUnavailable(TEXT("AcceptP2PSessionWithUser"));
		return false;
	}
	return User.IsValid() && SteamNetworking()->AcceptP2PSessionWithUser(CSteamID(User.Value));
#else
	LogSteamUnavailable(TEXT("AcceptP2PSessionWithUser"));
	return false;
#endif
}

bool UESteamNetworkingSubsystem::CloseP2PSessionWithUser(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworking() && User.IsValid())
	{
		return SteamNetworking()->CloseP2PSessionWithUser(CSteamID(User.Value));
	}
#endif
	return false;
}

bool UESteamNetworkingSubsystem::CloseP2PChannelWithUser(FESteamId User, int32 Channel)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamNetworking() && User.IsValid())
	{
		return SteamNetworking()->CloseP2PChannelWithUser(CSteamID(User.Value), Channel);
	}
#endif
	return false;
}

bool UESteamNetworkingSubsystem::AllowP2PPacketRelay(bool bAllow)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworking())
	{
		LogSteamUnavailable(TEXT("AllowP2PPacketRelay"));
		return false;
	}
	return SteamNetworking()->AllowP2PPacketRelay(bAllow);
#else
	LogSteamUnavailable(TEXT("AllowP2PPacketRelay"));
	return false;
#endif
}

bool UESteamNetworkingSubsystem::GetP2PSessionState(FESteamId User, FESteamP2PSessionState& OutState)
{
	OutState = FESteamP2PSessionState();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamNetworking())
	{
		return false;
	}

	P2PSessionState_t State;
	FMemory::Memzero(State);
	if (!User.IsValid() || !SteamNetworking()->GetP2PSessionState(CSteamID(User.Value), &State))
	{
		return false;
	}

	OutState.bConnectionActive = State.m_bConnectionActive != 0;
	OutState.bConnecting = State.m_bConnecting != 0;
	OutState.P2PSessionError = ESteamNetworkingPrivate::ToESteamP2PSessionError(State.m_eP2PSessionError);
	OutState.bUsingRelay = State.m_bUsingRelay != 0;
	OutState.BytesQueuedForSend = State.m_nBytesQueuedForSend;
	OutState.PacketsQueuedForSend = State.m_nPacketsQueuedForSend;
	OutState.RemoteIP = ESteamNetworkingPrivate::IpToString(State.m_nRemoteIP);
	OutState.RemotePort = static_cast<int32>(State.m_nRemotePort);
	return true;
#else
	return false;
#endif
}

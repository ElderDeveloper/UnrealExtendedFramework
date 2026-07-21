// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "GameServer/ESteamGameServerSubsystem.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	void LogGameServerUnavailable(const TCHAR* Context)
	{
		UE_LOG(LogExtendedSteam, Warning,
			TEXT("%s: Steam game server is not available (InitializeSteamGameServer has not succeeded)"), Context);
	}
}

/**
 * Native Steam GAME SERVER callback listeners; alive only while the game server API is initialized.
 *
 * All four callbacks below are dispatched by SteamGameServer_RunCallbacks when they originate
 * from the game server pipe, so they register with the gameserver form CCallback<T, P, true>.
 * SteamServersConnected_t / SteamServerConnectFailure_t / SteamServersDisconnected_t are declared
 * in isteamuser.h (shared with the client connection state), and GSPolicyResponse_t even carries a
 * k_iSteamUserCallbacks-range id — but for a logged-on game server they all arrive on the
 * game-server pipe.
 */
class FESteamGameServerCallbacks
{
public:
	explicit FESteamGameServerCallbacks(UESteamGameServerSubsystem* InOwner)
		: Owner(InOwner)
		, ServersConnectedCallback(this, &FESteamGameServerCallbacks::HandleServersConnected)
		, ServerConnectFailureCallback(this, &FESteamGameServerCallbacks::HandleServerConnectFailure)
		, ServersDisconnectedCallback(this, &FESteamGameServerCallbacks::HandleServersDisconnected)
		, PolicyResponseCallback(this, &FESteamGameServerCallbacks::HandlePolicyResponse)
	{
	}

	void TrackAssociateWithClan(SteamAPICall_t Call)
	{
		AssociateWithClanResult.Set(Call, this, &FESteamGameServerCallbacks::HandleAssociateWithClan);
	}

	void TrackComputeNewPlayerCompatibility(SteamAPICall_t Call)
	{
		ComputeCompatibilityResult.Set(Call, this, &FESteamGameServerCallbacks::HandleComputeCompatibility);
	}

private:
	void HandleServersConnected(SteamServersConnected_t* Data)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnConnected.Broadcast();
		}
	}

	void HandleServerConnectFailure(SteamServerConnectFailure_t* Data)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnConnectFailure.Broadcast(Data->m_bStillRetrying);
		}
	}

	void HandleServersDisconnected(SteamServersDisconnected_t* Data)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnDisconnected.Broadcast();
		}
	}

	void HandlePolicyResponse(GSPolicyResponse_t* Data)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPolicyResponse.Broadcast(Data->m_bSecure != 0);
		}
	}

	void HandleAssociateWithClan(AssociateWithClanResult_t* Data, bool bIOFailure)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAssociateWithClan.Broadcast(!bIOFailure && Data->m_eResult == k_EResultOK);
		}
	}

	void HandleComputeCompatibility(ComputeNewPlayerCompatibilityResult_t* Data, bool bIOFailure)
	{
		if (UESteamGameServerSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnPlayerCompatibility.Broadcast(
				bSuccess,
				bSuccess ? Data->m_cPlayersThatDontLikeCandidate : 0,
				bSuccess ? Data->m_cPlayersThatCandidateDoesntLike : 0,
				bSuccess ? Data->m_cClanPlayersThatDontLikeCandidate : 0);
		}
	}

	TWeakObjectPtr<UESteamGameServerSubsystem> Owner;
	CCallback<FESteamGameServerCallbacks, SteamServersConnected_t, true> ServersConnectedCallback;
	CCallback<FESteamGameServerCallbacks, SteamServerConnectFailure_t, true> ServerConnectFailureCallback;
	CCallback<FESteamGameServerCallbacks, SteamServersDisconnected_t, true> ServersDisconnectedCallback;
	CCallback<FESteamGameServerCallbacks, GSPolicyResponse_t, true> PolicyResponseCallback;
	// AssociateWithClan / ComputeNewPlayerCompatibility issue calls on the game server pipe; their
	// results are dispatched by SteamGameServer_RunCallbacks, so plain CCallResults suffice.
	CCallResult<FESteamGameServerCallbacks, AssociateWithClanResult_t> AssociateWithClanResult;
	CCallResult<FESteamGameServerCallbacks, ComputeNewPlayerCompatibilityResult_t> ComputeCompatibilityResult;
};
#else
class FESteamGameServerCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamGameServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The base class hooks (HandleSteamClientInitialized/Shutdown) follow the Steam CLIENT API,
	// which is independent of the game server API wrapped here — bind to the shared module's
	// dedicated game server lifecycle delegates instead.
	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		GameServerInitializedHandle = Shared.OnSteamGameServerInitialized.AddUObject(this, &UESteamGameServerSubsystem::HandleGameServerInitialized);
		GameServerShutdownHandle = Shared.OnSteamGameServerShutdown.AddUObject(this, &UESteamGameServerSubsystem::HandleGameServerShutdown);

		if (Shared.IsSteamGameServerInitialized())
		{
			HandleGameServerInitialized();
		}
	}
}

void UESteamGameServerSubsystem::Deinitialize()
{
	if (FExtendedSteamSharedModule::IsModuleAvailable())
	{
		FExtendedSteamSharedModule& Shared = FExtendedSteamSharedModule::Get();
		Shared.OnSteamGameServerInitialized.Remove(GameServerInitializedHandle);
		Shared.OnSteamGameServerShutdown.Remove(GameServerShutdownHandle);
	}

	Callbacks.Reset();
	Super::Deinitialize();
}

void UESteamGameServerSubsystem::HandleGameServerInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamGameServerCallbacks>(this);
	}
#endif
}

void UESteamGameServerSubsystem::HandleGameServerShutdown()
{
	Callbacks.Reset();
}

bool UESteamGameServerSubsystem::IsGameServerAvailable() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamGameServerInitialized()
		&& SteamGameServer() != nullptr;
#else
	return false;
#endif
}

void UESteamGameServerSubsystem::SetProduct(const FString& Product)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetProduct(TCHAR_TO_UTF8(*Product));
	}
#endif
}

void UESteamGameServerSubsystem::SetGameDescription(const FString& GameDescription)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetGameDescription(TCHAR_TO_UTF8(*GameDescription));
	}
#endif
}

void UESteamGameServerSubsystem::SetModDir(const FString& ModDir)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetModDir(TCHAR_TO_UTF8(*ModDir));
	}
#endif
}

void UESteamGameServerSubsystem::SetDedicatedServer(bool bDedicated)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetDedicatedServer(bDedicated);
	}
#endif
}

void UESteamGameServerSubsystem::SetMaxPlayerCount(int32 MaxPlayers)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetMaxPlayerCount(FMath::Max(0, MaxPlayers));
	}
#endif
}

void UESteamGameServerSubsystem::SetBotPlayerCount(int32 BotPlayers)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetBotPlayerCount(FMath::Max(0, BotPlayers));
	}
#endif
}

void UESteamGameServerSubsystem::SetServerName(const FString& ServerName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetServerName(TCHAR_TO_UTF8(*ServerName));
	}
#endif
}

void UESteamGameServerSubsystem::SetMapName(const FString& MapName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetMapName(TCHAR_TO_UTF8(*MapName));
	}
#endif
}

void UESteamGameServerSubsystem::SetPasswordProtected(bool bPasswordProtected)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetPasswordProtected(bPasswordProtected);
	}
#endif
}

void UESteamGameServerSubsystem::SetSpectatorPort(int32 SpectatorPort)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetSpectatorPort(static_cast<uint16>(FMath::Clamp(SpectatorPort, 0, 65535)));
	}
#endif
}

void UESteamGameServerSubsystem::SetSpectatorServerName(const FString& SpectatorServerName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetSpectatorServerName(TCHAR_TO_UTF8(*SpectatorServerName));
	}
#endif
}

void UESteamGameServerSubsystem::SetGameTags(const FString& GameTags)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetGameTags(TCHAR_TO_UTF8(*GameTags));
	}
#endif
}

void UESteamGameServerSubsystem::SetGameData(const FString& GameData)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetGameData(TCHAR_TO_UTF8(*GameData));
	}
#endif
}

void UESteamGameServerSubsystem::SetRegion(const FString& Region)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetRegion(TCHAR_TO_UTF8(*Region));
	}
#endif
}

void UESteamGameServerSubsystem::SetAdvertiseServerActive(bool bActive)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetAdvertiseServerActive(bActive);
	}
#endif
}

void UESteamGameServerSubsystem::SetKeyValue(const FString& Key, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->SetKeyValue(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
	}
#endif
}

void UESteamGameServerSubsystem::ClearAllKeyValues()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->ClearAllKeyValues();
	}
#endif
}

bool UESteamGameServerSubsystem::BUpdateUserData(FESteamId User, const FString& PlayerName, int32 Score)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		return SteamGameServer()->BUpdateUserData(
			CSteamID(User.Value),
			TCHAR_TO_UTF8(*PlayerName),
			static_cast<uint32>(FMath::Max(0, Score)));
	}
#endif
	return false;
}

void UESteamGameServerSubsystem::LogOnAnonymous()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerAvailable())
	{
		LogGameServerUnavailable(TEXT("LogOnAnonymous"));
		return;
	}
	SteamGameServer()->LogOnAnonymous();
#endif
}

void UESteamGameServerSubsystem::LogOn(const FString& Token)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerAvailable())
	{
		LogGameServerUnavailable(TEXT("LogOn"));
		return;
	}
	SteamGameServer()->LogOn(TCHAR_TO_UTF8(*Token));
#endif
}

void UESteamGameServerSubsystem::LogOff()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		SteamGameServer()->LogOff();
	}
#endif
}

bool UESteamGameServerSubsystem::IsLoggedOn() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsGameServerAvailable() && SteamGameServer()->BLoggedOn();
#else
	return false;
#endif
}

bool UESteamGameServerSubsystem::IsSecure() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsGameServerAvailable() && SteamGameServer()->BSecure();
#else
	return false;
#endif
}

FESteamId UESteamGameServerSubsystem::GetServerSteamId() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		return FESteamId(SteamGameServer()->GetSteamID().ConvertToUint64());
	}
#endif
	return FESteamId();
}

FString UESteamGameServerSubsystem::GetPublicIP() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsGameServerAvailable())
	{
		const SteamIPAddress_t PublicIP = SteamGameServer()->GetPublicIP();
		if (PublicIP.m_eType == k_ESteamIPTypeIPv4 && PublicIP.m_unIPv4 != 0)
		{
			// m_unIPv4 is in host byte order.
			return FString::Printf(TEXT("%u.%u.%u.%u"),
				(PublicIP.m_unIPv4 >> 24) & 0xFF,
				(PublicIP.m_unIPv4 >> 16) & 0xFF,
				(PublicIP.m_unIPv4 >> 8) & 0xFF,
				PublicIP.m_unIPv4 & 0xFF);
		}
	}
#endif
	return FString();
}

bool UESteamGameServerSubsystem::WasRestartRequested() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsGameServerAvailable() && SteamGameServer()->WasRestartRequested();
#else
	return false;
#endif
}

void UESteamGameServerSubsystem::AssociateWithClan(FESteamId ClanId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerAvailable() || !Callbacks)
	{
		LogGameServerUnavailable(TEXT("AssociateWithClan"));
		OnAssociateWithClan.Broadcast(false);
		return;
	}
	const SteamAPICall_t Call = SteamGameServer()->AssociateWithClan(CSteamID(ClanId.Value));
	if (Call == k_uAPICallInvalid)
	{
		// Dispatch failed: no CallResult will ever complete, so report failure now (matching the
		// client subsystems) instead of leaving the caller waiting on a delegate that never fires.
		OnAssociateWithClan.Broadcast(false);
		return;
	}
	Callbacks->TrackAssociateWithClan(Call);
#else
	LogGameServerUnavailable(TEXT("AssociateWithClan"));
	OnAssociateWithClan.Broadcast(false);
#endif
}

void UESteamGameServerSubsystem::ComputeNewPlayerCompatibility(FESteamId NewPlayer)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsGameServerAvailable() || !Callbacks)
	{
		LogGameServerUnavailable(TEXT("ComputeNewPlayerCompatibility"));
		OnPlayerCompatibility.Broadcast(false, 0, 0, 0);
		return;
	}
	const SteamAPICall_t Call = SteamGameServer()->ComputeNewPlayerCompatibility(CSteamID(NewPlayer.Value));
	if (Call == k_uAPICallInvalid)
	{
		// Dispatch failed: no CallResult will ever complete, so report failure now (matching the
		// client subsystems) instead of leaving the caller waiting on a delegate that never fires.
		OnPlayerCompatibility.Broadcast(false, 0, 0, 0);
		return;
	}
	Callbacks->TrackComputeNewPlayerCompatibility(Call);
#else
	LogGameServerUnavailable(TEXT("ComputeNewPlayerCompatibility"));
	OnPlayerCompatibility.Broadcast(false, 0, 0, 0);
#endif
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Session/OnlineSessionExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "Friends/OnlineFriendsExtendedSteam.h"
#include "Presence/OnlinePresenceExtendedSteam.h"
#include "ExternalUI/OnlineExternalUIExtendedSteam.h"
#include "UserCloud/OnlineUserCloudExtendedSteam.h"
#include "SharedCloud/OnlineSharedCloudExtendedSteam.h"
#include "Achievements/OnlineAchievementsExtendedSteam.h"
#include "Leaderboards/OnlineLeaderboardsExtendedSteam.h"
#include "Voice/OnlineVoiceExtendedSteam.h"
#include "Auth/OnlineAuthExtendedSteam.h"
#include "EncryptedAppTicket/OnlineEncryptedAppTicketExtendedSteam.h"
#include "Ping/OnlinePingExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"
#include "Shared/ESteamSettings.h"

#include "Misc/ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "OnlineSubsystemExtendedSteam"

namespace
{
	/** Engine ini section for OSS-level overrides (wins over UESteamSettings when a key is present). */
	const TCHAR* GetOSSConfigSection()
	{
		return TEXT("OnlineSubsystemExtendedSteam");
	}
}

void FOnlineSubsystemExtendedSteam::ReadConfig()
{
	const UESteamSettings* Settings = UESteamSettings::Get();

	// App id: [OnlineSubsystemExtendedSteam] SteamDevAppId wins when present and positive,
	// otherwise fall back to the project settings app id.
	ResolvedAppId = Settings->SteamAppId;
	int32 ConfigAppId = 0;
	if (GConfig->GetInt(GetOSSConfigSection(), TEXT("SteamDevAppId"), ConfigAppId, GEngineIni) && ConfigAppId > 0)
	{
		ResolvedAppId = ConfigAppId;
	}

	// bRelaunchInSteam: UESteamSettings also declares this flag; the OSS ini key wins when present.
	// The actual relaunch (shipping only) is executed by FExtendedSteamSharedModule during Steam
	// client init — this subsystem only resolves and exposes the authoritative config value.
	bRelaunchInSteam = Settings->bRelaunchInSteam;
	bool bConfigRelaunch = false;
	if (GConfig->GetBool(GetOSSConfigSection(), TEXT("bRelaunchInSteam"), bConfigRelaunch, GEngineIni))
	{
		bRelaunchInSteam = bConfigRelaunch;
	}
}

bool FOnlineSubsystemExtendedSteam::Init()
{
	ReadConfig();

#if !WITH_EXTENDEDSTEAM_SDK
	UE_LOG(LogExtendedSteam, Log, TEXT("FOnlineSubsystemExtendedSteam::Init: built without Steamworks SDK support; subsystem unavailable"));
	return false;
#else
	FExtendedSteamSharedModule& SteamShared = FExtendedSteamSharedModule::Get();

	if (IsDedicated())
	{
		// Dedicated servers use the Steam game server API, and its setup is explicit: ports, auth
		// mode and version are deployment decisions, so server bootstrap code must have called
		// FExtendedSteamSharedModule::InitializeSteamGameServer before this subsystem is created.
		// We never auto-initialize the game server here.
		if (!SteamShared.IsSteamGameServerInitialized())
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("FOnlineSubsystemExtendedSteam::Init: dedicated server without an initialized Steam game server API. ")
				TEXT("Call FExtendedSteamSharedModule::InitializeSteamGameServer before requesting the subsystem."));
			return false;
		}
	}
	else
	{
		// Client: the shared module owns Steam client init; if it has not run yet (e.g. auto-init
		// disabled), attempt it once here.
		if (!SteamShared.IsSteamClientInitialized() && !SteamShared.InitializeSteamClient())
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("FOnlineSubsystemExtendedSteam::Init: Steam client API unavailable (is the Steam client running?). ")
				TEXT("The instance will be destroyed and IOnlineSubsystem::Get(\"%s\") will return null."),
				*ESTEAM_SUBSYSTEM.ToString());
			return false;
		}
	}

	SessionInterface = MakeShared<FOnlineSessionExtendedSteam, ESPMode::ThreadSafe>(this);
	IdentityInterface = MakeShared<FOnlineIdentityExtendedSteam, ESPMode::ThreadSafe>(this);
	FriendsInterface = MakeShared<FOnlineFriendsExtendedSteam, ESPMode::ThreadSafe>(this);
	PresenceInterface = MakeShared<FOnlinePresenceExtendedSteam, ESPMode::ThreadSafe>(this);
	ExternalUIInterface = MakeShared<FOnlineExternalUIExtendedSteam, ESPMode::ThreadSafe>(this);
	UserCloudInterface = MakeShared<FOnlineUserCloudExtendedSteam, ESPMode::ThreadSafe>(this);
	SharedCloudInterface = MakeShared<FOnlineSharedCloudExtendedSteam, ESPMode::ThreadSafe>(this);
	AchievementsInterface = MakeShared<FOnlineAchievementsExtendedSteam, ESPMode::ThreadSafe>(this);
	LeaderboardsInterface = MakeShared<FOnlineLeaderboardsExtendedSteam, ESPMode::ThreadSafe>(this);
	VoiceInterface = MakeShared<FOnlineVoiceExtendedSteam, ESPMode::ThreadSafe>(this);
	VoiceInterface->Init();

	// Plugin-specific (non-IOnlineSubsystem) interfaces, constructed after the standard ones.
	AuthInterface = MakeShared<FOnlineAuthExtendedSteam, ESPMode::ThreadSafe>(this);
	EncryptedAppTicketInterface = MakeShared<FOnlineEncryptedAppTicketExtendedSteam, ESPMode::ThreadSafe>(this);
	PingInterface = MakeShared<FOnlinePingExtendedSteam, ESPMode::ThreadSafe>(this);

	UE_LOG(LogExtendedSteam, Log, TEXT("Extended Steam online subsystem initialized (instance: %s, app id: %s, dedicated: %s)"),
		*GetInstanceName().ToString(), *GetAppId(), IsDedicated() ? TEXT("yes") : TEXT("no"));
	return true;
#endif
}

bool FOnlineSubsystemExtendedSteam::Shutdown()
{
	UE_LOG(LogExtendedSteam, Log, TEXT("FOnlineSubsystemExtendedSteam::Shutdown (instance: %s)"), *GetInstanceName().ToString());

	FOnlineSubsystemImpl::Shutdown();

	PingInterface.Reset();
	EncryptedAppTicketInterface.Reset();
	AuthInterface.Reset();
	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Shutdown();
	}
	VoiceInterface.Reset();
	LeaderboardsInterface.Reset();
	AchievementsInterface.Reset();
	ExternalUIInterface.Reset();
	PresenceInterface.Reset();
	FriendsInterface.Reset();
	IdentityInterface.Reset();
	SessionInterface.Reset();
	UserCloudInterface.Reset();
	SharedCloudInterface.Reset();

	// The Steamworks API itself stays up: its lifetime belongs to FExtendedSteamSharedModule,
	// which other plugin features keep using after this subsystem instance is gone.
	return true;
}

bool FOnlineSubsystemExtendedSteam::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	// Deliberately NO SteamAPI_RunCallbacks / SteamGameServer_RunCallbacks here:
	// FExtendedSteamSharedModule owns the callback pump for the whole plugin.
	return true;
}

FString FOnlineSubsystemExtendedSteam::GetAppId() const
{
	return LexToString(ResolvedAppId > 0 ? ResolvedAppId : UESteamSettings::Get()->SteamAppId);
}

FText FOnlineSubsystemExtendedSteam::GetOnlineServiceName() const
{
	return LOCTEXT("OnlineServiceName", "Steam");
}

// -------------------------------------------------------------------------------------------
// Plugin-specific interface accessors (no IOnlineSubsystem getter exists for these).
// -------------------------------------------------------------------------------------------

FOnlineAuthExtendedSteamPtr FOnlineSubsystemExtendedSteam::GetAuthInterfaceExtended() const
{
	return AuthInterface;
}

FOnlineEncryptedAppTicketExtendedSteamPtr FOnlineSubsystemExtendedSteam::GetEncryptedAppTicketInterfaceExtended() const
{
	return EncryptedAppTicketInterface;
}

FOnlinePingExtendedSteamPtr FOnlineSubsystemExtendedSteam::GetPingInterfaceExtended() const
{
	return PingInterface;
}

// -------------------------------------------------------------------------------------------
// Interface getters — the TODO ledger. Implemented interfaces return their instance; the rest
// return nullptr until their phase lands.
// -------------------------------------------------------------------------------------------

IOnlineIdentityPtr FOnlineSubsystemExtendedSteam::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineSessionPtr FOnlineSubsystemExtendedSteam::GetSessionInterface() const
{
	return SessionInterface; // Implemented: Steam lobbies (create/find/join/destroy) + advertised sessions.
}

IOnlineFriendsPtr FOnlineSubsystemExtendedSteam::GetFriendsInterface() const
{
	return FriendsInterface; // Implemented: Steam roster, blocked players, coplay recent players.
}

IOnlineGroupsPtr FOnlineSubsystemExtendedSteam::GetGroupsInterface() const
{
	return nullptr; // Implemented in a later phase (Steam clans), if ever needed.
}

IOnlinePartyPtr FOnlineSubsystemExtendedSteam::GetPartyInterface() const
{
	return nullptr; // Implemented in a later phase.
}

IOnlineSharedCloudPtr FOnlineSubsystemExtendedSteam::GetSharedCloudInterface() const
{
	return SharedCloudInterface; // Implemented: Remote Storage UGC sharing (FileShare/UGCDownload).
}

IOnlineUserCloudPtr FOnlineSubsystemExtendedSteam::GetUserCloudInterface() const
{
	return UserCloudInterface; // Implemented: Steam Cloud via ISteamRemoteStorage (local user).
}

IOnlineEntitlementsPtr FOnlineSubsystemExtendedSteam::GetEntitlementsInterface() const
{
	return nullptr; // Implemented in a later phase (DLC / inventory entitlements).
}

IOnlineLeaderboardsPtr FOnlineSubsystemExtendedSteam::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface; // Implemented: ISteamUserStats leaderboards (find/download/upload).
}

IOnlineVoicePtr FOnlineSubsystemExtendedSteam::GetVoiceInterface() const
{
	return VoiceInterface; // Implemented: ISteamUser local voice capture (transport/playback are game-layer).
}

IOnlineExternalUIPtr FOnlineSubsystemExtendedSteam::GetExternalUIInterface() const
{
	return ExternalUIInterface; // Implemented: Steam overlay dialogs.
}

IOnlineTimePtr FOnlineSubsystemExtendedSteam::GetTimeInterface() const
{
	return nullptr; // Implemented in a later phase, if ever needed (Steam has no server time API).
}

IOnlineTitleFilePtr FOnlineSubsystemExtendedSteam::GetTitleFileInterface() const
{
	return nullptr; // Implemented in a later phase.
}

IOnlineStoreV2Ptr FOnlineSubsystemExtendedSteam::GetStoreV2Interface() const
{
	return nullptr; // Implemented in a later phase (microtransactions via ISteamInventory/web API).
}

IOnlinePurchasePtr FOnlineSubsystemExtendedSteam::GetPurchaseInterface() const
{
	return nullptr; // Implemented in a later phase.
}

IOnlineEventsPtr FOnlineSubsystemExtendedSteam::GetEventsInterface() const
{
	return nullptr; // Implemented in a later phase.
}

IOnlineAchievementsPtr FOnlineSubsystemExtendedSteam::GetAchievementsInterface() const
{
	return AchievementsInterface; // Implemented: ISteamUserStats achievements (local user only).
}

IOnlineSharingPtr FOnlineSubsystemExtendedSteam::GetSharingInterface() const
{
	return nullptr; // Implemented in a later phase, if ever needed.
}

IOnlineUserPtr FOnlineSubsystemExtendedSteam::GetUserInterface() const
{
	return nullptr; // Implemented in a later phase.
}

IOnlineMessagePtr FOnlineSubsystemExtendedSteam::GetMessageInterface() const
{
	return nullptr; // Implemented in a later phase, if ever needed.
}

IOnlinePresencePtr FOnlineSubsystemExtendedSteam::GetPresenceInterface() const
{
	return PresenceInterface; // Implemented: Steam rich presence.
}

IOnlineChatPtr FOnlineSubsystemExtendedSteam::GetChatInterface() const
{
	return nullptr; // Implemented in a later phase, if ever needed.
}

IOnlineStatsPtr FOnlineSubsystemExtendedSteam::GetStatsInterface() const
{
	return nullptr; // Implemented in a later phase (ISteamUserStats stats).
}

IOnlineGameActivityPtr FOnlineSubsystemExtendedSteam::GetGameActivityInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

IOnlineGameItemStatsPtr FOnlineSubsystemExtendedSteam::GetGameItemStatsInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

IOnlineGameMatchesPtr FOnlineSubsystemExtendedSteam::GetGameMatchesInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

IOnlineTurnBasedPtr FOnlineSubsystemExtendedSteam::GetTurnBasedInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

IOnlineTournamentPtr FOnlineSubsystemExtendedSteam::GetTournamentInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

IOnlineContentAgeRestrictionPtr FOnlineSubsystemExtendedSteam::GetOnlineContentAgeRestrictionInterface() const
{
	return nullptr; // Not applicable to Steam; stays null.
}

#undef LOCTEXT_NAMESPACE

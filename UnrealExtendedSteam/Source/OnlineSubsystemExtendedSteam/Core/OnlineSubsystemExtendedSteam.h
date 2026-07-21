// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemImpl.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineIdentityExtendedSteam;
class FOnlineFriendsExtendedSteam;
class FOnlinePresenceExtendedSteam;
class FOnlineExternalUIExtendedSteam;
class FOnlineUserCloudExtendedSteam;
class FOnlineSharedCloudExtendedSteam;
class FOnlineAchievementsExtendedSteam;
class FOnlineLeaderboardsExtendedSteam;
class FOnlineAuthExtendedSteam;
class FOnlineEncryptedAppTicketExtendedSteam;
class FOnlinePingExtendedSteam;

typedef TSharedPtr<class FOnlineSessionExtendedSteam, ESPMode::ThreadSafe> FOnlineSessionExtendedSteamPtr;
typedef TSharedPtr<class FOnlineIdentityExtendedSteam, ESPMode::ThreadSafe> FOnlineIdentityExtendedSteamPtr;
typedef TSharedPtr<class FOnlineFriendsExtendedSteam, ESPMode::ThreadSafe> FOnlineFriendsExtendedSteamPtr;
typedef TSharedPtr<class FOnlinePresenceExtendedSteam, ESPMode::ThreadSafe> FOnlinePresenceExtendedSteamPtr;
typedef TSharedPtr<class FOnlineExternalUIExtendedSteam, ESPMode::ThreadSafe> FOnlineExternalUIExtendedSteamPtr;
typedef TSharedPtr<class FOnlineUserCloudExtendedSteam, ESPMode::ThreadSafe> FOnlineUserCloudExtendedSteamPtr;
typedef TSharedPtr<class FOnlineSharedCloudExtendedSteam, ESPMode::ThreadSafe> FOnlineSharedCloudExtendedSteamPtr;
typedef TSharedPtr<class FOnlineAchievementsExtendedSteam, ESPMode::ThreadSafe> FOnlineAchievementsExtendedSteamPtr;
typedef TSharedPtr<class FOnlineLeaderboardsExtendedSteam, ESPMode::ThreadSafe> FOnlineLeaderboardsExtendedSteamPtr;
typedef TSharedPtr<class FOnlineVoiceExtendedSteam, ESPMode::ThreadSafe> FOnlineVoiceExtendedSteamPtr;
typedef TSharedPtr<class FOnlineAuthExtendedSteam, ESPMode::ThreadSafe> FOnlineAuthExtendedSteamPtr;
typedef TSharedPtr<class FOnlineEncryptedAppTicketExtendedSteam, ESPMode::ThreadSafe> FOnlineEncryptedAppTicketExtendedSteamPtr;
typedef TSharedPtr<class FOnlinePingExtendedSteam, ESPMode::ThreadSafe> FOnlinePingExtendedSteamPtr;
typedef TSharedPtr<class FOnlineSubsystemExtendedSteam, ESPMode::ThreadSafe> FOnlineSubsystemExtendedSteamPtr;

/**
 * Steam implementation of the online subsystem, registered as platform service "EXTENDEDSTEAM".
 *
 * Steamworks lifecycle ownership: FExtendedSteamSharedModule (ExtendedSteamShared) is the single
 * owner of SteamAPI init/shutdown AND of the callback pump. This subsystem NEVER calls
 * SteamAPI_RunCallbacks / SteamGameServer_RunCallbacks — the shared module's core ticker pumps
 * both callback queues every frame while either API is initialized.
 *
 * Init requirements:
 *  - Client (game/editor): the shared module must have the Steam client API initialized; if it has
 *    not, Init attempts FExtendedSteamSharedModule::InitializeSteamClient() once. When Steam is
 *    still unavailable Init returns false, the factory destroys the instance and
 *    IOnlineSubsystem::Get(EXTENDEDSTEAM_SUBSYSTEM) yields null.
 *  - Dedicated server: Init succeeds only when the shared module's Steam game server API is
 *    already initialized. Game server setup is explicit (ports, auth mode and version are
 *    deployment decisions) — call FExtendedSteamSharedModule::InitializeSteamGameServer before
 *    the subsystem is first requested; this subsystem never auto-initializes the game server.
 *
 * Config ([OnlineSubsystemExtendedSteam] in Engine ini):
 *  - bRelaunchInSteam: overrides UESteamSettings::bRelaunchInSteam when the key is present.
 *  - SteamDevAppId: overrides UESteamSettings::SteamAppId for GetAppId when present and > 0.
 */
class ONLINESUBSYSTEMEXTENDEDSTEAM_API FOnlineSubsystemExtendedSteam : public FOnlineSubsystemImpl
{
public:
	explicit FOnlineSubsystemExtendedSteam(FName InInstanceName)
		: FOnlineSubsystemImpl(ESTEAM_SUBSYSTEM, InInstanceName)
	{
	}

	FOnlineSubsystemExtendedSteam() = delete;
	virtual ~FOnlineSubsystemExtendedSteam() = default;

	//~ Begin IOnlineSubsystem lifecycle
	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual FText GetOnlineServiceName() const override;
	//~ End IOnlineSubsystem lifecycle

	//~ Begin FTSTickerObjectBase
	virtual bool Tick(float DeltaTime) override;
	//~ End FTSTickerObjectBase

	// ---------------------------------------------------------------------------------------
	// IOnlineSubsystem interface getters. This block is the single TODO ledger for the module:
	// it enumerates every interface getter FOnlineSubsystemImpl declares virtual. Implemented
	// interfaces return their instance; everything else returns nullptr until its phase lands.
	// ---------------------------------------------------------------------------------------
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineStatsPtr GetStatsInterface() const override;
	virtual IOnlineGameActivityPtr GetGameActivityInterface() const override;
	virtual IOnlineGameItemStatsPtr GetGameItemStatsInterface() const override;
	virtual IOnlineGameMatchesPtr GetGameMatchesInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual IOnlineTournamentPtr GetTournamentInterface() const override;
	virtual IOnlineContentAgeRestrictionPtr GetOnlineContentAgeRestrictionInterface() const override;

	/** [OnlineSubsystemExtendedSteam] bRelaunchInSteam when present, otherwise UESteamSettings::bRelaunchInSteam. */
	bool ShouldRelaunchInSteam() const { return bRelaunchInSteam; }

	// ---------------------------------------------------------------------------------------
	// Plugin-specific interface accessors. These interfaces have no IOnlineSubsystem getter in
	// the engine framework (Steam exposes them as vendor-specific surfaces), so they are reached
	// through the concrete FOnlineSubsystemExtendedSteam rather than IOnlineSubsystem. Each is
	// valid between successful Init and Shutdown; null otherwise.
	// ---------------------------------------------------------------------------------------

	/** Auth interface (ISteamUser session tickets + GetAuthTicketForWebApi). Valid between Init and Shutdown. */
	FOnlineAuthExtendedSteamPtr GetAuthInterfaceExtended() const;

	/** Encrypted app ticket interface (ISteamUser Request/GetEncryptedAppTicket). Valid between Init and Shutdown. */
	FOnlineEncryptedAppTicketExtendedSteamPtr GetEncryptedAppTicketInterfaceExtended() const;

	/** Ping interface (ISteamNetworkingUtils relay ping estimation). Valid between Init and Shutdown. */
	FOnlinePingExtendedSteamPtr GetPingInterfaceExtended() const;

private:
	/** Resolves config: OSS ini section keys win over UESteamSettings project settings. */
	void ReadConfig();

	/** Session interface (Steam lobbies + advertised sessions). Valid between successful Init and Shutdown. */
	FOnlineSessionExtendedSteamPtr SessionInterface;

	/** Identity interface (local Steam user). Valid between successful Init and Shutdown. */
	FOnlineIdentityExtendedSteamPtr IdentityInterface;

	/** Friends interface (Steam roster, blocked players, coplay recent players). Valid between successful Init and Shutdown. */
	FOnlineFriendsExtendedSteamPtr FriendsInterface;

	/** Presence interface (Steam rich presence). Valid between successful Init and Shutdown. */
	FOnlinePresenceExtendedSteamPtr PresenceInterface;

	/** External UI interface (Steam overlay). Valid between successful Init and Shutdown. */
	FOnlineExternalUIExtendedSteamPtr ExternalUIInterface;

	/** User cloud interface (Steam Remote Storage, local user). Valid between successful Init and Shutdown. */
	FOnlineUserCloudExtendedSteamPtr UserCloudInterface;

	/** Shared cloud interface (Steam Remote Storage UGC sharing). Valid between successful Init and Shutdown. */
	FOnlineSharedCloudExtendedSteamPtr SharedCloudInterface;

	/** Achievements interface (ISteamUserStats, local user only). Valid between successful Init and Shutdown. */
	FOnlineAchievementsExtendedSteamPtr AchievementsInterface;

	/** Leaderboards interface (ISteamUserStats leaderboards). Valid between successful Init and Shutdown. */
	FOnlineLeaderboardsExtendedSteamPtr LeaderboardsInterface;

	/** Voice interface (ISteamUser local voice capture). Valid between successful Init and Shutdown. */
	FOnlineVoiceExtendedSteamPtr VoiceInterface;

	/** Auth interface (ISteamUser session/web-api tickets). Valid between successful Init and Shutdown. */
	FOnlineAuthExtendedSteamPtr AuthInterface;

	/** Encrypted app ticket interface (ISteamUser encrypted app tickets). Valid between successful Init and Shutdown. */
	FOnlineEncryptedAppTicketExtendedSteamPtr EncryptedAppTicketInterface;

	/** Ping interface (ISteamNetworkingUtils relay ping estimation). Valid between successful Init and Shutdown. */
	FOnlinePingExtendedSteamPtr PingInterface;

	/** App id resolved in Init: [OnlineSubsystemExtendedSteam] SteamDevAppId, falling back to UESteamSettings::SteamAppId. */
	int32 ResolvedAppId = 0;

	/** See ShouldRelaunchInSteam. The relaunch itself is executed by FExtendedSteamSharedModule during client init (shipping only). */
	bool bRelaunchInSteam = false;
};

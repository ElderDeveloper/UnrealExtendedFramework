// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * External UI interface backed by the Steam overlay (ISteamFriends::ActivateGameOverlay*).
 *
 * Dialog names verified against isteamfriends.h (SDK 1.64):
 *  - ActivateGameOverlay: "Friends", "Community", "Players", "Settings", "OfficialGameGroup",
 *    "Stats", "Achievements", "chatroomgroup/nnnn"
 *  - ActivateGameOverlayToUser: "steamid", "chat", "jointrade", "stats", "achievements",
 *    "friendadd", "friendremove", "friendrequestaccept", "friendrequestignore"
 *
 * Mappings: ShowFriendsUI -> "Friends", ShowAchievementsUI -> "Achievements",
 * ShowProfileUI -> ActivateGameOverlayToUser("steamid"), ShowWebURL -> ActivateGameOverlayToWebPage,
 * ShowStoreUI -> ActivateGameOverlayToStore(app id), ShowSendMessageToUserUI ->
 * ActivateGameOverlayToUser("chat"), ShowInviteUI -> ActivateGameOverlayInviteDialogConnectString
 * with the named session's resolved connect string (1.64 has no "LobbyInvite" overlay dialog; the
 * lobby-id ActivateGameOverlayInviteDialog needs session internals, so the connect-string variant
 * is used and the call fails with a warning when no connect string resolves).
 *
 * Everything Steam has no overlay for (login, account creation/upgrade, a specific leaderboard,
 * composing a mailbox message without a recipient) fails honestly with false.
 *
 * Completion delegates: the overlay reports open/close only globally (GameOverlayActivated_t ->
 * OnExternalUIChange); there is no per-dialog or per-URL close notification. Delegates that
 * expect one (ShowWebURL, ShowProfileUI, ShowStoreUI, ShowSendMessageToUserUI) therefore fire
 * immediately with the most sensible result (the URL shown, bPurchased=false, bMessageSent=false).
 */
class FOnlineExternalUIExtendedSteam : public IOnlineExternalUI
{
public:
	explicit FOnlineExternalUIExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineExternalUIExtendedSteam();

	//~ Begin IOnlineExternalUI
	virtual bool ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate = FOnLoginUIClosedDelegate()) override;
	virtual bool ShowAccountCreationUI(const int ControllerIndex, const FOnAccountCreationUIClosedDelegate& Delegate = FOnAccountCreationUIClosedDelegate()) override;
	virtual bool ShowFriendsUI(int32 LocalUserNum) override;
	virtual bool ShowInviteUI(int32 LocalUserNum, FName SessionName = NAME_GameSession) override;
	virtual bool ShowAchievementsUI(int32 LocalUserNum) override;
	virtual bool ShowLeaderboardUI(const FString& LeaderboardName) override;
	virtual bool ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate = FOnShowWebUrlClosedDelegate()) override;
	virtual bool CloseWebURL() override;
	virtual bool ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate = FOnProfileUIClosedDelegate()) override;
	virtual bool ShowAccountUpgradeUI(const FUniqueNetId& UniqueId) override;
	virtual bool ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate = FOnShowStoreUIClosedDelegate()) override;
	virtual bool ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;
	virtual bool ShowSendMessageToUserUI(int32 LocalUserNum, const FUniqueNetId& Recipient, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate = FOnShowSendMessageUIClosedDelegate()) override;
	//~ End IOnlineExternalUI

	/** Called by the cpp-local Steam callback holder when a GameOverlayActivated_t arrives. */
	void HandleOverlayActivated(bool bActive);

private:
	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Steam callback holder (defined in the cpp, only constructed while the Steam client is up). */
	TSharedPtr<class FExternalUIExtendedSteamCallbacks> Callbacks;
};

typedef TSharedPtr<FOnlineExternalUIExtendedSteam, ESPMode::ThreadSafe> FOnlineExternalUIExtendedSteamPtr;

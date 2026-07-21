// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExternalUI/OnlineExternalUIExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineError.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamExternalUI
{
	/** True while the shared module has the Steam client API up. */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized()
			&& SteamFriends() != nullptr;
#else
		return false;
#endif
	}

	/** Extracts the SteamID64 from a unique net id of our service type; 0 otherwise. */
	static uint64 GetSteamId64(const FUniqueNetId& NetId)
	{
		if (NetId.GetType() == ESTEAM_SUBSYSTEM && NetId.GetSize() == sizeof(uint64))
		{
			uint64 SteamId64 = 0;
			FMemory::Memcpy(&SteamId64, NetId.GetBytes(), sizeof(uint64));
			return SteamId64;
		}
		return 0;
	}
}

#if WITH_EXTENDEDSTEAM_SDK

/** Steam callback holder — kept out of the header so SDK types stay private to this cpp. */
class FExternalUIExtendedSteamCallbacks
{
public:
	explicit FExternalUIExtendedSteamCallbacks(FOnlineExternalUIExtendedSteam& InOwner)
		: Owner(InOwner)
		, OverlayActivatedCallback(this, &FExternalUIExtendedSteamCallbacks::OnGameOverlayActivated)
	{
	}

private:
	void OnGameOverlayActivated(GameOverlayActivated_t* Data)
	{
		if (Data != nullptr)
		{
			Owner.HandleOverlayActivated(Data->m_bActive != 0);
		}
	}

	FOnlineExternalUIExtendedSteam& Owner;
	CCallback<FExternalUIExtendedSteamCallbacks, GameOverlayActivated_t> OverlayActivatedCallback;
};

#else

class FExternalUIExtendedSteamCallbacks
{
};

#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineExternalUIExtendedSteam::FOnlineExternalUIExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamExternalUI::IsSteamClientUp())
	{
		Callbacks = MakeShared<FExternalUIExtendedSteamCallbacks>(*this);
	}
#endif
}

FOnlineExternalUIExtendedSteam::~FOnlineExternalUIExtendedSteam() = default;

void FOnlineExternalUIExtendedSteam::HandleOverlayActivated(bool bActive)
{
	// GameOverlayActivated_t is the only open/close signal the overlay gives (global, not per dialog).
	TriggerOnExternalUIChangeDelegates(bActive);
}

bool FOnlineExternalUIExtendedSteam::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	// Steam has no login UI: the user is whoever the running Steam client is signed in as.
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowLoginUI: Steam has no interactive login UI"));
	Delegate.ExecuteIfBound(nullptr, ControllerIndex, FOnlineError(EOnlineErrorResult::NotImplemented));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowAccountCreationUI(const int ControllerIndex, const FOnAccountCreationUIClosedDelegate& Delegate)
{
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowAccountCreationUI: Steam accounts cannot be created from inside a game"));
	Delegate.ExecuteIfBound(ControllerIndex, FOnlineAccountCredentials(), FOnlineError(EOnlineErrorResult::NotImplemented));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowFriendsUI(int32 LocalUserNum)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamExternalUI::IsSteamClientUp())
	{
		// "Friends" — verified against the ActivateGameOverlay dialog list in isteamfriends.h.
		SteamFriends()->ActivateGameOverlay("Friends");
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowFriendsUI: Steam client is not initialized"));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!ExtendedSteamExternalUI::IsSteamClientUp())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("ShowInviteUI: Steam client is not initialized"));
		return false;
	}

	// 1.64 has no generic "LobbyInvite" ActivateGameOverlay dialog; the overlay invite dialog is
	// opened either per-lobby (ActivateGameOverlayInviteDialog, needs a lobby CSteamID) or with a
	// game-defined connect string (ActivateGameOverlayInviteDialogConnectString). We resolve the
	// named session's connect string through the session interface, which stays decoupled from
	// the session implementation's internals.
	FString ConnectString;
	const IOnlineSessionPtr SessionInterface = Subsystem != nullptr ? Subsystem->GetSessionInterface() : nullptr;
	if (SessionInterface.IsValid())
	{
		SessionInterface->GetResolvedConnectString(SessionName, ConnectString);
	}

	if (ConnectString.IsEmpty())
	{
		UE_LOG(LogExtendedSteam, Warning,
			TEXT("ShowInviteUI: no resolvable connect string for session '%s' (session interface %s); cannot open the overlay invite dialog"),
			*SessionName.ToString(), SessionInterface.IsValid() ? TEXT("resolved nothing") : TEXT("unavailable"));
		return false;
	}

	// Recipients receive the connect string back via GameRichPresenceJoinRequested_t.
	SteamFriends()->ActivateGameOverlayInviteDialogConnectString(TCHAR_TO_UTF8(*ConnectString));
	return true;
#else
	return false;
#endif
}

bool FOnlineExternalUIExtendedSteam::ShowAchievementsUI(int32 LocalUserNum)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamExternalUI::IsSteamClientUp())
	{
		// "Achievements" — verified against the ActivateGameOverlay dialog list in isteamfriends.h.
		SteamFriends()->ActivateGameOverlay("Achievements");
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowAchievementsUI: Steam client is not initialized"));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowLeaderboardUI(const FString& LeaderboardName)
{
	// The overlay has no page for a specific leaderboard ("Stats" exists, but it is the whole
	// stats page, not the requested leaderboard) — fail honestly.
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowLeaderboardUI: the Steam overlay has no per-leaderboard page (requested '%s')"), *LeaderboardName);
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamExternalUI::IsSteamClientUp() && !Url.IsEmpty())
	{
		// FShowWebUrlParams sizing/embedding does not apply to the overlay browser; only the URL
		// is honored (full address with protocol required by Steam).
		SteamFriends()->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*Url));

		// The overlay has no per-URL close notification, so the "closed" delegate fires
		// immediately with the URL we showed.
		Delegate.ExecuteIfBound(Url);
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowWebURL: Steam client is not initialized or URL is empty"));
	return false;
}

bool FOnlineExternalUIExtendedSteam::CloseWebURL()
{
	// The overlay browser cannot be closed programmatically; report handled so callers proceed.
	return true;
}

bool FOnlineExternalUIExtendedSteam::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	const uint64 RequesteeId64 = ExtendedSteamExternalUI::GetSteamId64(Requestee);
	if (ExtendedSteamExternalUI::IsSteamClientUp() && RequesteeId64 != 0)
	{
		// "steamid" — verified against the ActivateGameOverlayToUser dialog list in isteamfriends.h.
		SteamFriends()->ActivateGameOverlayToUser("steamid", CSteamID(RequesteeId64));

		// No per-dialog close notification exists; fire the closed delegate immediately.
		Delegate.ExecuteIfBound();
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowProfileUI: Steam client is not initialized or requestee id is invalid (%s)"), *Requestee.ToDebugString());
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	// Steam has no premium account tier to upgrade to.
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowAccountUpgradeUI: not applicable to Steam"));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamExternalUI::IsSteamClientUp())
	{
		// Open this app's store page (the configured app id); Steam cannot browse arbitrary
		// categories or third-party offer ids through the overlay.
		int32 AppId = 0;
		if (Subsystem != nullptr)
		{
			LexFromString(AppId, *Subsystem->GetAppId());
		}

		const EOverlayToStoreFlag StoreFlag = ShowParams.bAddToCart ? k_EOverlayToStoreFlag_AddToCartAndShow : k_EOverlayToStoreFlag_None;
		SteamFriends()->ActivateGameOverlayToStore(static_cast<AppId_t>(FMath::Max(AppId, 0)), StoreFlag);

		// Purchases finish inside the overlay with no notification back; report no-purchase.
		Delegate.ExecuteIfBound(false);
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowStoreUI: Steam client is not initialized"));
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	// Steam has no recipient-less compose UI; use ShowSendMessageToUserUI for a chat window.
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowSendMessageUI: Steam has no mailbox compose UI; use ShowSendMessageToUserUI to open a chat with a specific user"));
	Delegate.ExecuteIfBound(false);
	return false;
}

bool FOnlineExternalUIExtendedSteam::ShowSendMessageToUserUI(int32 LocalUserNum, const FUniqueNetId& Recipient, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	const uint64 RecipientId64 = ExtendedSteamExternalUI::GetSteamId64(Recipient);
	if (ExtendedSteamExternalUI::IsSteamClientUp() && RecipientId64 != 0)
	{
		// "chat" — verified against the ActivateGameOverlayToUser dialog list in isteamfriends.h.
		// The message content cannot be pre-filled or auto-sent; whether the user actually sends
		// anything is unknowable, so the delegate reports bMessageSent=false immediately.
		SteamFriends()->ActivateGameOverlayToUser("chat", CSteamID(RecipientId64));
		Delegate.ExecuteIfBound(false);
		return true;
	}
#endif
	UE_LOG(LogExtendedSteam, Warning, TEXT("ShowSendMessageToUserUI: Steam client is not initialized or recipient id is invalid (%s)"), *Recipient.ToDebugString());
	Delegate.ExecuteIfBound(false);
	return false;
}

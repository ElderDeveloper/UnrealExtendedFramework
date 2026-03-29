// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSUISubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IsEOSAvailable())
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
		if (ExternalUI.IsValid())
		{
			ExternalUIChangeHandle = ExternalUI->OnExternalUIChangeDelegates.AddLambda(
				[this](bool bIsOpening)
				{
					bOverlayVisible = bIsOpening;
					OnOverlayStateChanged.Broadcast(bIsOpening);
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSUISubsystem: Overlay %s"), bIsOpening ? TEXT("opened") : TEXT("closed"));
				});
		}
	}
}

void UEEOSUISubsystem::Deinitialize()
{
	if (IsEOSAvailable() && ExternalUIChangeHandle.IsValid())
	{
		IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
		IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
		if (ExternalUI.IsValid())
		{
			ExternalUI->OnExternalUIChangeDelegates.Remove(ExternalUIChangeHandle);
		}
	}
	Super::Deinitialize();
}

// ── Friends Overlay ──────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowFriendsUI()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowFriendsUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSUISubsystem::ShowFriendsUI — ExternalUI interface not available"));
		return false;
	}

	return ExternalUI->ShowFriendsUI(0);
}

bool UEEOSUISubsystem::ShowInviteUI(const FString& SessionName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowInviteUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	return ExternalUI->ShowInviteUI(0, FName(*SessionName));
}

// ── Player Profile ───────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowProfileUI(const FString& TargetUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowProfileUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!ExternalUI.IsValid() || !Identity.IsValid()) return false;

	FUniqueNetIdPtr LocalUserId = Identity->GetUniquePlayerId(0);
	FUniqueNetIdPtr TargetId = Identity->CreateUniquePlayerId(TargetUserId);
	if (!LocalUserId.IsValid() || !TargetId.IsValid()) return false;

	return ExternalUI->ShowProfileUI(*LocalUserId, *TargetId,
		FOnProfileUIClosedDelegate::CreateLambda([this]()
		{
			OnProfileClosed.Broadcast(true);
		}));
}

// ── Achievements / Leaderboards ──────────────────────────────────────────────

bool UEEOSUISubsystem::ShowAchievementsUI()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowAchievementsUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	return ExternalUI->ShowAchievementsUI(0);
}

bool UEEOSUISubsystem::ShowLeaderboardUI(const FString& LeaderboardName)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowLeaderboardUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	return ExternalUI->ShowLeaderboardUI(LeaderboardName);
}

// ── Store ────────────────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowStoreUI(const FString& ProductId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowStoreUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	FShowStoreParams Params;
	if (!ProductId.IsEmpty())
	{
		Params.ProductId = ProductId;
	}

	return ExternalUI->ShowStoreUI(0, Params);
}

// ── Web View ─────────────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowWebURL(const FString& URL)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowWebURL"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	FShowWebUrlParams Params;
	return ExternalUI->ShowWebURL(URL, Params);
}

bool UEEOSUISubsystem::CloseWebURL()
{
	if (!IsEOSAvailable()) return false;

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	return ExternalUI->CloseWebURL();
}

// ── Login UI ─────────────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowLoginUI()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowLoginUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	if (!ExternalUI.IsValid()) return false;

	return ExternalUI->ShowLoginUI(0, false, true);
}

// ── Account Upgrade ──────────────────────────────────────────────────────────

bool UEEOSUISubsystem::ShowAccountUpgradeUI()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ShowAccountUpgradeUI"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineExternalUIPtr ExternalUI = EOSSub->GetExternalUIInterface();
	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!ExternalUI.IsValid() || !Identity.IsValid()) return false;

	FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return false;

	return ExternalUI->ShowAccountUpgradeUI(*UserId);
}

// ── Overlay State ────────────────────────────────────────────────────────────

bool UEEOSUISubsystem::IsOverlayVisible() const
{
	return bOverlayVisible;
}

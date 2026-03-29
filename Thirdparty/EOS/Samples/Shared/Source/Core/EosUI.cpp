// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Game.h"
#include "GameEvent.h"
#include "Input.h"
#include "Main.h"
#include "Platform.h"
#include "Users.h"
#include "Player.h"
#include "EosUI.h"
#include "eos_sdk.h"
#include "eos_friends.h"
#include "eos_presence.h"
#include "eos_userinfo.h"
#include "eos_connect.h"
#include "eos_ui.h"

/**
 * These function are part of the EOS SDK, but is not exposed in header files.
 * They are intended for internal use only. The end-point is not always available.
 */
#if defined(DEV_BUILD) && (defined(_DEBUG) || defined(_TEST))
EOS_DECLARE_FUNC(EOS_EResult) EOS_Private_UI_RunPactTests(EOS_HUI Handle, const EOS_EpicAccountId LocalUserId);
EOS_DECLARE_FUNC(EOS_EResult) EOS_Private_UI_LoadURLCustom(EOS_HUI Handle, const EOS_EpicAccountId LocalUserId, const char* URL);
#endif // DEV_BUILD


FEosUI::FEosUI() 
{
	
}

FEosUI::~FEosUI()
{

}

std::wstring FEosUI::LocationToString(EOS_UI_ENotificationLocation Location)
{
	switch (Location)
	{
		case EOS_UI_ENotificationLocation::EOS_UNL_TopLeft:
			return L"EOS_UNL_TopLeft";
		case EOS_UI_ENotificationLocation::EOS_UNL_TopRight:
			return L"EOS_UNL_TopRight";
		case EOS_UI_ENotificationLocation::EOS_UNL_BottomLeft:
			return L"EOS_UNL_TopLeft";
		case EOS_UI_ENotificationLocation::EOS_UNL_BottomRight:
		default:
			return L"EOS_UNL_TopRight";
	}
}

void FEosUI::Init()
{
	if (DisplayUpdateNotificationId == EOS_INVALID_NOTIFICATIONID)
	{
		EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

		EOS_UI_AddNotifyDisplaySettingsUpdatedOptions Options = {};
		Options.ApiVersion = EOS_UI_ADDNOTIFYDISPLAYSETTINGSUPDATED_API_LATEST;

		DisplayUpdateNotificationId = EOS_UI_AddNotifyDisplaySettingsUpdated(ExternalUIHandle, &Options, nullptr, FEosUI::OnDisplaySettingsUpdated);
	}
}

void FEosUI::OnShutdown()
{
	if (DisplayUpdateNotificationId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

		EOS_UI_RemoveNotifyDisplaySettingsUpdated(ExternalUIHandle, DisplayUpdateNotificationId);
		DisplayUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
	}
}

void FEosUI::ShowFriendsOverlay()
{
	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

	EOS_UI_ShowFriendsOptions Options = {};
	Options.ApiVersion = EOS_UI_SHOWFRIENDS_API_LATEST;

	// It is possible to pass null in as the LocalUserId. VPC could be active for the current external account,
	// in which case there the user has a headless account, meaning there is no user id.
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player.get())
	{
		Options.LocalUserId = Player->GetUserID();
	}

	EOS_UI_ShowFriends(ExternalUIHandle, &Options, nullptr, FEosUI::ShowFriendsCallbackFn);
}

void FEosUI::SetToggleFriendsKey(const std::vector<std::wstring>& Keys)
{
	using FKeyStringToKeyMap = std::map<std::wstring, EOS_UI_EKeyCombination>;
	static const FKeyStringToKeyMap StringToKeyMappings{
#define EOS_UI_TEXT_PASTE(X) L ## X
#define EOS_UI_TEXT(X) EOS_UI_TEXT_PASTE(X)
#define EOS_UI_KEY_CONSTANT(Prefix, Name, Value) { EOS_UI_TEXT(#Name), EOS_UI_EKeyCombination::Prefix ## Name },
#define EOS_UI_KEY_MODIFIER(Prefix, Name, Value) { EOS_UI_TEXT(#Name), EOS_UI_EKeyCombination::Prefix ## Name },
#define EOS_UI_KEY_ENTRY_FIRST(Prefix, Name, Value) { EOS_UI_TEXT(#Name), EOS_UI_EKeyCombination::Prefix ## Name },
#define EOS_UI_KEY_ENTRY(Prefix, Name) { EOS_UI_TEXT(#Name), EOS_UI_EKeyCombination::Prefix ## Name },
#define EOS_UI_KEY_CONSTANT_LAST(Prefix, Name) { EOS_UI_TEXT(#Name), EOS_UI_EKeyCombination::Prefix ## Name }
#include "eos_ui_keys.h"
#undef EOS_UI_TEXT_PASTE
#undef EOS_UI_TEXT
#undef EOS_UI_KEY_CONSTANT
#undef EOS_UI_KEY_MODIFIER
#undef EOS_UI_KEY_ENTRY_FIRST
#undef EOS_UI_KEY_ENTRY
#undef EOS_UI_KEY_CONSTANT_LAST
	};

	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

	EOS_UI_EKeyCombination NewKey = EOS_UI_EKeyCombination::EOS_UIK_None;
	for (const std::wstring& Key : Keys)
	{
		FKeyStringToKeyMap::const_iterator KeyIter = StringToKeyMappings.find(Key);
		if (KeyIter != StringToKeyMappings.end())
		{
			NewKey = NewKey | KeyIter->second;
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Set Toggle Friends Key error: Could not find a key match for `%ls`.", Key.c_str());
			return;
		}
	}

	EOS_UI_SetToggleFriendsKeyOptions Options = {};
	Options.ApiVersion = EOS_UI_SETTOGGLEFRIENDSKEY_API_LATEST;
	Options.KeyCombination = NewKey;

	const EOS_EResult Result = EOS_UI_SetToggleFriendsKey(ExternalUIHandle, &Options);

	if (Result == EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Set Toggle Friends Key succeeded.");
	}
	else
	{
		FDebugLog::LogError(L"[EOS SDK] Set Toggle Friends Key error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
	}
}

void FEosUI::SetDisplayPreference(EOS_UI_ENotificationLocation Location)
{
	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

	EOS_UI_SetDisplayPreferenceOptions Options = {};
	Options.ApiVersion = EOS_UI_SETDISPLAYPREFERENCE_API_LATEST;
	Options.NotificationLocation = Location;

	const EOS_EResult Result = EOS_UI_SetDisplayPreference(ExternalUIHandle, &Options);

	if (Result == EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Set Display Preference succeeded: %ls.", LocationToString(EOS_UI_GetNotificationLocationPreference(ExternalUIHandle)).c_str());
	}
	else
	{
		FDebugLog::LogError(L"[EOS SDK] Set Display Preference error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
	}
}

#if defined(DEV_BUILD) && (defined(_DEBUG) || defined(_TEST))
void FEosUI::RunPactTests()
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		FDebugLog::LogError(L"RunPactTests: Current player is invalid!");
		return;
	}

	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());
	EOS_EpicAccountId LocalUserId = Player->GetUserID();
	EOS_Private_UI_RunPactTests(ExternalUIHandle, LocalUserId);
}

void FEosUI::LoadURLCustom(const char* URL)
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		FDebugLog::LogError(L"LoadURLCustom: Current player is invalid!");
		return;
	}

	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());
	EOS_EpicAccountId LocalUserId = Player->GetUserID();
	EOS_Private_UI_LoadURLCustom(ExternalUIHandle, LocalUserId, URL);
}
#endif // DEV_BUILD

void FEosUI::OnGameEvent(const FGameEvent& Event)
{
	
}

void FEosUI::ShowBlockPlayer(const FEpicAccountId TargetPlayer)
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		FDebugLog::LogError(L"ShowBlockPlayer: Current player is invalid!");
		return;
	}

	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

	EOS_UI_ShowBlockPlayerOptions Options = {};
	Options.ApiVersion = EOS_UI_SHOWBLOCKPLAYER_API_LATEST;
	Options.LocalUserId = Player->GetUserID();
	Options.TargetUserId = TargetPlayer;

	EOS_UI_ShowBlockPlayer(ExternalUIHandle, &Options, nullptr, FEosUI::ShowBlockPlayerCallbackFn);
}

void EOS_CALL FEosUI::ShowBlockPlayerCallbackFn(const EOS_UI_OnShowBlockPlayerCallbackInfo* Data)
{
	FDebugLog::Log(L"[EOS SDK] Show Block Player completed with result: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
}

void FEosUI::ShowReportPlayer(const FEpicAccountId TargetPlayer)
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		FDebugLog::LogError(L"ShowReportPlayer: Current player is invalid!");
		return;
	}

	EOS_HUI ExternalUIHandle = EOS_Platform_GetUIInterface(FPlatform::GetPlatformHandle());

	EOS_UI_ShowReportPlayerOptions Options = {};
	Options.ApiVersion = EOS_UI_SHOWREPORTPLAYER_API_LATEST;
	Options.LocalUserId = Player->GetUserID();
	Options.TargetUserId = TargetPlayer;

	EOS_UI_ShowReportPlayer(ExternalUIHandle, &Options, nullptr, FEosUI::ShowReportPlayerCallbackFn);
}

void EOS_CALL FEosUI::ShowReportPlayerCallbackFn(const EOS_UI_OnShowReportPlayerCallbackInfo* Data)
{
	FDebugLog::Log(L"[EOS SDK] Show Report Player completed with result: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
}

void EOS_CALL FEosUI::ShowFriendsCallbackFn(const EOS_UI_ShowFriendsCallbackInfo* UiData)
{
	if (UiData)
	{
		if (UiData->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Show Friends successfully shown.");
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Show Friends error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(UiData->ResultCode)).c_str());
		}
	}
}

void EOS_CALL FEosUI::OnDisplaySettingsUpdated(const EOS_UI_OnDisplaySettingsUpdatedCallbackInfo* UpdatedData)
{
	if (UpdatedData)
	{
		FDebugLog::Log(L"[EOS SDK] Overlay Display Settings Updated: Visible(%ls) ExclusiveInput(%ls)"
			, UpdatedData->bIsVisible ? L"TRUE" : L"FALSE"
			, UpdatedData->bIsExclusiveInput ? L"TRUE" : L"FALSE");
	}
}

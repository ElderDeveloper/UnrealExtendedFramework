// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Settings.h"
#include "GameEvent.h"
#include "Platform.h"
#include "Game.h"
#include "Users.h"
#include "Player.h"
#include "Console.h"

#include "SampleConstants.h"
#include "eos_presence.h"

FPlayer::FPlayer(const FEpicAccountId PlayerId) :
	UserId(PlayerId)
{
	FGame::Get().GetUsers()->QueryUserInfo(UserId, nullptr);
}

FPlayer::~FPlayer()
{

}

/**
* Singleton Implementation
*/
class FPlayerManager::Impl
{
public:
	Impl(FPlayerManager* owner) :
		mOwner(owner)
	{
		if (SInstance)
		{
			throw std::runtime_error("FPlayerManager is a singleton");
		}

		SInstance = (FPlayerManager::Impl *)this;
	}

	~Impl()
	{
		SInstance = nullptr;
	}

	FPlayerManager*  mOwner;

	static FPlayerManager::Impl* SInstance;
};

FPlayerManager::Impl* FPlayerManager::Impl::SInstance = nullptr;

FPlayerManager::FPlayerManager() :
	TheImpl(std::make_unique<Impl>(this))
{

}

FPlayerManager::~FPlayerManager()
{

}

FPlayerManager& FPlayerManager::Get()
{
	if (!Impl::SInstance || !Impl::SInstance->mOwner)
	{
		throw std::runtime_error("FPlayerManager singleton not created");
	}

	return *Impl::SInstance->mOwner;
}

void FPlayerManager::Add(FEpicAccountId UserId)
{
	auto Itr = Players.find(UserId);
	if (Itr == Players.end())
	{
		PlayerPtr Player = std::make_shared<FPlayer>(UserId);
		Players.insert({ UserId, Player });

		PlayerIds.emplace_back(UserId);

		FDebugLog::Log(L"Player added - Id: %ls", UserId.ToString().c_str());
	}
	else
	{
		FDebugLog::LogError(L"Trying to add duplicate player - Id: %ls", UserId.ToString().c_str());
	}
}

void FPlayerManager::Remove(FEpicAccountId UserId)
{
	auto Itr = Players.find(UserId);
	if (Itr != Players.end())
	{
		PlayerPtr Player = Itr->second;
		if (Player != nullptr)
		{
			ConnectPlayers.erase(Player->GetProductUserID());
		}

		Players.erase(Itr);

		FDebugLog::Log(L"Player removed - Id: %ls", UserId.ToString().c_str());
	}
	else
	{
		FDebugLog::LogError(L"Can't find player to remove - Id: %ls", UserId.ToString().c_str());
	}

	auto PIDItr = std::find(PlayerIds.begin(), PlayerIds.end(), UserId);
	if (PIDItr != PlayerIds.end())
	{
		PlayerIds.erase(PIDItr);
	}
	else
	{
		FDebugLog::LogError(L"Can't find player to remove - Id: %ls", UserId.ToString().c_str());
	}
}

PlayerPtr FPlayerManager::GetPlayer(FEpicAccountId UserId)
{
	auto Itr = Players.find(UserId);
	if (Itr != Players.end())
	{
		return Itr->second;
	}
	return nullptr;
}

PlayerPtr FPlayerManager::GetPlayer(FProductUserId ProductUserId)
{
	auto Itr = ConnectPlayers.find(ProductUserId);
	if (Itr != ConnectPlayers.end())
	{
		return Itr->second;
	}
	return nullptr;
}

PlayerPtr FPlayerManager::GetPlayer(const std::wstring DisplayName)
{
	for (auto Itr = Players.begin(); Itr != Players.end(); ++Itr)
	{
		PlayerPtr Player = Itr->second;
		if (Player && Player->GetDisplayName() == DisplayName)
		{
			return Player;
		}
	}
	return nullptr;
}

int FPlayerManager::GetPlayerIndex(FEpicAccountId UserId)
{
	for (int i = 0; i < (int)PlayerIds.size(); ++i)
	{
		FEpicAccountId PlayerId = PlayerIds[i];
		if (PlayerId == UserId)
		{
			return i  + 1;
		}
	}
	return 0;
}

FEpicAccountId FPlayerManager::GetPrevPlayerId(FEpicAccountId UserId)
{
	for (int i = 0; i < (int)PlayerIds.size(); ++i)
	{
		FEpicAccountId PlayerId = PlayerIds[i];
		if (PlayerId == UserId)
		{
			int NextIdx = i - 1;
			if (NextIdx < 0)
			{
				NextIdx = (int)PlayerIds.size() - 1;
			}
			return PlayerIds[NextIdx];
		}
	}
	if (PlayerIds.size() > 0)
	{
		return PlayerIds[0];
	}
	return FEpicAccountId();
}

FEpicAccountId FPlayerManager::GetNextPlayerId(FEpicAccountId UserId)
{
	for (int i = 0; i < (int)PlayerIds.size(); ++i)
	{
		FEpicAccountId PlayerId = PlayerIds[i];
		if (PlayerId == UserId)
		{
			int NextIdx = i + 1;
			if (NextIdx >= (int)PlayerIds.size())
			{
				NextIdx = 0;
			}
			return PlayerIds[NextIdx];
		}
	}
	if (PlayerIds.size() > 0)
	{
		return PlayerIds[0];
	}
	return FEpicAccountId();
}

void FPlayerManager::SetDisplayName(FEpicAccountId UserId, const std::wstring DisplayName)
{
	if (UserId.IsValid())
	{
		PlayerPtr Player = GetPlayer(UserId);
		if (Player)
		{
			Player->SetDisplayName(DisplayName);
		}
	}
	else
	{
		FDebugLog::LogError(L"User Id is invalid!");
	}
}

std::wstring FPlayerManager::GetDisplayName(FEpicAccountId UserId)
{
	if (UserId.IsValid())
	{
		PlayerPtr Player = GetPlayer(UserId);
		if (Player)
		{
			return Player->GetDisplayName();
		}
	}
	else
	{
		FDebugLog::LogError(L"User Id is invalid!");
	}
	return std::wstring();
}

void FPlayerManager::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		FEpicAccountId UserId = Event.GetUserId();
		Add(UserId);

		if (!GetCurrentUser().IsValid())
		{
			SetCurrentUser(UserId);
		}

		SetUserLocale(UserId);

		SetInitialPresence();
	}
	else if (Event.GetType() == EGameEventType::UserLoggedOut)
	{
		FEpicAccountId UserId = Event.GetUserId();
		FGameEvent Event(EGameEventType::PlayerSessionEnd, UserId);
		FGame::Get().OnGameEvent(Event);

		Remove(UserId);

		if (GetNumPlayers() > 0)
		{
			if (GetCurrentUser() == UserId)
			{
				FGameEvent Event(EGameEventType::ShowNextUser);
				OnGameEvent(Event);
			}
		}
		else
		{
			SetCurrentUser(FEpicAccountId());
		}

	}
	else if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
		if (GetNumPlayers() > 0)
		{
			FEpicAccountId PrevPlayerId = GetPrevPlayerId(GetCurrentUser());
			if (PrevPlayerId.IsValid())
			{
				SetCurrentUser(PrevPlayerId);
			}
			else
			{
				SetCurrentUser(FEpicAccountId());
			}
		}
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
		if (GetNumPlayers() > 0)
		{
			FEpicAccountId NextPlayerId = GetNextPlayerId(GetCurrentUser());
			if (NextPlayerId.IsValid())
			{
				SetCurrentUser(NextPlayerId);
			}
			else
			{
				SetCurrentUser(FEpicAccountId());
			}
		}
	}
	else if (Event.GetType() == EGameEventType::UserInfoRetrieved)
	{
		
	}
	else if (Event.GetType() == EGameEventType::UserConnectLoggedIn)
	{
		FProductUserId ProductUserId = Event.GetProductUserId();
		FEpicAccountId UserId = Event.GetUserId();

		if (UserId.IsValid())
		{
			SetProductUserID(UserId, ProductUserId);

			FGameEvent Event(EGameEventType::PlayerSessionBegin, UserId);
			FGame::Get().OnGameEvent(Event);

			std::wstring Command;
			if (FSettings::Get().TryGetAsString(SettingsConstants::PostLoginCommand, Command))
			{
				FGame::Get().GetConsole()->RunCommand(Command);
			}
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] UserConnectLoggedIn was raised without the required EpicAccountId.");
		}
	}
	else if (Event.GetType() == EGameEventType::SetLocale)
	{
		SetActiveLocale(Event.GetFirstStr());
	}
	else if (Event.GetType() == EGameEventType::TestSetPresence)
	{
		SetPresenceRichText(Event.GetFirstStr());
	}
}

void FPlayerManager::SetActiveLocale(const std::wstring& NewLocale)
{
	std::string NarrowLocale = FStringUtils::Narrow(NewLocale);

	EOS_EResult LocaleCodeResult = (NarrowLocale.size() > 0)
		? EOS_Platform_SetOverrideLocaleCode(FPlatform::GetPlatformHandle(), NarrowLocale.c_str())
		: EOS_Platform_SetOverrideLocaleCode(FPlatform::GetPlatformHandle(), nullptr);
	if (LocaleCodeResult == EOS_EResult::EOS_Success)
	{
		for (auto Itr = Players.begin(); Itr != Players.end(); ++Itr)
		{
			PlayerPtr Player = Itr->second;
			if (Player)
			{
				Player->SetLocale(NarrowLocale);
			}
		}
	}
	else
	{
		FDebugLog::LogError(L"[EOS SDK] Set override active locale code failed: %ls.", FStringUtils::Widen(EOS_EResult_ToString(LocaleCodeResult)).c_str());
	}
}

void FPlayerManager::SetUserLocale(FEpicAccountId UserId)
{
	PlayerPtr Player = GetPlayer(UserId);
	if (Player != nullptr)
	{
		char LocaleBuffer[EOS_LOCALECODE_MAX_BUFFER_LEN];
		int32_t LocaleBufferLen = sizeof(LocaleBuffer);
		EOS_EResult LocaleCodeResult = EOS_Platform_GetActiveLocaleCode(FPlatform::GetPlatformHandle(), UserId, LocaleBuffer, &LocaleBufferLen);
		if (LocaleCodeResult == EOS_EResult::EOS_Success)
		{
			std::string Locale = std::string(LocaleBuffer);
			Player->SetLocale(Locale);
		}
	}
}

void FPlayerManager::SetProductUserID(FEpicAccountId UserId, FProductUserId ProductUserId)
{
	PlayerPtr Player = GetPlayer(UserId);
	if (Player != nullptr)
	{
		Player->SetProductUserID(ProductUserId);

		if (ConnectPlayers.find(ProductUserId) == ConnectPlayers.end())
		{
			ConnectPlayers.insert({ ProductUserId, Player });
		}
	}
	else
	{
		FDebugLog::LogError(L"SetProductUserID - Can't find player, Id: %ls", UserId.ToString().c_str());
	}
}

void FPlayerManager::SetPresenceRichText(FEpicAccountId UserId, const std::string RichText)
{
	EOS_Presence_CreatePresenceModificationOptions CreateModOpt = {};
	CreateModOpt.ApiVersion = EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST;
	CreateModOpt.LocalUserId = UserId;

	EOS_HPresence PresenceHandle = EOS_Platform_GetPresenceInterface(FPlatform::GetPlatformHandle());

	EOS_HPresenceModification PresenceModification;
	EOS_EResult Result = EOS_Presence_CreatePresenceModification(PresenceHandle, &CreateModOpt, &PresenceModification);
	if (Result != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Create presence modification failed: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		return;
	}

	EOS_PresenceModification_SetRawRichTextOptions SetTextOpt = {};
	SetTextOpt.ApiVersion = EOS_PRESENCE_SETRAWRICHTEXT_API_LATEST;
	SetTextOpt.RichText = RichText.c_str();
	Result = EOS_PresenceModification_SetRawRichText(PresenceModification, &SetTextOpt);
	if (Result != EOS_EResult::EOS_Success)
	{
		EOS_PresenceModification_Release(PresenceModification);

		FDebugLog::LogError(L"[EOS SDK] Set presence rich text failed: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		return;
	}

	EOS_Presence_SetPresenceOptions SetOpt = {};
	SetOpt.ApiVersion = EOS_PRESENCE_SETPRESENCE_API_LATEST;
	SetOpt.LocalUserId = UserId;
	SetOpt.PresenceModificationHandle = PresenceModification;
	EOS_Presence_SetPresence(PresenceHandle, &SetOpt, nullptr, FPlayerManager::SetPresenceCallbackFn);

	EOS_PresenceModification_Release(PresenceModification);
}

void EOS_CALL FPlayerManager::SetPresenceCallbackFn(const EOS_Presence_SetPresenceCallbackInfo* Data)
{
	if (!Data)
	{
		FDebugLog::LogError(L"[EOS SDK] Set presence failed: no data");
		return;
	}

	if (EOS_EResult_IsOperationComplete(Data->ResultCode) == EOS_FALSE)
	{
		// Operation is retrying so it is not complete yet
		return;
	}

	if (Data->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Set presence failed: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Set presence success");
}

void FPlayerManager::SetPresenceRichText(const std::wstring& RichText)
{
	if (!GetCurrentUser().IsValid())
	{
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Setting presence rich text: %ls", RichText.c_str());
	std::string RichTextStr = FStringUtils::Narrow(RichText);
	SetPresenceRichText(GetCurrentUser(), RichTextStr);
}

void FPlayerManager::SetInitialPresence()
{
	if (!GetCurrentUser().IsValid())
	{
		return;
	}

	const std::string RichTextStr = "Using " + std::string(SampleConstants::GameName);
	FDebugLog::Log(L"[EOS SDK] Setting initial presence rich text: %ls", FStringUtils::Widen(RichTextStr).c_str());
	SetPresenceRichText(GetCurrentUser(), RichTextStr);
}
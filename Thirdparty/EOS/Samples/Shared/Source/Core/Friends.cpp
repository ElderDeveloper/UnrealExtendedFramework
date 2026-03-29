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
#include "Friends.h"
#include "eos_sdk.h"
#include "eos_friends.h"
#include "eos_presence.h"
#include "eos_userinfo.h"
#include "eos_connect.h"

const double MinDelayBetweenMappingQueriesSeconds = 60.0;

FFriends::FFriends() :
	DirtyCounter(0)
{
	LastFoundFriend.UserId = FEpicAccountId();
	FoundFriendAction = FriendAction::None;
	CurrentUserId = FEpicAccountId();
}

FFriends::~FFriends()
{

}

void FFriends::Update()
{
	if (FPlayerManager::Get().GetNumPlayers() == 0)
	{
		return;
	}

	// Do we need to update mappings?
	bool bThereAreStillMissingProductUserIds = false;
	for (FFriendData& Friend : Friends)
	{
		if (!Friend.UserProductUserId.IsValid())
		{
			Friend.UserProductUserId = FGame::Get().GetUsers()->GetExternalAccountMapping(Friend.UserId);

			// Still missing?
			if (!Friend.UserProductUserId.IsValid())
			{
				bThereAreStillMissingProductUserIds = true;
			}
			else
			{
				SetDirty();
			}
		}
	}

	double TimeSinceLastQuery = Main->GetTimer().GetTotalSeconds() - LastMappingQueryTimestamp;
	const bool bHasQueriedFriendMappings = LastMappingQueryTimestamp > 0.0;

	if (bThereAreStillMissingProductUserIds && (!bHasQueriedFriendMappings || TimeSinceLastQuery > MinDelayBetweenMappingQueriesSeconds))
	{
		QueryFriendsConnectMappings(CurrentUserId);
	}
}

void FFriends::AddFriend(FEpicAccountId FriendUserId)
{
	AddFriend(CurrentUserId, FriendUserId);
}

void FFriends::AddFriend(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId)
{
	FDebugLog::Log(L"EOS SDK AddFriend: %ls", FriendUserId.ToString().c_str());

	if (!FriendUserId)
	{
		FDebugLog::LogError(L"EOS SDK AddFriend: bad friend ID provided: %ls", FriendUserId.ToString().c_str());
		return;
	}

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_Friends_SendInviteOptions SendInviteOptions = {};
	SendInviteOptions.ApiVersion = EOS_FRIENDS_SENDINVITE_API_LATEST;
	SendInviteOptions.LocalUserId = LocalUserId;
	SendInviteOptions.TargetUserId = FriendUserId;
	EOS_Friends_SendInvite(FriendsHandle, &SendInviteOptions, nullptr, &SendFriendInviteCompleteCallbackFn);

	if (LastFoundFriend.UserId == FriendUserId)
	{
		LastFoundFriend = FFriendData();
	}
}

void FFriends::AcceptInvite(FEpicAccountId FriendUserId)
{
	AcceptInvite(CurrentUserId, FriendUserId);
}

void FFriends::AcceptInvite(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId)
{
	FDebugLog::Log(L"EOS SDK Accept invite: %ls", FriendUserId.ToString().c_str());

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_Friends_AcceptInviteOptions AcceptInviteOptions = {};
	AcceptInviteOptions.ApiVersion = EOS_FRIENDS_ACCEPTINVITE_API_LATEST;
	AcceptInviteOptions.LocalUserId = LocalUserId;
	AcceptInviteOptions.TargetUserId = FriendUserId;
	EOS_Friends_AcceptInvite(FriendsHandle, &AcceptInviteOptions, nullptr, &AcceptFriendInviteCompleteCallbackFn);
}

void FFriends::RejectInvite(FEpicAccountId FriendUserId)
{
	RejectInvite(CurrentUserId, FriendUserId);
}

void FFriends::RejectInvite(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId)
{
	FDebugLog::Log(L"EOS SDK Reject invite: %ls", FriendUserId.ToString().c_str());

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_Friends_RejectInviteOptions RejectInviteOptions = {};
	RejectInviteOptions.ApiVersion = EOS_FRIENDS_REJECTINVITE_API_LATEST;
	RejectInviteOptions.LocalUserId = LocalUserId;
	RejectInviteOptions.TargetUserId = FriendUserId;
	EOS_Friends_RejectInvite(FriendsHandle, &RejectInviteOptions, nullptr, &RejectFriendInviteCompleteCallbackFn);
}

void FFriends::QueryFriends(EOS_EpicAccountId LocalUserId)
{
	if (!FPlatform::IsInitialized())
	{
		FDebugLog::LogWarning(L"[EOS SDK] Can't Query Friends - Platform Not Initialized");
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Querying Friends");

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_Friends_QueryFriendsOptions QueryFriendsOptions = {};
	QueryFriendsOptions.ApiVersion = EOS_FRIENDS_QUERYFRIENDS_API_LATEST;
	QueryFriendsOptions.LocalUserId = LocalUserId;
	EOS_Friends_QueryFriends(FriendsHandle, &QueryFriendsOptions, NULL, QueryFriendsCompleteCallbackFn);
}

void FFriends::QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId)
{
	FGame::Get().GetUsers()->QueryUserInfo(LocalUserId, TargetUserId, QueryUserInfoCompleteCallbackFn, nullptr);
}

void FFriends::QueryUserInfo(const std::wstring& DisplayName)
{
	QueryUserInfo(CurrentUserId, DisplayName);
}

void FFriends::QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName)
{
	FGame::Get().GetUsers()->QueryUserInfo(LocalUserId, DisplayName, QueryUserInfoByDisplayNameCompleteCallbackFn, nullptr);

	if (FoundFriendAction == FriendAction::None)
	{
		QueueFoundFriendAction(FriendAction::AddToList);
	}
}

void FFriends::QueueFoundFriendAction(FriendAction action)
{
	FoundFriendAction = action;
}

void FFriends::QueryFriendsPresenceInfo(EOS_EpicAccountId LocalUserId)
{
	for (const FFriendData& Friend : Friends)
	{
		if (Friend.Status == EOS_EFriendsStatus::EOS_FS_Friends)
		{
			FGame::Get().GetUsers()->QueryPresenceInfo(LocalUserId, Friend.UserId);
		}
	}
}

void FFriends::QueryFriendsConnectMappings(EOS_EpicAccountId LocalUserId)
{
	if (!CurrentProductUserId.IsValid())
	{
		return;
	}

	std::vector<FEpicAccountId> FriendsToQuery;
	for (const FFriendData& Friend : Friends)
	{
		if (!Friend.UserProductUserId.IsValid())
		{
			FriendsToQuery.push_back(Friend.UserId);
		}
	}

	if (!FriendsToQuery.empty())
	{
		FGame::Get().GetUsers()->QueryExternalAccountMappings(CurrentProductUserId, FriendsToQuery);
		LastMappingQueryTimestamp = Main->GetTimer().GetTotalSeconds();
	}
}

void FFriends::SetFriends(std::vector<FFriendData>&& InFriends)
{
	SetFriends(CurrentUserId, std::move(InFriends));
}

void FFriends::SetFriends(EOS_EpicAccountId LocalUserId, std::vector<FFriendData>&& InFriends)
{
	if (LocalUserId == CurrentUserId)
	{
		// check if we can reuse user data from last time - if the display name from last time is still Pending, try to QueryInfo
		for (FFriendData& NextFriend : InFriends)
		{
			auto Iter = std::find_if(Friends.begin(), Friends.end(), [NextFriend](const FFriendData& FriendEntry) { return FriendEntry.UserId == NextFriend.UserId; });
			if (Iter != Friends.end() && Iter->Name != L"Pending...")
			{
				// copy previously cached data
				NextFriend.Name = Iter->Name;
			}
			else if(NextFriend.UserId.IsValid())
			{
				// query friend info only if the UserId is valid. We have a TESTFRIENDS command that passes Friends info without UserId. This check avoids errors.
				QueryUserInfo(LocalUserId, NextFriend.UserId);
			}
		}

		Friends.swap(InFriends);

		if (LastFoundFriend.IsValid())
		{
			Friends.push_back(LastFoundFriend);
		}

		SetDirty();

		bInitialFriendQueryFinished = true;
	}
}

void FFriends::CreateFriendData(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId)
{
	FFriendData RetrievedFriendData;
	RetrievedFriendData.LocalUserId = LocalUserId;
	RetrievedFriendData.UserId = TargetUserId;

	FUserData RetrievedUserData = FGame::Get().GetUsers()->CreateUserData(LocalUserId, TargetUserId);
	if (RetrievedUserData.IsValid())
	{
		RetrievedFriendData.Name = RetrievedUserData.Name;
	}
	// OnUserInfoRetrieved handles adding or updating friend data
	FGame::Get().GetFriends()->OnUserInfoRetrieved(RetrievedFriendData);
}

void FFriends::SubscribeToFriendStatusUpdates(void* Owner, std::function<void(const std::vector<FFriendData>&)> Callback)
{
	if (Owner)
	{
		FriendStatusUpdateCallbacks[Owner] = Callback;
	}
}

void FFriends::UnsubscribeToFriendStatusUpdates(void* Owner)
{
	if (Owner)
	{
		FriendStatusUpdateCallbacks.erase(Owner);
	}
}

void FFriends::OnUserInfoRetrieved(const FFriendData& FriendData)
{
	auto Iter = std::find_if(Friends.begin(), Friends.end(), [FriendData](const FFriendData& FriendEntry) { return FriendEntry.UserId == FriendData.UserId; });
	if (Iter != Friends.end())
	{
		if (Iter->Name != FriendData.Name)
		{
			Iter->Name = FriendData.Name;
			SetDirty();
		}
	}
	else if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_NotFriends)
	{
		Friends.push_back(FriendData);
		SetDirty();
	}
}

void FFriends::OnUserFound(const FFriendData& FriendData)
{
	switch (FoundFriendAction)
	{
	case FriendAction::AddToList:
		if (!FriendData.bPlaceholder)
		{
			LastFoundFriend = FriendData;
		}
		Friends.push_back(FriendData);
		break;
	case FriendAction::Invite:
		AddFriend(FriendData.LocalUserId, FriendData.UserId);
		break;
	default:
		// no action, do nothing
		break;
	}

	FoundFriendAction = FriendAction::None;
}

void FFriends::OnLoggedIn(FEpicAccountId UserId)
{
	if (!CurrentUserId.IsValid())
	{
		SetCurrentUser(UserId);
	}

	SubscribeToFriendUpdates(UserId);
}

void FFriends::OnConnectLoggedIn(FProductUserId ProductUserId)
{
	if (!CurrentProductUserId.IsValid())
	{
		CurrentProductUserId = ProductUserId;
		// QueryFriends will end up calling EOS_Connect_QueryExternalAccountMappings and needs a valid local PUID
		bInitialFriendQueryFinished = false;
		QueryFriends(CurrentUserId);
	}
}

void FFriends::OnLoggedOut(FEpicAccountId UserId)
{
	Friends.clear();

	UnsubscribeFromFriendUpdates(UserId);

	if (FPlayerManager::Get().GetNumPlayers() > 0)
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
		CurrentProductUserId = FProductUserId();
	}
}

void FFriends::SubscribeToFriendUpdates(FEpicAccountId UserId)
{
	UnsubscribeFromFriendUpdates(UserId);

	// Subscribe for friend status updates
	EOS_Friends_AddNotifyFriendsUpdateOptions Options = {};
	Options.ApiVersion = EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST;

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_NotificationId NotificationId = EOS_Friends_AddNotifyFriendsUpdate(FriendsHandle, &Options, nullptr, FriendUpdateCallback);
	if (!NotificationId)
	{
		FDebugLog::LogError(L"[EOS SDK]: could not subscribe to friend status updates.");
	}
	else
	{
		FriendNotifications[UserId] = NotificationId;
	}

	// Also subscribe to presence updates
	EOS_Presence_AddNotifyOnPresenceChangedOptions PresenceOptions = {};
	PresenceOptions.ApiVersion = EOS_PRESENCE_ADDNOTIFYONPRESENCECHANGED_API_LATEST;
	EOS_HPresence PresenceHandle = EOS_Platform_GetPresenceInterface(FPlatform::GetPlatformHandle());
	EOS_NotificationId PresenceNotificationId = EOS_Presence_AddNotifyOnPresenceChanged(PresenceHandle, &PresenceOptions, nullptr, PresenceUpdateCallback);
	if (!PresenceNotificationId)
	{
		FDebugLog::LogError(L"[EOS SDK]: could not subscribe to presence updates.");
	}
	else
	{
		PresenceNotifications[UserId] = PresenceNotificationId;
	}
}

void FFriends::UnsubscribeFromFriendUpdates(FEpicAccountId UserId)
{
	auto Iter = FriendNotifications.find(UserId);
	if (Iter != FriendNotifications.end())
	{
		EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());
		EOS_Friends_RemoveNotifyFriendsUpdate(FriendsHandle, Iter->second);
		FriendNotifications.erase(Iter);
	}

	auto PresenceIter = PresenceNotifications.find(UserId);
	if (PresenceIter != PresenceNotifications.end())
	{
		EOS_HPresence PresenceHandle = EOS_Platform_GetPresenceInterface(FPlatform::GetPlatformHandle());
		EOS_Presence_RemoveNotifyOnPresenceChanged(PresenceHandle, PresenceIter->second);
		PresenceNotifications.erase(PresenceIter);
	}
}

void FFriends::FriendStatusChanged(FEpicAccountId LocalUserId, FEpicAccountId TargetUserId, EOS_EFriendsStatus NewStatus)
{
	if (CurrentUserId == LocalUserId)
	{
		if (NewStatus == EOS_EFriendsStatus::EOS_FS_NotFriends)
		{
			auto Iter = std::find_if(Friends.begin(), Friends.end(), [TargetUserId](const FFriendData& Friend) { return Friend.UserId == TargetUserId; });
			if (Iter != Friends.end())
			{
				Friends.erase(Iter);
				SetDirty();
			}
		}
		else
		{
			bool bFoundFriend = false;
			for (FFriendData& NextFriend : Friends)
			{
				if (NextFriend.UserId == TargetUserId)
				{
					if (NextFriend.Status != NewStatus)
					{
						NextFriend.Status = NewStatus;
						bFoundFriend = true;
					}
					break;
				}
			}

			// No such friend, need to refresh list (must be new friend?) unless initial friend query is not finished yet.
			if (!bFoundFriend && bInitialFriendQueryFinished)
			{
				QueryFriends(LocalUserId);
			}
		}

		for(auto& CallbackPair : FriendStatusUpdateCallbacks)
		{
			CallbackPair.second(Friends);
		}
	}
}

void FFriends::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		FEpicAccountId UserId = Event.GetUserId();
		OnLoggedIn(UserId);
	}
	else if (Event.GetType() == EGameEventType::UserLoggedOut)
	{
		FEpicAccountId UserId = Event.GetUserId();
		OnLoggedOut(UserId);
	}
	else if (Event.GetType() == EGameEventType::UserConnectLoggedIn)
	{
		OnConnectLoggedIn(Event.GetProductUserId());
	}
	else if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
		if (FPlayerManager::Get().GetNumPlayers() > 0)
		{
			FEpicAccountId PrevPlayerId = FPlayerManager::Get().GetPrevPlayerId(GetCurrentUser());
			if (PrevPlayerId.IsValid())
			{
				SetCurrentUser(PrevPlayerId);
				SetDirty();
				QueryFriends(PrevPlayerId);
			}
			else
			{
				SetCurrentUser(FEpicAccountId());
			}
		}
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
		if (FPlayerManager::Get().GetNumPlayers() > 0)
		{
			FEpicAccountId NextPlayerId = FPlayerManager::Get().GetNextPlayerId(GetCurrentUser());
			if (NextPlayerId.IsValid())
			{
				SetCurrentUser(NextPlayerId);
				SetDirty();
				QueryFriends(NextPlayerId);
			}
			else
			{
				SetCurrentUser(FEpicAccountId());
			}
		}
	}
	else if (Event.GetType() == EGameEventType::ExternalAccountsMappingRetrieved)
	{
		// SetFriends will end up calling EOS_UserInfo_CopyBestDisplayName. This can only be called after external account mappings are retrieved
		if (const std::unique_ptr<FFriends>& FriendsPtr = FGame::Get().GetFriends())
		{
			FriendsPtr->SetFriends(CurrentUserId, std::move(Friends));
		}
	}
	else if (Event.GetType() == EGameEventType::CancelLogin)
	{
		SetDirty();
	}
}

std::wstring FFriends::GetFriendName(FEpicAccountId FriendId) const
{
	for (const FFriendData& Friend : Friends)
	{
		if (Friend.IsValid() && Friend.UserId == FriendId)
		{
			return Friend.Name;
		}
	}

	return std::wstring();
}

std::wstring FFriends::GetFriendName(FProductUserId FriendId) const
{
	for (const FFriendData& Friend : Friends)
	{
		if (Friend.IsValid() && Friend.UserProductUserId == FriendId)
		{
			return Friend.Name;
		}
	}

	return std::wstring();
}

void FFriends::OnUserPresenceInfoRetrieved(FEpicAccountId UserId, FPresenceInfo& Info)
{
	auto Iter = std::find_if(Friends.begin(), Friends.end(), [UserId](const FFriendData& FriendEntry) { return FriendEntry.UserId == UserId; });
	if (Iter != Friends.end())
	{
		Iter->Presence = Info;

		SetDirty();

		FDebugLog::Log(L"[EOS SDK] User presence Info retrieved - UserId: %ls", UserId.ToString().c_str());
		FDebugLog::Log(L"  Status: %ls", FFriends::FriendPresenceToString(Info.Status).c_str());
		FDebugLog::Log(L"  Application: %ls", Info.Application.c_str());
		FDebugLog::Log(L"  Rich Text: %ls", Info.RichText.c_str());
		FDebugLog::Log(L"  Platform: %ls", Info.Platform.c_str());
	}
}

std::wstring FFriends::FriendPresenceToString(EOS_Presence_EStatus PresenceStatus)
{
	switch (PresenceStatus)
	{
		case EOS_Presence_EStatus::EOS_PS_Online:
			return L"Online";
		case EOS_Presence_EStatus::EOS_PS_Offline:
			return L"Offline";
		case EOS_Presence_EStatus::EOS_PS_Away:
			return L"Away";
		case EOS_Presence_EStatus::EOS_PS_DoNotDisturb:
			return L"Do not disturb";
		case EOS_Presence_EStatus::EOS_PS_ExtendedAway:
			return L"Extended away";
		default:
			return L"Unknown";
	}
}

void FFriends::SetCurrentUser(FEpicAccountId UserId)
{
	CurrentUserId = UserId;

	if (CurrentUserId.IsValid())
	{
		PlayerPtr CurrentPlayer = FPlayerManager::Get().GetPlayer(CurrentUserId);
		if (CurrentPlayer)
		{
			CurrentProductUserId = CurrentPlayer->GetProductUserID();
		}
		else
		{
			CurrentProductUserId = FProductUserId();
		}
	}
	else
	{
		CurrentProductUserId = FProductUserId();
	}

	bInitialFriendQueryFinished = false;

	// QueryFriends calls EOS_Connect_QueryExternalAccountMapings which requires a valid local PUID
	if (CurrentUserId.IsValid() && CurrentProductUserId.IsValid())
	{
		QueryFriends(CurrentUserId);
	}
}

std::wstring FFriends::FriendStatusToString(EOS_EFriendsStatus status)
{
	switch (status)
	{
	case EOS_EFriendsStatus::EOS_FS_NotFriends:
		return L"Not Friends";
	case EOS_EFriendsStatus::EOS_FS_InviteSent:
		return L"Invite Sent";
	case EOS_EFriendsStatus::EOS_FS_InviteReceived:
		return L"Invite Received";
	case EOS_EFriendsStatus::EOS_FS_Friends:
		return L"Friends";
	}

	return L"Unknown";
}

void EOS_CALL FFriends::QueryFriendsCompleteCallbackFn(const EOS_Friends_QueryFriendsCallbackInfo* FriendData)
{
	assert(FriendData != NULL);

	if (FriendData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query friends error: %ls", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Query Friends Complete - User ID: %ls", FEpicAccountId(FriendData->LocalUserId).ToString().c_str());

	EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(FPlatform::GetPlatformHandle());

	EOS_Friends_GetFriendsCountOptions FriendsCountOptions = {};
	FriendsCountOptions.ApiVersion = EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST;
	FriendsCountOptions.LocalUserId = FriendData->LocalUserId;
	int32_t FriendCount = EOS_Friends_GetFriendsCount(FriendsHandle, &FriendsCountOptions);

	FDebugLog::Log(L"[EOS SDK] NumFriends: %d", FriendCount);

	std::vector<FFriendData> NewFriends;

	EOS_Friends_GetFriendAtIndexOptions IndexOptions = {};
	IndexOptions.ApiVersion = EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST;
	IndexOptions.LocalUserId = FriendData->LocalUserId;
	for (int32_t FriendIdx = 0; FriendIdx < FriendCount; ++FriendIdx)
	{
		IndexOptions.Index = FriendIdx;
		EOS_EpicAccountId FriendUserId = EOS_Friends_GetFriendAtIndex(FriendsHandle, &IndexOptions);

		if (EOS_EpicAccountId_IsValid(FriendUserId))
		{
			EOS_Friends_GetStatusOptions StatusOptions = {};
			StatusOptions.ApiVersion = EOS_FRIENDS_GETSTATUS_API_LATEST;
			StatusOptions.LocalUserId = FriendData->LocalUserId;
			StatusOptions.TargetUserId = FriendUserId;
			EOS_EFriendsStatus FriendStatus = EOS_Friends_GetStatus(FriendsHandle, &StatusOptions);

			FDebugLog::Log(L"[EOS SDK] FriendStatus: %ls: %ls", FEpicAccountId(FriendUserId).ToString().c_str(), FriendStatusToString(FriendStatus).c_str());

			FFriendData FriendDataEntry;
			FriendDataEntry.LocalUserId = FriendData->LocalUserId;
			FriendDataEntry.UserId = FriendUserId;
			FriendDataEntry.Name = L"Pending...";
			FriendDataEntry.Status = FriendStatus;

			NewFriends.push_back(FriendDataEntry);
		}
		else
		{
			FDebugLog::LogWarning(L"[EOS SDK] Friend ID was invalid!");
		}
	}

	if (const std::unique_ptr<FFriends>& Friends = FGame::Get().GetFriends())
	{
		// Friends list is incomplete at this time. The Name is set to "Pending". The Name will be set from SetFriends when QueryFriendsAccountMappings completes
		Friends->Friends = NewFriends; 
		Friends->QueryFriendsPresenceInfo(FriendData->LocalUserId);
		Friends->QueryFriendsConnectMappings(FriendData->LocalUserId);
	}
}

void EOS_CALL FFriends::QueryUserInfoCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoCallbackInfo* FriendData)
{
	assert(FriendData != NULL);

	if (FriendData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query User Info error: %ls", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());
		return;
	}

	FUserInfoQueryPayload* UserInfoQueryData = (FUserInfoQueryPayload*)(FriendData->ClientData);
	FDebugLog::Log(L"[EOS SDK] Query User Info Complete - User ID: %ls", FEpicAccountId(UserInfoQueryData->TargetUserId).ToString().c_str());

	FGame::Get().GetFriends()->CreateFriendData(FriendData->LocalUserId, UserInfoQueryData->TargetUserId);

	delete UserInfoQueryData;
}

void EOS_CALL FFriends::QueryUserInfoByDisplayNameCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* FriendData)
{
	assert(FriendData != NULL);

	if (FriendData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query User Info error: %ls", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());

		// send placeholder friend to show it was not found
		FFriendData NotFoundUserInfo;
		NotFoundUserInfo.Name = FStringUtils::Widen(std::string(FriendData->DisplayName) + " NOT FOUND");
		NotFoundUserInfo.Status = EOS_EFriendsStatus::EOS_FS_NotFriends;
		NotFoundUserInfo.bPlaceholder = true;
		FGame::Get().GetFriends()->OnUserFound(NotFoundUserInfo);

		return;
	}

	FDebugLog::Log(L"[EOS SDK] Query User Info Complete - User ID: %ls", FEpicAccountId(FriendData->TargetUserId).ToString().c_str());

	FFriendData FoundUserInfo;
	FoundUserInfo.Name = FStringUtils::Widen(FriendData->DisplayName);
	FoundUserInfo.Status = EOS_EFriendsStatus::EOS_FS_NotFriends;
	FoundUserInfo.LocalUserId = FriendData->LocalUserId;
	FoundUserInfo.UserId = FriendData->TargetUserId;
	FoundUserInfo.UserProductUserId = FGame::Get().GetUsers()->GetExternalAccountMapping(FriendData->TargetUserId);

	FGame::Get().GetFriends()->OnUserFound(FoundUserInfo);
}

void EOS_CALL FFriends::SendFriendInviteCompleteCallbackFn(const EOS_Friends_SendInviteCallbackInfo* FriendData)
{
	if (FriendData)
	{
		if (FriendData->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Friend invitation successfully sent.");

			FGame::Get().GetFriends()->QueryFriends(FriendData->LocalUserId);
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Send friend invite error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());
		}
	}
}

void EOS_CALL FFriends::AcceptFriendInviteCompleteCallbackFn(const EOS_Friends_AcceptInviteCallbackInfo* FriendData)
{
	if (FriendData)
	{
		if (FriendData->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Friend invitation successfully accepted.");

			FGame::Get().GetFriends()->QueryFriends(FriendData->LocalUserId);
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Accept friend invite error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());
		}
	}
}

void EOS_CALL FFriends::RejectFriendInviteCompleteCallbackFn(const EOS_Friends_RejectInviteCallbackInfo* FriendData)
{
	if (FriendData)
	{
		if (FriendData->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Friend invitation successfully rejected.");

			FGame::Get().GetFriends()->QueryFriends(FriendData->LocalUserId);
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Reject friend invite error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(FriendData->ResultCode)).c_str());
		}
	}
}

void EOS_CALL FFriends::FriendUpdateCallback(const EOS_Friends_OnFriendsUpdateInfo* Data)
{
	if (Data)
	{
		FGame::Get().GetFriends()->FriendStatusChanged(Data->LocalUserId, Data->TargetUserId, Data->CurrentStatus);
	}
}

void EOS_CALL FFriends::PresenceUpdateCallback(const EOS_Presence_PresenceChangedCallbackInfo* Data)
{
	if (Data)
	{
		// Query presence info
		FGame::Get().GetUsers()->QueryPresenceInfo(Data->LocalUserId, Data->PresenceUserId);
	}
}

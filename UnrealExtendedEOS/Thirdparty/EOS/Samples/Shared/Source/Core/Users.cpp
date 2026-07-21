// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Platform.h"
#include "Game.h"
#include "GameEvent.h"
#include "Player.h"
#include "Users.h"
#include "eos_sdk.h"
#include "eos_userinfo.h"
#include "eos_presence.h"

FUsers::FUsers()
{

}

FUsers::~FUsers()
{

}

void FUsers::Update()
{
	if (!FPlayerManager::Get().GetCurrentUser().IsValid())
	{
		return;
	}

	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		return;
	}

	if (!CurrentlyQueriedExternalToEpicAccounts.empty())
	{
		UpdateQueriedAccountMappings(Player->GetProductUserID());
	}

	if (!CurrentlyQueriedExternalAccounts.empty())
	{
		UpdateExternalAccountMappings(Player->GetProductUserID());
	}
}

void FUsers::QueryUserInfo(EOS_EpicAccountId TargetUserId, std::function<void(const FUserData&)> UserInfoRetrievedCallback)
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player != nullptr)
	{
		QueryUserInfo(Player->GetUserID(), TargetUserId, QueryUserInfoCompleteCallbackFn, UserInfoRetrievedCallback);
	}
	else
	{
		QueryUserInfo(TargetUserId, TargetUserId, QueryUserInfoCompleteCallbackFn, UserInfoRetrievedCallback);
	}
}

void FUsers::QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId, std::function<void(const FUserData&)> UserInfoRetrievedCallback)
{
	QueryUserInfo(LocalUserId, TargetUserId, QueryUserInfoCompleteCallbackFn, UserInfoRetrievedCallback);
}

void FUsers::QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName, std::function<void(const FUserData&)> UserInfoRetrievedCallback)
{
	QueryUserInfo(LocalUserId, DisplayName, QueryUserInfoByDisplayNameCompleteCallbackFn, UserInfoRetrievedCallback);
}

void FUsers::QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId, const EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate, std::function<void(const FUserData&)> UserInfoRetrievedCallback)
{
	EOS_HUserInfo UserInfoInterface = EOS_Platform_GetUserInfoInterface(FPlatform::GetPlatformHandle());

	EOS_UserInfo_QueryUserInfoOptions QueryUserInfoOptions = {};
	QueryUserInfoOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
	QueryUserInfoOptions.LocalUserId = LocalUserId;
	QueryUserInfoOptions.TargetUserId = TargetUserId;

	FUserInfoQueryPayload* UserInfoQueryData = new FUserInfoQueryPayload();
	UserInfoQueryData->TargetUserId = TargetUserId;
	UserInfoQueryData->OnUserInfoRetrievedCallback = UserInfoRetrievedCallback;

	EOS_UserInfo_QueryUserInfo(UserInfoInterface, &QueryUserInfoOptions, UserInfoQueryData, CompletionDelegate);
}

void FUsers::QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName, const EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback CompletionDelegate, std::function<void(const FUserData&)> UserInfoRetrievedCallback)
{
	EOS_HUserInfo UserInfoInterface = EOS_Platform_GetUserInfoInterface(FPlatform::GetPlatformHandle());

	EOS_UserInfo_QueryUserInfoByDisplayNameOptions QueryUserInfoOptions = {};
	QueryUserInfoOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST;
	QueryUserInfoOptions.LocalUserId = LocalUserId;
	std::string DisplayNameString = FStringUtils::Narrow(DisplayName);
	QueryUserInfoOptions.DisplayName = DisplayNameString.c_str();

	FUserInfoQueryPayload* UserInfoQueryData = new FUserInfoQueryPayload();
	UserInfoQueryData->TargetUserId = EOS_EpicAccountId(); // not used
	UserInfoQueryData->OnUserInfoRetrievedCallback = UserInfoRetrievedCallback;

	EOS_UserInfo_QueryUserInfoByDisplayName(UserInfoInterface, &QueryUserInfoOptions, UserInfoQueryData, CompletionDelegate);
}

void EOS_CALL QueryUserPresenceCompleteCallbackFn(const EOS_Presence_QueryPresenceCallbackInfo* UserData)
{
	assert(UserData != NULL);

	if (UserData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query User presence error: %ls", FStringUtils::Widen(EOS_EResult_ToString(UserData->ResultCode)).c_str());
		return;
	}

	EOS_HPresence PresenceInfoInterface = EOS_Platform_GetPresenceInterface(FPlatform::GetPlatformHandle());
	EOS_Presence_CopyPresenceOptions Options = {};
	Options.ApiVersion = EOS_PRESENCE_COPYPRESENCE_API_LATEST;
	Options.LocalUserId = UserData->LocalUserId;
	Options.TargetUserId = UserData->TargetUserId;

	EOS_Presence_Info* Presence = nullptr;

	if (EOS_Presence_CopyPresence(PresenceInfoInterface, &Options, &Presence) == EOS_EResult::EOS_Success && Presence)
	{
		FPresenceInfo PresenceInfo;
		PresenceInfo.Application = FStringUtils::Widen(Presence->ProductId);
		PresenceInfo.Platform = FStringUtils::Widen(Presence->Platform);
		PresenceInfo.Status = Presence->Status;
		PresenceInfo.RichText = FStringUtils::Widen(Presence->RichText);

		FGame::Get().GetFriends()->OnUserPresenceInfoRetrieved(UserData->TargetUserId, PresenceInfo);

		EOS_Presence_Info_Release(Presence);
	}
	else
	{
		FDebugLog::LogError(L"[EOS SDK] Copy user presence error.");
	}
}

void FUsers::QueryPresenceInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId)
{
	EOS_HPresence PresenceInfoInterface = EOS_Platform_GetPresenceInterface(FPlatform::GetPlatformHandle());
	EOS_Presence_QueryPresenceOptions Options = {};
	Options.ApiVersion = EOS_PRESENCE_QUERYPRESENCE_API_LATEST;
	Options.LocalUserId = LocalUserId;
	Options.TargetUserId = TargetUserId;
	EOS_Presence_QueryPresence(PresenceInfoInterface, &Options, nullptr, QueryUserPresenceCompleteCallbackFn);
}

void FUsers::QueryExternalAccountMappings(FProductUserId CurrentUser, const std::vector<FEpicAccountId>& AccountsToQuery)
{
	if (!CurrentUser.IsValid() || AccountsToQuery.empty())
	{
		FDebugLog::LogError(L"[EOS SDK] QueryExternalAccountMappings: bad user or nothing to query.");
		return;
	}

	std::vector<FEpicAccountId> FilteredAccountsToQuery;

	// Filter out the once that were already queried lately.
	for (const FEpicAccountId& NextAccountId : AccountsToQuery)
	{
		if (ExternalAccountsMap.find(NextAccountId) == ExternalAccountsMap.end())
		{
			if (std::find(CurrentlyQueriedExternalAccounts.begin(), CurrentlyQueriedExternalAccounts.end(), NextAccountId) == CurrentlyQueriedExternalAccounts.end())
			{
				FilteredAccountsToQuery.push_back(NextAccountId);
			}
		}
	}

	if (FilteredAccountsToQuery.empty())
	{
		// Nothing left to query
		return;
	}

	std::vector<std::string> OutstandingExternalAccountsToQueryStrings;
	std::vector<const char*> OutstandingExternalAccountsToQueryChars;
	for (const FEpicAccountId& NextAccountId : FilteredAccountsToQuery)
	{
		OutstandingExternalAccountsToQueryStrings.push_back(FAccountHelpers::EpicAccountIDToString(NextAccountId));
		OutstandingExternalAccountsToQueryChars.push_back(OutstandingExternalAccountsToQueryStrings.back().c_str());
	}

	EOS_Connect_QueryExternalAccountMappingsOptions QueryOptions = {};
	QueryOptions.ApiVersion = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
	QueryOptions.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
	QueryOptions.LocalUserId = CurrentUser;

	std::vector<const char*> ExternalAccountsToQuery;
	for (const char* ExternalAccountId : OutstandingExternalAccountsToQueryChars)
	{
		ExternalAccountsToQuery.push_back(ExternalAccountId);

		if (ExternalAccountsToQuery.size() == OutstandingExternalAccountsToQueryChars.size() ||
			ExternalAccountsToQuery.size() >= EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_MAX_ACCOUNT_IDS)
		{
			QueryOptions.ExternalAccountIdCount = static_cast<uint32_t>(ExternalAccountsToQuery.size());
			QueryOptions.ExternalAccountIds = ExternalAccountsToQuery.data();

			EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
			EOS_Connect_QueryExternalAccountMappings(ConnectHandle, &QueryOptions, nullptr, OnQueryExternalAccountMappingsCallback);

			ExternalAccountsToQuery.clear();
		}
	}

	for (const FEpicAccountId& NextAccountId : FilteredAccountsToQuery)
	{
		CurrentlyQueriedExternalAccounts.insert(NextAccountId);
	}
}

FProductUserId FUsers::GetExternalAccountMapping(FEpicAccountId TargetUserId)
{
	if (ExternalAccountsMap.find(TargetUserId) != ExternalAccountsMap.end())
	{
		return ExternalAccountsMap[TargetUserId];
	}

	return FProductUserId();
}

void FUsers::QueryAccountMappings(FProductUserId CurrentUser, const std::vector<FProductUserId>& AccountsToQuery)
{
	if (!CurrentUser.IsValid() || AccountsToQuery.empty())
	{
		FDebugLog::LogError(L"[EOS SDK] QueryAccountMappings: bad user or nothing to query.");
		return;
	}

	std::vector<FProductUserId> FilteredAccountsToQuery;

	// Filter out the once that were already queried lately.
	for (const FProductUserId& NextProductId : AccountsToQuery)
	{
		if (ExternalToEpicAccountsMap.find(NextProductId) == ExternalToEpicAccountsMap.end())
		{
			if (std::find(CurrentlyQueriedExternalToEpicAccounts.begin(), CurrentlyQueriedExternalToEpicAccounts.end(), NextProductId) == CurrentlyQueriedExternalToEpicAccounts.end())
			{
				FilteredAccountsToQuery.push_back(NextProductId);
			}
		}
	}

	if (FilteredAccountsToQuery.empty())
	{
		FDebugLog::Log(L"[EOS SDK] QueryAccountMappings: no change.");

		FGameEvent Event(EGameEventType::EpicAccountsMappingNoChange, CurrentUser);
		FGame::Get().OnGameEvent(Event);

		// Nothing left to query
		return;
	}

	std::vector<EOS_ProductUserId> ProductIds;
	for (const FProductUserId& NextProductId : AccountsToQuery)
	{
		ProductIds.push_back(NextProductId.AccountId);
	}

	EOS_Connect_QueryProductUserIdMappingsOptions QueryOptions = {};
	QueryOptions.ApiVersion = EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST;
	QueryOptions.LocalUserId = CurrentUser;

	QueryOptions.ProductUserIdCount = static_cast<uint32_t>(ProductIds.size());
	QueryOptions.ProductUserIds = ProductIds.data();

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
	EOS_Connect_QueryProductUserIdMappings(ConnectHandle, &QueryOptions, nullptr, OnQueryAccountMappingsCallback);

	for (const FProductUserId& NextProductId : FilteredAccountsToQuery)
	{
		CurrentlyQueriedExternalToEpicAccounts.insert(NextProductId);
	}
}

FEpicAccountId FUsers::GetAccountMapping(FProductUserId TargetUserId)
{
	if (ExternalToEpicAccountsMap.find(TargetUserId) != ExternalToEpicAccountsMap.end())
	{
		return ExternalToEpicAccountsMap[TargetUserId];
	}

	return FEpicAccountId();
}

void FUsers::ClearMappings()
{
	ExternalAccountsMap.clear();
	ExternalToEpicAccountsMap.clear();
}

void FUsers::QueryDisplayName(FEpicAccountId TargetUserId)
{
	if (!TargetUserId)
	{
		return;
	}

	auto Iter = CurrentlyQueriedDisplayNames.find(TargetUserId);
	if (Iter != CurrentlyQueriedDisplayNames.end())
	{
		// Already queried
		return;
	}

	CurrentlyQueriedDisplayNames.insert(TargetUserId);

	// Do the actual query
	FUsers::QueryUserInfo(TargetUserId, OnQueryDisplayNameFinishedCallback);
}

std::wstring FUsers::GetDisplayName(FEpicAccountId TargetUserId)
{
	auto Iter = DisplayNamesMap.find(TargetUserId);
	if (Iter != DisplayNamesMap.end())
	{
		return Iter->second;
	}

	return std::wstring();
}

void FUsers::SetDisplayName(FEpicAccountId TargetUserId, const std::wstring& DisplayName)
{
	DisplayNamesMap[TargetUserId] = DisplayName;
	CurrentlyQueriedDisplayNames.erase(TargetUserId);
}

std::wstring FUsers::GetExternalAccountDisplayName(FEpicAccountId LocalUserId, FEpicAccountId TargetUserId, EOS_EExternalAccountType ExternalAccountType)
{
	EOS_HUserInfo UserInfoInterface = EOS_Platform_GetUserInfoInterface(FPlatform::GetPlatformHandle());

	EOS_UserInfo_CopyExternalUserInfoByAccountTypeOptions ExternalUserInfoOptions = {};
	ExternalUserInfoOptions.ApiVersion = EOS_USERINFO_COPYEXTERNALUSERINFOBYACCOUNTTYPE_API_LATEST;
	ExternalUserInfoOptions.LocalUserId = LocalUserId;
	ExternalUserInfoOptions.TargetUserId = TargetUserId;
	ExternalUserInfoOptions.AccountType = ExternalAccountType;

	EOS_UserInfo_ExternalUserInfo* OutExternalUserInfo = nullptr;

	EOS_EResult CopyResult = EOS_UserInfo_CopyExternalUserInfoByAccountType(UserInfoInterface, &ExternalUserInfoOptions, &OutExternalUserInfo);

	std::wstring DisplayName;
	
	if (CopyResult == EOS_EResult::EOS_Success)
	{
		if (OutExternalUserInfo != nullptr && OutExternalUserInfo->DisplayName != nullptr)
		{
			DisplayName = FStringUtils::Widen(OutExternalUserInfo->DisplayName);
		}

		EOS_UserInfo_ExternalUserInfo_Release(OutExternalUserInfo);
	}

	return DisplayName;
}

void FUsers::UpdateExternalAccountMappings(EOS_ProductUserId LocalProductUserId)
{
	std::vector<FEpicAccountId> MappingsReceived;
	for (const FEpicAccountId& NextId : CurrentlyQueriedExternalAccounts)
	{
		EOS_Connect_GetExternalAccountMappingsOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST;
		Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
		Options.LocalUserId = LocalProductUserId;
		std::string NextIdString = FAccountHelpers::EpicAccountIDToString(NextId);
		Options.TargetExternalUserId = NextIdString.c_str();

		EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
		EOS_ProductUserId NewMapping = EOS_Connect_GetExternalAccountMapping(ConnectHandle, &Options);
		if (NewMapping)
		{
			ExternalAccountsMap[NextId] = NewMapping;
			MappingsReceived.push_back(NextId);
		}
	}

	for (const FEpicAccountId& NextId : MappingsReceived)
	{
		CurrentlyQueriedExternalAccounts.erase(NextId);
	}

	if (!MappingsReceived.empty())
	{
		FGameEvent Event(EGameEventType::ExternalAccountsMappingRetrieved, LocalProductUserId);
		FGame::Get().OnGameEvent(Event);
	}
}

void FUsers::OnExternalAccountMappingsQueryFailure(EOS_ProductUserId LocalProductUserId)
{
	// Clear account ids that we were trying to query
	CurrentlyQueriedExternalAccounts.clear();
}

void FUsers::UpdateQueriedAccountMappings(EOS_ProductUserId LocalProductUserId)
{
	std::vector<FProductUserId> MappingsReceived;
	for (const FProductUserId& NextId : CurrentlyQueriedExternalToEpicAccounts)
	{
		EOS_Connect_GetProductUserIdMappingOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST;
		Options.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;
		Options.LocalUserId = LocalProductUserId;
		Options.TargetProductUserId = NextId.AccountId;

		EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
		char Buffer[EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH];
		int32_t IDStringSize = EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH;
		EOS_EResult Result = EOS_Connect_GetProductUserIdMapping(ConnectHandle, &Options, Buffer, &IDStringSize);
		std::string IDString(Buffer, IDStringSize);
		if (Result == EOS_EResult::EOS_Success)
		{
			EOS_EpicAccountId NewMapping = FAccountHelpers::EpicAccountIDFromString(IDString.c_str());
			ExternalToEpicAccountsMap[NextId] = NewMapping;
			MappingsReceived.push_back(NextId);
		}
	}

	for (const FProductUserId& NextId : MappingsReceived)
	{
		CurrentlyQueriedExternalToEpicAccounts.erase(NextId);
	}

	if (!MappingsReceived.empty())
	{
		FGameEvent Event(EGameEventType::EpicAccountsMappingRetrieved, LocalProductUserId);
		FGame::Get().OnGameEvent(Event);
	}
}

void FUsers::OnAccountMappingsQueryFailure(EOS_ProductUserId LocalProductUserId)
{
	// Clear account ids that we were trying to query
	CurrentlyQueriedExternalToEpicAccounts.clear();
}

FUserData FUsers::CreateUserData(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId)
{
	FUserData UserData;

	EOS_HUserInfo UserInfoInterface = EOS_Platform_GetUserInfoInterface(FPlatform::GetPlatformHandle());

	EOS_UserInfo_CopyBestDisplayNameOptions Options = {};
	Options.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST;
	Options.LocalUserId = LocalUserId;
	Options.TargetUserId = TargetUserId;

	EOS_UserInfo_BestDisplayName* BestDisplayName = nullptr;

	EOS_EResult Result = EOS_UserInfo_CopyBestDisplayName(UserInfoInterface, &Options, &BestDisplayName);

	if (Result == EOS_EResult::EOS_Success)
	{
		assert(BestDisplayName->DisplayName);

		UserData.UserId = TargetUserId;
		if (BestDisplayName->DisplayName)
		{
			UserData.Name = FStringUtils::Widen(BestDisplayName->DisplayName);
		}

		EOS_UserInfo_BestDisplayName_Release(BestDisplayName);
	}
	else if (Result == EOS_EResult::EOS_UserInfo_BestDisplayNameIndeterminate)
	{
		EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions WithPlatformOptions = {};
		WithPlatformOptions.ApiVersion = EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST;
		WithPlatformOptions.LocalUserId = LocalUserId;
		WithPlatformOptions.TargetUserId = TargetUserId;
		// Showcase samples don't render a platform specific list of users, e.g. list of platform friends, which is why falling back to EOS_OPT_Epic platform is appropriate here.
		WithPlatformOptions.TargetPlatformType = EOS_OPT_Epic;

		EOS_EResult Result = EOS_UserInfo_CopyBestDisplayNameWithPlatform(UserInfoInterface, &WithPlatformOptions, &BestDisplayName);

		if (Result == EOS_EResult::EOS_Success)
		{
			assert(BestDisplayName->DisplayName);

			UserData.UserId = TargetUserId;
			if (BestDisplayName->DisplayName)
			{
				UserData.Name = FStringUtils::Widen(BestDisplayName->DisplayName);
			}

			EOS_UserInfo_BestDisplayName_Release(BestDisplayName);
		}
	}

	return UserData;
}

void EOS_CALL FUsers::QueryUserInfoCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoCallbackInfo* UserData)
{
	assert(UserData != NULL);

	if (UserData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query User Info error: %ls", FStringUtils::Widen(EOS_EResult_ToString(UserData->ResultCode)).c_str());
		return;
	}

	FUserInfoQueryPayload* UserInfoQueryData = (FUserInfoQueryPayload*)(UserData->ClientData);

	FDebugLog::Log(L"[EOS SDK] Query User Info Complete - User ID: %ls", FEpicAccountId(UserInfoQueryData->TargetUserId).ToString().c_str());

	FUserData RetrievedUserData = FGame::Get().GetUsers()->CreateUserData(UserData->LocalUserId, UserInfoQueryData->TargetUserId);
	if (RetrievedUserData.IsValid())
	{
		FPlayerManager::Get().SetDisplayName(FEpicAccountId(RetrievedUserData.UserId), RetrievedUserData.Name);

		FGameEvent Event(EGameEventType::UserInfoRetrieved, RetrievedUserData.UserId);
		FGame::Get().OnGameEvent(Event);

		// Trigger the callback if one is defined
		if (UserInfoQueryData != nullptr && UserInfoQueryData->OnUserInfoRetrievedCallback != nullptr)
		{
			UserInfoQueryData->OnUserInfoRetrievedCallback(RetrievedUserData);
		}
	}

	delete UserInfoQueryData;
}

void EOS_CALL FUsers::QueryUserInfoByDisplayNameCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* UserData)
{
	assert(UserData != NULL);

	if (UserData->ResultCode != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Query User Info error: %ls", FStringUtils::Widen(EOS_EResult_ToString(UserData->ResultCode)).c_str());
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Query User Info Complete - User ID: %ls", FEpicAccountId(UserData->TargetUserId).ToString().c_str());

	FUserData FoundUserInfo;
	FoundUserInfo.Name = FStringUtils::Widen(UserData->DisplayName);
	FoundUserInfo.UserId = UserData->TargetUserId;

	// Trigger the callback if one is defined
	FUserInfoQueryPayload* UserInfoQueryData = (FUserInfoQueryPayload*)(UserData->ClientData);
	if (UserInfoQueryData != nullptr && UserInfoQueryData->OnUserInfoRetrievedCallback != nullptr)
	{
		UserInfoQueryData->OnUserInfoRetrievedCallback(FoundUserInfo);
	}
}

void EOS_CALL FUsers::OnQueryExternalAccountMappingsCallback(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
{
	if (Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Updated external account mappings successfully.");

			FGame::Get().GetUsers()->UpdateExternalAccountMappings(Data->LocalUserId);
		}
		else if (EOS_EResult_IsOperationComplete(Data->ResultCode))
		{
			FDebugLog::LogError(L"[EOS SDK] Query external account mappings error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());

			// Cancel current query
			FGame::Get().GetUsers()->OnExternalAccountMappingsQueryFailure(Data->LocalUserId);
		}
	}
}

void EOS_CALL FUsers::OnQueryAccountMappingsCallback(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data)
{
	if (Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			FDebugLog::Log(L"[EOS SDK] Updated account mappings successfully.");

			FGame::Get().GetUsers()->UpdateQueriedAccountMappings(Data->LocalUserId);
		}
		else if (EOS_EResult_IsOperationComplete(Data->ResultCode))
		{
			FDebugLog::LogError(L"[EOS SDK] Query account mappings error: %ls.", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());

			// Cancel current query
			FGame::Get().GetUsers()->OnAccountMappingsQueryFailure(Data->LocalUserId);
		}
	}
}

void EOS_CALL FUsers::OnQueryDisplayNameFinishedCallback(const FUserData& UserData)
{
	if (UserData.IsValid())
	{
		FGame::Get().GetUsers()->SetDisplayName(UserData.UserId, UserData.Name);

		FGameEvent Event(EGameEventType::EpicAccountDisplayNameRetrieved, UserData.UserId, UserData.Name);
		FGame::Get().OnGameEvent(Event);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Container for storing info for user data retrieved from user info queries
 */
struct FUserData
{
	EOS_EpicAccountId UserId;
	std::wstring Name;

	bool IsValid() const
	{
		return EOS_EpicAccountId_IsValid(UserId) || !Name.empty();
	}
};

/**
 * Container for storing user info for user info queries
 */
struct FUserInfoQueryPayload
{
	EOS_EpicAccountId TargetUserId;
	std::function<void(FUserData&)> OnUserInfoRetrievedCallback;
};

/**
 * Manages EOS SDK user data
 */
class FUsers
{
public:
	/**
	 * Constructor
	 */
	FUsers() noexcept(false);

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FUsers(FUsers const&) = delete;
	FUsers& operator=(FUsers const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FUsers();


	virtual void Update();

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param TargetUserId - Account ID for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId TargetUserId, std::function<void(const FUserData&)> UserInfoRetrievedCallback);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param TargetUserId - Account ID for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId, std::function<void(const FUserData&)> UserInfoRetrievedCallback);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param DisplayName - display name for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName, std::function<void(const FUserData&)> UserInfoRetrievedCallback);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param TargetUserId - Account ID for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId, const EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate, std::function<void(const FUserData&)> UserInfoRetrievedCallback);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param DisplayName - display name for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName, const EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback CompletionDelegate, std::function<void(const FUserData&)> UserInfoRetrievedCallback);

	/**
	 * Starts a new async query to retrieve presence info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param TargetUserId - Account ID for the user to be queried
	 */
	void QueryPresenceInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId);

	/**
	 * Query external account mappings for accounts that were previously added. Mappings will be cached locally upon arrival.
	 *
	 * @param TargetUserId - Account ID for local user.
	 */
	void QueryExternalAccountMappings(FProductUserId CurrentUser, const std::vector<FEpicAccountId>& AccountsToQuery);

	/**
	 * Use mappings to get product user ID. Target user should be added using AddExternalAccount before calling this function.
	 *
	 * @param TargetUserId - Account ID for the target user.
	 */
	FProductUserId GetExternalAccountMapping(FEpicAccountId TargetUserId);

	/**
	 * Query account mappings for accounts that were previously added. Mappings will be cached locally upon arrival.
	 *
	 * @param TargetUserId - Account ID for local user.
	 */
	void QueryAccountMappings(FProductUserId CurrentUser, const std::vector<FProductUserId>& AccountsToQuery);

	/**
	 * Use mappings to get account ID. Target user should be added using AddExternalAccount before calling this function.
	 *
	 * @param TargetUserId - Product ID for the target user.
	 */
	FEpicAccountId GetAccountMapping(FProductUserId TargetUserId);

	/**
	 * Clear currently cached mappings.
	 */
	void ClearMappings();

	void QueryDisplayName(FEpicAccountId TargetUserId);
	std::wstring GetDisplayName(FEpicAccountId TargetUserId);
	void SetDisplayName(FEpicAccountId TargetUserId, const std::wstring& DisplayName);

	/** Gets display name for user's external account based on external account type */
	std::wstring GetExternalAccountDisplayName(FEpicAccountId LocalUserId, FEpicAccountId TargetUserId, EOS_EExternalAccountType ExternalAccountType);

	/**
	 * Creates user data for a specific user
	 *
	 * @param LocalUserId - Account ID for the local user
	 * @param TargetUserId - Account ID for the user being looked up
	 */
	FUserData CreateUserData(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId);

private:
	/**
	 * Called internally when external account mappings are ready to be retrieved from SDK.
	 */
	void UpdateExternalAccountMappings(EOS_ProductUserId LocalProductUserId);

	/**
	 * Called internally when we got an error while querying
	 */
	void OnExternalAccountMappingsQueryFailure(EOS_ProductUserId LocalProductUserId);

	/**
	 * Called internally when account mappings are ready to be retrieved from SDK.
	 */
	void UpdateQueriedAccountMappings(EOS_ProductUserId LocalProductUserId);

	/**
	 * Called internally when we got an error while querying
	 */
	void OnAccountMappingsQueryFailure(EOS_ProductUserId LocalProductUserId);

	/**
	 * Callback that is fired when user info is retrieved from a user info query
	 *
	 * @param UserData - Output parameters for the EOS_UserInfo_QueryUserInfo Function
	 */
	static void EOS_CALL QueryUserInfoCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoCallbackInfo* UserData);

	/**
	 * Callback that is fired when user info is retrieved from a user info by display name query
	 *
	 * @param UserData - Output parameters for the EOS_UserInfo_QueryUserInfoByDisplayName Function
	 */
	static void EOS_CALL QueryUserInfoByDisplayNameCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* UserData);

	/**
	 * Callback that is fired when external account mappings query is finished
	 *
	 * @param Data - Output parameters for the EOS_Connect_QueryExternalAccountMappings Function
	 */
	static void EOS_CALL OnQueryExternalAccountMappingsCallback(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data);

	/**
	 * Callback that is fired when account mappings query is finished
	 *
	 * @param Data - Output parameters for the EOS_Connect_QueryProductUserIdMappings Function
	 */
	static void EOS_CALL OnQueryAccountMappingsCallback(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data);

	/**
	 * Callback that is fired when display name query is finished
	 *
	 * @param UserData - Output for the QueryDisplayName Function 
	 */
	static void EOS_CALL OnQueryDisplayNameFinishedCallback(const FUserData& UserData);


	std::map<FEpicAccountId, FProductUserId> ExternalAccountsMap;
	std::set<FEpicAccountId> CurrentlyQueriedExternalAccounts;

	std::map<FProductUserId, FEpicAccountId> ExternalToEpicAccountsMap;
	std::set<FProductUserId> CurrentlyQueriedExternalToEpicAccounts;

	std::map<FEpicAccountId, std::wstring> DisplayNamesMap;
	std::set<FEpicAccountId> CurrentlyQueriedDisplayNames;
};
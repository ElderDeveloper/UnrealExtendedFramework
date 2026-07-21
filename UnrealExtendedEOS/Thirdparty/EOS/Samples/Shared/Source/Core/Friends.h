// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "eos_sdk.h"
#include "AccountHelpers.h"

/**
 * Forward declarations
 */
class FGameEvent;

/**
 * Data representing user presence
 */
struct FPresenceInfo
{
	/** Presence status */
	EOS_Presence_EStatus Status = EOS_Presence_EStatus::EOS_PS_Offline;

	/** Rich text */
	std::wstring RichText;

	/** Application product id */
	std::wstring Application;

	/** Platform the user is logged in on */
	std::wstring Platform;

	/** Equality operator */
	bool operator==(const FPresenceInfo& Other) const
	{
		return Status == Other.Status &&
			RichText == Other.RichText &&
			Application == Other.Application &&
			Platform == Other.Platform;
	}
};

/**
 * Container for storing info for friend data retrieved from friend queries
 */
struct FFriendData
{
	/** User Id for local user who has the friends */
	FEpicAccountId LocalUserId;

	/** User Id for user who is a friend of local user */
	FEpicAccountId UserId;

	/** Friend's product user id */
	FProductUserId UserProductUserId;

	/** Display name of friend */
	std::wstring Name;

	/** Status of friend */
	EOS_EFriendsStatus Status = EOS_EFriendsStatus::EOS_FS_NotFriends;

	/** Presence info of friend */
	FPresenceInfo Presence;

	/** Used to show friend was not found after a query */
	bool bPlaceholder = false;

	/** Default constructor */
	FFriendData() = default;

	/** Constructor */
	FFriendData(const std::wstring& Label) : Name(Label), bPlaceholder(true)
	{}

	/**
	 * True if data for this friend is valid
	 */
	bool IsValid() const
	{
		return bPlaceholder || UserId || !Name.empty();
	}
};

/**
 * Actions that can be done with friends
 */
enum class FriendAction
{
	/** None */
	None,

	/** Invite */
	Invite,

	/** Add */
	AddToList,

	/** Last element */
	Last
};

/**
 * Manages friend data for local user
 */
class FFriends
{
public:
	/**
	 * Constructor
	 */
	FFriends() noexcept(false);

	/**
	 * No copying or copy assignment allowed for this class.
	 */
	FFriends(FFriends const&) = delete;
	FFriends& operator=(FFriends const&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FFriends();

	/**
	 * Update query for updating friend info
	 */
	void Update();

	/**
	 * Add a friend
	 *
	 * @param FriendUserId - Account ID for the user to be added as a friend
	 */
	void AddFriend(FEpicAccountId FriendUserId);

	/**
	 * Add a friend
	 *
	 * @param LocalUserId - Account ID for the user who is adding a friend
	 * @param FriendUserId - Account ID for the user to be added as a friend
	 */
	void AddFriend(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId);

	/**
	 * Accept invite from a new friend request
	 *
	 * @param FriendUserId - Account ID for the user to be added as a friend
	 */
	void AcceptInvite(FEpicAccountId FriendUserId);

	/**
	 * Accept invite from a new friend request
	 *
	 * @param LocalUserId - Account ID for the user who is accepting the invite
	 * @param FriendUserId - Account ID for the user to be added as a friend
	 */
	void AcceptInvite(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId);

	/**
	 * Reject invite from a new friend request
	 *
	 * @param FriendUserId - Account ID for the user to be rejected as a friend
	 */
	void RejectInvite(FEpicAccountId FriendUserId);

	/**
	 * Reject invite from a new friend request
	 *
	 * @param LocalUserId - Account ID for the user who is rejecting the invite
	 * @param FriendUserId - Account ID for the user to be rejected as a friend
	 */
	void RejectInvite(EOS_EpicAccountId LocalUserId, FEpicAccountId FriendUserId);

	/**
	 * Initiates a new async friends query for the local user to retrieve list of data about their friends
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 */
	void QueryFriends(EOS_EpicAccountId LocalUserId);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param TargetUserId - Account ID for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId);

	/**
	 * Starts a new async query to retrieve info about current user
	 *
	 * @param DisplayName - display name for the user to be queried
	 */
	void QueryUserInfo(const std::wstring& DisplayName);

	/**
	 * Starts a new async query to retrieve info about a user
	 *
	 * @param LocalUserId - Account ID for the user who is performing the query
	 * @param DisplayName - display name for the user to be queried
	 */
	void QueryUserInfo(EOS_EpicAccountId LocalUserId, const std::wstring& DisplayName);

	/**
	 * Automatically perform action once friend search query finishes.
	 *
	 */
	void QueueFoundFriendAction(FriendAction action);

	/**
	 * Update presence info for the friend list.
	 */
	void QueryFriendsPresenceInfo(EOS_EpicAccountId LocalUserId);

	/**
	 * Update friends' connect mappings
	 */
	void QueryFriendsConnectMappings(EOS_EpicAccountId LocalUserId);

	/**
	 * Stores friend data after a successful query of all friends
	 *
	 * @param InFriends - List of data for each friend to store
	 */
	void SetFriends(std::vector<FFriendData>&& InFriends);

	/**
	 * Stores friend data after a successful query of all friends
	 *
	 * @param LocalUserId - Account ID for the user who is setting friends
	 * @param InFriends - List of data for each friend to store
	 */
	void SetFriends(EOS_EpicAccountId LocalUserId, std::vector<FFriendData>&& InFriends);

	void SubscribeToFriendStatusUpdates(void* Owner, std::function<void(const std::vector<FFriendData>&)> Callback);
	void UnsubscribeToFriendStatusUpdates(void* Owner);

	/**
	 * Retrieves a list of info about friends for local user
	 *
	 * @return list of friend data
	 */
	const std::vector<FFriendData>& GetFriends() const { return Friends; }

	/**
	 * Updates info for user found after a user info query
	 *
	 * @param Data - Friend data to be updated
	 */
	void OnUserInfoRetrieved(const FFriendData& FriendData);

	/**
	 * Sets results of user search
	 *
	 * @param Data - Found user data
	 */
	void OnUserFound(const FFriendData& FriendData);

	/**
	 * Updates info for user after a user presence query
	 *
	 * @param UserId - Found user id
	 * @param UserId - Found user info
	 */
	void OnUserPresenceInfoRetrieved(FEpicAccountId UserId, FPresenceInfo& Info);

	/**
	 * Retrieves a string representing the status of a friend's presence
	 *
	 * @param PresenceStatus - Presence status
	 *
	 * @return A string representing the status
	 */
	static std::wstring FriendPresenceToString(EOS_Presence_EStatus PresenceStatus);

	/**
	 * Sets the current user id to be updated
	 *
	 * @param UserId - Id for the user to be updated
	 */
	void SetCurrentUser(FEpicAccountId UserId);

	/**
	 * Accessor for current user id
	 *
	 * @return User id for current user
	 */
	FEpicAccountId GetCurrentUser() { return CurrentUserId; };

	/**
	 * Receives game event
	 *
	 * @param Event - Game event to act on
	 */
	void OnGameEvent(const FGameEvent& Event);

	/**
	 * Returns true if friends have been modified
	 *
	 * @return true if friends have been modified
	 */
	uint64_t GetDirtyCounter() { return DirtyCounter; };

	/**
	 * Increments the dirty counter
	 */
	void SetDirty() { DirtyCounter++; };

	/**
	 * Retrieves the display name of a friend. Can return empty string if friend's display name is not retrieved yet.
	 */
	std::wstring GetFriendName(FEpicAccountId FriendId) const;

	/**
	 * Retrieves the display name of a friend. Can return empty string if friend's display name is not retrieved yet.
	 */
	std::wstring GetFriendName(FProductUserId FriendId) const;

private:
	/**
	 * Called when a user has logged in
	 */
	void OnLoggedIn(FEpicAccountId UserId);

	/**
	 * Called when user has connect logged in
	 */
	void OnConnectLoggedIn(FProductUserId ProductUserId);

	/**
	 * Called when a user has logged out
	 */
	void OnLoggedOut(FEpicAccountId UserId);

	/**
	 * Subscribes specified user to friend notification about friend-list changes.
	 */
	void SubscribeToFriendUpdates(FEpicAccountId UserId);

	/**
	 * Unsubscribes specified user from friend notification about friend-list changes.
	 */
	void UnsubscribeFromFriendUpdates(FEpicAccountId UserId);

	/**
	 * Called to change friend's status.
	 */
	void FriendStatusChanged(FEpicAccountId LocalUserId, FEpicAccountId TargetUserId, EOS_EFriendsStatus NewStatus);

	/**
	 * Retrieves a string representing the status of a friendship
	 *
	 * @param status - Friendship status
	 *
	 * @return A string representing the status
	 */
	static std::wstring FriendStatusToString(EOS_EFriendsStatus status);

	/**
	 * Creates friend data for a specific friend
	 *
	 * @param LocalUserId - Account ID for the local user
	 * @param TargetUserId - Account ID for the friend being looked up
	 */
	void CreateFriendData(EOS_EpicAccountId LocalUserId, EOS_EpicAccountId TargetUserId);

	/**
	 * Callback that is fired when the query friends async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_Friends_QueryFriends Function
	 */
	static void EOS_CALL QueryFriendsCompleteCallbackFn(const EOS_Friends_QueryFriendsCallbackInfo* FriendData);

	/**
	 * Callback that is fired when the query user info async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_UserInfo_QueryUserInfo Function
	 */
	static void EOS_CALL QueryUserInfoCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoCallbackInfo* FriendData);

	/**
	 * Callback that is fired when the query user info by display name async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_UserInfo_QueryUserInfoByDisplayName Function
	 */
	static void EOS_CALL QueryUserInfoByDisplayNameCompleteCallbackFn(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* FriendData);

	/**
	 * Callback that is fired when the send invite async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_Friends_SendInvite Function
	 */
	static void EOS_CALL SendFriendInviteCompleteCallbackFn(const EOS_Friends_SendInviteCallbackInfo* FriendData);

	/**
	 * Callback that is fired when the accept invite async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_Friends_AcceptInvite Function
	 */
	static void EOS_CALL AcceptFriendInviteCompleteCallbackFn(const EOS_Friends_AcceptInviteCallbackInfo* FriendData);

	/**
	 * Callback that is fired when the reject invite async operation completes, either successfully or in error
	 *
	 * @param FriendData - Output parameters for the EOS_Friends_RejectInvite Function
	 */
	static void EOS_CALL RejectFriendInviteCompleteCallbackFn(const EOS_Friends_RejectInviteCallbackInfo* FriendData);

	/**
	 * Callback that is fired when friend status is changed.
	 *
	 * @param Data - Output parameters for the EOS_Friends_AddNotifyFriendsUpdate Function
	 */
	static void EOS_CALL FriendUpdateCallback(const EOS_Friends_OnFriendsUpdateInfo* Data);

	/**
	 * Callback that is fired when presence is updated.
	 *
	 * @param Data - Output parameters for the EOS_Presence_AddNotifyOnPresenceChanged Function
	 */
	static void EOS_CALL PresenceUpdateCallback(const EOS_Presence_PresenceChangedCallbackInfo* Data);

	/** List of data about local user's friends */
	std::vector<FFriendData> Friends;

	/** Map that contains friend notification ids for all local users. */
	std::map<FEpicAccountId, EOS_NotificationId> FriendNotifications;

	/** Map that contains presence notification ids for all local users. */
	std::map<FEpicAccountId, EOS_NotificationId> PresenceNotifications;

	/** Last found friend */
	FFriendData LastFoundFriend;

	/** Friend action to be performed once search is finished */
	FriendAction FoundFriendAction;

	/** Id for current user. */
	FEpicAccountId CurrentUserId;

	/** Current product user id. */
	FProductUserId CurrentProductUserId;

	/* Callbacks to run whenever Friend Status is updated */
	std::map<void*, std::function<void(const std::vector<FFriendData>& Friends)>> FriendStatusUpdateCallbacks;

	/** Timestamp that stores info when the last mapping query was. */
	double LastMappingQueryTimestamp = 0.0;

	/** Is the very first friend list query done for current user? */
	bool bInitialFriendQueryFinished = false;

	/** Increments whenever friends have been updated */
	uint64_t DirtyCounter = 0;
};
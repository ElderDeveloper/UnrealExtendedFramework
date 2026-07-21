// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AccountHelpers.h"
#include "Friends.h"

/**
* Game Event Types
*/
enum class EGameEventType: unsigned char
{
	/** No Game Event */
	None,

	/** Checks for auto login process via command line param */
	CheckAutoLogin,

	/** Start user log in */
	StartUserLogin,

	/** User has logged in */
	UserLoggedIn,

	/** User login failed */
	UserLoginFailed,

	/** User login requires MFA */
	UserLoginRequiresMFA,

	/** User has entered MFA */
	UserLoginEnteredMFA,

	/** Continue login, continues external auth login (which uses stored continuance token) */
	ContinueLogin,

	/** Continuance token, used to store continuance token for linking external account at later time */
	ContinuanceToken,

	/** User wants to logout */
	UserLogOutTriggered,

	/** Start user log out */
	StartUserLogout,

	/** User has logged out */
	UserLoggedOut,

	/** User info has been retrieved */
	UserInfoRetrieved,

	/** Epic account mappings have been retrieved */
	EpicAccountsMappingRetrieved,

	/** Epic account mappings have already been retrieved so no changes */
	EpicAccountsMappingNoChange,

	/** External account mappings have been retrieved */
	ExternalAccountsMappingRetrieved,

	/** Epic account display name has been retrieved */
	EpicAccountDisplayNameRetrieved,

	/** User connect logged in */
	UserConnectLoggedIn,

	/** User connect token verified */
	UserConnectIdTokenVerified,

	/** User connect token verification failed */
	UserConnectIdTokenVerificationFailed,

	/** User connect auth expiring soon */
	UserConnectAuthExpiration,

	/** Beginning a player session */
	PlayerSessionBegin,

	/** Ending a player session */
	PlayerSessionEnd,

	/** New User Login */
	NewUserLogin,

	/** Previous User */
	ShowPrevUser,

	/** Next User */
	ShowNextUser,

	/** No User Logged In */
	NoUserLoggedIn,

	/** Cancel Login */
	CancelLogin,

	/** Toggle FPS */
	ToggleFPS,

	/** Exiting game */
	ExitGame,

	/** Add notification */
	AddNotification,

	/** Toggle notification */
	ToggleNotification,

	/** Invite to session pressed */
	InviteFriendToSession,

	/** Invite to session received */
	InviteToSessionReceived,

	/** Invite to session was accepted in the overlay */
	OverlayInviteToSessionAccepted,

	/** Invite to session was rejected in the overlay */
	OverlayInviteToSessionRejected,

	/** Request to Join session pressed */
	RequestToJoinFriendSession,

	/** Request to Join session received */
	RequestToJoinSessionReceived,

	/** User wants to create new session */
	NewSession,

	/** User created (or canceled creation of) new session */
	SessionCreationFinished,

	/** Register friend with the session */
	RegisterFriendWithSession,

	/** Unregister friend with the session */
	UnregisterFriendWithSession,

	/** User joined session */
	SessionJoined,

	/** Start chat with your friend */
	StartChatWithFriend,

	/** File transfer started */
	FileTransferStarted,

	/** File transfer finished */
	FileTransferFinished,

	/** Refresh Offers */
	RefreshOffers,

	/** Definitions received */
	DefinitionsReceived,

	/** Player Achievements received */
	PlayerAchievementsReceived,

	/** Achievements unlocked */
	AchievementsUnlocked,

	/** Stats ingest completed */
	StatsIngested,

	/** Stats query completed */
	StatsQueried,

	/** New file creation triggered */
	NewFileCreationStarted,

	/** Leaderboard records received */
	LeaderboardRecordsReceived,

	/** Leaderboard user scores received */
	LeaderboardUserScoresReceived,

	/** Lobby invite received */
	LobbyInviteReceived,
	
	/** Invite to lobby was accepted in the overlay */
	OverlayInviteToLobbyAccepted,

	/** User wants to create new lobby */
	NewLobby,

	/**  User created (or canceled creation of) new lobby */
	LobbyCreationFinished,

	/** Invite to lobby pressed */
	InviteFriendToLobby,

	/** Invite to lobby received */
	InviteToLobbyReceived,

	/** Lobby joined */
	LobbyJoined,

	/** User wants to modify current lobby parameters */
	ModifyLobby,

	/** Lobby modification finished */
	LobbyModificationFinished,

	/** Event to notify when a lobby string data is received. */
	LobbyStringDataReceived,

	/** Sets the locale used by the EOS SDK */
	SetLocale,

	/** Deletes any locally stored persistent auth credentials */
	DeletePersistentAuth,

	/** Show popup dialog */
	ShowPopupDialog,

	/** User wants to install mod*/
	InstallMod,

	/** User wants to update mod*/
	UpdateMod,

	/** User wants to uninstall mod*/
	UninstallMod,

	/** Test Set Presence */
	TestSetPresence,

	/** Prints Auth Token Info to console */
	PrintAuth,

	/** Join room button has been pressed */
	JoinRoom,

	/** Leave room button has been pressed */
	LeaveRoom,

	/** A participant has joined the current room */
	RoomJoined,

	/** A participant has left the current room */
	RoomLeft,

	/** Attempt to kick a participant from the current room (server only) */
	Kick,

	/** Attempt to kick a participant from the current room (server only) */
	RemoteMute,

	/** Audio Input devices have been queried and available devices have been updated */
	AudioInputDevicesUpdated,

	/** Audio output devices have been queried and available devices have been updated */
	AudioOutputDevicesUpdated,

	/** Voice Room Data Updated */
	RoomDataUpdated,

	/** Custom Invite received */
	CustomInviteReceived,

	/** Custom Invite accepted */
	CustomInviteAccepted,

	/** Custom Invite declined */
	CustomInviteDeclined,

	/** Change room's receiving volume */
	UpdateReceivingVolume,

	/** Change participant's room receiving volume */
	UpdateParticipantVolume,

	/** Client has been kicked because of an anti-cheat issue */
	AntiCheatKicked,

	Total
};

/**
* Game Event
*/
class FGameEvent
{
public:
	/** Default constructor */
	FGameEvent() = default;

	/** Constructor with just type */
	explicit FGameEvent(EGameEventType InType):
		Type(InType)
	{}

	/** Constructor with type and user id */
	explicit FGameEvent(EGameEventType InType, FEpicAccountId InUserId) :
		Type(InType), UserId(InUserId)
	{}

	/** Constructor with type and product user id */
	explicit FGameEvent(EGameEventType InType, FProductUserId InProductUserId) :
		Type(InType), ProductUserId(InProductUserId)
	{}

	/** Constructor with type, user id product user id */
	explicit FGameEvent(EGameEventType InType, FEpicAccountId InUserId, FProductUserId InProductUserId) :
		Type(InType), UserId(InUserId), ProductUserId(InProductUserId)
	{}

	/** Constructor with type, user id and one string */
	explicit FGameEvent(EGameEventType InType, FEpicAccountId InUserId, std::wstring InFirstStr) :
		Type(InType), UserId(InUserId), FirstStr(InFirstStr)
	{}

	/** Constructor with type, product user id and one string */
	explicit FGameEvent(EGameEventType InType, FProductUserId InProductUserId, std::wstring InFirstStr) :
		Type(InType), ProductUserId(InProductUserId), FirstStr(InFirstStr)
	{}

	/** Constructor with type and one string */
	explicit FGameEvent(EGameEventType InType, std::wstring InFirstStr) :
		Type(InType), FirstStr(InFirstStr)
	{}

	/** Constructor with type and two strings */
	explicit FGameEvent(EGameEventType InType, std::wstring InFirstStr, std::wstring InSecondStr) :
		Type(InType), FirstStr(InFirstStr), SecondStr(InSecondStr)
	{}

	/** Constructor with type and extended type */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType) :
		Type(InType), FirstExtendedType(InFirstExtendedType)
	{}

	/** Constructor with type, extended type and one string */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType, std::wstring InFirstStr) :
		Type(InType), FirstExtendedType(InFirstExtendedType), FirstStr(InFirstStr)
	{}

	/** Constructor with type, extended type and two strings */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType, std::wstring InFirstStr, std::wstring InSecondStr) :
		Type(InType), FirstExtendedType(InFirstExtendedType), FirstStr(InFirstStr), SecondStr(InSecondStr)
	{}

	/** Constructor with type, extended type, one string and user id */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType, std::wstring InFirstStr, FEpicAccountId InUserId) :
		Type(InType), FirstExtendedType(InFirstExtendedType), FirstStr(InFirstStr), UserId(InUserId)
	{}

	/** Constructor with type, extended type, one string and product user id */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType, std::wstring InFirstStr, FProductUserId InProductUserId) :
		Type(InType), FirstExtendedType(InFirstExtendedType), FirstStr(InFirstStr), ProductUserId(InProductUserId)
	{}

	/** Constructor with type, one string and two extended types */
	explicit FGameEvent(EGameEventType InType, std::wstring InFirstStr, int InFirstExtendedType, int InSecondExtendedType) :
		Type(InType), FirstStr(InFirstStr), FirstExtendedType(InFirstExtendedType), SecondExtendedType(InSecondExtendedType)
	{}

	/** Constructor with type and a continuance token */
	explicit FGameEvent(EGameEventType InType, EOS_ContinuanceToken InContinuanceToken) :
		Type(InType), ContinuanceToken(InContinuanceToken)
	{}

	/** Constructor with type, user id, product user id and one string */
	explicit FGameEvent(EGameEventType InType, FEpicAccountId InUserId, FProductUserId InProductUserId, std::wstring InFirstStr) :
		Type(InType), UserId(InUserId), ProductUserId(InProductUserId), FirstStr(InFirstStr)
	{}

	/** Constructor with type, extended type and product user id */
	explicit FGameEvent(EGameEventType InType, int InFirstExtendedType, FProductUserId InProductUserId) :
		Type(InType), FirstExtendedType(InFirstExtendedType), ProductUserId(InProductUserId)
	{}

	/**
	* Accessor for type
	*/
	EGameEventType GetType() const { return Type; }

	/**
	* Accessor for first extended type
	*/
	int GetFirstExtendedType() const { return FirstExtendedType; }

	/**
	* Accessor for second extended type
	*/
	int GetSecondExtendedType() const { return SecondExtendedType; }

	/**
	* Accessor for first string
	*/
	std::wstring GetFirstStr() const { return FirstStr; }

	/**
	* Accessor for second string
	*/
	std::wstring GetSecondStr() const { return SecondStr; }

	/**
	* Accessor for user id
	*/
	FEpicAccountId GetUserId() const { return UserId; }

	/**
	* Accessor for product user id
	*/
	FProductUserId GetProductUserId() const { return ProductUserId; }

	/**
	* Accessor for continuance token
	*/
	EOS_ContinuanceToken GetContinuanceToken() const { return ContinuanceToken; }

private:
	/** Game event type */
	EGameEventType Type = EGameEventType::None;

	/** First extended type */
	int FirstExtendedType = 0;

	/** Second extended type */
	int SecondExtendedType = 0;
	
	/** First optional string */
	std::wstring FirstStr;

	/** Second optional string */
	std::wstring SecondStr;

	/** User Id */
	FEpicAccountId UserId;

	/** EOS Product User Id */
	FProductUserId ProductUserId;

	/** EOS Continuance Token */
	EOS_ContinuanceToken ContinuanceToken;
};

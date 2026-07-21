// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * Friends interface backed by the Steam client's roster (ISteamFriends).
 *
 * Steam keeps the friends list hot on the client, so ReadFriendsList completes synchronously:
 * it snapshots k_EFriendFlagImmediate into a cache and fires the completion delegate before
 * returning. Only the default list (EFriendsLists::Default) exists on Steam; requests for any
 * other list name fail with a warning.
 *
 * Friendship management (invites, accepts, rejects, removals, blocking) is NOT exposed to games
 * by Steamworks — it happens in the Steam UI. Those methods fail honestly through their
 * delegates; steer players to the overlay instead (IOnlineExternalUI::ShowFriendsUI, which is
 * ActivateGameOverlay("Friends"), or ActivateGameOverlayToUser("friendadd"/"friendremove", ...)).
 *
 * Blocked players come from the k_EFriendFlagBlocked enumeration; recent players come from
 * Steam's coplay list (GetCoplayFriendCount/GetCoplayFriend, last seen = GetFriendCoplayTime).
 * PersonaStateChange_t drives the OnFriendsChange broadcast.
 */
class FOnlineFriendsExtendedSteam : public IOnlineFriends
{
public:
	explicit FOnlineFriendsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineFriendsExtendedSteam();

	//~ Begin IOnlineFriends
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual void SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate = FOnSetFriendAliasComplete()) override;
	virtual void DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate = FOnDeleteFriendAliasComplete()) override;
	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers) override;
	virtual void DumpRecentPlayers() const override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray<TSharedRef<FOnlineBlockedPlayer>>& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;
	//~ End IOnlineFriends

	/** Called by the cpp-local Steam callback holder when a PersonaStateChange_t arrives. */
	void HandlePersonaStateChange(uint64 SteamId64, int32 ChangeFlags);

private:
	/** True when the given list name refers to the only list Steam has (default, or empty). */
	static bool IsDefaultFriendsList(const FString& ListName);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Steam callback holder (defined in the cpp, only constructed while the Steam client is up). */
	TSharedPtr<class FFriendsExtendedSteamCallbacks> Callbacks;

	/** Snapshot of the default friends list, filled by ReadFriendsList. */
	TArray<TSharedRef<FOnlineFriend>> CachedFriends;

	/** False until the first successful ReadFriendsList — GetFriendsList reports failure before that. */
	bool bFriendsListRead = false;

	/** Snapshot of blocked users (k_EFriendFlagBlocked), filled by QueryBlockedPlayers. */
	TArray<TSharedRef<FOnlineBlockedPlayer>> CachedBlockedPlayers;
	bool bBlockedPlayersQueried = false;

	/** Snapshot of Steam's coplay list, filled by QueryRecentPlayers. */
	TArray<TSharedRef<FOnlineRecentPlayer>> CachedRecentPlayers;
	bool bRecentPlayersQueried = false;
};

typedef TSharedPtr<FOnlineFriendsExtendedSteam, ESPMode::ThreadSafe> FOnlineFriendsExtendedSteamPtr;

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Friends/OnlineFriendsExtendedSteam.h"
#include "Presence/OnlinePresenceExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineError.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamFriends
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

	/**
	 * Friendship management is not exposed to games by Steamworks: invites, accepts, rejects and
	 * removals all happen in the Steam UI. The in-game path is the overlay —
	 * IOnlineExternalUI::ShowFriendsUI (ActivateGameOverlay("Friends")) or
	 * ActivateGameOverlayToUser("friendadd"/"friendremove"/"friendrequestaccept"/"friendrequestignore", user).
	 */
	static const TCHAR* FriendManagementUnsupportedError()
	{
		return TEXT("Steamworks exposes no in-game friendship management; use the Steam overlay (IOnlineExternalUI::ShowFriendsUI)");
	}
}

/** Friend entry snapshotted from the Steam roster: id, persona name, always-accepted invite status, presence snapshot. */
class FOnlineFriendExtendedSteam : public FOnlineFriend
{
public:
	FOnlineFriendExtendedSteam(uint64 InSteamId64, const FString& InDisplayName, const FOnlineUserPresence& InPresence)
		: UserId(FUniqueNetIdExtendedSteam::Create(InSteamId64))
		, DisplayName(InDisplayName)
		, Presence(InPresence)
	{
	}

	//~ Begin FOnlineUser
	virtual FUniqueNetIdRef GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		// Steam does not expose real names through the client API.
		return FString();
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return DisplayName;
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}
	//~ End FOnlineUser

	//~ Begin FOnlineFriend
	virtual EInviteStatus::Type GetInviteStatus() const override
	{
		// Steam only enumerates established friendships through k_EFriendFlagImmediate.
		return EInviteStatus::Accepted;
	}

	virtual const FOnlineUserPresence& GetPresence() const override
	{
		return Presence;
	}
	//~ End FOnlineFriend

private:
	FUniqueNetIdRef UserId;
	FString DisplayName;
	FOnlineUserPresence Presence;
};

/** Blocked user entry snapshotted from the k_EFriendFlagBlocked enumeration. */
class FOnlineBlockedPlayerExtendedSteam : public FOnlineBlockedPlayer
{
public:
	FOnlineBlockedPlayerExtendedSteam(uint64 InSteamId64, const FString& InDisplayName)
		: UserId(FUniqueNetIdExtendedSteam::Create(InSteamId64))
		, DisplayName(InDisplayName)
	{
	}

	virtual FUniqueNetIdRef GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		return FString();
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return DisplayName;
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}

private:
	FUniqueNetIdRef UserId;
	FString DisplayName;
};

/** Recent player entry from Steam's coplay list; last seen is GetFriendCoplayTime (unix time). */
class FOnlineRecentPlayerExtendedSteam : public FOnlineRecentPlayer
{
public:
	FOnlineRecentPlayerExtendedSteam(uint64 InSteamId64, const FString& InDisplayName, const FDateTime& InLastSeen)
		: UserId(FUniqueNetIdExtendedSteam::Create(InSteamId64))
		, DisplayName(InDisplayName)
		, LastSeen(InLastSeen)
	{
	}

	virtual FUniqueNetIdRef GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		return FString();
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return DisplayName;
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}

	virtual FDateTime GetLastSeen() const override
	{
		return LastSeen;
	}

private:
	FUniqueNetIdRef UserId;
	FString DisplayName;
	FDateTime LastSeen;
};

#if WITH_EXTENDEDSTEAM_SDK

/** Steam callback holder — kept out of the header so SDK types stay private to this cpp. */
class FFriendsExtendedSteamCallbacks
{
public:
	explicit FFriendsExtendedSteamCallbacks(FOnlineFriendsExtendedSteam& InOwner)
		: Owner(InOwner)
		, PersonaStateChangeCallback(this, &FFriendsExtendedSteamCallbacks::OnPersonaStateChange)
	{
	}

private:
	void OnPersonaStateChange(PersonaStateChange_t* Data)
	{
		if (Data != nullptr)
		{
			Owner.HandlePersonaStateChange(Data->m_ulSteamID, Data->m_nChangeFlags);
		}
	}

	FOnlineFriendsExtendedSteam& Owner;
	CCallback<FFriendsExtendedSteamCallbacks, PersonaStateChange_t> PersonaStateChangeCallback;
};

#else

class FFriendsExtendedSteamCallbacks
{
};

#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineFriendsExtendedSteam::FOnlineFriendsExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamFriends::IsSteamClientUp())
	{
		Callbacks = MakeShared<FFriendsExtendedSteamCallbacks>(*this);
	}
#endif
}

FOnlineFriendsExtendedSteam::~FOnlineFriendsExtendedSteam() = default;

bool FOnlineFriendsExtendedSteam::IsDefaultFriendsList(const FString& ListName)
{
	return ListName.IsEmpty() || ListName.Equals(EFriendsLists::ToString(EFriendsLists::Default), ESearchCase::IgnoreCase);
}

bool FOnlineFriendsExtendedSteam::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate)
{
	if (LocalUserNum != 0)
	{
		const FString Error = FString::Printf(TEXT("Steam supports a single local user; invalid LocalUserNum %d"), LocalUserNum);
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadFriendsList: %s"), *Error);
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, Error);
		return false;
	}

	if (!IsDefaultFriendsList(ListName))
	{
		const FString Error = FString::Printf(TEXT("Steam has a single friends list; unsupported list name '%s'"), *ListName);
		UE_LOG(LogExtendedSteam, Warning, TEXT("ReadFriendsList: %s"), *Error);
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, Error);
		return false;
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamFriends::IsSteamClientUp())
	{
		// Synchronous by design: the Steam client keeps the roster hot, so there is nothing to
		// wait for — snapshot it and fire the completion delegate before returning.
		CachedFriends.Reset();

		const int32 FriendCount = FMath::Max(0, SteamFriends()->GetFriendCount(k_EFriendFlagImmediate));
		CachedFriends.Reserve(FriendCount);
		for (int32 FriendIndex = 0; FriendIndex < FriendCount; ++FriendIndex)
		{
			const CSteamID FriendSteamId = SteamFriends()->GetFriendByIndex(FriendIndex, k_EFriendFlagImmediate);
			if (!FriendSteamId.IsValid())
			{
				continue;
			}

			const uint64 FriendId64 = FriendSteamId.ConvertToUint64();
			CachedFriends.Add(MakeShared<FOnlineFriendExtendedSteam>(
				FriendId64,
				FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(FriendSteamId))),
				FOnlinePresenceExtendedSteam::BuildPresenceSnapshot(FriendId64)));
		}

		bFriendsListRead = true;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("ReadFriendsList: cached %d friends"), CachedFriends.Num());
		Delegate.ExecuteIfBound(LocalUserNum, true, ListName, FString());
		return true;
	}
#endif

	const FString Error = TEXT("Steam client is not initialized");
	UE_LOG(LogExtendedSteam, Warning, TEXT("ReadFriendsList: %s"), *Error);
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate)
{
	const FString Error = TEXT("The Steam friends list cannot be deleted by the game");
	UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteFriendsList: %s"), *Error);
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate)
{
	const FString Error = ExtendedSteamFriends::FriendManagementUnsupportedError();
	UE_LOG(LogExtendedSteam, Warning, TEXT("SendInvite: %s"), *Error);
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate)
{
	const FString Error = ExtendedSteamFriends::FriendManagementUnsupportedError();
	UE_LOG(LogExtendedSteam, Warning, TEXT("AcceptInvite: %s"), *Error);
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	const FString Error = ExtendedSteamFriends::FriendManagementUnsupportedError();
	UE_LOG(LogExtendedSteam, Warning, TEXT("RejectInvite: %s"), *Error);
	TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, Error);
	return false;
}

void FOnlineFriendsExtendedSteam::SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate)
{
	// Steam nicknames are managed by the Steam client UI, not the game.
	UE_LOG(LogExtendedSteam, Warning, TEXT("SetFriendAlias: not supported by Steamworks"));
	Delegate.ExecuteIfBound(LocalUserNum, FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
}

void FOnlineFriendsExtendedSteam::DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate)
{
	UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteFriendAlias: not supported by Steamworks"));
	Delegate.ExecuteIfBound(LocalUserNum, FriendId, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
}

bool FOnlineFriendsExtendedSteam::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	const FString Error = ExtendedSteamFriends::FriendManagementUnsupportedError();
	UE_LOG(LogExtendedSteam, Warning, TEXT("DeleteFriend: %s"), *Error);
	TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, FriendId, ListName, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray<TSharedRef<FOnlineFriend>>& OutFriends)
{
	OutFriends.Reset();

	if (LocalUserNum != 0 || !IsDefaultFriendsList(ListName))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetFriendsList: only LocalUserNum 0 and the default list exist on Steam (requested user %d, list '%s')"),
			LocalUserNum, *ListName);
		return false;
	}

	if (!bFriendsListRead)
	{
		// Contract: the cache is only valid after a successful ReadFriendsList.
		return false;
	}

	OutFriends = CachedFriends;
	return true;
}

TSharedPtr<FOnlineFriend> FOnlineFriendsExtendedSteam::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (LocalUserNum != 0 || !IsDefaultFriendsList(ListName) || !bFriendsListRead)
	{
		return nullptr;
	}

	for (const TSharedRef<FOnlineFriend>& Friend : CachedFriends)
	{
		if (*Friend->GetUserId() == FriendId)
		{
			return Friend;
		}
	}
	return nullptr;
}

bool FOnlineFriendsExtendedSteam::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	return GetFriend(LocalUserNum, FriendId, ListName).IsValid();
}

bool FOnlineFriendsExtendedSteam::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamFriends::IsSteamClientUp())
	{
		// Steam's coplay list is the closest thing to recent players; it is client-local and hot,
		// so this completes synchronously. Namespaces do not exist on Steam — the same list is
		// returned whatever the namespace.
		CachedRecentPlayers.Reset();

		const int32 CoplayCount = FMath::Max(0, SteamFriends()->GetCoplayFriendCount());
		CachedRecentPlayers.Reserve(CoplayCount);
		for (int32 CoplayIndex = 0; CoplayIndex < CoplayCount; ++CoplayIndex)
		{
			const CSteamID CoplaySteamId = SteamFriends()->GetCoplayFriend(CoplayIndex);
			if (!CoplaySteamId.IsValid())
			{
				continue;
			}

			// GetFriendCoplayTime returns a unix time.
			const int32 CoplayUnixTime = SteamFriends()->GetFriendCoplayTime(CoplaySteamId);
			CachedRecentPlayers.Add(MakeShared<FOnlineRecentPlayerExtendedSteam>(
				CoplaySteamId.ConvertToUint64(),
				FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(CoplaySteamId))),
				CoplayUnixTime > 0 ? FDateTime::FromUnixTimestamp(CoplayUnixTime) : FDateTime::MinValue()));
		}

		bRecentPlayersQueried = true;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("QueryRecentPlayers: cached %d coplay entries"), CachedRecentPlayers.Num());
		TriggerOnQueryRecentPlayersCompleteDelegates(UserId, Namespace, true, FString());
		return true;
	}
#endif

	const FString Error = TEXT("Steam client is not initialized");
	UE_LOG(LogExtendedSteam, Warning, TEXT("QueryRecentPlayers: %s"), *Error);
	TriggerOnQueryRecentPlayersCompleteDelegates(UserId, Namespace, false, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineRecentPlayer>>& OutRecentPlayers)
{
	OutRecentPlayers.Reset();
	if (!bRecentPlayersQueried)
	{
		return false;
	}

	OutRecentPlayers = CachedRecentPlayers;
	return true;
}

void FOnlineFriendsExtendedSteam::DumpRecentPlayers() const
{
	UE_LOG(LogExtendedSteam, Log, TEXT("Recent players (Steam coplay list, queried: %s): %d entries"),
		bRecentPlayersQueried ? TEXT("yes") : TEXT("no"), CachedRecentPlayers.Num());
	for (const TSharedRef<FOnlineRecentPlayer>& RecentPlayer : CachedRecentPlayers)
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("  %s (%s), last seen %s"),
			*RecentPlayer->GetDisplayName(), *RecentPlayer->GetUserId()->ToDebugString(), *RecentPlayer->GetLastSeen().ToString());
	}
}

bool FOnlineFriendsExtendedSteam::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	// Blocking is a Steam-UI action; the client API only exposes the resulting block list.
	const FString Error = TEXT("Steamworks exposes no in-game block API; blocking happens in the Steam UI");
	UE_LOG(LogExtendedSteam, Warning, TEXT("BlockPlayer: %s"), *Error);
	TriggerOnBlockedPlayerCompleteDelegates(LocalUserNum, false, PlayerId, EFriendsLists::ToString(EFriendsLists::Default), Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	const FString Error = TEXT("Steamworks exposes no in-game unblock API; unblocking happens in the Steam UI");
	UE_LOG(LogExtendedSteam, Warning, TEXT("UnblockPlayer: %s"), *Error);
	TriggerOnUnblockedPlayerCompleteDelegates(LocalUserNum, false, PlayerId, EFriendsLists::ToString(EFriendsLists::Default), Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamFriends::IsSteamClientUp())
	{
		// The block list is client-local (k_EFriendFlagBlocked); snapshot synchronously.
		CachedBlockedPlayers.Reset();

		const int32 BlockedCount = FMath::Max(0, SteamFriends()->GetFriendCount(k_EFriendFlagBlocked));
		CachedBlockedPlayers.Reserve(BlockedCount);
		for (int32 BlockedIndex = 0; BlockedIndex < BlockedCount; ++BlockedIndex)
		{
			const CSteamID BlockedSteamId = SteamFriends()->GetFriendByIndex(BlockedIndex, k_EFriendFlagBlocked);
			if (!BlockedSteamId.IsValid())
			{
				continue;
			}

			CachedBlockedPlayers.Add(MakeShared<FOnlineBlockedPlayerExtendedSteam>(
				BlockedSteamId.ConvertToUint64(),
				FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(BlockedSteamId)))));
		}

		bBlockedPlayersQueried = true;
		UE_LOG(LogExtendedSteam, Verbose, TEXT("QueryBlockedPlayers: cached %d blocked users"), CachedBlockedPlayers.Num());
		TriggerOnQueryBlockedPlayersCompleteDelegates(UserId, true, FString());
		return true;
	}
#endif

	const FString Error = TEXT("Steam client is not initialized");
	UE_LOG(LogExtendedSteam, Warning, TEXT("QueryBlockedPlayers: %s"), *Error);
	TriggerOnQueryBlockedPlayersCompleteDelegates(UserId, false, Error);
	return false;
}

bool FOnlineFriendsExtendedSteam::GetBlockedPlayers(const FUniqueNetId& UserId, TArray<TSharedRef<FOnlineBlockedPlayer>>& OutBlockedPlayers)
{
	OutBlockedPlayers.Reset();
	if (!bBlockedPlayersQueried)
	{
		return false;
	}

	OutBlockedPlayers = CachedBlockedPlayers;
	return true;
}

void FOnlineFriendsExtendedSteam::DumpBlockedPlayers() const
{
	UE_LOG(LogExtendedSteam, Log, TEXT("Blocked players (queried: %s): %d entries"),
		bBlockedPlayersQueried ? TEXT("yes") : TEXT("no"), CachedBlockedPlayers.Num());
	for (const TSharedRef<FOnlineBlockedPlayer>& BlockedPlayer : CachedBlockedPlayers)
	{
		UE_LOG(LogExtendedSteam, Log, TEXT("  %s (%s)"),
			*BlockedPlayer->GetDisplayName(), *BlockedPlayer->GetUserId()->ToDebugString());
	}
}

void FOnlineFriendsExtendedSteam::HandlePersonaStateChange(uint64 SteamId64, int32 ChangeFlags)
{
#if WITH_EXTENDEDSTEAM_SDK
	// Broadcast roster-relevant changes only; avatar/nickname/level churn stays quiet.
	constexpr int32 RelevantChanges =
		k_EPersonaChangeName | k_EPersonaChangeStatus | k_EPersonaChangeComeOnline |
		k_EPersonaChangeGoneOffline | k_EPersonaChangeGamePlayed | k_EPersonaChangeRelationshipChanged;

	if ((ChangeFlags & RelevantChanges) != 0)
	{
		TriggerOnFriendsChangeDelegates(0);
	}
#endif
}

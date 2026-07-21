// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamFriendsSubsystem.generated.h"

class UTexture2D;

/** Online status of a Steam user (mirrors Steamworks EPersonaState). */
UENUM(BlueprintType)
enum class EESteamPersonaState : uint8
{
	Offline,
	Online,
	Busy,
	Away,
	Snooze,
	LookingToTrade,
	LookingToPlay,
	Invisible
};

/** Steam avatar resolution (Small = 32x32, Medium = 64x64, Large = 184x184). */
UENUM(BlueprintType)
enum class EESteamAvatarSize : uint8
{
	Small,
	Medium,
	Large
};

/** Behaviour of ActivateGameOverlayToStore (mirrors Steamworks EOverlayToStoreFlag). */
UENUM(BlueprintType)
enum class EESteamOverlayToStoreFlag : uint8
{
	/** Just show the store page for the app. */
	None,
	/** Add the app to the cart without leaving the current overlay page. */
	AddToCart,
	/** Add the app to the cart and jump straight to the cart page. */
	AddToCartAndShow
};

/** A single entry of the local user's friends list. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamFriend
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Friends")
	FESteamId SteamId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Friends")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Friends")
	EESteamPersonaState State = EESteamPersonaState::Offline;

	/** True when the friend is playing this game right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Friends")
	bool bPlayingThisGame = false;

	/** App id of the game the friend is playing (0 when not in a game). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Friends")
	int32 InGameAppId = 0;
};

/** Fired when the Steam overlay is activated (true) or deactivated (false). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamOverlayActivated, bool, bActive);

/**
 * Fired when a friend's persona state (name, status, avatar...) changed. ChangeFlags is the raw
 * Steamworks EPersonaChange bitmask (k_EPersonaChangeName, ...ComeOnline, ...GamePlayed, ...),
 * telling consumers which fields changed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPersonaStateChanged, FESteamId, SteamId, int32, ChangeFlags);

/** Fired when a friend's rich-presence data finished downloading (RequestFriendRichPresence). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamFriendRichPresenceUpdate, FESteamId, FriendId, int32, AppId);

/** Fired when the user accepts a rich-presence game invite or joins via a friend ("connect" key). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamRichPresenceJoinRequested, FESteamId, FriendId, const FString&, ConnectString);

/** Fired when the user tries to join a lobby from the friends list / an invite. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyJoinRequested, FESteamId, LobbyId, FESteamId, FriendId);

/** Fired when a previously requested avatar image finished loading (query it again now). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamAvatarLoaded, FESteamId, SteamId);

/** Fired when a DownloadClanActivityCounts request completed; read the counts with GetClanActivityCounts. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamClanActivityCountsReceived, bool, bSuccess);

/** Fired when a RequestClanOfficerList request completed; officer getters are usable when bSuccess is true. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamClanOfficerListReceived, bool, bSuccess, FESteamId, Clan, int32, OfficerCount);

/** Fired when a GetFollowerCount request completed (Count is 0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamFollowerCount, bool, bSuccess, FESteamId, User, int32, Count);

/** Fired when an IsFollowing request completed (bIsFollowing is false on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamIsFollowing, bool, bSuccess, FESteamId, User, bool, bIsFollowing);

/** Fired when an EnumerateFollowingList request completed; Users is a page, TotalCount the grand total. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamFollowingListEnumerated, bool, bSuccess, const TArray<FESteamId>&, Users, int32, TotalCount);

/**
 * Wraps ISteamFriends: friends list, persona info, avatars, game overlay,
 * rich presence and game invites.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamFriendsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// -------- Local persona --------

	/** The local user's persona (display) name; empty when Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	FString GetPersonaName() const;

	/** The local user's online status; Offline when Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	EESteamPersonaState GetPersonaState() const;

	// -------- Friends list --------

	/** Number of regular (immediate) friends; 0 when Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	int32 GetFriendCount() const;

	/** Fills the local user's regular friends list (empty when Steam is unavailable). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Friends")
	void GetFriends(TArray<FESteamFriend>& OutFriends);

	// -------- Per-friend queries --------

	/** A friend's persona name; empty when unknown or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	FString GetFriendPersonaName(FESteamId SteamId) const;

	/** A friend's online status; Offline when unknown or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	EESteamPersonaState GetFriendPersonaState(FESteamId SteamId) const;

	/** True when the given user is a regular (immediate) friend of the local user. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	bool IsFriend(FESteamId SteamId) const;

	/** A friend's Steam community level (0 when unknown or Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	int32 GetFriendSteamLevel(FESteamId SteamId) const;

	/** The local user's private nickname for a friend, or empty when none is set or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Friends")
	FString GetPlayerNickname(FESteamId SteamId) const;

	/** Marks a user as "recently played with" (populates the Steam "players you've played with" list). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Friends")
	void SetPlayedWith(FESteamId SteamId);

	/**
	 * Starts downloading persona (and optionally full) information for a user the local client does
	 * not know about yet. Returns true when data is still loading (watch OnPersonaStateChanged for the
	 * id), false when the data is already available locally or Steam is unavailable. When bRequireNameOnly
	 * is true only the persona name is fetched, which is cheaper than fetching the avatar too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Friends")
	bool RequestUserInformation(FESteamId SteamId, bool bRequireNameOnly);

	// -------- Avatars --------

	/**
	 * A user's avatar as a transient texture, or null when Steam is unavailable, the user
	 * has no avatar, or the image is not downloaded yet (retry after OnAvatarLoaded fires).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Friends")
	UTexture2D* GetFriendAvatarTexture(FESteamId SteamId, EESteamAvatarSize Size);

	// -------- Overlay --------

	/** Opens a game overlay dialog: "friends", "community", "players", "settings", "officialgamegroup", "stats", "achievements". */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay")
	void ActivateGameOverlay(const FString& Dialog);

	/** Opens a game overlay dialog for a user: "steamid", "chat", "jointrade", "stats", "achievements", "friendadd", "friendremove", "friendrequestaccept", "friendrequestignore". */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay")
	void ActivateGameOverlayToUser(const FString& Dialog, FESteamId SteamId);

	/** Opens the game overlay web browser at the given URL. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay")
	void ActivateGameOverlayToWebPage(const FString& Url);

	/** Opens the overlay invite dialog; accepted invites deliver ConnectString via OnRichPresenceJoinRequested on the peer. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay")
	void ActivateGameOverlayInviteDialog(const FString& ConnectString);

	/** Opens the overlay store page for an app; Flag can also add the app to the cart. AppId 0 opens the store front page. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay")
	void ActivateGameOverlayToStore(int32 AppId, EESteamOverlayToStoreFlag Flag);

	// -------- Rich presence --------

	/** Sets a local rich presence key ("status", "connect", "steam_display"...); false on failure or when Steam is unavailable. */
	UFUNCTION(BlueprintCallable, Category = "Steam|RichPresence")
	bool SetRichPresence(const FString& Key, const FString& Value);

	/** Clears all local rich presence keys. */
	UFUNCTION(BlueprintCallable, Category = "Steam|RichPresence")
	void ClearRichPresence();

	/** A friend's rich presence value for the given key; empty when unset or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|RichPresence")
	FString GetFriendRichPresence(FESteamId SteamId, const FString& Key) const;

	/**
	 * Requests a friend's rich-presence data so GetFriendRichPresence* can read it. Completion is
	 * signalled by OnFriendRichPresenceUpdate for the friend. No-op when Steam is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|RichPresence")
	void RequestFriendRichPresence(FESteamId SteamId);

	/** Number of rich-presence keys currently set for a friend (0 when none or Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|RichPresence")
	int32 GetFriendRichPresenceKeyCount(FESteamId SteamId) const;

	/** The rich-presence key name at Index in [0, GetFriendRichPresenceKeyCount); empty when out of range. */
	UFUNCTION(BlueprintPure, Category = "Steam|RichPresence")
	FString GetFriendRichPresenceKeyByIndex(FESteamId SteamId, int32 Index) const;

	// -------- Invites --------

	/** Invites a friend to play; they receive ConnectString via OnRichPresenceJoinRequested (in-game) or the command line (launching). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Friends")
	bool InviteUserToGame(FESteamId SteamId, const FString& ConnectString);

	// -------- Clans / groups --------

	/** Number of Steam groups (clans) the local user belongs to; 0 when Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	int32 GetClanCount() const;

	/** The clan at Index in [0, GetClanCount); invalid when out of range or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FESteamId GetClanByIndex(int32 Index) const;

	/** A clan's display name; empty when unknown or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FString GetClanName(FESteamId ClanId) const;

	/** A clan's short tag; empty when unknown or Steam is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FString GetClanTag(FESteamId ClanId) const;

	/**
	 * Reads a clan's cached activity counts (online/in-game/chatting). Call DownloadClanActivityCounts
	 * first and wait for OnClanActivityCountsReceived. Returns false when the counts are not cached
	 * or Steam is unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Clans")
	bool GetClanActivityCounts(FESteamId ClanId, int32& OutOnline, int32& OutInGame, int32& OutChatting) const;

	/**
	 * Requests up-to-date activity counts for a clan. Result arrives on OnClanActivityCountsReceived;
	 * read the counts with GetClanActivityCounts afterwards. Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Clans")
	bool DownloadClanActivityCounts(FESteamId ClanId);

	/** Number of members in a source (clan or chat) the local user shares; 0 when unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	int32 GetFriendCountFromSource(FESteamId SourceId) const;

	/** The member at Index in a shared source; invalid when out of range or unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FESteamId GetFriendFromSourceByIndex(FESteamId SourceId, int32 Index) const;

	/**
	 * Requests the officer list (owner + officers) of a clan so GetClanOfficer* and GetClanOwner
	 * return meaningful data. Result arrives on OnClanOfficerListReceived. Returns false when the
	 * request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Clans")
	bool RequestClanOfficerList(FESteamId ClanId);

	/** Number of officers in a clan (requires a completed RequestClanOfficerList); 0 when unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	int32 GetClanOfficerCount(FESteamId ClanId) const;

	/** The officer at Index in [0, GetClanOfficerCount); invalid when out of range or unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FESteamId GetClanOfficerByIndex(FESteamId ClanId, int32 Index) const;

	/** The owner of a clan (requires a completed RequestClanOfficerList); invalid when unavailable. */
	UFUNCTION(BlueprintPure, Category = "Steam|Clans")
	FESteamId GetClanOwner(FESteamId ClanId) const;

	// -------- Followers --------

	/**
	 * Requests how many followers a user has. Result arrives on OnFollowerCount. Returns false when
	 * the request could not be issued (Steam unavailable or invalid id).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Followers")
	bool GetFollowerCount(FESteamId User);

	/**
	 * Requests whether the local user follows another user. Result arrives on OnIsFollowing. Returns
	 * false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Followers")
	bool IsFollowing(FESteamId User);

	/**
	 * Enumerates the users the local user follows, one page at a time (Steam returns up to 50 per
	 * call). Result arrives on OnFollowingListEnumerated; pass the running total as StartIndex to page.
	 * Returns false when the request could not be issued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Followers")
	bool EnumerateFollowingList(int32 StartIndex);

	// -------- Events --------

	UPROPERTY(BlueprintAssignable, Category = "Steam|Overlay")
	FOnSteamOverlayActivated OnOverlayActivated;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends")
	FOnSteamPersonaStateChanged OnPersonaStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|RichPresence")
	FOnSteamFriendRichPresenceUpdate OnFriendRichPresenceUpdate;

	UPROPERTY(BlueprintAssignable, Category = "Steam|RichPresence")
	FOnSteamRichPresenceJoinRequested OnRichPresenceJoinRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends")
	FOnSteamLobbyJoinRequested OnLobbyJoinRequested;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends")
	FOnSteamAvatarLoaded OnAvatarLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Clans")
	FOnSteamClanActivityCountsReceived OnClanActivityCountsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Clans")
	FOnSteamClanOfficerListReceived OnClanOfficerListReceived;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Followers")
	FOnSteamFollowerCount OnFollowerCount;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Followers")
	FOnSteamIsFollowing OnIsFollowing;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Followers")
	FOnSteamFollowingListEnumerated OnFollowingListEnumerated;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamFriendsCallbacks;
	TSharedPtr<class FESteamFriendsCallbacks> Callbacks;
};

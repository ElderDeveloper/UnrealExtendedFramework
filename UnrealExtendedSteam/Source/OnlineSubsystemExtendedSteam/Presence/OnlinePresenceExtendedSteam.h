// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

/**
 * Presence interface backed by Steam rich presence (ISteamFriends).
 *
 * SetPresence writes the status string under the Steam-conventional "status" rich-presence key
 * and every FOnlineUserPresenceStatus property under its own key (values stringified via
 * FVariantData::ToString). A "steam_display" property therefore passes straight through to
 * Steam's localized-display mechanism. Steam caps rich presence at k_cchMaxRichPresenceKeys (30)
 * keys, k_cchMaxRichPresenceKeyLength (64) key chars and k_cchMaxRichPresenceValueLength (256)
 * value chars; violations are warned about and reported through the completion delegate.
 *
 * QueryPresence answers synchronously from the client's local knowledge for the local user and
 * for friends (Steam keeps friends' state hot) while still issuing RequestFriendRichPresence for
 * a refresh; refreshed data arrives via FriendRichPresenceUpdate_t and re-broadcasts through
 * OnPresenceReceived. For non-friends the delegate is parked until the update callback arrives —
 * Steam sends none for users it knows nothing about, so such queries may never complete.
 */
class FOnlinePresenceExtendedSteam : public IOnlinePresence
{
public:
	explicit FOnlinePresenceExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlinePresenceExtendedSteam();

	//~ Begin IOnlinePresence
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& UserIds, const FOnPresenceTaskCompleteDelegate& Delegate) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	//~ End IOnlinePresence

	/**
	 * Builds a presence snapshot for a Steam user from the client's local knowledge:
	 * bIsOnline from the persona state, bIsPlaying/bIsPlayingThisGame from GetFriendGamePlayed,
	 * status string from the "status" rich-presence key, joinability from a lobby or "connect" key.
	 * Also used by the friends interface for the per-friend presence snapshot.
	 */
	static FOnlineUserPresence BuildPresenceSnapshot(uint64 SteamId64);

	/** Called by the cpp-local Steam callback holder when a FriendRichPresenceUpdate_t arrives. */
	void HandleRichPresenceUpdate(uint64 SteamId64);

	/** Called by the cpp-local Steam callback holder when a PersonaStateChange_t arrives. */
	void HandlePersonaStateChange(uint64 SteamId64, int32 ChangeFlags);

private:
	/** Rebuilds and caches the snapshot for a user, then broadcasts OnPresenceReceived/OnPresenceArrayUpdated. */
	TSharedRef<FOnlineUserPresence> CacheAndBroadcast(uint64 SteamId64);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Steam callback holder (defined in the cpp, only constructed while the Steam client is up). */
	TSharedPtr<class FPresenceExtendedSteamCallbacks> Callbacks;

	/** Last known presence per SteamID64, fed by SetPresence/QueryPresence and Steam callbacks. */
	TMap<uint64, TSharedRef<FOnlineUserPresence>> PresenceCache;

	/** QueryPresence delegates for non-friends, parked until their FriendRichPresenceUpdate_t arrives. */
	TMap<uint64, TArray<FOnPresenceTaskCompleteDelegate>> PendingQueries;
};

typedef TSharedPtr<FOnlinePresenceExtendedSteam, ESPMode::ThreadSafe> FOnlinePresenceExtendedSteamPtr;

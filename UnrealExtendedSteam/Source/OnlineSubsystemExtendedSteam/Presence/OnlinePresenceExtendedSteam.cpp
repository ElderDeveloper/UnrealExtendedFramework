// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Presence/OnlinePresenceExtendedSteam.h"
#include "Identity/OnlineIdentityExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamPresence
{
#if WITH_EXTENDEDSTEAM_SDK
	/** Steam's conventional rich-presence key for the user-facing status line. */
	static const char* StatusKey = "status";

	/** Rich-presence key set by joinable games; presence is joinable when it is non-empty. */
	static const char* ConnectKey = "connect";
#endif

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

	/** Extracts the SteamID64 from a unique net id of our service type; 0 otherwise. */
	static uint64 GetSteamId64(const FUniqueNetId& NetId)
	{
		if (NetId.GetType() == ESTEAM_SUBSYSTEM && NetId.GetSize() == sizeof(uint64))
		{
			uint64 SteamId64 = 0;
			FMemory::Memcpy(&SteamId64, NetId.GetBytes(), sizeof(uint64));
			return SteamId64;
		}
		return 0;
	}

	/** SteamID64 of the local Steam user; 0 when unavailable. */
	static uint64 GetLocalSteamId64()
	{
#if WITH_EXTENDEDSTEAM_SDK
		if (IsSteamClientUp() && SteamUser() != nullptr)
		{
			return SteamUser()->GetSteamID().ConvertToUint64();
		}
#endif
		return 0;
	}

#if WITH_EXTENDEDSTEAM_SDK
	/** Maps Steam persona states onto the OSS presence states. */
	static EOnlinePresenceState::Type MapPersonaState(EPersonaState PersonaState)
	{
		switch (PersonaState)
		{
			case k_EPersonaStateOnline:
			case k_EPersonaStateLookingToTrade:
			case k_EPersonaStateLookingToPlay:
				return EOnlinePresenceState::Online;
			case k_EPersonaStateBusy:
				return EOnlinePresenceState::DoNotDisturb;
			case k_EPersonaStateAway:
				return EOnlinePresenceState::Away;
			case k_EPersonaStateSnooze:
				return EOnlinePresenceState::ExtendedAway;
			case k_EPersonaStateOffline:
			case k_EPersonaStateInvisible: // never published to other clients anyway
			default:
				return EOnlinePresenceState::Offline;
		}
	}
#endif
}

#if WITH_EXTENDEDSTEAM_SDK

/** Steam callback holder — kept out of the header so SDK types stay private to this cpp. */
class FPresenceExtendedSteamCallbacks
{
public:
	explicit FPresenceExtendedSteamCallbacks(FOnlinePresenceExtendedSteam& InOwner)
		: Owner(InOwner)
		, RichPresenceUpdateCallback(this, &FPresenceExtendedSteamCallbacks::OnRichPresenceUpdate)
		, PersonaStateChangeCallback(this, &FPresenceExtendedSteamCallbacks::OnPersonaStateChange)
	{
	}

private:
	void OnRichPresenceUpdate(FriendRichPresenceUpdate_t* Data)
	{
		if (Data != nullptr)
		{
			Owner.HandleRichPresenceUpdate(Data->m_steamIDFriend.ConvertToUint64());
		}
	}

	void OnPersonaStateChange(PersonaStateChange_t* Data)
	{
		if (Data != nullptr)
		{
			Owner.HandlePersonaStateChange(Data->m_ulSteamID, Data->m_nChangeFlags);
		}
	}

	FOnlinePresenceExtendedSteam& Owner;
	CCallback<FPresenceExtendedSteamCallbacks, FriendRichPresenceUpdate_t> RichPresenceUpdateCallback;
	CCallback<FPresenceExtendedSteamCallbacks, PersonaStateChange_t> PersonaStateChangeCallback;
};

#else

class FPresenceExtendedSteamCallbacks
{
};

#endif // WITH_EXTENDEDSTEAM_SDK

FOnlinePresenceExtendedSteam::FOnlinePresenceExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamPresence::IsSteamClientUp())
	{
		Callbacks = MakeShared<FPresenceExtendedSteamCallbacks>(*this);
	}
#endif
}

FOnlinePresenceExtendedSteam::~FOnlinePresenceExtendedSteam() = default;

FOnlineUserPresence FOnlinePresenceExtendedSteam::BuildPresenceSnapshot(uint64 SteamId64)
{
	FOnlineUserPresence Presence;

#if WITH_EXTENDEDSTEAM_SDK
	if (SteamId64 == 0 || !ExtendedSteamPresence::IsSteamClientUp())
	{
		return Presence;
	}

	const CSteamID SteamId(SteamId64);
	const bool bIsLocalUser = SteamId64 == ExtendedSteamPresence::GetLocalSteamId64();

	// Persona state: GetPersonaState for the local user, GetFriendPersonaState for everyone else
	// (which only knows about friends — unknown users read as offline).
	const EPersonaState PersonaState = bIsLocalUser
		? SteamFriends()->GetPersonaState()
		: SteamFriends()->GetFriendPersonaState(SteamId);
	Presence.bIsOnline = PersonaState != k_EPersonaStateOffline && PersonaState != k_EPersonaStateInvisible;
	Presence.Status.State = ExtendedSteamPresence::MapPersonaState(PersonaState);

	if (bIsLocalUser)
	{
		// The local user is by definition running this game right now.
		Presence.bIsPlaying = true;
		Presence.bIsPlayingThisGame = true;
	}
	else
	{
		FriendGameInfo_t GameInfo{};
		if (SteamFriends()->GetFriendGamePlayed(SteamId, &GameInfo))
		{
			Presence.bIsPlaying = true;
			Presence.bIsPlayingThisGame = SteamUtils() != nullptr && GameInfo.m_gameID.AppID() == SteamUtils()->GetAppID();
			Presence.bIsJoinable = GameInfo.m_steamIDLobby.IsValid();
		}
	}

	// Status string and joinability from rich presence ("" when the key is not set).
	Presence.Status.StatusStr = FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendRichPresence(SteamId, ExtendedSteamPresence::StatusKey)));
	const char* ConnectValue = SteamFriends()->GetFriendRichPresence(SteamId, ExtendedSteamPresence::ConnectKey);
	if (ConnectValue != nullptr && ConnectValue[0] != '\0')
	{
		Presence.bIsJoinable = true;
	}
#endif

	return Presence;
}

void FOnlinePresenceExtendedSteam::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	const uint64 UserId64 = ExtendedSteamPresence::GetSteamId64(User);
	const uint64 LocalId64 = ExtendedSteamPresence::GetLocalSteamId64();

	if (!ExtendedSteamPresence::IsSteamClientUp() || LocalId64 == 0)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SetPresence: Steam client is not initialized"));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	if (UserId64 != LocalId64)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SetPresence: presence can only be set for the local Steam user (requested %s)"), *User.ToDebugString());
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	// Steam caps rich presence at k_cchMaxRichPresenceKeys (30) keys; we spend one on "status".
	const int32 KeyBudget = k_cchMaxRichPresenceKeys - 1;
	if (Status.Properties.Num() > KeyBudget)
	{
		UE_LOG(LogExtendedSteam, Warning,
			TEXT("SetPresence: %d presence properties exceed Steam's rich-presence key budget (%d of %d after the status key); extra keys will fail"),
			Status.Properties.Num(), KeyBudget, k_cchMaxRichPresenceKeys);
	}

	// The user-facing status line goes under Steam's conventional "status" key (what a
	// "steam_display" value of "#Status_<...>"/"{#status}" style localization tokens reference).
	bool bAllKeysSet = SteamFriends()->SetRichPresence(ExtendedSteamPresence::StatusKey, TCHAR_TO_UTF8(*Status.StatusStr));

	// Every property becomes its own rich-presence key with the stringified value. A
	// "steam_display" property therefore passes straight through to Steam's localized display.
	// Steam also enforces k_cchMaxRichPresenceKeyLength (64) / k_cchMaxRichPresenceValueLength (256).
	for (const TPair<FPresenceKey, FVariantData>& Property : Status.Properties)
	{
		const FString Value = Property.Value.ToString();
		if (!SteamFriends()->SetRichPresence(TCHAR_TO_UTF8(*Property.Key), TCHAR_TO_UTF8(*Value)))
		{
			UE_LOG(LogExtendedSteam, Warning,
				TEXT("SetPresence: SetRichPresence failed for key '%s' (key limit %d, key length limit %d, value length limit %d)"),
				*Property.Key, k_cchMaxRichPresenceKeys, k_cchMaxRichPresenceKeyLength, k_cchMaxRichPresenceValueLength);
			bAllKeysSet = false;
		}
	}

	// Refresh the local user's cached presence so GetCachedPresence reflects what we just set.
	TSharedRef<FOnlineUserPresence> LocalPresence = MakeShared<FOnlineUserPresence>(BuildPresenceSnapshot(LocalId64));
	LocalPresence->Status = Status;
	LocalPresence->bIsOnline = true;
	LocalPresence->bIsPlaying = true;
	LocalPresence->bIsPlayingThisGame = true;
	PresenceCache.Add(LocalId64, LocalPresence);

	Delegate.ExecuteIfBound(User, bAllKeysSet);
#else
	Delegate.ExecuteIfBound(User, false);
#endif
}

void FOnlinePresenceExtendedSteam::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	const uint64 UserId64 = ExtendedSteamPresence::GetSteamId64(User);

	if (UserId64 == 0 || !ExtendedSteamPresence::IsSteamClientUp())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("QueryPresence: Steam client not initialized or invalid user id (%s)"), *User.ToDebugString());
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	const CSteamID SteamId(UserId64);
	const bool bIsLocalUser = UserId64 == ExtendedSteamPresence::GetLocalSteamId64();
	const bool bIsFriend = SteamFriends()->HasFriend(SteamId, k_EFriendFlagImmediate);

	if (bIsFriend)
	{
		// Ask Steam to refresh this friend's rich presence; the FriendRichPresenceUpdate_t that
		// answers it re-broadcasts through OnPresenceReceived.
		SteamFriends()->RequestFriendRichPresence(SteamId);
	}

	if (bIsLocalUser || bIsFriend)
	{
		// The Steam client already keeps local/friend state hot: answer synchronously from it.
		CacheAndBroadcast(UserId64);
		Delegate.ExecuteIfBound(User, true);
		return;
	}

	// Non-friend: nothing is known locally. Request the data and park the delegate until the
	// FriendRichPresenceUpdate_t arrives. Steam sends no update for users it cannot resolve,
	// so this query may never complete — callers should prefer querying friends.
	SteamFriends()->RequestFriendRichPresence(SteamId);
	PendingQueries.FindOrAdd(UserId64).Add(Delegate);
	UE_LOG(LogExtendedSteam, Verbose, TEXT("QueryPresence: requested rich presence for non-friend %s; completion depends on Steam answering"), *User.ToDebugString());
#else
	Delegate.ExecuteIfBound(User, false);
#endif
}

void FOnlinePresenceExtendedSteam::QueryPresence(const FUniqueNetId& LocalUserId, const TArray<FUniqueNetIdRef>& UserIds, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	// Best effort: kick off the single-user path for each id (friends complete synchronously,
	// non-friends when/if Steam answers) and report the batch as issued.
	for (const FUniqueNetIdRef& UserId : UserIds)
	{
		QueryPresence(*UserId, FOnPresenceTaskCompleteDelegate());
	}
	Delegate.ExecuteIfBound(LocalUserId, ExtendedSteamPresence::IsSteamClientUp());
}

EOnlineCachedResult::Type FOnlinePresenceExtendedSteam::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	const uint64 UserId64 = ExtendedSteamPresence::GetSteamId64(User);
	if (const TSharedRef<FOnlineUserPresence>* CachedPresence = PresenceCache.Find(UserId64))
	{
		OutPresence = *CachedPresence;
		return EOnlineCachedResult::Success;
	}

	OutPresence = nullptr;
	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlinePresenceExtendedSteam::GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	// Steam rich presence is scoped to the running app; only our own app id can be answered.
	if (Subsystem != nullptr && AppId == Subsystem->GetAppId())
	{
		return GetCachedPresence(User, OutPresence);
	}

	OutPresence = nullptr;
	return EOnlineCachedResult::NotFound;
}

TSharedRef<FOnlineUserPresence> FOnlinePresenceExtendedSteam::CacheAndBroadcast(uint64 SteamId64)
{
	TSharedRef<FOnlineUserPresence> Presence = MakeShared<FOnlineUserPresence>(BuildPresenceSnapshot(SteamId64));
	PresenceCache.Add(SteamId64, Presence);

	const FUniqueNetIdExtendedSteamRef UserId = FUniqueNetIdExtendedSteam::Create(SteamId64);
	TriggerOnPresenceReceivedDelegates(*UserId, Presence);

	// Steam has exactly one presence entry per user; the "array" is that single entry.
	TArray<TSharedRef<FOnlineUserPresence>> PresenceArray;
	PresenceArray.Add(Presence);
	TriggerOnPresenceArrayUpdatedDelegates(*UserId, PresenceArray);

	return Presence;
}

void FOnlinePresenceExtendedSteam::HandleRichPresenceUpdate(uint64 SteamId64)
{
	CacheAndBroadcast(SteamId64);

	// Complete any parked non-friend queries for this user.
	TArray<FOnPresenceTaskCompleteDelegate> Pending;
	if (PendingQueries.RemoveAndCopyValue(SteamId64, Pending))
	{
		const FUniqueNetIdExtendedSteamRef UserId = FUniqueNetIdExtendedSteam::Create(SteamId64);
		for (const FOnPresenceTaskCompleteDelegate& PendingDelegate : Pending)
		{
			PendingDelegate.ExecuteIfBound(*UserId, true);
		}
	}
}

void FOnlinePresenceExtendedSteam::HandlePersonaStateChange(uint64 SteamId64, int32 ChangeFlags)
{
#if WITH_EXTENDEDSTEAM_SDK
	// Only presence-relevant changes re-broadcast; avatar/nickname/level churn stays quiet.
	constexpr int32 RelevantChanges =
		k_EPersonaChangeStatus | k_EPersonaChangeComeOnline | k_EPersonaChangeGoneOffline |
		k_EPersonaChangeGamePlayed | k_EPersonaChangeRichPresence;

	if ((ChangeFlags & RelevantChanges) == 0)
	{
		return;
	}

	// Re-broadcast for users someone is tracking (cached) or friends (always interesting).
	const bool bIsTracked = PresenceCache.Contains(SteamId64)
		|| (ExtendedSteamPresence::IsSteamClientUp() && SteamFriends()->HasFriend(CSteamID(SteamId64), k_EFriendFlagImmediate));
	if (bIsTracked)
	{
		CacheAndBroadcast(SteamId64);
	}
#endif
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Social/ESteamFriendsSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Engine/Texture2D.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ESteamFriendsPrivate
{
	EESteamPersonaState ConvertPersonaState(const EPersonaState State)
	{
		switch (State)
		{
		case k_EPersonaStateOnline:         return EESteamPersonaState::Online;
		case k_EPersonaStateBusy:           return EESteamPersonaState::Busy;
		case k_EPersonaStateAway:           return EESteamPersonaState::Away;
		case k_EPersonaStateSnooze:         return EESteamPersonaState::Snooze;
		case k_EPersonaStateLookingToTrade: return EESteamPersonaState::LookingToTrade;
		case k_EPersonaStateLookingToPlay:  return EESteamPersonaState::LookingToPlay;
		case k_EPersonaStateInvisible:      return EESteamPersonaState::Invisible;
		case k_EPersonaStateOffline:
		default:                            return EESteamPersonaState::Offline;
		}
	}

	EOverlayToStoreFlag ToSteamOverlayToStoreFlag(const EESteamOverlayToStoreFlag Flag)
	{
		switch (Flag)
		{
		case EESteamOverlayToStoreFlag::AddToCart:        return k_EOverlayToStoreFlag_AddToCart;
		case EESteamOverlayToStoreFlag::AddToCartAndShow: return k_EOverlayToStoreFlag_AddToCartAndShow;
		default:                                          return k_EOverlayToStoreFlag_None;
		}
	}
}

// Parameters captured to (re-)issue queued ISteamFriends CCallResult requests. Each serialized op
// has its own FIFO queue in FESteamFriendsCallbacks so overlapping requests are never dropped.
struct FESteamPendingFollowerCount { FESteamId User; };
struct FESteamPendingIsFollowing { FESteamId User; };
struct FESteamPendingEnumerateFollowing { int32 StartIndex = 0; };
struct FESteamPendingClanActivityCounts { FESteamId Clan; };
struct FESteamPendingClanOfficerList { FESteamId Clan; };

/** Native Steam callback listeners; alive only while the Steam client API is initialized. */
class FESteamFriendsCallbacks
{
public:
	explicit FESteamFriendsCallbacks(UESteamFriendsSubsystem* InOwner)
		: Owner(InOwner)
		, OverlayActivated(this, &FESteamFriendsCallbacks::HandleOverlayActivated)
		, PersonaStateChange(this, &FESteamFriendsCallbacks::HandlePersonaStateChange)
		, FriendRichPresenceUpdate(this, &FESteamFriendsCallbacks::HandleFriendRichPresenceUpdate)
		, RichPresenceJoinRequested(this, &FESteamFriendsCallbacks::HandleRichPresenceJoinRequested)
		, LobbyJoinRequested(this, &FESteamFriendsCallbacks::HandleLobbyJoinRequested)
		, AvatarImageLoaded(this, &FESteamFriendsCallbacks::HandleAvatarImageLoaded)
	{
	}

	// Each Enqueue* issues the Steam CCallResult immediately when its operation is idle, otherwise it
	// queues the request and issues it in order as earlier ones complete. Returns true when issued or
	// queued; false only on the immediate path when the Steam call could not be issued.

	bool EnqueueFollowerCount(const FESteamPendingFollowerCount& Request)
	{
		if (bFollowerCountBusy)
		{
			FollowerCountQueue.Enqueue(Request);
			return true;
		}
		return IssueFollowerCount(Request);
	}

	bool EnqueueIsFollowing(const FESteamPendingIsFollowing& Request)
	{
		if (bIsFollowingBusy)
		{
			IsFollowingQueue.Enqueue(Request);
			return true;
		}
		return IssueIsFollowing(Request);
	}

	bool EnqueueEnumerateFollowing(const FESteamPendingEnumerateFollowing& Request)
	{
		if (bEnumerateFollowingBusy)
		{
			EnumerateFollowingQueue.Enqueue(Request);
			return true;
		}
		return IssueEnumerateFollowing(Request);
	}

	bool EnqueueClanActivityCounts(const FESteamPendingClanActivityCounts& Request)
	{
		if (bClanActivityBusy)
		{
			ClanActivityQueue.Enqueue(Request);
			return true;
		}
		return IssueClanActivityCounts(Request);
	}

	bool EnqueueClanOfficerList(const FESteamPendingClanOfficerList& Request)
	{
		if (bClanOfficerListBusy)
		{
			ClanOfficerListQueue.Enqueue(Request);
			return true;
		}
		return IssueClanOfficerList(Request);
	}

private:
	void HandleOverlayActivated(GameOverlayActivated_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnOverlayActivated.Broadcast(Data->m_bActive != 0);
		}
	}

	void HandlePersonaStateChange(PersonaStateChange_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnPersonaStateChanged.Broadcast(
				FESteamId(Data->m_ulSteamID),
				static_cast<int32>(Data->m_nChangeFlags));
		}
	}

	void HandleFriendRichPresenceUpdate(FriendRichPresenceUpdate_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnFriendRichPresenceUpdate.Broadcast(
				FESteamId(Data->m_steamIDFriend.ConvertToUint64()),
				static_cast<int32>(Data->m_nAppID));
		}
	}

	void HandleRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnRichPresenceJoinRequested.Broadcast(
				FESteamId(Data->m_steamIDFriend.ConvertToUint64()),
				FString(UTF8_TO_TCHAR(Data->m_rgchConnect)));
		}
	}

	void HandleLobbyJoinRequested(GameLobbyJoinRequested_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyJoinRequested.Broadcast(
				FESteamId(Data->m_steamIDLobby.ConvertToUint64()),
				FESteamId(Data->m_steamIDFriend.ConvertToUint64()));
		}
	}

	void HandleAvatarImageLoaded(AvatarImageLoaded_t* Data)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnAvatarLoaded.Broadcast(FESteamId(Data->m_steamID.ConvertToUint64()));
		}
	}

	// ---- GetFollowerCount (serialized) ----

	bool IssueFollowerCount(const FESteamPendingFollowerCount& Request)
	{
		UESteamFriendsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamFriends())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamFriends()->GetFollowerCount(CSteamID(Request.User.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		FollowerCountResult.Set(Call, this, &FESteamFriendsCallbacks::HandleFollowerCount);
		bFollowerCountBusy = true;
		return true;
	}

	void HandleFollowerCount(FriendsGetFollowerCount_t* Data, bool bIOFailure)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnFollowerCount.Broadcast(
				bSuccess, FESteamId(Data->m_steamID.ConvertToUint64()), bSuccess ? Data->m_nCount : 0);
		}
		bFollowerCountBusy = false;
		DrainFollowerCountQueue();
	}

	void DrainFollowerCountQueue()
	{
		while (!bFollowerCountBusy)
		{
			FESteamPendingFollowerCount Request;
			if (!FollowerCountQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueFollowerCount(Request))
			{
				if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnFollowerCount.Broadcast(false, Request.User, 0);
				}
			}
		}
	}

	// ---- IsFollowing (serialized) ----

	bool IssueIsFollowing(const FESteamPendingIsFollowing& Request)
	{
		UESteamFriendsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamFriends())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamFriends()->IsFollowing(CSteamID(Request.User.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		IsFollowingResult.Set(Call, this, &FESteamFriendsCallbacks::HandleIsFollowing);
		bIsFollowingBusy = true;
		return true;
	}

	void HandleIsFollowing(FriendsIsFollowing_t* Data, bool bIOFailure)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnIsFollowing.Broadcast(
				bSuccess, FESteamId(Data->m_steamID.ConvertToUint64()), bSuccess && Data->m_bIsFollowing);
		}
		bIsFollowingBusy = false;
		DrainIsFollowingQueue();
	}

	void DrainIsFollowingQueue()
	{
		while (!bIsFollowingBusy)
		{
			FESteamPendingIsFollowing Request;
			if (!IsFollowingQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueIsFollowing(Request))
			{
				if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnIsFollowing.Broadcast(false, Request.User, false);
				}
			}
		}
	}

	// ---- EnumerateFollowingList (serialized) ----

	bool IssueEnumerateFollowing(const FESteamPendingEnumerateFollowing& Request)
	{
		UESteamFriendsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamFriends())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamFriends()->EnumerateFollowingList(static_cast<uint32>(FMath::Max(0, Request.StartIndex)));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		EnumerateFollowingResult.Set(Call, this, &FESteamFriendsCallbacks::HandleEnumerateFollowing);
		bEnumerateFollowingBusy = true;
		return true;
	}

	void HandleEnumerateFollowing(FriendsEnumerateFollowingList_t* Data, bool bIOFailure)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			TArray<FESteamId> Users;
			int32 TotalCount = 0;
			if (bSuccess)
			{
				const int32 Returned = FMath::Clamp<int32>(Data->m_nResultsReturned, 0, static_cast<int32>(k_cEnumerateFollowersMax));
				Users.Reserve(Returned);
				for (int32 Index = 0; Index < Returned; ++Index)
				{
					Users.Add(FESteamId(Data->m_rgSteamID[Index].ConvertToUint64()));
				}
				TotalCount = Data->m_nTotalResultCount;
			}
			Subsystem->OnFollowingListEnumerated.Broadcast(bSuccess, Users, TotalCount);
		}
		bEnumerateFollowingBusy = false;
		DrainEnumerateFollowingQueue();
	}

	void DrainEnumerateFollowingQueue()
	{
		while (!bEnumerateFollowingBusy)
		{
			FESteamPendingEnumerateFollowing Request;
			if (!EnumerateFollowingQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueEnumerateFollowing(Request))
			{
				if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnFollowingListEnumerated.Broadcast(false, TArray<FESteamId>(), 0);
				}
			}
		}
	}

	// ---- DownloadClanActivityCounts (serialized) ----

	bool IssueClanActivityCounts(const FESteamPendingClanActivityCounts& Request)
	{
		UESteamFriendsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamFriends())
		{
			return false;
		}

		CSteamID ClanId(Request.Clan.Value);
		const SteamAPICall_t Call = SteamFriends()->DownloadClanActivityCounts(&ClanId, 1);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		ClanActivityResult.Set(Call, this, &FESteamFriendsCallbacks::HandleClanActivityCounts);
		bClanActivityBusy = true;
		return true;
	}

	void HandleClanActivityCounts(DownloadClanActivityCountsResult_t* Data, bool bIOFailure)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnClanActivityCountsReceived.Broadcast(!bIOFailure && Data->m_bSuccess);
		}
		bClanActivityBusy = false;
		DrainClanActivityQueue();
	}

	void DrainClanActivityQueue()
	{
		while (!bClanActivityBusy)
		{
			FESteamPendingClanActivityCounts Request;
			if (!ClanActivityQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueClanActivityCounts(Request))
			{
				if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnClanActivityCountsReceived.Broadcast(false);
				}
			}
		}
	}

	// ---- RequestClanOfficerList (serialized) ----

	bool IssueClanOfficerList(const FESteamPendingClanOfficerList& Request)
	{
		UESteamFriendsSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamFriends())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamFriends()->RequestClanOfficerList(CSteamID(Request.Clan.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		ClanOfficerListResult.Set(Call, this, &FESteamFriendsCallbacks::HandleClanOfficerList);
		bClanOfficerListBusy = true;
		return true;
	}

	void HandleClanOfficerList(ClanOfficerListResponse_t* Data, bool bIOFailure)
	{
		if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_bSuccess;
			Subsystem->OnClanOfficerListReceived.Broadcast(
				bSuccess, FESteamId(Data->m_steamIDClan.ConvertToUint64()), bSuccess ? Data->m_cOfficers : 0);
		}
		bClanOfficerListBusy = false;
		DrainClanOfficerListQueue();
	}

	void DrainClanOfficerListQueue()
	{
		while (!bClanOfficerListBusy)
		{
			FESteamPendingClanOfficerList Request;
			if (!ClanOfficerListQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueClanOfficerList(Request))
			{
				if (UESteamFriendsSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnClanOfficerListReceived.Broadcast(false, Request.Clan, 0);
				}
			}
		}
	}

	TWeakObjectPtr<UESteamFriendsSubsystem> Owner;
	CCallback<FESteamFriendsCallbacks, GameOverlayActivated_t> OverlayActivated;
	CCallback<FESteamFriendsCallbacks, PersonaStateChange_t> PersonaStateChange;
	CCallback<FESteamFriendsCallbacks, FriendRichPresenceUpdate_t> FriendRichPresenceUpdate;
	CCallback<FESteamFriendsCallbacks, GameRichPresenceJoinRequested_t> RichPresenceJoinRequested;
	CCallback<FESteamFriendsCallbacks, GameLobbyJoinRequested_t> LobbyJoinRequested;
	CCallback<FESteamFriendsCallbacks, AvatarImageLoaded_t> AvatarImageLoaded;

	CCallResult<FESteamFriendsCallbacks, FriendsGetFollowerCount_t> FollowerCountResult;
	CCallResult<FESteamFriendsCallbacks, FriendsIsFollowing_t> IsFollowingResult;
	CCallResult<FESteamFriendsCallbacks, FriendsEnumerateFollowingList_t> EnumerateFollowingResult;
	CCallResult<FESteamFriendsCallbacks, DownloadClanActivityCountsResult_t> ClanActivityResult;
	CCallResult<FESteamFriendsCallbacks, ClanOfficerListResponse_t> ClanOfficerListResult;

	// In-flight flags + FIFO queues, one per serialized CCallResult operation type.
	bool bFollowerCountBusy = false;
	bool bIsFollowingBusy = false;
	bool bEnumerateFollowingBusy = false;
	bool bClanActivityBusy = false;
	bool bClanOfficerListBusy = false;

	TQueue<FESteamPendingFollowerCount> FollowerCountQueue;
	TQueue<FESteamPendingIsFollowing> IsFollowingQueue;
	TQueue<FESteamPendingEnumerateFollowing> EnumerateFollowingQueue;
	TQueue<FESteamPendingClanActivityCounts> ClanActivityQueue;
	TQueue<FESteamPendingClanOfficerList> ClanOfficerListQueue;
};
#else
class FESteamFriendsCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamFriendsSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamFriendsSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamFriendsCallbacks>(this);
	}
#endif
}

void UESteamFriendsSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

FString UESteamFriendsSubsystem::GetPersonaName() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends())
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetPersonaName()));
	}
#endif
	return FString();
}

EESteamPersonaState UESteamFriendsSubsystem::GetPersonaState() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends())
	{
		return ESteamFriendsPrivate::ConvertPersonaState(SteamFriends()->GetPersonaState());
	}
#endif
	return EESteamPersonaState::Offline;
}

int32 UESteamFriendsSubsystem::GetFriendCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends())
	{
		return FMath::Max(0, SteamFriends()->GetFriendCount(k_EFriendFlagImmediate));
	}
#endif
	return 0;
}

void UESteamFriendsSubsystem::GetFriends(TArray<FESteamFriend>& OutFriends)
{
	OutFriends.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		return;
	}

	const int32 Count = FMath::Max(0, SteamFriends()->GetFriendCount(k_EFriendFlagImmediate));
	OutFriends.Reserve(Count);

	const AppId_t OwnAppId = SteamUtils() ? SteamUtils()->GetAppID() : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const CSteamID FriendId = SteamFriends()->GetFriendByIndex(Index, k_EFriendFlagImmediate);
		if (!FriendId.IsValid())
		{
			continue;
		}

		FESteamFriend& Friend = OutFriends.AddDefaulted_GetRef();
		Friend.SteamId = FESteamId(FriendId.ConvertToUint64());
		Friend.DisplayName = FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(FriendId)));
		Friend.State = ESteamFriendsPrivate::ConvertPersonaState(SteamFriends()->GetFriendPersonaState(FriendId));

		FriendGameInfo_t GameInfo{};
		if (SteamFriends()->GetFriendGamePlayed(FriendId, &GameInfo))
		{
			Friend.InGameAppId = static_cast<int32>(GameInfo.m_gameID.AppID());
			Friend.bPlayingThisGame = OwnAppId != 0 && GameInfo.m_gameID.AppID() == OwnAppId;
		}
	}
#endif
}

FString UESteamFriendsSubsystem::GetFriendPersonaName(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendPersonaName(CSteamID(SteamId.Value))));
	}
#endif
	return FString();
}

EESteamPersonaState UESteamFriendsSubsystem::GetFriendPersonaState(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		return ESteamFriendsPrivate::ConvertPersonaState(SteamFriends()->GetFriendPersonaState(CSteamID(SteamId.Value)));
	}
#endif
	return EESteamPersonaState::Offline;
}

bool UESteamFriendsSubsystem::IsFriend(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	return IsSteamAvailable() && SteamFriends() && SteamId.IsValid()
		&& SteamFriends()->HasFriend(CSteamID(SteamId.Value), k_EFriendFlagImmediate);
#else
	return false;
#endif
}

int32 UESteamFriendsSubsystem::GetFriendSteamLevel(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		return SteamFriends()->GetFriendSteamLevel(CSteamID(SteamId.Value));
	}
#endif
	return 0;
}

UTexture2D* UESteamFriendsSubsystem::GetFriendAvatarTexture(FESteamId SteamId, EESteamAvatarSize Size)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !SteamUtils() || !SteamId.IsValid())
	{
		return nullptr;
	}

	const CSteamID Id(SteamId.Value);
	int ImageHandle = 0;
	switch (Size)
	{
	case EESteamAvatarSize::Small:  ImageHandle = SteamFriends()->GetSmallFriendAvatar(Id);  break;
	case EESteamAvatarSize::Medium: ImageHandle = SteamFriends()->GetMediumFriendAvatar(Id); break;
	case EESteamAvatarSize::Large:  ImageHandle = SteamFriends()->GetLargeFriendAvatar(Id);  break;
	}

	// 0 = no avatar set; -1 = not downloaded yet (AvatarImageLoaded_t fires when ready).
	if (ImageHandle <= 0)
	{
		return nullptr;
	}

	uint32 Width = 0;
	uint32 Height = 0;
	if (!SteamUtils()->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
	{
		return nullptr;
	}

	// Widen before multiplying: Width*Height*4 evaluated in uint32 could wrap for a corrupt or
	// pathologically large size, yielding a bogus (possibly negative) allocation.
	const int64 PixelBytes = static_cast<int64>(Width) * static_cast<int64>(Height) * 4;
	if (PixelBytes <= 0 || PixelBytes > MAX_int32)
	{
		return nullptr;
	}
	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(static_cast<int32>(PixelBytes));
	if (!SteamUtils()->GetImageRGBA(ImageHandle, Pixels.GetData(), Pixels.Num()))
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(static_cast<int32>(Width), static_cast<int32>(Height), PF_R8G8B8A8);
	if (!Texture)
	{
		return nullptr;
	}

	Texture->SRGB = true;
	void* MipData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(MipData, Pixels.GetData(), Pixels.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
#else
	return nullptr;
#endif
}

void UESteamFriendsSubsystem::ActivateGameOverlay(const FString& Dialog)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("ActivateGameOverlay"));
		return;
	}
	SteamFriends()->ActivateGameOverlay(TCHAR_TO_UTF8(*Dialog));
#else
	LogSteamUnavailable(TEXT("ActivateGameOverlay"));
#endif
}

void UESteamFriendsSubsystem::ActivateGameOverlayToUser(const FString& Dialog, FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("ActivateGameOverlayToUser"));
		return;
	}
	SteamFriends()->ActivateGameOverlayToUser(TCHAR_TO_UTF8(*Dialog), CSteamID(SteamId.Value));
#else
	LogSteamUnavailable(TEXT("ActivateGameOverlayToUser"));
#endif
}

void UESteamFriendsSubsystem::ActivateGameOverlayToWebPage(const FString& Url)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("ActivateGameOverlayToWebPage"));
		return;
	}
	SteamFriends()->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*Url));
#else
	LogSteamUnavailable(TEXT("ActivateGameOverlayToWebPage"));
#endif
}

void UESteamFriendsSubsystem::ActivateGameOverlayInviteDialog(const FString& ConnectString)
{
#if ESTEAM_SDK_AT_LEAST(152)
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("ActivateGameOverlayInviteDialog"));
		return;
	}
	SteamFriends()->ActivateGameOverlayInviteDialogConnectString(TCHAR_TO_UTF8(*ConnectString));
#else
	LogSteamUnavailable(TEXT("ActivateGameOverlayInviteDialog"));
#endif
}

bool UESteamFriendsSubsystem::SetRichPresence(const FString& Key, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("SetRichPresence"));
		return false;
	}
	return SteamFriends()->SetRichPresence(TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
#else
	return false;
#endif
}

void UESteamFriendsSubsystem::ClearRichPresence()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends())
	{
		SteamFriends()->ClearRichPresence();
	}
#endif
}

FString UESteamFriendsSubsystem::GetFriendRichPresence(FESteamId SteamId, const FString& Key) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendRichPresence(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*Key))));
	}
#endif
	return FString();
}

bool UESteamFriendsSubsystem::InviteUserToGame(FESteamId SteamId, const FString& ConnectString)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("InviteUserToGame"));
		return false;
	}
	if (!SteamId.IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("InviteUserToGame: invalid Steam id"));
		return false;
	}
	return SteamFriends()->InviteUserToGame(CSteamID(SteamId.Value), TCHAR_TO_UTF8(*ConnectString));
#else
	return false;
#endif
}


FString UESteamFriendsSubsystem::GetPlayerNickname(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		const char* Nickname = SteamFriends()->GetPlayerNickname(CSteamID(SteamId.Value));
		if (Nickname)
		{
			return FString(UTF8_TO_TCHAR(Nickname));
		}
	}
#endif
	return FString();
}

void UESteamFriendsSubsystem::SetPlayedWith(FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		SteamFriends()->SetPlayedWith(CSteamID(SteamId.Value));
	}
#endif
}

bool UESteamFriendsSubsystem::RequestUserInformation(FESteamId SteamId, bool bRequireNameOnly)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		// Returns true when data is still being fetched (watch OnPersonaStateChanged), false when
		// the data was already available locally.
		return SteamFriends()->RequestUserInformation(CSteamID(SteamId.Value), bRequireNameOnly);
	}
#endif
	return false;
}

void UESteamFriendsSubsystem::ActivateGameOverlayToStore(int32 AppId, EESteamOverlayToStoreFlag Flag)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends())
	{
		LogSteamUnavailable(TEXT("ActivateGameOverlayToStore"));
		return;
	}
	SteamFriends()->ActivateGameOverlayToStore(
		static_cast<AppId_t>(AppId), ESteamFriendsPrivate::ToSteamOverlayToStoreFlag(Flag));
#else
	LogSteamUnavailable(TEXT("ActivateGameOverlayToStore"));
#endif
}

void UESteamFriendsSubsystem::RequestFriendRichPresence(FESteamId SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		SteamFriends()->RequestFriendRichPresence(CSteamID(SteamId.Value));
	}
#endif
}

int32 UESteamFriendsSubsystem::GetFriendRichPresenceKeyCount(FESteamId SteamId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid())
	{
		return FMath::Max(0, SteamFriends()->GetFriendRichPresenceKeyCount(CSteamID(SteamId.Value)));
	}
#endif
	return 0;
}

FString UESteamFriendsSubsystem::GetFriendRichPresenceKeyByIndex(FESteamId SteamId, int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SteamId.IsValid() && Index >= 0)
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetFriendRichPresenceKeyByIndex(CSteamID(SteamId.Value), Index)));
	}
#endif
	return FString();
}

int32 UESteamFriendsSubsystem::GetClanCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends())
	{
		return FMath::Max(0, SteamFriends()->GetClanCount());
	}
#endif
	return 0;
}

FESteamId UESteamFriendsSubsystem::GetClanByIndex(int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && Index >= 0)
	{
		return FESteamId(SteamFriends()->GetClanByIndex(Index).ConvertToUint64());
	}
#endif
	return FESteamId();
}

FString UESteamFriendsSubsystem::GetClanName(FESteamId ClanId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetClanName(CSteamID(ClanId.Value))));
	}
#endif
	return FString();
}

FString UESteamFriendsSubsystem::GetClanTag(FESteamId ClanId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetClanTag(CSteamID(ClanId.Value))));
	}
#endif
	return FString();
}

bool UESteamFriendsSubsystem::GetClanActivityCounts(FESteamId ClanId, int32& OutOnline, int32& OutInGame, int32& OutChatting) const
{
	OutOnline = 0;
	OutInGame = 0;
	OutChatting = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid())
	{
		int Online = 0;
		int InGame = 0;
		int Chatting = 0;
		if (SteamFriends()->GetClanActivityCounts(CSteamID(ClanId.Value), &Online, &InGame, &Chatting))
		{
			OutOnline = Online;
			OutInGame = InGame;
			OutChatting = Chatting;
			return true;
		}
	}
#endif
	return false;
}

bool UESteamFriendsSubsystem::DownloadClanActivityCounts(FESteamId ClanId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("DownloadClanActivityCounts"));
		return false;
	}
	if (!ClanId.IsValid())
	{
		return false;
	}

	FESteamPendingClanActivityCounts Request;
	Request.Clan = ClanId;
	if (!Callbacks->EnqueueClanActivityCounts(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("DownloadClanActivityCounts failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("DownloadClanActivityCounts"));
	return false;
#endif
}

int32 UESteamFriendsSubsystem::GetFriendCountFromSource(FESteamId SourceId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SourceId.IsValid())
	{
		return FMath::Max(0, SteamFriends()->GetFriendCountFromSource(CSteamID(SourceId.Value)));
	}
#endif
	return 0;
}

FESteamId UESteamFriendsSubsystem::GetFriendFromSourceByIndex(FESteamId SourceId, int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && SourceId.IsValid() && Index >= 0)
	{
		return FESteamId(SteamFriends()->GetFriendFromSourceByIndex(CSteamID(SourceId.Value), Index).ConvertToUint64());
	}
#endif
	return FESteamId();
}

bool UESteamFriendsSubsystem::RequestClanOfficerList(FESteamId ClanId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestClanOfficerList"));
		return false;
	}
	if (!ClanId.IsValid())
	{
		return false;
	}

	FESteamPendingClanOfficerList Request;
	Request.Clan = ClanId;
	if (!Callbacks->EnqueueClanOfficerList(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestClanOfficerList failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("RequestClanOfficerList"));
	return false;
#endif
}

int32 UESteamFriendsSubsystem::GetClanOfficerCount(FESteamId ClanId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid())
	{
		return FMath::Max(0, SteamFriends()->GetClanOfficerCount(CSteamID(ClanId.Value)));
	}
#endif
	return 0;
}

FESteamId UESteamFriendsSubsystem::GetClanOfficerByIndex(FESteamId ClanId, int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid() && Index >= 0)
	{
		return FESteamId(SteamFriends()->GetClanOfficerByIndex(CSteamID(ClanId.Value), Index).ConvertToUint64());
	}
#endif
	return FESteamId();
}

FESteamId UESteamFriendsSubsystem::GetClanOwner(FESteamId ClanId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamFriends() && ClanId.IsValid())
	{
		return FESteamId(SteamFriends()->GetClanOwner(CSteamID(ClanId.Value)).ConvertToUint64());
	}
#endif
	return FESteamId();
}

bool UESteamFriendsSubsystem::GetFollowerCount(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("GetFollowerCount"));
		return false;
	}
	if (!User.IsValid())
	{
		return false;
	}

	FESteamPendingFollowerCount Request;
	Request.User = User;
	if (!Callbacks->EnqueueFollowerCount(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetFollowerCount failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("GetFollowerCount"));
	return false;
#endif
}

bool UESteamFriendsSubsystem::IsFollowing(FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("IsFollowing"));
		return false;
	}
	if (!User.IsValid())
	{
		return false;
	}

	FESteamPendingIsFollowing Request;
	Request.User = User;
	if (!Callbacks->EnqueueIsFollowing(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("IsFollowing failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("IsFollowing"));
	return false;
#endif
}

bool UESteamFriendsSubsystem::EnumerateFollowingList(int32 StartIndex)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamFriends() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("EnumerateFollowingList"));
		return false;
	}

	FESteamPendingEnumerateFollowing Request;
	Request.StartIndex = FMath::Max(0, StartIndex);
	if (!Callbacks->EnqueueEnumerateFollowing(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("EnumerateFollowingList failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("EnumerateFollowingList"));
	return false;
#endif
}

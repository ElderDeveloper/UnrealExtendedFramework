// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Matchmaking/ESteamMatchmakingSubsystem.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamSDK.h"
#include "Containers/Queue.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	ELobbyType ToSteamLobbyType(EESteamLobbyType LobbyType)
	{
		switch (LobbyType)
		{
		case EESteamLobbyType::FriendsOnly: return k_ELobbyTypeFriendsOnly;
		case EESteamLobbyType::Public:      return k_ELobbyTypePublic;
		case EESteamLobbyType::Invisible:   return k_ELobbyTypeInvisible;
		default:                            return k_ELobbyTypePrivate;
		}
	}

	ELobbyComparison ToSteamLobbyComparison(EESteamLobbyComparison Comparison)
	{
		switch (Comparison)
		{
		case EESteamLobbyComparison::EqualToOrLessThan:    return k_ELobbyComparisonEqualToOrLessThan;
		case EESteamLobbyComparison::LessThan:             return k_ELobbyComparisonLessThan;
		case EESteamLobbyComparison::GreaterThan:          return k_ELobbyComparisonGreaterThan;
		case EESteamLobbyComparison::EqualToOrGreaterThan: return k_ELobbyComparisonEqualToOrGreaterThan;
		case EESteamLobbyComparison::NotEqual:             return k_ELobbyComparisonNotEqual;
		default:                                           return k_ELobbyComparisonEqual;
		}
	}

	ELobbyDistanceFilter ToSteamLobbyDistanceFilter(EESteamLobbyDistanceFilter DistanceFilter)
	{
		switch (DistanceFilter)
		{
		case EESteamLobbyDistanceFilter::Close:     return k_ELobbyDistanceFilterClose;
		case EESteamLobbyDistanceFilter::Far:       return k_ELobbyDistanceFilterFar;
		case EESteamLobbyDistanceFilter::Worldwide: return k_ELobbyDistanceFilterWorldwide;
		default:                                    return k_ELobbyDistanceFilterDefault;
		}
	}

	/** Collapses the EChatMemberStateChange bitmask to its single highest-priority set bit. */
	EESteamChatMemberStateChange ToChatMemberStateChange(const uint32 StateChangeMask)
	{
		if (StateChangeMask & k_EChatMemberStateChangeBanned)       { return EESteamChatMemberStateChange::Banned; }
		if (StateChangeMask & k_EChatMemberStateChangeKicked)       { return EESteamChatMemberStateChange::Kicked; }
		if (StateChangeMask & k_EChatMemberStateChangeDisconnected) { return EESteamChatMemberStateChange::Disconnected; }
		if (StateChangeMask & k_EChatMemberStateChangeLeft)         { return EESteamChatMemberStateChange::Left; }
		return EESteamChatMemberStateChange::Entered;
	}

	/** Parses "a.b.c.d" into a host-byte-order uint32 (the format ISteamMatchmaking expects). */
	bool ParseIPv4(const FString& Ip, uint32& OutHostOrderIp)
	{
		TArray<FString> Octets;
		Ip.ParseIntoArray(Octets, TEXT("."), /*bCullEmpty*/ false);
		if (Octets.Num() != 4)
		{
			return false;
		}

		uint32 Result = 0;
		for (const FString& Octet : Octets)
		{
			if (Octet.IsEmpty() || !Octet.IsNumeric())
			{
				return false;
			}
			const int32 Value = FCString::Atoi(*Octet);
			if (Value < 0 || Value > 255)
			{
				return false;
			}
			Result = (Result << 8) | static_cast<uint32>(Value);
		}

		OutHostOrderIp = Result;
		return true;
	}

	/** Formats a host-byte-order uint32 as "a.b.c.d" (empty when the address is 0). */
	FString HostOrderIpToString(const uint32 HostOrderIp)
	{
		if (HostOrderIp == 0)
		{
			return FString();
		}
		return FString::Printf(TEXT("%u.%u.%u.%u"),
			(HostOrderIp >> 24) & 0xFF, (HostOrderIp >> 16) & 0xFF, (HostOrderIp >> 8) & 0xFF, HostOrderIp & 0xFF);
	}
}

/** Parameters captured to (re-)issue a queued ISteamMatchmaking::CreateLobby call. */
struct FESteamPendingCreateLobby
{
	ELobbyType LobbyType = k_ELobbyTypePrivate;
	int32 MaxMembers = 0;
};

/** Parameters captured to (re-)issue a queued ISteamMatchmaking::JoinLobby call. */
struct FESteamPendingJoinLobby
{
	FESteamId LobbyId;
};

/**
 * Native Steam callback listeners; alive only while the Steam client API is initialized.
 *
 * Same-type async requests are serialized via a per-operation FIFO queue: while a given op's
 * CCallResult is in flight, further requests of that type are enqueued and issued in order as
 * each completion arrives, so none are dropped. Queued-but-unissued requests are abandoned
 * cleanly when the holder is destroyed on Steam shutdown / Deinitialize.
 *
 * RequestLobbyList carries no per-request parameters here (its search filters live on the
 * Steam side, see the header note), so its "queue" is just a count of pending intents.
 */
class FESteamMatchmakingCallbacks
{
public:
	explicit FESteamMatchmakingCallbacks(UESteamMatchmakingSubsystem* InOwner)
		: Owner(InOwner)
		, LobbyChatMsgCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyChatMsg)
		, LobbyDataUpdateCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyDataUpdate)
		, LobbyChatUpdateCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyChatUpdate)
		, LobbyInviteCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyInvite)
		, LobbyGameCreatedCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyGameCreated)
		, LobbyKickedCallback(this, &FESteamMatchmakingCallbacks::HandleLobbyKicked)
	{
	}

	// Each Enqueue* issues the Steam call immediately when its operation is idle, otherwise it
	// queues the request. Returns true when the request was issued or queued; false only on the
	// immediate path when the Steam call could not be issued (preserves the public methods'
	// historical "return false when the request could not be issued" contract).

	bool EnqueueCreateLobby(const FESteamPendingCreateLobby& Request)
	{
		if (bCreateLobbyBusy)
		{
			CreateLobbyQueue.Enqueue(Request);
			return true;
		}
		return IssueCreateLobby(Request);
	}

	bool EnqueueJoinLobby(const FESteamPendingJoinLobby& Request)
	{
		if (bJoinLobbyBusy)
		{
			JoinLobbyQueue.Enqueue(Request);
			return true;
		}
		return IssueJoinLobby(Request);
	}

	bool EnqueueLobbyList()
	{
		if (bLobbyListBusy)
		{
			++PendingLobbyListRequestCount;
			return true;
		}
		return IssueLobbyList();
	}

private:
	// ---- CreateLobby (serialized) ----

	bool IssueCreateLobby(const FESteamPendingCreateLobby& Request)
	{
		UESteamMatchmakingSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamMatchmaking())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamMatchmaking()->CreateLobby(Request.LobbyType, Request.MaxMembers);
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		CreateLobbyResult.Set(Call, this, &FESteamMatchmakingCallbacks::HandleCreateLobbyResult);
		bCreateLobbyBusy = true;
		return true;
	}

	void HandleCreateLobbyResult(LobbyCreated_t* Data, bool bIOFailure)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_eResult == k_EResultOK;
			Subsystem->OnLobbyCreated.Broadcast(bSuccess, FESteamId(bSuccess ? Data->m_ulSteamIDLobby : 0));
		}
		bCreateLobbyBusy = false;
		DrainCreateLobbyQueue();
	}

	void DrainCreateLobbyQueue()
	{
		while (!bCreateLobbyBusy)
		{
			FESteamPendingCreateLobby Request;
			if (!CreateLobbyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueCreateLobby(Request))
			{
				// Steam went away while draining: fail this queued request instead of dropping it.
				if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnLobbyCreated.Broadcast(false, FESteamId(0));
				}
			}
		}
	}

	// ---- JoinLobby (serialized) ----

	bool IssueJoinLobby(const FESteamPendingJoinLobby& Request)
	{
		UESteamMatchmakingSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamMatchmaking())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamMatchmaking()->JoinLobby(CSteamID(Request.LobbyId.Value));
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		JoinLobbyResult.Set(Call, this, &FESteamMatchmakingCallbacks::HandleJoinLobbyResult);
		bJoinLobbyBusy = true;
		return true;
	}

	void HandleJoinLobbyResult(LobbyEnter_t* Data, bool bIOFailure)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			const bool bSuccess = !bIOFailure && Data->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess;
			Subsystem->OnLobbyEntered.Broadcast(bSuccess, FESteamId(Data->m_ulSteamIDLobby));
		}
		bJoinLobbyBusy = false;
		DrainJoinLobbyQueue();
	}

	void DrainJoinLobbyQueue()
	{
		while (!bJoinLobbyBusy)
		{
			FESteamPendingJoinLobby Request;
			if (!JoinLobbyQueue.Dequeue(Request))
			{
				break;
			}
			if (!IssueJoinLobby(Request))
			{
				if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
				{
					Subsystem->OnLobbyEntered.Broadcast(false, Request.LobbyId);
				}
			}
		}
	}

	// ---- RequestLobbyList (serialized) ----

	bool IssueLobbyList()
	{
		UESteamMatchmakingSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsSteamAvailable() || !SteamMatchmaking())
		{
			return false;
		}

		const SteamAPICall_t Call = SteamMatchmaking()->RequestLobbyList();
		if (Call == k_uAPICallInvalid)
		{
			return false;
		}
		LobbyListResult.Set(Call, this, &FESteamMatchmakingCallbacks::HandleLobbyListResult);
		bLobbyListBusy = true;
		return true;
	}

	void HandleLobbyListResult(LobbyMatchList_t* Data, bool bIOFailure)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			TArray<FESteamId> Lobbies;
			const bool bSuccess = !bIOFailure && SteamMatchmaking() != nullptr;
			if (bSuccess)
			{
				const int32 LobbyCount = static_cast<int32>(Data->m_nLobbiesMatching);
				Lobbies.Reserve(LobbyCount);
				for (int32 Index = 0; Index < LobbyCount; ++Index)
				{
					const CSteamID LobbyId = SteamMatchmaking()->GetLobbyByIndex(Index);
					if (LobbyId.IsValid())
					{
						Lobbies.Add(FESteamId(LobbyId.ConvertToUint64()));
					}
				}
			}
			Subsystem->OnLobbyListReceived.Broadcast(bSuccess, Lobbies);
		}
		bLobbyListBusy = false;
		DrainLobbyListQueue();
	}

	void DrainLobbyListQueue()
	{
		while (!bLobbyListBusy && PendingLobbyListRequestCount > 0)
		{
			--PendingLobbyListRequestCount;
			if (!IssueLobbyList())
			{
				if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
				{
					const TArray<FESteamId> EmptyLobbies;
					Subsystem->OnLobbyListReceived.Broadcast(false, EmptyLobbies);
				}
			}
		}
	}

	// ---- Continuously-listening callbacks (not request/response) ----

	void HandleLobbyChatMsg(LobbyChatMsg_t* Data)
	{
		UESteamMatchmakingSubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !SteamMatchmaking())
		{
			return;
		}

		// Chat messages are capped at 4k bytes; keep one byte spare so the buffer is
		// always null-terminated even when the sender omitted the terminator.
		char Buffer[4096 + 1] = {};
		CSteamID SenderId;
		EChatEntryType EntryType = k_EChatEntryTypeInvalid;
		SteamMatchmaking()->GetLobbyChatEntry(
			CSteamID(Data->m_ulSteamIDLobby), static_cast<int>(Data->m_iChatID),
			&SenderId, Buffer, sizeof(Buffer) - 1, &EntryType);

		Subsystem->OnLobbyChatMessage.Broadcast(
			FESteamId(Data->m_ulSteamIDLobby),
			FESteamId(SenderId.ConvertToUint64()),
			FString(UTF8_TO_TCHAR(Buffer)));
	}

	void HandleLobbyDataUpdate(LobbyDataUpdate_t* Data)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyDataUpdated.Broadcast(
				FESteamId(Data->m_ulSteamIDLobby),
				FESteamId(Data->m_ulSteamIDMember),
				Data->m_bSuccess != 0);
		}
	}

	void HandleLobbyChatUpdate(LobbyChatUpdate_t* Data)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyMemberStateChanged.Broadcast(
				FESteamId(Data->m_ulSteamIDLobby),
				FESteamId(Data->m_ulSteamIDUserChanged),
				(Data->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered) != 0,
				ToChatMemberStateChange(Data->m_rgfChatMemberStateChange));
		}
	}

	void HandleLobbyInvite(LobbyInvite_t* Data)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyInviteReceived.Broadcast(
				FESteamId(Data->m_ulSteamIDUser),
				FESteamId(Data->m_ulSteamIDLobby));
		}
	}

	void HandleLobbyGameCreated(LobbyGameCreated_t* Data)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyGameCreated.Broadcast(
				FESteamId(Data->m_ulSteamIDLobby),
				FESteamId(Data->m_ulSteamIDGameServer),
				HostOrderIpToString(Data->m_unIP),
				static_cast<int32>(Data->m_usPort));
		}
	}

	void HandleLobbyKicked(LobbyKicked_t* Data)
	{
		if (UESteamMatchmakingSubsystem* Subsystem = Owner.Get())
		{
			Subsystem->OnLobbyKicked.Broadcast(
				FESteamId(Data->m_ulSteamIDLobby),
				FESteamId(Data->m_ulSteamIDAdmin),
				Data->m_bKickedDueToDisconnect != 0);
		}
	}

	TWeakObjectPtr<UESteamMatchmakingSubsystem> Owner;

	CCallback<FESteamMatchmakingCallbacks, LobbyChatMsg_t> LobbyChatMsgCallback;
	CCallback<FESteamMatchmakingCallbacks, LobbyDataUpdate_t> LobbyDataUpdateCallback;
	CCallback<FESteamMatchmakingCallbacks, LobbyChatUpdate_t> LobbyChatUpdateCallback;
	CCallback<FESteamMatchmakingCallbacks, LobbyInvite_t> LobbyInviteCallback;
	CCallback<FESteamMatchmakingCallbacks, LobbyGameCreated_t> LobbyGameCreatedCallback;
	CCallback<FESteamMatchmakingCallbacks, LobbyKicked_t> LobbyKickedCallback;

	CCallResult<FESteamMatchmakingCallbacks, LobbyCreated_t> CreateLobbyResult;
	CCallResult<FESteamMatchmakingCallbacks, LobbyEnter_t> JoinLobbyResult;
	CCallResult<FESteamMatchmakingCallbacks, LobbyMatchList_t> LobbyListResult;

	// In-flight flags + FIFO queues, one per serialized operation type. RequestLobbyList has no
	// per-request parameters, so it uses a pending-intent counter instead of a queue.
	bool bCreateLobbyBusy = false;
	bool bJoinLobbyBusy = false;
	bool bLobbyListBusy = false;

	TQueue<FESteamPendingCreateLobby> CreateLobbyQueue;
	TQueue<FESteamPendingJoinLobby> JoinLobbyQueue;
	int32 PendingLobbyListRequestCount = 0;
};
#else
class FESteamMatchmakingCallbacks
{
};
#endif // WITH_EXTENDEDSTEAM_SDK

void UESteamMatchmakingSubsystem::Deinitialize()
{
	Super::Deinitialize();
	Callbacks.Reset();
}

void UESteamMatchmakingSubsystem::HandleSteamClientInitialized()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!Callbacks)
	{
		Callbacks = MakeShared<FESteamMatchmakingCallbacks>(this);
	}
#endif
}

void UESteamMatchmakingSubsystem::HandleSteamClientShutdown()
{
	Callbacks.Reset();
}

bool UESteamMatchmakingSubsystem::CreateLobby(EESteamLobbyType LobbyType, int32 MaxMembers)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("CreateLobby"));
		return false;
	}

	FESteamPendingCreateLobby Request;
	Request.LobbyType = ToSteamLobbyType(LobbyType);
	Request.MaxMembers = MaxMembers;
	if (!Callbacks->EnqueueCreateLobby(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("CreateLobby failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("CreateLobby"));
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::JoinLobby(FESteamId LobbyId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("JoinLobby"));
		return false;
	}
	if (!LobbyId.IsValid())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinLobby called with an invalid lobby id"));
		return false;
	}

	FESteamPendingJoinLobby Request;
	Request.LobbyId = LobbyId;
	if (!Callbacks->EnqueueJoinLobby(Request))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("JoinLobby failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("JoinLobby"));
	return false;
#endif
}

void UESteamMatchmakingSubsystem::LeaveLobby(FESteamId LobbyId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		SteamMatchmaking()->LeaveLobby(CSteamID(LobbyId.Value));
	}
#endif
}

bool UESteamMatchmakingSubsystem::InviteUserToLobby(FESteamId LobbyId, FESteamId User)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("InviteUserToLobby"));
		return false;
	}
	if (!LobbyId.IsValid() || !User.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->InviteUserToLobby(CSteamID(LobbyId.Value), CSteamID(User.Value));
#else
	return false;
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListStringFilter(const FString& Key, const FString& Value, EESteamLobbyComparison Comparison)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		SteamMatchmaking()->AddRequestLobbyListStringFilter(
			TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value), ToSteamLobbyComparison(Comparison));
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListNumericalFilter(const FString& Key, int32 Value, EESteamLobbyComparison Comparison)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		SteamMatchmaking()->AddRequestLobbyListNumericalFilter(
			TCHAR_TO_UTF8(*Key), Value, ToSteamLobbyComparison(Comparison));
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListDistanceFilter(EESteamLobbyDistanceFilter DistanceFilter)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		SteamMatchmaking()->AddRequestLobbyListDistanceFilter(ToSteamLobbyDistanceFilter(DistanceFilter));
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListResultCountFilter(int32 MaxResults)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		SteamMatchmaking()->AddRequestLobbyListResultCountFilter(MaxResults);
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListNearValueFilter(const FString& Key, int32 Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && !Key.IsEmpty())
	{
		SteamMatchmaking()->AddRequestLobbyListNearValueFilter(TCHAR_TO_UTF8(*Key), Value);
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListFilterSlotsAvailable(int32 SlotsAvailable)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		SteamMatchmaking()->AddRequestLobbyListFilterSlotsAvailable(SlotsAvailable);
	}
#endif
}

void UESteamMatchmakingSubsystem::AddRequestLobbyListCompatibleMembersFilter(FESteamId LobbyId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		SteamMatchmaking()->AddRequestLobbyListCompatibleMembersFilter(CSteamID(LobbyId.Value));
	}
#endif
}

bool UESteamMatchmakingSubsystem::RequestLobbyList()
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !Callbacks)
	{
		LogSteamUnavailable(TEXT("RequestLobbyList"));
		return false;
	}

	if (!Callbacks->EnqueueLobbyList())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RequestLobbyList failed to issue the request"));
		return false;
	}
	return true;
#else
	LogSteamUnavailable(TEXT("RequestLobbyList"));
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::RequestLobbyData(FESteamId LobbyId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("RequestLobbyData"));
		return false;
	}
	if (!LobbyId.IsValid())
	{
		return false;
	}
	// Returns false when the data was already available locally (no update will be posted).
	return SteamMatchmaking()->RequestLobbyData(CSteamID(LobbyId.Value));
#else
	LogSteamUnavailable(TEXT("RequestLobbyData"));
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::SetLobbyData(FESteamId LobbyId, const FString& Key, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("SetLobbyData"));
		return false;
	}
	if (!LobbyId.IsValid() || Key.IsEmpty())
	{
		return false;
	}
	return SteamMatchmaking()->SetLobbyData(CSteamID(LobbyId.Value), TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
#else
	return false;
#endif
}

FString UESteamMatchmakingSubsystem::GetLobbyData(FESteamId LobbyId, const FString& Key) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyData(CSteamID(LobbyId.Value), TCHAR_TO_UTF8(*Key))));
	}
#endif
	return FString();
}

bool UESteamMatchmakingSubsystem::GetAllLobbyData(FESteamId LobbyId, TMap<FString, FString>& OutData) const
{
	OutData.Reset();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		return false;
	}

	const CSteamID SteamLobbyId(LobbyId.Value);
	const int32 DataCount = SteamMatchmaking()->GetLobbyDataCount(SteamLobbyId);
	OutData.Reserve(DataCount);
	for (int32 Index = 0; Index < DataCount; ++Index)
	{
		char Key[k_nMaxLobbyKeyLength + 1] = {};
		char Value[k_cubChatMetadataMax + 1] = {};
		if (SteamMatchmaking()->GetLobbyDataByIndex(SteamLobbyId, Index, Key, sizeof(Key), Value, sizeof(Value)))
		{
			OutData.Add(FString(UTF8_TO_TCHAR(Key)), FString(UTF8_TO_TCHAR(Value)));
		}
	}
	return true;
#else
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::DeleteLobbyData(FESteamId LobbyId, const FString& Key)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid() || Key.IsEmpty())
	{
		return false;
	}
	return SteamMatchmaking()->DeleteLobbyData(CSteamID(LobbyId.Value), TCHAR_TO_UTF8(*Key));
#else
	return false;
#endif
}

void UESteamMatchmakingSubsystem::SetLobbyMemberData(FESteamId LobbyId, const FString& Key, const FString& Value)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid() && !Key.IsEmpty())
	{
		SteamMatchmaking()->SetLobbyMemberData(CSteamID(LobbyId.Value), TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
	}
#endif
}

FString UESteamMatchmakingSubsystem::GetLobbyMemberData(FESteamId LobbyId, FESteamId User, const FString& Key) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid() && User.IsValid())
	{
		return FString(UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyMemberData(
			CSteamID(LobbyId.Value), CSteamID(User.Value), TCHAR_TO_UTF8(*Key))));
	}
#endif
	return FString();
}

int32 UESteamMatchmakingSubsystem::GetNumLobbyMembers(FESteamId LobbyId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		return SteamMatchmaking()->GetNumLobbyMembers(CSteamID(LobbyId.Value));
	}
#endif
	return 0;
}

FESteamId UESteamMatchmakingSubsystem::GetLobbyMemberByIndex(FESteamId LobbyId, int32 Index) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		return FESteamId(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(LobbyId.Value), Index).ConvertToUint64());
	}
#endif
	return FESteamId();
}

FESteamId UESteamMatchmakingSubsystem::GetLobbyOwner(FESteamId LobbyId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		return FESteamId(SteamMatchmaking()->GetLobbyOwner(CSteamID(LobbyId.Value)).ConvertToUint64());
	}
#endif
	return FESteamId();
}

bool UESteamMatchmakingSubsystem::SetLobbyOwner(FESteamId LobbyId, FESteamId NewOwner)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid() || !NewOwner.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->SetLobbyOwner(CSteamID(LobbyId.Value), CSteamID(NewOwner.Value));
#else
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::SetLobbyType(FESteamId LobbyId, EESteamLobbyType LobbyType)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->SetLobbyType(CSteamID(LobbyId.Value), ToSteamLobbyType(LobbyType));
#else
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::SetLobbyJoinable(FESteamId LobbyId, bool bJoinable)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->SetLobbyJoinable(CSteamID(LobbyId.Value), bJoinable);
#else
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::SetLobbyMemberLimit(FESteamId LobbyId, int32 MaxMembers)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->SetLobbyMemberLimit(CSteamID(LobbyId.Value), MaxMembers);
#else
	return false;
#endif
}

int32 UESteamMatchmakingSubsystem::GetLobbyMemberLimit(FESteamId LobbyId) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking() && LobbyId.IsValid())
	{
		return SteamMatchmaking()->GetLobbyMemberLimit(CSteamID(LobbyId.Value));
	}
#endif
	return 0;
}

bool UESteamMatchmakingSubsystem::SendLobbyChatMessage(FESteamId LobbyId, const FString& Message)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("SendLobbyChatMessage"));
		return false;
	}
	if (!LobbyId.IsValid())
	{
		return false;
	}

	// SendLobbyChatMsg expects the null terminator included for text payloads.
	const FTCHARToUTF8 Utf8Message(*Message);
	return SteamMatchmaking()->SendLobbyChatMsg(CSteamID(LobbyId.Value), Utf8Message.Get(), Utf8Message.Length() + 1);
#else
	return false;
#endif
}

void UESteamMatchmakingSubsystem::SetLobbyGameServer(FESteamId LobbyId, const FString& ServerIp, int32 ServerPort, FESteamId ServerSteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		LogSteamUnavailable(TEXT("SetLobbyGameServer"));
		return;
	}

	uint32 HostOrderIp = 0;
	if (!ServerIp.IsEmpty() && !ParseIPv4(ServerIp, HostOrderIp))
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("SetLobbyGameServer: invalid address %s"), *ServerIp);
		return;
	}
	const uint16 Port = (ServerPort > 0 && ServerPort <= 65535) ? static_cast<uint16>(ServerPort) : 0;
	SteamMatchmaking()->SetLobbyGameServer(CSteamID(LobbyId.Value), HostOrderIp, Port, CSteamID(ServerSteamId.Value));
#else
	LogSteamUnavailable(TEXT("SetLobbyGameServer"));
#endif
}

bool UESteamMatchmakingSubsystem::GetLobbyGameServer(FESteamId LobbyId, FString& OutIp, int32& OutPort, FESteamId& OutServerSteamId) const
{
	OutIp.Reset();
	OutPort = 0;
	OutServerSteamId = FESteamId();
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid())
	{
		return false;
	}

	uint32 HostOrderIp = 0;
	uint16 Port = 0;
	CSteamID ServerId;
	if (!SteamMatchmaking()->GetLobbyGameServer(CSteamID(LobbyId.Value), &HostOrderIp, &Port, &ServerId))
	{
		return false;
	}

	OutIp = HostOrderIpToString(HostOrderIp);
	OutPort = static_cast<int32>(Port);
	OutServerSteamId = FESteamId(ServerId.ConvertToUint64());
	return true;
#else
	return false;
#endif
}

bool UESteamMatchmakingSubsystem::SetLinkedLobby(FESteamId LobbyId, FESteamId LobbyDependent)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || !LobbyId.IsValid() || !LobbyDependent.IsValid())
	{
		return false;
	}
	return SteamMatchmaking()->SetLinkedLobby(CSteamID(LobbyId.Value), CSteamID(LobbyDependent.Value));
#else
	return false;
#endif
}

int32 UESteamMatchmakingSubsystem::GetFavoriteGameCount() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailable() && SteamMatchmaking())
	{
		return FMath::Max(0, SteamMatchmaking()->GetFavoriteGameCount());
	}
#endif
	return 0;
}

bool UESteamMatchmakingSubsystem::GetFavoriteGame(int32 Index, int32& OutAppId, FString& OutIp, int32& OutConnPort, int32& OutQueryPort, int32& OutFlags, int32& OutLastPlayed) const
{
	OutAppId = 0;
	OutIp.Reset();
	OutConnPort = 0;
	OutQueryPort = 0;
	OutFlags = 0;
	OutLastPlayed = 0;
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking() || Index < 0)
	{
		return false;
	}

	AppId_t AppId = 0;
	uint32 HostOrderIp = 0;
	uint16 ConnPort = 0;
	uint16 QueryPort = 0;
	uint32 Flags = 0;
	uint32 LastPlayed = 0;
	if (!SteamMatchmaking()->GetFavoriteGame(Index, &AppId, &HostOrderIp, &ConnPort, &QueryPort, &Flags, &LastPlayed))
	{
		return false;
	}

	OutAppId = static_cast<int32>(AppId);
	OutIp = HostOrderIpToString(HostOrderIp);
	OutConnPort = static_cast<int32>(ConnPort);
	OutQueryPort = static_cast<int32>(QueryPort);
	OutFlags = static_cast<int32>(Flags);
	OutLastPlayed = static_cast<int32>(LastPlayed);
	return true;
#else
	return false;
#endif
}

int32 UESteamMatchmakingSubsystem::AddFavoriteGame(int32 AppId, const FString& Ip, int32 ConnPort, int32 QueryPort, int32 Flags, int32 LastPlayed)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("AddFavoriteGame"));
		return -1;
	}

	uint32 HostOrderIp = 0;
	if (!ParseIPv4(Ip, HostOrderIp) || ConnPort <= 0 || ConnPort > 65535 || QueryPort <= 0 || QueryPort > 65535)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("AddFavoriteGame: invalid address %s:%d (query %d)"), *Ip, ConnPort, QueryPort);
		return -1;
	}

	return SteamMatchmaking()->AddFavoriteGame(
		static_cast<AppId_t>(AppId), HostOrderIp,
		static_cast<uint16>(ConnPort), static_cast<uint16>(QueryPort),
		static_cast<uint32>(Flags), static_cast<uint32>(LastPlayed));
#else
	LogSteamUnavailable(TEXT("AddFavoriteGame"));
	return -1;
#endif
}

bool UESteamMatchmakingSubsystem::RemoveFavoriteGame(int32 AppId, const FString& Ip, int32 ConnPort, int32 QueryPort, int32 Flags)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (!IsSteamAvailable() || !SteamMatchmaking())
	{
		LogSteamUnavailable(TEXT("RemoveFavoriteGame"));
		return false;
	}

	uint32 HostOrderIp = 0;
	if (!ParseIPv4(Ip, HostOrderIp) || ConnPort <= 0 || ConnPort > 65535 || QueryPort <= 0 || QueryPort > 65535)
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("RemoveFavoriteGame: invalid address %s:%d (query %d)"), *Ip, ConnPort, QueryPort);
		return false;
	}

	return SteamMatchmaking()->RemoveFavoriteGame(
		static_cast<AppId_t>(AppId), HostOrderIp,
		static_cast<uint16>(ConnPort), static_cast<uint16>(QueryPort), static_cast<uint32>(Flags));
#else
	LogSteamUnavailable(TEXT("RemoveFavoriteGame"));
	return false;
#endif
}

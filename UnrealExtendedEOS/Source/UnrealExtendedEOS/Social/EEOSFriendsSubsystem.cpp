// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSFriendsSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "UnrealExtendedEOS.h"

void UEEOSFriendsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSFriendsSubsystem::Deinitialize()
{
	CachedFriends.Empty();
	Super::Deinitialize();
}

bool UEEOSFriendsSubsystem::ReadFriendsList()
{
	// Every path must terminate observably: there is no failure-capable delegate on this
	// subsystem, so failures broadcast OnFriendsListReady with an empty list (never a silent
	// return — async waiters would hang). OnFriendsListChanged is NOT fired on failure.
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadFriendsList"));
		OnFriendsListReady.Broadcast(TArray<FEEOSFriendInfo>());
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSFriendsSubsystem::ReadFriendsList — Friends interface not available"));
		OnFriendsListReady.Broadcast(TArray<FEEOSFriendInfo>());
		return false;
	}

	// The engine executes the per-call delegate on its synchronous failure path too
	// (UserManagerEOS.cpp:2742-2749), so HandleReadFriendsListComplete fires exactly once
	// whether this returns true or false.
	const bool bStarted = FriendsInterface->ReadFriendsList(0, TEXT("default"),
		FOnReadFriendsListComplete::CreateUObject(this, &UEEOSFriendsSubsystem::HandleReadFriendsListComplete));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::ReadFriendsList — Reading friends list... (%s)"),
		bStarted ? TEXT("started") : TEXT("failed synchronously"));
	return bStarted;
}

bool UEEOSFriendsSubsystem::SendFriendInvite(const FString& FriendUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendFriendInvite"));
		OnFriendInviteSent.Broadcast(false, FriendUserId);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSFriendsSubsystem::SendFriendInvite — Friends interface not available"));
		OnFriendInviteSent.Broadcast(false, FriendUserId);
		return false;
	}

	// Must be constructed by the identity interface: the EOS OSS downcasts incoming ids to
	// FUniqueNetIdEOS, so a generic FUniqueNetIdString here is undefined behavior.
	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too.
	const FUniqueNetIdPtr NetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(FriendUserId);
	if (!NetId.IsValid() || !NetId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem::SendFriendInvite — Could not parse user id '%s'"), *FriendUserId);
		OnFriendInviteSent.Broadcast(false, FriendUserId);
		return false;
	}

	// Broadcast from the engine's per-call completion — the REAL backend result — instead of
	// the synchronous started-bool (which reported success for invites the backend then
	// rejected). The engine executes this delegate exactly once on every path: both
	// synchronous failures and the async EOS_Friends_SendInvite completion
	// (UserManagerEOS.cpp:2927-2968).
	const bool bStarted = FriendsInterface->SendInvite(0, *NetId, TEXT("default"),
		FOnSendInviteComplete::CreateWeakLambda(this,
			[this, FriendUserId](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& FriendId, const FString& ListName, const FString& ErrorStr)
			{
				if (bWasSuccessful)
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem: Friend invite to %s sent"), *FriendUserId);
				}
				else
				{
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem: Friend invite to %s failed — %s"), *FriendUserId, *ErrorStr);
				}
				OnFriendInviteSent.Broadcast(bWasSuccessful, FriendUserId);
			}));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::SendFriendInvite — Invite to %s %s"),
		*FriendUserId, bStarted ? TEXT("started") : TEXT("failed synchronously"));
	return bStarted;
}

bool UEEOSFriendsSubsystem::AcceptFriendInvite(const FString& FriendUserId)
{
	// No completion delegate exists for accept/reject — failures must at least be loud
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptFriendInvite"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSFriendsSubsystem::AcceptFriendInvite — Friends interface not available, invite from '%s' NOT accepted"), *FriendUserId);
		return false;
	}

	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
	const FUniqueNetIdPtr NetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(FriendUserId);
	if (!NetId.IsValid() || !NetId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem::AcceptFriendInvite — Could not parse user id '%s', invite NOT accepted"), *FriendUserId);
		return false;
	}

	const bool bStarted = FriendsInterface->AcceptInvite(0, *NetId, TEXT("default"));
	if (bStarted)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::AcceptFriendInvite — Accepting invite from %s"), *FriendUserId);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem::AcceptFriendInvite — Engine refused the accept for %s (unknown/invalid friend id?)"), *FriendUserId);
	}
	return bStarted;
}

bool UEEOSFriendsSubsystem::RejectFriendInvite(const FString& FriendUserId)
{
	// No completion delegate exists for accept/reject — failures must at least be loud
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RejectFriendInvite"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSFriendsSubsystem::RejectFriendInvite — Friends interface not available, invite from '%s' NOT rejected"), *FriendUserId);
		return false;
	}

	// CreateUniquePlayerId on the EOS OSS returns the non-null registry EmptyId on parse
	// failure — Ptr.IsValid() alone is not a validity check, ask the id itself too
	const FUniqueNetIdPtr NetId = EOSSub->GetIdentityInterface()->CreateUniquePlayerId(FriendUserId);
	if (!NetId.IsValid() || !NetId->IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem::RejectFriendInvite — Could not parse user id '%s', invite NOT rejected"), *FriendUserId);
		return false;
	}

	const bool bStarted = FriendsInterface->RejectInvite(0, *NetId, TEXT("default"));
	if (bStarted)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::RejectFriendInvite — Rejecting invite from %s"), *FriendUserId);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem::RejectFriendInvite — Engine refused the reject for %s (unknown/invalid friend id?)"), *FriendUserId);
	}
	return bStarted;
}

TArray<FEEOSFriendInfo> UEEOSFriendsSubsystem::GetFriendsList() const
{
	return CachedFriends;
}

int32 UEEOSFriendsSubsystem::GetOnlineFriendCount() const
{
	int32 Count = 0;
	for (const auto& Friend : CachedFriends)
	{
		if (Friend.bIsOnline) Count++;
	}
	return Count;
}

void UEEOSFriendsSubsystem::HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	CachedFriends.Empty();

	if (bWasSuccessful)
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
			if (FriendsInterface.IsValid())
			{
				TArray<TSharedRef<FOnlineFriend>> Friends;
				FriendsInterface->GetFriendsList(0, ListName, Friends);

				for (const auto& Friend : Friends)
				{
					FEEOSFriendInfo Info;
					Info.UserId = Friend->GetUserId()->ToString();
					Info.DisplayName = Friend->GetDisplayName();
					const FOnlineUserPresence& Presence = Friend->GetPresence();
					Info.PresenceStatus = Presence.Status.StatusStr;
					Info.bIsOnline = Presence.bIsOnline;
					Info.bIsPlayingThisGame = Presence.bIsPlayingThisGame;

					CachedFriends.Add(Info);
				}
			}
		}
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem: Friends list read — %d friends"), CachedFriends.Num());
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSFriendsSubsystem: Failed to read friends list — %s"), *ErrorStr);
	}

	OnFriendsListReady.Broadcast(CachedFriends);

	// A failed read is not a list change — only signal Changed when the refresh succeeded
	if (bWasSuccessful)
	{
		OnFriendsListChanged.Broadcast();
	}
}

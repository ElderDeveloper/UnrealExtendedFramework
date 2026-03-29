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

void UEEOSFriendsSubsystem::ReadFriendsList()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ReadFriendsList"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSFriendsSubsystem::ReadFriendsList — Friends interface not available"));
		return;
	}

	FriendsInterface->ReadFriendsList(0, TEXT("default"),
		FOnReadFriendsListComplete::CreateUObject(this, &UEEOSFriendsSubsystem::HandleReadFriendsListComplete));

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::ReadFriendsList — Reading friends list..."));
}

void UEEOSFriendsSubsystem::SendFriendInvite(const FString& FriendUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("SendFriendInvite"));
		OnFriendInviteSent.Broadcast(false, FriendUserId);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid())
	{
		OnFriendInviteSent.Broadcast(false, FriendUserId);
		return;
	}

	const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(FriendUserId, EOS_SUBSYSTEM);
	const bool bSent = FriendsInterface->SendInvite(0, *NetId, TEXT("default"));
	OnFriendInviteSent.Broadcast(bSent, FriendUserId);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::SendFriendInvite — Invite %s to %s"), bSent ? TEXT("sent") : TEXT("failed"), *FriendUserId);
}

void UEEOSFriendsSubsystem::AcceptFriendInvite(const FString& FriendUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("AcceptFriendInvite"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid()) return;

	const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(FriendUserId, EOS_SUBSYSTEM);
	FriendsInterface->AcceptInvite(0, *NetId, TEXT("default"));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::AcceptFriendInvite — Accepted invite from %s"), *FriendUserId);
}

void UEEOSFriendsSubsystem::RejectFriendInvite(const FString& FriendUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("RejectFriendInvite"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineFriendsPtr FriendsInterface = EOSSub->GetFriendsInterface();
	if (!FriendsInterface.IsValid()) return;

	const FUniqueNetIdRef NetId = FUniqueNetIdString::Create(FriendUserId, EOS_SUBSYSTEM);
	FriendsInterface->RejectInvite(0, *NetId, TEXT("default"));
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSFriendsSubsystem::RejectFriendInvite — Rejected invite from %s"), *FriendUserId);
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
	OnFriendsListChanged.Broadcast();
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Friends/ESteamFriendsAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamFriendsSubsystem* GetFriendsSubsystem(const UObject* WorldContextObject)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamFriendsSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncRequestUserInformation ----

USteamAsyncRequestUserInformation* USteamAsyncRequestUserInformation::RequestUserInformation(UObject* WorldContext, FESteamId User, bool bRequireNameOnly, float Timeout)
{
	USteamAsyncRequestUserInformation* Action = NewObject<USteamAsyncRequestUserInformation>();
	Action->WorldContextObject = WorldContext;
	Action->User = User;
	Action->bRequireNameOnly = bRequireNameOnly;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncRequestUserInformation::Activate()
{
	UESteamFriendsSubsystem* Subsystem = GetFriendsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false);
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnPersonaStateChanged.AddDynamic(this, &USteamAsyncRequestUserInformation::HandlePersonaStateChanged);

	// RequestUserInformation returns false when the data is already available locally: succeed now.
	if (!Subsystem->RequestUserInformation(User, bRequireNameOnly))
	{
		Complete(true);
		return;
	}

	// Fail the node if the persona update never arrives.
	ArmTimeout(Timeout);
}

void USteamAsyncRequestUserInformation::HandlePersonaStateChanged(FESteamId SteamId, int32 ChangeFlags)
{
	// Only react to the user this node requested.
	if (SteamId != User)
	{
		return;
	}
	Complete(true);
}

void USteamAsyncRequestUserInformation::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncRequestUserInformation::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnPersonaStateChanged.RemoveDynamic(this, &USteamAsyncRequestUserInformation::HandlePersonaStateChanged);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User);
	}
	else
	{
		OnFailure.Broadcast(User);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncGetFollowerCount ----

USteamAsyncGetFollowerCount* USteamAsyncGetFollowerCount::GetFollowerCount(UObject* WorldContext, FESteamId User, float Timeout)
{
	USteamAsyncGetFollowerCount* Action = NewObject<USteamAsyncGetFollowerCount>();
	Action->WorldContextObject = WorldContext;
	Action->User = User;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetFollowerCount::Activate()
{
	UESteamFriendsSubsystem* Subsystem = GetFriendsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, 0);
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnFollowerCount.AddDynamic(this, &USteamAsyncGetFollowerCount::HandleFollowerCount);

	if (!Subsystem->GetFollowerCount(User))
	{
		Complete(false, 0);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncGetFollowerCount::HandleFollowerCount(bool bSuccess, FESteamId InUser, int32 Count)
{
	// The subsystem echoes the queried user, so filter to this node's request.
	if (InUser != User)
	{
		return;
	}
	Complete(bSuccess, Count);
}

void USteamAsyncGetFollowerCount::OnTimeoutFailure()
{
	Complete(false, 0);
}

void USteamAsyncGetFollowerCount::Complete(bool bSuccess, int32 Count)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnFollowerCount.RemoveDynamic(this, &USteamAsyncGetFollowerCount::HandleFollowerCount);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User, Count);
	}
	else
	{
		OnFailure.Broadcast(User, Count);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncIsFollowing ----

USteamAsyncIsFollowing* USteamAsyncIsFollowing::IsFollowing(UObject* WorldContext, FESteamId User, float Timeout)
{
	USteamAsyncIsFollowing* Action = NewObject<USteamAsyncIsFollowing>();
	Action->WorldContextObject = WorldContext;
	Action->User = User;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncIsFollowing::Activate()
{
	UESteamFriendsSubsystem* Subsystem = GetFriendsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, false);
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnIsFollowing.AddDynamic(this, &USteamAsyncIsFollowing::HandleIsFollowing);

	if (!Subsystem->IsFollowing(User))
	{
		Complete(false, false);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncIsFollowing::HandleIsFollowing(bool bSuccess, FESteamId InUser, bool bIsFollowing)
{
	if (InUser != User)
	{
		return;
	}
	Complete(bSuccess, bIsFollowing);
}

void USteamAsyncIsFollowing::OnTimeoutFailure()
{
	Complete(false, false);
}

void USteamAsyncIsFollowing::Complete(bool bSuccess, bool bIsFollowing)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnIsFollowing.RemoveDynamic(this, &USteamAsyncIsFollowing::HandleIsFollowing);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User, bIsFollowing);
	}
	else
	{
		OnFailure.Broadcast(User, bIsFollowing);
	}

	SetReadyToDestroy();
}

// ---- USteamAsyncEnumerateFollowingList ----

USteamAsyncEnumerateFollowingList* USteamAsyncEnumerateFollowingList::EnumerateFollowingList(UObject* WorldContext, int32 StartIndex, float Timeout)
{
	USteamAsyncEnumerateFollowingList* Action = NewObject<USteamAsyncEnumerateFollowingList>();
	Action->WorldContextObject = WorldContext;
	Action->StartIndex = StartIndex;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncEnumerateFollowingList::Activate()
{
	UESteamFriendsSubsystem* Subsystem = GetFriendsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		Complete(false, TArray<FESteamId>(), 0);
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnFollowingListEnumerated.AddDynamic(this, &USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated);

	if (!Subsystem->EnumerateFollowingList(StartIndex))
	{
		Complete(false, TArray<FESteamId>(), 0);
		return;
	}

	ArmTimeout(Timeout);
}

void USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount)
{
	// NOTE: FOnSteamFollowingListEnumerated carries no start index; cannot discriminate concurrent
	// enumerations. Relies on Timeout + the subsystem's serialized (one-in-flight) FIFO ordering.
	Complete(bSuccess, Users, TotalCount);
}

void USteamAsyncEnumerateFollowingList::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamId>(), 0);
}

void USteamAsyncEnumerateFollowingList::Complete(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnFollowingListEnumerated.RemoveDynamic(this, &USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Users, TotalCount);
	}
	else
	{
		OnFailure.Broadcast(Users, TotalCount);
	}

	SetReadyToDestroy();
}

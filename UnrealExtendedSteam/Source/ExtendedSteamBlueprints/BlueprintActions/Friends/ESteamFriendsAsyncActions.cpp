// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/Friends/ESteamFriendsAsyncActions.h"

// ---- USteamAsyncRequestUserInformation ----

USteamAsyncRequestUserInformation* USteamAsyncRequestUserInformation::RequestUserInformation(UObject* WorldContext, FESteamId User, bool bRequireNameOnly, float Timeout)
{
	USteamAsyncRequestUserInformation* Action = NewObject<USteamAsyncRequestUserInformation>();
	Action->User = User;
	Action->bRequireNameOnly = bRequireNameOnly;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncRequestUserInformation::ExecuteAction()
{
	UESteamFriendsSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamFriendsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnPersonaStateChanged.AddDynamic(this, &USteamAsyncRequestUserInformation::HandlePersonaStateChanged);

	// RequestUserInformation returns false when the data is already available locally: succeed now.
	bool bRequested = false;
	CallWithContext([&] { bRequested = Subsystem->RequestUserInformation(User, bRequireNameOnly); });
	if (!bRequested)
	{
		Complete(true);
	}
}

void USteamAsyncRequestUserInformation::HandlePersonaStateChanged(FESteamId SteamId, int32 ChangeFlags)
{
	// Only react to the user this node requested.
	if (!IsResultForThisCall() || SteamId != User)
	{
		return;
	}
	Complete(true);
}

void USteamAsyncRequestUserInformation::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(User);
}

void USteamAsyncRequestUserInformation::CancelWork()
{
	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnPersonaStateChanged.RemoveDynamic(this, &USteamAsyncRequestUserInformation::HandlePersonaStateChanged);
	}
}

void USteamAsyncRequestUserInformation::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User);
	}
	else
	{
		OnFailure.Broadcast(User);
	}
}

// ---- USteamAsyncGetFollowerCount ----

USteamAsyncGetFollowerCount* USteamAsyncGetFollowerCount::GetFollowerCount(UObject* WorldContext, FESteamId User, float Timeout)
{
	USteamAsyncGetFollowerCount* Action = NewObject<USteamAsyncGetFollowerCount>();
	Action->User = User;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetFollowerCount::ExecuteAction()
{
	UESteamFriendsSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamFriendsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnFollowerCount.AddDynamic(this, &USteamAsyncGetFollowerCount::HandleFollowerCount);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->GetFollowerCount(User); });
	if (!bStarted)
	{
		Complete(false, 0);
	}
}

void USteamAsyncGetFollowerCount::HandleFollowerCount(bool bSuccess, FESteamId InUser, int32 Count)
{
	// The subsystem echoes the queried user and tags the result with this node's call context.
	if (!IsResultForThisCall() || InUser != User)
	{
		return;
	}
	Complete(bSuccess, Count);
}

void USteamAsyncGetFollowerCount::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(User, 0);
}

void USteamAsyncGetFollowerCount::CancelWork()
{
	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnFollowerCount.RemoveDynamic(this, &USteamAsyncGetFollowerCount::HandleFollowerCount);
	}
}

void USteamAsyncGetFollowerCount::Complete(bool bSuccess, int32 Count)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User, Count);
	}
	else
	{
		OnFailure.Broadcast(User, Count);
	}
}

// ---- USteamAsyncIsFollowing ----

USteamAsyncIsFollowing* USteamAsyncIsFollowing::IsFollowing(UObject* WorldContext, FESteamId User, float Timeout)
{
	USteamAsyncIsFollowing* Action = NewObject<USteamAsyncIsFollowing>();
	Action->User = User;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncIsFollowing::ExecuteAction()
{
	UESteamFriendsSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamFriendsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnIsFollowing.AddDynamic(this, &USteamAsyncIsFollowing::HandleIsFollowing);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->IsFollowing(User); });
	if (!bStarted)
	{
		Complete(false, false);
	}
}

void USteamAsyncIsFollowing::HandleIsFollowing(bool bSuccess, FESteamId InUser, bool bIsFollowing)
{
	if (!IsResultForThisCall() || InUser != User)
	{
		return;
	}
	Complete(bSuccess, bIsFollowing);
}

void USteamAsyncIsFollowing::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(User, false);
}

void USteamAsyncIsFollowing::CancelWork()
{
	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnIsFollowing.RemoveDynamic(this, &USteamAsyncIsFollowing::HandleIsFollowing);
	}
}

void USteamAsyncIsFollowing::Complete(bool bSuccess, bool bIsFollowing)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(User, bIsFollowing);
	}
	else
	{
		OnFailure.Broadcast(User, bIsFollowing);
	}
}

// ---- USteamAsyncEnumerateFollowingList ----

USteamAsyncEnumerateFollowingList* USteamAsyncEnumerateFollowingList::EnumerateFollowingList(UObject* WorldContext, int32 StartIndex, float Timeout)
{
	USteamAsyncEnumerateFollowingList* Action = NewObject<USteamAsyncEnumerateFollowingList>();
	Action->StartIndex = StartIndex;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncEnumerateFollowingList::ExecuteAction()
{
	UESteamFriendsSubsystem* Subsystem = RequireGameInstanceSubsystem<UESteamFriendsSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FriendsSubsystem = Subsystem;
	Subsystem->OnFollowingListEnumerated.AddDynamic(this, &USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated);

	bool bStarted = false;
	CallWithContext([&] { bStarted = Subsystem->EnumerateFollowingList(StartIndex); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamId>(), 0);
	}
}

void USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount)
{
	// FOnSteamFollowingListEnumerated carries no start index; the subsystem tags each result with
	// the call context of the request it answers, so concurrent enumerations are told apart here.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Users, TotalCount);
}

void USteamAsyncEnumerateFollowingList::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamId>(), 0);
}

void USteamAsyncEnumerateFollowingList::CancelWork()
{
	if (UESteamFriendsSubsystem* Subsystem = FriendsSubsystem.Get())
	{
		Subsystem->OnFollowingListEnumerated.RemoveDynamic(this, &USteamAsyncEnumerateFollowingList::HandleFollowingListEnumerated);
	}
}

void USteamAsyncEnumerateFollowingList::Complete(bool bSuccess, const TArray<FESteamId>& Users, int32 TotalCount)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Users, TotalCount);
	}
	else
	{
		OnFailure.Broadcast(Users, TotalCount);
	}
}

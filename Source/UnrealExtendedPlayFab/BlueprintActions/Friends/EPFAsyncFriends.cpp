// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncFriends.h"
#include "Engine/GameInstance.h"

// ── Get Friends ──────────────────────────────────────────────────────────────
UEPFAsyncGetFriends* UEPFAsyncGetFriends::GetFriendsList(UObject* WorldContext, bool bIncludeSteamFriends)
{
	auto* A = NewObject<UEPFAsyncGetFriends>(); A->bSteam = bIncludeSteamFriends; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetFriends::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Friends subsystem not available")); SetReadyToDestroy(); return; }
	S->OnFriendsReceived.AddDynamic(this, &UEPFAsyncGetFriends::HandleComplete);
	S->GetFriendsList(bSteam);
}
void UEPFAsyncGetFriends::HandleComplete(const FEPFResult& Result, const TArray<FEPFFriend>& Friends)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get())) S->OnFriendsReceived.RemoveDynamic(this, &UEPFAsyncGetFriends::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Friends) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get friends list")));
	SetReadyToDestroy();
}

// ── Add Friend ───────────────────────────────────────────────────────────────
UEPFAsyncAddFriend* UEPFAsyncAddFriend::AddFriend(UObject* WorldContext, const FString& FriendPlayFabId)
{
	auto* A = NewObject<UEPFAsyncAddFriend>(); A->FriendId = FriendPlayFabId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncAddFriend::Activate()
{
	if (FriendId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("FriendPlayFabId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Friends subsystem not available")); SetReadyToDestroy(); return; }
	S->OnFriendAdded.AddDynamic(this, &UEPFAsyncAddFriend::HandleComplete);
	S->AddFriend(FriendId);
}
void UEPFAsyncAddFriend::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get())) S->OnFriendAdded.RemoveDynamic(this, &UEPFAsyncAddFriend::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add friend")));
	SetReadyToDestroy();
}

// ── Remove Friend ────────────────────────────────────────────────────────────
UEPFAsyncRemoveFriend* UEPFAsyncRemoveFriend::RemoveFriend(UObject* WorldContext, const FString& FriendPlayFabId)
{
	auto* A = NewObject<UEPFAsyncRemoveFriend>(); A->FriendId = FriendPlayFabId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncRemoveFriend::Activate()
{
	if (FriendId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("FriendPlayFabId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Friends subsystem not available")); SetReadyToDestroy(); return; }
	S->OnFriendRemoved.AddDynamic(this, &UEPFAsyncRemoveFriend::HandleComplete);
	S->RemoveFriend(FriendId);
}
void UEPFAsyncRemoveFriend::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get())) S->OnFriendRemoved.RemoveDynamic(this, &UEPFAsyncRemoveFriend::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to remove friend")));
	SetReadyToDestroy();
}

// ── Set Friend Tags ──────────────────────────────────────────────────────────
UEPFAsyncSetFriendTags* UEPFAsyncSetFriendTags::SetFriendTags(UObject* WorldContext, const FString& FriendPlayFabId, const TArray<FString>& Tags)
{
	auto* A = NewObject<UEPFAsyncSetFriendTags>(); A->FriendId = FriendPlayFabId; A->Tags = Tags; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncSetFriendTags::Activate()
{
	if (FriendId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("FriendPlayFabId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Friends subsystem not available")); SetReadyToDestroy(); return; }
	S->OnFriendTagsUpdated.AddDynamic(this, &UEPFAsyncSetFriendTags::HandleComplete);
	S->SetFriendTags(FriendId, Tags);
}
void UEPFAsyncSetFriendTags::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFFriendsSubsystem>(WorldContext.Get())) S->OnFriendTagsUpdated.RemoveDynamic(this, &UEPFAsyncSetFriendTags::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set friend tags")));
	SetReadyToDestroy();
}

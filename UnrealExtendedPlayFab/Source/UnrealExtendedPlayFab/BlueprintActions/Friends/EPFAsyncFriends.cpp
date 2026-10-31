// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncFriends.h"

// ── Get Friends ──────────────────────────────────────────────────────────────
UEPFAsyncGetFriends* UEPFAsyncGetFriends::GetFriendsList(UObject* WorldContext, bool bIncludeSteamFriends)
{
	auto* A = NewObject<UEPFAsyncGetFriends>(); A->bSteam = bIncludeSteamFriends; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetFriends::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFFriendsSubsystem>();
	if (!S) return;
	S->OnFriendsReceived.AddDynamic(this, &UEPFAsyncGetFriends::HandleComplete);
	CallWithContext([&] { S->GetFriendsList(bSteam); });
}
void UEPFAsyncGetFriends::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFFriendsSubsystem>()) S->OnFriendsReceived.RemoveDynamic(this, &UEPFAsyncGetFriends::HandleComplete);
}
void UEPFAsyncGetFriends::HandleComplete(const FEPFResult& Result, const TArray<FEPFFriend>& Friends)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Friends) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get friends list")));
}

// ── Add Friend ───────────────────────────────────────────────────────────────
UEPFAsyncAddFriend* UEPFAsyncAddFriend::AddFriend(UObject* WorldContext, const FString& FriendPlayFabId)
{
	auto* A = NewObject<UEPFAsyncAddFriend>(); A->FriendId = FriendPlayFabId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncAddFriend::ExecuteAction()
{
	if (FriendId.IsEmpty()) { FailInvalidArgument(TEXT("FriendPlayFabId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFFriendsSubsystem>();
	if (!S) return;
	S->OnFriendAdded.AddDynamic(this, &UEPFAsyncAddFriend::HandleComplete);
	CallWithContext([&] { S->AddFriend(FriendId); });
}
void UEPFAsyncAddFriend::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFFriendsSubsystem>()) S->OnFriendAdded.RemoveDynamic(this, &UEPFAsyncAddFriend::HandleComplete);
}
void UEPFAsyncAddFriend::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add friend")));
}

// ── Remove Friend ────────────────────────────────────────────────────────────
UEPFAsyncRemoveFriend* UEPFAsyncRemoveFriend::RemoveFriend(UObject* WorldContext, const FString& FriendPlayFabId)
{
	auto* A = NewObject<UEPFAsyncRemoveFriend>(); A->FriendId = FriendPlayFabId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncRemoveFriend::ExecuteAction()
{
	if (FriendId.IsEmpty()) { FailInvalidArgument(TEXT("FriendPlayFabId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFFriendsSubsystem>();
	if (!S) return;
	S->OnFriendRemoved.AddDynamic(this, &UEPFAsyncRemoveFriend::HandleComplete);
	CallWithContext([&] { S->RemoveFriend(FriendId); });
}
void UEPFAsyncRemoveFriend::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFFriendsSubsystem>()) S->OnFriendRemoved.RemoveDynamic(this, &UEPFAsyncRemoveFriend::HandleComplete);
}
void UEPFAsyncRemoveFriend::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to remove friend")));
}

// ── Set Friend Tags ──────────────────────────────────────────────────────────
UEPFAsyncSetFriendTags* UEPFAsyncSetFriendTags::SetFriendTags(UObject* WorldContext, const FString& FriendPlayFabId, const TArray<FString>& Tags)
{
	auto* A = NewObject<UEPFAsyncSetFriendTags>(); A->FriendId = FriendPlayFabId; A->FriendTags = Tags; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncSetFriendTags::ExecuteAction()
{
	if (FriendId.IsEmpty()) { FailInvalidArgument(TEXT("FriendPlayFabId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFFriendsSubsystem>();
	if (!S) return;
	S->OnFriendTagsUpdated.AddDynamic(this, &UEPFAsyncSetFriendTags::HandleComplete);
	CallWithContext([&] { S->SetFriendTags(FriendId, FriendTags); });
}
void UEPFAsyncSetFriendTags::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFFriendsSubsystem>()) S->OnFriendTagsUpdated.RemoveDynamic(this, &UEPFAsyncSetFriendTags::HandleComplete);
}
void UEPFAsyncSetFriendTags::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set friend tags")));
}

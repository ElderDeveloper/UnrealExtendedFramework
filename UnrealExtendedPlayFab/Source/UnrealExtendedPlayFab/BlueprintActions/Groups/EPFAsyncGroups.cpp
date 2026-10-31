// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncGroups.h"

// ── Create ───────────────────────────────────────────────────────────────────

UEPFAsyncCreateGroup* UEPFAsyncCreateGroup::CreateGroup(UObject* WorldContext, const FString& GroupName)
{
	auto* A = NewObject<UEPFAsyncCreateGroup>(); A->NewGroupName = GroupName; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncCreateGroup::ExecuteAction()
{
	if (NewGroupName.IsEmpty()) { FailInvalidArgument(TEXT("Group name cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnGroupCreated.AddDynamic(this, &UEPFAsyncCreateGroup::HandleComplete);
	CallWithContext([&] { S->CreateGroup(NewGroupName); });
}
void UEPFAsyncCreateGroup::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnGroupCreated.RemoveDynamic(this, &UEPFAsyncCreateGroup::HandleComplete);
}
void UEPFAsyncCreateGroup::HandleComplete(const FEPFResult& Result, const FString& GroupId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(GroupId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create group")));
}

// ── Delete ───────────────────────────────────────────────────────────────────

UEPFAsyncDeleteGroup* UEPFAsyncDeleteGroup::DeleteGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncDeleteGroup>(); A->TargetGroupId = GroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncDeleteGroup::ExecuteAction()
{
	if (TargetGroupId.IsEmpty()) { FailInvalidArgument(TEXT("GroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnGroupDeleted.AddDynamic(this, &UEPFAsyncDeleteGroup::HandleComplete);
	CallWithContext([&] { S->DeleteGroup(TargetGroupId); });
}
void UEPFAsyncDeleteGroup::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnGroupDeleted.RemoveDynamic(this, &UEPFAsyncDeleteGroup::HandleComplete);
}
void UEPFAsyncDeleteGroup::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete group")));
}

// ── Get Group Info ───────────────────────────────────────────────────────────

UEPFAsyncGetGroupInfo* UEPFAsyncGetGroupInfo::GetGroupInfo(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncGetGroupInfo>(); A->TargetGroupId = GroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetGroupInfo::ExecuteAction()
{
	if (TargetGroupId.IsEmpty()) { FailInvalidArgument(TEXT("GroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnGroupInfoReceived.AddDynamic(this, &UEPFAsyncGetGroupInfo::HandleComplete);
	CallWithContext([&] { S->GetGroup(TargetGroupId); });
}
void UEPFAsyncGetGroupInfo::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnGroupInfoReceived.RemoveDynamic(this, &UEPFAsyncGetGroupInfo::HandleComplete);
}
void UEPFAsyncGetGroupInfo::HandleComplete(const FEPFResult& Result, const FEPFGroupInfo& Info)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Info) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get group info")));
}

// ── List Memberships ─────────────────────────────────────────────────────────

UEPFAsyncListMemberships* UEPFAsyncListMemberships::ListMemberships(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncListMemberships>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncListMemberships::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnGroupsListed.AddDynamic(this, &UEPFAsyncListMemberships::HandleComplete);
	CallWithContext([&] { S->ListMembership(); });
}
void UEPFAsyncListMemberships::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnGroupsListed.RemoveDynamic(this, &UEPFAsyncListMemberships::HandleComplete);
}
void UEPFAsyncListMemberships::HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupInfo>& Groups)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Groups) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to list memberships")));
}

// ── List Group Members ───────────────────────────────────────────────────────

UEPFAsyncListGroupMembers* UEPFAsyncListGroupMembers::ListGroupMembers(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncListGroupMembers>(); A->TargetGroupId = GroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncListGroupMembers::ExecuteAction()
{
	if (TargetGroupId.IsEmpty()) { FailInvalidArgument(TEXT("GroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnGroupMembersReceived.AddDynamic(this, &UEPFAsyncListGroupMembers::HandleComplete);
	CallWithContext([&] { S->ListGroupMembers(TargetGroupId); });
}
void UEPFAsyncListGroupMembers::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnGroupMembersReceived.RemoveDynamic(this, &UEPFAsyncListGroupMembers::HandleComplete);
}
void UEPFAsyncListGroupMembers::HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupMember>& Members)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Members) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to list group members")));
}

// ── Apply / Leave ────────────────────────────────────────────────────────────

UEPFAsyncApplyToGroup* UEPFAsyncApplyToGroup::ApplyToGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncApplyToGroup>(); A->TargetGroupId = GroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncApplyToGroup::ExecuteAction()
{
	if (TargetGroupId.IsEmpty()) { FailInvalidArgument(TEXT("GroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnMembershipChanged.AddDynamic(this, &UEPFAsyncApplyToGroup::HandleComplete);
	CallWithContext([&] { S->ApplyToGroup(TargetGroupId); });
}
void UEPFAsyncApplyToGroup::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnMembershipChanged.RemoveDynamic(this, &UEPFAsyncApplyToGroup::HandleComplete);
}
void UEPFAsyncApplyToGroup::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to apply to group")));
}

UEPFAsyncLeaveGroup* UEPFAsyncLeaveGroup::LeaveGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncLeaveGroup>(); A->TargetGroupId = GroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncLeaveGroup::ExecuteAction()
{
	if (TargetGroupId.IsEmpty()) { FailInvalidArgument(TEXT("GroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFGroupsSubsystem>();
	if (!S) return;
	S->OnMembershipChanged.AddDynamic(this, &UEPFAsyncLeaveGroup::HandleComplete);
	CallWithContext([&] { S->LeaveGroup(TargetGroupId); });
}
void UEPFAsyncLeaveGroup::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFGroupsSubsystem>()) S->OnMembershipChanged.RemoveDynamic(this, &UEPFAsyncLeaveGroup::HandleComplete);
}
void UEPFAsyncLeaveGroup::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to leave group")));
}

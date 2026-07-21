// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncGroups.h"
#include "Engine/GameInstance.h"

// ── Create ───────────────────────────────────────────────────────────────────

UEPFAsyncCreateGroup* UEPFAsyncCreateGroup::CreateGroup(UObject* WorldContext, const FString& GroupName)
{
	auto* A = NewObject<UEPFAsyncCreateGroup>(); A->GroupName = GroupName; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncCreateGroup::Activate()
{
	if (GroupName.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Group name cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnGroupCreated.AddDynamic(this, &UEPFAsyncCreateGroup::HandleComplete);
	S->CreateGroup(GroupName);
}
void UEPFAsyncCreateGroup::HandleComplete(const FEPFResult& Result, const FString& GroupId)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnGroupCreated.RemoveDynamic(this, &UEPFAsyncCreateGroup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(GroupId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create group")));
	SetReadyToDestroy();
}

// ── Delete ───────────────────────────────────────────────────────────────────

UEPFAsyncDeleteGroup* UEPFAsyncDeleteGroup::DeleteGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncDeleteGroup>(); A->GroupId = GroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncDeleteGroup::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("GroupId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnGroupDeleted.AddDynamic(this, &UEPFAsyncDeleteGroup::HandleComplete);
	S->DeleteGroup(GroupId);
}
void UEPFAsyncDeleteGroup::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnGroupDeleted.RemoveDynamic(this, &UEPFAsyncDeleteGroup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete group")));
	SetReadyToDestroy();
}

// ── Get Group Info ───────────────────────────────────────────────────────────

UEPFAsyncGetGroupInfo* UEPFAsyncGetGroupInfo::GetGroupInfo(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncGetGroupInfo>(); A->GroupId = GroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetGroupInfo::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("GroupId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnGroupInfoReceived.AddDynamic(this, &UEPFAsyncGetGroupInfo::HandleComplete);
	S->GetGroup(GroupId);
}
void UEPFAsyncGetGroupInfo::HandleComplete(const FEPFResult& Result, const FEPFGroupInfo& Info)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnGroupInfoReceived.RemoveDynamic(this, &UEPFAsyncGetGroupInfo::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Info) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get group info")));
	SetReadyToDestroy();
}

// ── List Memberships ─────────────────────────────────────────────────────────

UEPFAsyncListMemberships* UEPFAsyncListMemberships::ListMemberships(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncListMemberships>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncListMemberships::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnGroupsListed.AddDynamic(this, &UEPFAsyncListMemberships::HandleComplete);
	S->ListMembership();
}
void UEPFAsyncListMemberships::HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupInfo>& Groups)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnGroupsListed.RemoveDynamic(this, &UEPFAsyncListMemberships::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Groups) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to list memberships")));
	SetReadyToDestroy();
}

// ── List Group Members ───────────────────────────────────────────────────────

UEPFAsyncListGroupMembers* UEPFAsyncListGroupMembers::ListGroupMembers(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncListGroupMembers>(); A->GroupId = GroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncListGroupMembers::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("GroupId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnGroupMembersReceived.AddDynamic(this, &UEPFAsyncListGroupMembers::HandleComplete);
	S->ListGroupMembers(GroupId);
}
void UEPFAsyncListGroupMembers::HandleComplete(const FEPFResult& Result, const TArray<FEPFGroupMember>& Members)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnGroupMembersReceived.RemoveDynamic(this, &UEPFAsyncListGroupMembers::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Members) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to list group members")));
	SetReadyToDestroy();
}

// ── Apply / Leave ────────────────────────────────────────────────────────────

UEPFAsyncApplyToGroup* UEPFAsyncApplyToGroup::ApplyToGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncApplyToGroup>(); A->GroupId = GroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncApplyToGroup::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("GroupId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnMembershipChanged.AddDynamic(this, &UEPFAsyncApplyToGroup::HandleComplete);
	S->ApplyToGroup(GroupId);
}
void UEPFAsyncApplyToGroup::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnMembershipChanged.RemoveDynamic(this, &UEPFAsyncApplyToGroup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to apply to group")));
	SetReadyToDestroy();
}

UEPFAsyncLeaveGroup* UEPFAsyncLeaveGroup::LeaveGroup(UObject* WorldContext, const FString& GroupId)
{
	auto* A = NewObject<UEPFAsyncLeaveGroup>(); A->GroupId = GroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncLeaveGroup::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("GroupId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Groups subsystem not available")); SetReadyToDestroy(); return; }
	S->OnMembershipChanged.AddDynamic(this, &UEPFAsyncLeaveGroup::HandleComplete);
	S->LeaveGroup(GroupId);
}
void UEPFAsyncLeaveGroup::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFGroupsSubsystem>(WorldContext.Get())) S->OnMembershipChanged.RemoveDynamic(this, &UEPFAsyncLeaveGroup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to leave group")));
	SetReadyToDestroy();
}

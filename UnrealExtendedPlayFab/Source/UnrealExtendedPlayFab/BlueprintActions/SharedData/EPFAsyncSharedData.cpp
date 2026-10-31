// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncSharedData.h"

// ── Create Shared Group ──────────────────────────────────────────────────────
UEPFAsyncCreateSharedGroup* UEPFAsyncCreateSharedGroup::CreateSharedGroup(UObject* WorldContext, const FString& SharedGroupId)
{
	auto* A = NewObject<UEPFAsyncCreateSharedGroup>(); A->GroupId = SharedGroupId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncCreateSharedGroup::ExecuteAction()
{
	if (GroupId.IsEmpty()) { FailInvalidArgument(TEXT("SharedGroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSharedDataSubsystem>();
	if (!S) return;
	S->OnGroupCreated.AddDynamic(this, &UEPFAsyncCreateSharedGroup::HandleComplete);
	CallWithContext([&] { S->CreateSharedGroup(GroupId); });
}
void UEPFAsyncCreateSharedGroup::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSharedDataSubsystem>()) S->OnGroupCreated.RemoveDynamic(this, &UEPFAsyncCreateSharedGroup::HandleComplete);
}
void UEPFAsyncCreateSharedGroup::HandleComplete(const FEPFResult& Result, const FString& Id)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Id) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create shared group")));
}

// ── Get Shared Data ──────────────────────────────────────────────────────────
UEPFAsyncGetSharedData* UEPFAsyncGetSharedData::GetSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& Keys)
{
	auto* A = NewObject<UEPFAsyncGetSharedData>(); A->GroupId = SharedGroupId; A->RequestedKeys = Keys; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetSharedData::ExecuteAction()
{
	if (GroupId.IsEmpty()) { FailInvalidArgument(TEXT("SharedGroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSharedDataSubsystem>();
	if (!S) return;
	S->OnDataReceived.AddDynamic(this, &UEPFAsyncGetSharedData::HandleComplete);
	CallWithContext([&] { S->GetSharedGroupData(GroupId, RequestedKeys); });
}
void UEPFAsyncGetSharedData::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSharedDataSubsystem>()) S->OnDataReceived.RemoveDynamic(this, &UEPFAsyncGetSharedData::HandleComplete);
}
void UEPFAsyncGetSharedData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get shared data")));
}

// ── Update Shared Data ───────────────────────────────────────────────────────
UEPFAsyncUpdateSharedData* UEPFAsyncUpdateSharedData::UpdateSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TMap<FString, FString>& Data)
{
	auto* A = NewObject<UEPFAsyncUpdateSharedData>(); A->GroupId = SharedGroupId; A->UpdatedData = Data; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncUpdateSharedData::ExecuteAction()
{
	if (GroupId.IsEmpty()) { FailInvalidArgument(TEXT("SharedGroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSharedDataSubsystem>();
	if (!S) return;
	S->OnDataUpdated.AddDynamic(this, &UEPFAsyncUpdateSharedData::HandleComplete);
	CallWithContext([&] { S->UpdateSharedGroupData(GroupId, UpdatedData); });
}
void UEPFAsyncUpdateSharedData::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSharedDataSubsystem>()) S->OnDataUpdated.RemoveDynamic(this, &UEPFAsyncUpdateSharedData::HandleComplete);
}
void UEPFAsyncUpdateSharedData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update shared data")));
}

// ── Add Members ──────────────────────────────────────────────────────────────
UEPFAsyncAddSharedMembers* UEPFAsyncAddSharedMembers::AddMembers(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& PlayFabIds)
{
	auto* A = NewObject<UEPFAsyncAddSharedMembers>(); A->GroupId = SharedGroupId; A->Ids = PlayFabIds; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncAddSharedMembers::ExecuteAction()
{
	if (GroupId.IsEmpty()) { FailInvalidArgument(TEXT("SharedGroupId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSharedDataSubsystem>();
	if (!S) return;
	S->OnMembersChanged.AddDynamic(this, &UEPFAsyncAddSharedMembers::HandleComplete);
	CallWithContext([&] { S->AddSharedGroupMembers(GroupId, Ids); });
}
void UEPFAsyncAddSharedMembers::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSharedDataSubsystem>()) S->OnMembersChanged.RemoveDynamic(this, &UEPFAsyncAddSharedMembers::HandleComplete);
}
void UEPFAsyncAddSharedMembers::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add members")));
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncSharedData.h"
#include "Engine/GameInstance.h"

// ── Create Shared Group ──────────────────────────────────────────────────────
UEPFAsyncCreateSharedGroup* UEPFAsyncCreateSharedGroup::CreateSharedGroup(UObject* WorldContext, const FString& SharedGroupId)
{
	auto* A = NewObject<UEPFAsyncCreateSharedGroup>(); A->GroupId = SharedGroupId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncCreateSharedGroup::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedGroupId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedData subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnGroupCreated.AddDynamic(this, &UEPFAsyncCreateSharedGroup::HandleComplete);
	S->CreateSharedGroup(GroupId);
}
void UEPFAsyncCreateSharedGroup::HandleComplete(const FEPFResult& Result, const FString& Id)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get())) S->OnGroupCreated.RemoveDynamic(this, &UEPFAsyncCreateSharedGroup::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Id) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create shared group")));
	SetReadyToDestroy();
}

// ── Get Shared Data ──────────────────────────────────────────────────────────
UEPFAsyncGetSharedData* UEPFAsyncGetSharedData::GetSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& Keys)
{
	auto* A = NewObject<UEPFAsyncGetSharedData>(); A->GroupId = SharedGroupId; A->Keys = Keys; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetSharedData::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedGroupId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedData subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnDataReceived.AddDynamic(this, &UEPFAsyncGetSharedData::HandleComplete);
	S->GetSharedGroupData(GroupId, Keys);
}
void UEPFAsyncGetSharedData::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get())) S->OnDataReceived.RemoveDynamic(this, &UEPFAsyncGetSharedData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get shared data")));
	SetReadyToDestroy();
}

// ── Update Shared Data ───────────────────────────────────────────────────────
UEPFAsyncUpdateSharedData* UEPFAsyncUpdateSharedData::UpdateSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TMap<FString, FString>& Data)
{
	auto* A = NewObject<UEPFAsyncUpdateSharedData>(); A->GroupId = SharedGroupId; A->Data = Data; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncUpdateSharedData::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedGroupId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedData subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnDataUpdated.AddDynamic(this, &UEPFAsyncUpdateSharedData::HandleComplete);
	S->UpdateSharedGroupData(GroupId, Data);
}
void UEPFAsyncUpdateSharedData::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get())) S->OnDataUpdated.RemoveDynamic(this, &UEPFAsyncUpdateSharedData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update shared data")));
	SetReadyToDestroy();
}

// ── Add Members ──────────────────────────────────────────────────────────────
UEPFAsyncAddSharedMembers* UEPFAsyncAddSharedMembers::AddMembers(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& PlayFabIds)
{
	auto* A = NewObject<UEPFAsyncAddSharedMembers>(); A->GroupId = SharedGroupId; A->Ids = PlayFabIds; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncAddSharedMembers::Activate()
{
	if (GroupId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedGroupId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("SharedData subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnMembersChanged.AddDynamic(this, &UEPFAsyncAddSharedMembers::HandleComplete);
	S->AddSharedGroupMembers(GroupId, Ids);
}
void UEPFAsyncAddSharedMembers::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSharedDataSubsystem>(WorldContext.Get())) S->OnMembersChanged.RemoveDynamic(this, &UEPFAsyncAddSharedMembers::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add members")));
	SetReadyToDestroy();
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncEntityFiles.h"

// ── Get Objects ──────────────────────────────────────────────────────────────
UEPFAsyncGetEntityObjects* UEPFAsyncGetEntityObjects::GetObjects(UObject* WorldContext, const FString& EntityId, const FString& EntityType)
{
	auto* A = NewObject<UEPFAsyncGetEntityObjects>(); A->TargetEntityId = EntityId; A->TargetEntityType = EntityType; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetEntityObjects::ExecuteAction()
{
	if (TargetEntityId.IsEmpty()) { FailInvalidArgument(TEXT("EntityId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFEntityFilesSubsystem>();
	if (!S) return;
	S->OnObjectsReceived.AddDynamic(this, &UEPFAsyncGetEntityObjects::HandleComplete);
	CallWithContext([&] { S->GetObjects(TargetEntityId, TargetEntityType); });
}
void UEPFAsyncGetEntityObjects::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFEntityFilesSubsystem>()) S->OnObjectsReceived.RemoveDynamic(this, &UEPFAsyncGetEntityObjects::HandleComplete);
}
void UEPFAsyncGetEntityObjects::HandleComplete(const FEPFResult& Result, const TArray<FEPFEntityObject>& Objects)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Objects) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get entity objects")));
}

// ── Set Objects ──────────────────────────────────────────────────────────────
UEPFAsyncSetEntityObjects* UEPFAsyncSetEntityObjects::SetObjects(UObject* WorldContext, const FString& EntityId, const TMap<FString, FString>& Objects, const FString& EntityType)
{
	auto* A = NewObject<UEPFAsyncSetEntityObjects>(); A->TargetEntityId = EntityId; A->TargetEntityType = EntityType; A->ObjectsToSet = Objects; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncSetEntityObjects::ExecuteAction()
{
	if (TargetEntityId.IsEmpty() || ObjectsToSet.Num() == 0) { FailInvalidArgument(TEXT("EntityId and Objects required")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFEntityFilesSubsystem>();
	if (!S) return;
	S->OnObjectsUpdated.AddDynamic(this, &UEPFAsyncSetEntityObjects::HandleComplete);
	CallWithContext([&] { S->SetObjects(TargetEntityId, ObjectsToSet, TargetEntityType); });
}
void UEPFAsyncSetEntityObjects::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFEntityFilesSubsystem>()) S->OnObjectsUpdated.RemoveDynamic(this, &UEPFAsyncSetEntityObjects::HandleComplete);
}
void UEPFAsyncSetEntityObjects::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set entity objects")));
}

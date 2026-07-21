// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncEntityFiles.h"
#include "Engine/GameInstance.h"

// ── Get Objects ──────────────────────────────────────────────────────────────
UEPFAsyncGetEntityObjects* UEPFAsyncGetEntityObjects::GetObjects(UObject* WorldContext, const FString& EntityId, const FString& EntityType)
{
	auto* A = NewObject<UEPFAsyncGetEntityObjects>(); A->EntityId = EntityId; A->EntityType = EntityType; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetEntityObjects::Activate()
{
	if (EntityId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("EntityId cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFEntityFilesSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("EntityFiles subsystem not available")); SetReadyToDestroy(); return; }
	S->OnObjectsReceived.AddDynamic(this, &UEPFAsyncGetEntityObjects::HandleComplete);
	S->GetObjects(EntityId, EntityType);
}
void UEPFAsyncGetEntityObjects::HandleComplete(const FEPFResult& Result, const TArray<FEPFEntityObject>& Objects)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFEntityFilesSubsystem>(WorldContext.Get())) S->OnObjectsReceived.RemoveDynamic(this, &UEPFAsyncGetEntityObjects::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Objects) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get entity objects")));
	SetReadyToDestroy();
}

// ── Set Objects ──────────────────────────────────────────────────────────────
UEPFAsyncSetEntityObjects* UEPFAsyncSetEntityObjects::SetObjects(UObject* WorldContext, const FString& EntityId, const TMap<FString, FString>& Objects, const FString& EntityType)
{
	auto* A = NewObject<UEPFAsyncSetEntityObjects>(); A->EntityId = EntityId; A->EntityType = EntityType; A->Objects = Objects; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncSetEntityObjects::Activate()
{
	if (EntityId.IsEmpty() || Objects.Num() == 0) { BroadcastEPFFailure(OnFailure, TEXT("EntityId and Objects required")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFEntityFilesSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("EntityFiles subsystem not available")); SetReadyToDestroy(); return; }
	S->OnObjectsUpdated.AddDynamic(this, &UEPFAsyncSetEntityObjects::HandleComplete);
	S->SetObjects(EntityId, Objects, EntityType);
}
void UEPFAsyncSetEntityObjects::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFEntityFilesSubsystem>(WorldContext.Get())) S->OnObjectsUpdated.RemoveDynamic(this, &UEPFAsyncSetEntityObjects::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set entity objects")));
	SetReadyToDestroy();
}

// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncSegments.h"
#include "Engine/GameInstance.h"

// ── Get Segments ─────────────────────────────────────────────────────────────

UEPFAsyncGetSegments* UEPFAsyncGetSegments::GetPlayerSegments(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetSegments>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetSegments::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Segments subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnSegmentsReceived.AddDynamic(this, &UEPFAsyncGetSegments::HandleComplete);
	S->GetPlayerSegments();
}
void UEPFAsyncGetSegments::HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerSegment>& Segments)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get())) S->OnSegmentsReceived.RemoveDynamic(this, &UEPFAsyncGetSegments::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Segments) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get segments")));
	SetReadyToDestroy();
}

// ── Get Tags ─────────────────────────────────────────────────────────────────

UEPFAsyncGetPlayerTags* UEPFAsyncGetPlayerTags::GetPlayerTags(UObject* WorldContext, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncGetPlayerTags>(); A->Namespace = Namespace; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetPlayerTags::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Segments subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnPlayerTagsReceived.AddDynamic(this, &UEPFAsyncGetPlayerTags::HandleComplete);
	S->GetPlayerTags(Namespace);
}
void UEPFAsyncGetPlayerTags::HandleComplete(const FEPFResult& Result, const TArray<FString>& Tags)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get())) S->OnPlayerTagsReceived.RemoveDynamic(this, &UEPFAsyncGetPlayerTags::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Tags) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get tags")));
	SetReadyToDestroy();
}

// ── Add Tag ──────────────────────────────────────────────────────────────────

UEPFAsyncAddPlayerTag* UEPFAsyncAddPlayerTag::AddPlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncAddPlayerTag>(); A->TagName = TagName; A->Namespace = Namespace; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncAddPlayerTag::Activate()
{
	if (TagName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Tag name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Segments subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnPlayerTagModified.AddDynamic(this, &UEPFAsyncAddPlayerTag::HandleComplete);
	S->AddPlayerTag(TagName, Namespace);
}
void UEPFAsyncAddPlayerTag::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get())) S->OnPlayerTagModified.RemoveDynamic(this, &UEPFAsyncAddPlayerTag::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add tag")));
	SetReadyToDestroy();
}

// ── Remove Tag ───────────────────────────────────────────────────────────────

UEPFAsyncRemovePlayerTag* UEPFAsyncRemovePlayerTag::RemovePlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncRemovePlayerTag>(); A->TagName = TagName; A->Namespace = Namespace; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncRemovePlayerTag::Activate()
{
	if (TagName.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Tag name cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Segments subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnPlayerTagModified.AddDynamic(this, &UEPFAsyncRemovePlayerTag::HandleComplete);
	S->RemovePlayerTag(TagName, Namespace);
}
void UEPFAsyncRemovePlayerTag::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFSegmentsSubsystem>(WorldContext.Get())) S->OnPlayerTagModified.RemoveDynamic(this, &UEPFAsyncRemovePlayerTag::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to remove tag")));
	SetReadyToDestroy();
}

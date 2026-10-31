// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncSegments.h"

// ── Get Segments ─────────────────────────────────────────────────────────────

UEPFAsyncGetSegments* UEPFAsyncGetSegments::GetPlayerSegments(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetSegments>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetSegments::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFSegmentsSubsystem>();
	if (!S) return;
	S->OnSegmentsReceived.AddDynamic(this, &UEPFAsyncGetSegments::HandleComplete);
	CallWithContext([&] { S->GetPlayerSegments(); });
}
void UEPFAsyncGetSegments::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSegmentsSubsystem>()) S->OnSegmentsReceived.RemoveDynamic(this, &UEPFAsyncGetSegments::HandleComplete);
}
void UEPFAsyncGetSegments::HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerSegment>& Segments)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Segments) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get segments")));
}

// ── Get Tags ─────────────────────────────────────────────────────────────────

UEPFAsyncGetPlayerTags* UEPFAsyncGetPlayerTags::GetPlayerTags(UObject* WorldContext, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncGetPlayerTags>(); A->TagNamespace = Namespace; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetPlayerTags::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFSegmentsSubsystem>();
	if (!S) return;
	S->OnPlayerTagsReceived.AddDynamic(this, &UEPFAsyncGetPlayerTags::HandleComplete);
	CallWithContext([&] { S->GetPlayerTags(TagNamespace); });
}
void UEPFAsyncGetPlayerTags::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSegmentsSubsystem>()) S->OnPlayerTagsReceived.RemoveDynamic(this, &UEPFAsyncGetPlayerTags::HandleComplete);
}
void UEPFAsyncGetPlayerTags::HandleComplete(const FEPFResult& Result, const TArray<FString>& Tags)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Tags) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get tags")));
}

// ── Add Tag ──────────────────────────────────────────────────────────────────

UEPFAsyncAddPlayerTag* UEPFAsyncAddPlayerTag::AddPlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncAddPlayerTag>(); A->RequestedTagName = TagName; A->TagNamespace = Namespace; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncAddPlayerTag::ExecuteAction()
{
	if (RequestedTagName.IsEmpty()) { FailInvalidArgument(TEXT("Tag name cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSegmentsSubsystem>();
	if (!S) return;
	S->OnPlayerTagModified.AddDynamic(this, &UEPFAsyncAddPlayerTag::HandleComplete);
	CallWithContext([&] { S->AddPlayerTag(RequestedTagName, TagNamespace); });
}
void UEPFAsyncAddPlayerTag::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSegmentsSubsystem>()) S->OnPlayerTagModified.RemoveDynamic(this, &UEPFAsyncAddPlayerTag::HandleComplete);
}
void UEPFAsyncAddPlayerTag::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to add tag")));
}

// ── Remove Tag ───────────────────────────────────────────────────────────────

UEPFAsyncRemovePlayerTag* UEPFAsyncRemovePlayerTag::RemovePlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace)
{
	auto* A = NewObject<UEPFAsyncRemovePlayerTag>(); A->RequestedTagName = TagName; A->TagNamespace = Namespace; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncRemovePlayerTag::ExecuteAction()
{
	if (RequestedTagName.IsEmpty()) { FailInvalidArgument(TEXT("Tag name cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFSegmentsSubsystem>();
	if (!S) return;
	S->OnPlayerTagModified.AddDynamic(this, &UEPFAsyncRemovePlayerTag::HandleComplete);
	CallWithContext([&] { S->RemovePlayerTag(RequestedTagName, TagNamespace); });
}
void UEPFAsyncRemovePlayerTag::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFSegmentsSubsystem>()) S->OnPlayerTagModified.RemoveDynamic(this, &UEPFAsyncRemovePlayerTag::HandleComplete);
}
void UEPFAsyncRemovePlayerTag::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to remove tag")));
}

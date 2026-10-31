// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncMessages.h"

// ── Fetch Messages ───────────────────────────────────────────────────────────

UEPFAsyncFetchMessages* UEPFAsyncFetchMessages::FetchMessages(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncFetchMessages>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncFetchMessages::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFMessagesSubsystem>();
	if (!S) return;
	S->OnMessagesReceived.AddDynamic(this, &UEPFAsyncFetchMessages::HandleComplete);
	CallWithContext([&] { S->FetchMessages(); });
}
void UEPFAsyncFetchMessages::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMessagesSubsystem>()) S->OnMessagesReceived.RemoveDynamic(this, &UEPFAsyncFetchMessages::HandleComplete);
}
void UEPFAsyncFetchMessages::HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerMessage>& Messages)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Messages) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to fetch messages")));
}

// ── Mark As Read ─────────────────────────────────────────────────────────────

UEPFAsyncMarkMessageRead* UEPFAsyncMarkMessageRead::MarkMessageRead(UObject* WorldContext, const FString& MessageId)
{
	auto* A = NewObject<UEPFAsyncMarkMessageRead>(); A->TargetMessageId = MessageId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncMarkMessageRead::ExecuteAction()
{
	if (TargetMessageId.IsEmpty()) { FailInvalidArgument(TEXT("MessageId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFMessagesSubsystem>();
	if (!S) return;
	S->OnMessageMarkedRead.AddDynamic(this, &UEPFAsyncMarkMessageRead::HandleComplete);
	CallWithContext([&] { S->MarkAsRead(TargetMessageId); });
}
void UEPFAsyncMarkMessageRead::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMessagesSubsystem>()) S->OnMessageMarkedRead.RemoveDynamic(this, &UEPFAsyncMarkMessageRead::HandleComplete);
}
void UEPFAsyncMarkMessageRead::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to mark message as read")));
}

// ── Delete Message ───────────────────────────────────────────────────────────

UEPFAsyncDeleteMessage* UEPFAsyncDeleteMessage::DeleteMessage(UObject* WorldContext, const FString& MessageId)
{
	auto* A = NewObject<UEPFAsyncDeleteMessage>(); A->TargetMessageId = MessageId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncDeleteMessage::ExecuteAction()
{
	if (TargetMessageId.IsEmpty()) { FailInvalidArgument(TEXT("MessageId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFMessagesSubsystem>();
	if (!S) return;
	S->OnMessageMarkedRead.AddDynamic(this, &UEPFAsyncDeleteMessage::HandleComplete);
	CallWithContext([&] { S->DeleteMessage(TargetMessageId); });
}
void UEPFAsyncDeleteMessage::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMessagesSubsystem>()) S->OnMessageMarkedRead.RemoveDynamic(this, &UEPFAsyncDeleteMessage::HandleComplete);
}
void UEPFAsyncDeleteMessage::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete message")));
}

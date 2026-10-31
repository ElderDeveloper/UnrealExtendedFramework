// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncMatchmaking.h"

// ── Create Ticket ────────────────────────────────────────────────────────────

UEPFAsyncCreateTicket* UEPFAsyncCreateTicket::CreateTicket(UObject* WorldContext, const FString& QueueName, const TMap<FString, FString>& Attributes, int32 GiveUpAfterSeconds)
{
	auto* A = NewObject<UEPFAsyncCreateTicket>(); A->TicketQueueName = QueueName; A->TicketAttributes = Attributes; A->Timeout = GiveUpAfterSeconds; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncCreateTicket::ExecuteAction()
{
	if (TicketQueueName.IsEmpty()) { FailInvalidArgument(TEXT("Queue name cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFMatchmakingSubsystem>();
	if (!S) return;
	S->OnTicketCreated.AddDynamic(this, &UEPFAsyncCreateTicket::HandleComplete);
	CallWithContext([&] { S->CreateTicket(TicketQueueName, TicketAttributes, Timeout); });
}
void UEPFAsyncCreateTicket::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMatchmakingSubsystem>()) S->OnTicketCreated.RemoveDynamic(this, &UEPFAsyncCreateTicket::HandleComplete);
}
void UEPFAsyncCreateTicket::HandleComplete(const FEPFResult& Result, const FString& TicketId)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(TicketId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create matchmaking ticket")));
}

// ── Get Ticket Status ────────────────────────────────────────────────────────

UEPFAsyncGetTicketStatus* UEPFAsyncGetTicketStatus::GetTicketStatus(UObject* WorldContext, const FString& QueueName, const FString& TicketId)
{
	auto* A = NewObject<UEPFAsyncGetTicketStatus>(); A->TicketQueueName = QueueName; A->TargetTicketId = TicketId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetTicketStatus::ExecuteAction()
{
	if (TicketQueueName.IsEmpty() || TargetTicketId.IsEmpty()) { FailInvalidArgument(TEXT("Queue name and ticket ID required")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFMatchmakingSubsystem>();
	if (!S) return;
	S->OnTicketStatusReceived.AddDynamic(this, &UEPFAsyncGetTicketStatus::HandleComplete);
	// The subsystem's status polling (StartPolling) broadcasts on the same delegate under a context of
	// its own, so this node only finishes on the status call it made.
	CallWithContext([&] { S->GetTicketStatus(TicketQueueName, TargetTicketId); });
}
void UEPFAsyncGetTicketStatus::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMatchmakingSubsystem>()) S->OnTicketStatusReceived.RemoveDynamic(this, &UEPFAsyncGetTicketStatus::HandleComplete);
}
void UEPFAsyncGetTicketStatus::HandleComplete(const FEPFResult& Result, const FEPFMatchmakingResult& MatchmakingResult)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(MatchmakingResult) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get ticket status")));
}

// ── Cancel Matchmaking ───────────────────────────────────────────────────────

UEPFAsyncCancelMatchmaking* UEPFAsyncCancelMatchmaking::CancelMatchmaking(UObject* WorldContext, const FString& QueueName, const FString& TicketId)
{
	auto* A = NewObject<UEPFAsyncCancelMatchmaking>(); A->TicketQueueName = QueueName; A->TargetTicketId = TicketId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncCancelMatchmaking::ExecuteAction()
{
	if (TicketQueueName.IsEmpty()) { FailInvalidArgument(TEXT("Queue name cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFMatchmakingSubsystem>();
	if (!S) return;
	S->OnMatchmakingCanceled.AddDynamic(this, &UEPFAsyncCancelMatchmaking::HandleComplete);
	CallWithContext([&]
	{
		if (TargetTicketId.IsEmpty()) { S->CancelAllTickets(TicketQueueName); }
		else { S->CancelTicket(TicketQueueName, TargetTicketId); }
	});
}
void UEPFAsyncCancelMatchmaking::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFMatchmakingSubsystem>()) S->OnMatchmakingCanceled.RemoveDynamic(this, &UEPFAsyncCancelMatchmaking::HandleComplete);
}
void UEPFAsyncCancelMatchmaking::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to cancel matchmaking")));
}

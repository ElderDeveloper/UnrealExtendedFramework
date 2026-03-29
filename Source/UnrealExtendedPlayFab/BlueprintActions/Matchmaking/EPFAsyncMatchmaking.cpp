// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncMatchmaking.h"
#include "Engine/GameInstance.h"

// ── Create Ticket ────────────────────────────────────────────────────────────

UEPFAsyncCreateTicket* UEPFAsyncCreateTicket::CreateTicket(UObject* WorldContext, const FString& QueueName, const TMap<FString, FString>& Attributes, int32 GiveUpAfterSeconds)
{
	auto* A = NewObject<UEPFAsyncCreateTicket>(); A->QueueName = QueueName; A->Attributes = Attributes; A->Timeout = GiveUpAfterSeconds; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncCreateTicket::Activate()
{
	if (QueueName.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Queue name cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Matchmaking subsystem not available")); SetReadyToDestroy(); return; }
	S->OnTicketCreated.AddDynamic(this, &UEPFAsyncCreateTicket::HandleComplete);
	S->CreateTicket(QueueName, Attributes, Timeout);
}
void UEPFAsyncCreateTicket::HandleComplete(const FEPFResult& Result, const FString& TicketId)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get())) S->OnTicketCreated.RemoveDynamic(this, &UEPFAsyncCreateTicket::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(TicketId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to create matchmaking ticket")));
	SetReadyToDestroy();
}

// ── Get Ticket Status ────────────────────────────────────────────────────────

UEPFAsyncGetTicketStatus* UEPFAsyncGetTicketStatus::GetTicketStatus(UObject* WorldContext, const FString& QueueName, const FString& TicketId)
{
	auto* A = NewObject<UEPFAsyncGetTicketStatus>(); A->QueueName = QueueName; A->TicketId = TicketId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetTicketStatus::Activate()
{
	if (QueueName.IsEmpty() || TicketId.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Queue name and ticket ID required")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Matchmaking subsystem not available")); SetReadyToDestroy(); return; }
	S->OnTicketStatusReceived.AddDynamic(this, &UEPFAsyncGetTicketStatus::HandleComplete);
	S->GetTicketStatus(QueueName, TicketId);
}
void UEPFAsyncGetTicketStatus::HandleComplete(const FEPFResult& Result, const FEPFMatchmakingResult& MatchmakingResult)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get())) S->OnTicketStatusReceived.RemoveDynamic(this, &UEPFAsyncGetTicketStatus::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(MatchmakingResult) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get ticket status")));
	SetReadyToDestroy();
}

// ── Cancel Matchmaking ───────────────────────────────────────────────────────

UEPFAsyncCancelMatchmaking* UEPFAsyncCancelMatchmaking::CancelMatchmaking(UObject* WorldContext, const FString& QueueName, const FString& TicketId)
{
	auto* A = NewObject<UEPFAsyncCancelMatchmaking>(); A->QueueName = QueueName; A->TicketId = TicketId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncCancelMatchmaking::Activate()
{
	if (QueueName.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Queue name cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Matchmaking subsystem not available")); SetReadyToDestroy(); return; }
	S->OnMatchmakingCanceled.AddDynamic(this, &UEPFAsyncCancelMatchmaking::HandleComplete);
	TicketId.IsEmpty() ? S->CancelAllTickets(QueueName) : S->CancelTicket(QueueName, TicketId);
}
void UEPFAsyncCancelMatchmaking::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMatchmakingSubsystem>(WorldContext.Get())) S->OnMatchmakingCanceled.RemoveDynamic(this, &UEPFAsyncCancelMatchmaking::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to cancel matchmaking")));
	SetReadyToDestroy();
}

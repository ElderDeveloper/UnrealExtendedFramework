// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncMessages.h"
#include "Engine/GameInstance.h"

// ── Fetch Messages ───────────────────────────────────────────────────────────

UEPFAsyncFetchMessages* UEPFAsyncFetchMessages::FetchMessages(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncFetchMessages>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncFetchMessages::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Messages subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnMessagesReceived.AddDynamic(this, &UEPFAsyncFetchMessages::HandleComplete);
	S->FetchMessages();
}
void UEPFAsyncFetchMessages::HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerMessage>& Messages)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get())) S->OnMessagesReceived.RemoveDynamic(this, &UEPFAsyncFetchMessages::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Messages) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to fetch messages")));
	SetReadyToDestroy();
}

// ── Mark As Read ─────────────────────────────────────────────────────────────

UEPFAsyncMarkMessageRead* UEPFAsyncMarkMessageRead::MarkMessageRead(UObject* WorldContext, const FString& MessageId)
{
	auto* A = NewObject<UEPFAsyncMarkMessageRead>(); A->MessageId = MessageId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncMarkMessageRead::Activate()
{
	if (MessageId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("MessageId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Messages subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnMessageMarkedRead.AddDynamic(this, &UEPFAsyncMarkMessageRead::HandleComplete);
	S->MarkAsRead(MessageId);
}
void UEPFAsyncMarkMessageRead::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get())) S->OnMessageMarkedRead.RemoveDynamic(this, &UEPFAsyncMarkMessageRead::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to mark message as read")));
	SetReadyToDestroy();
}

// ── Delete Message ───────────────────────────────────────────────────────────

UEPFAsyncDeleteMessage* UEPFAsyncDeleteMessage::DeleteMessage(UObject* WorldContext, const FString& MessageId)
{
	auto* A = NewObject<UEPFAsyncDeleteMessage>(); A->MessageId = MessageId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncDeleteMessage::Activate()
{
	if (MessageId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("MessageId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Messages subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnMessageMarkedRead.AddDynamic(this, &UEPFAsyncDeleteMessage::HandleComplete);
	S->DeleteMessage(MessageId);
}
void UEPFAsyncDeleteMessage::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFMessagesSubsystem>(WorldContext.Get())) S->OnMessageMarkedRead.RemoveDynamic(this, &UEPFAsyncDeleteMessage::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete message")));
	SetReadyToDestroy();
}

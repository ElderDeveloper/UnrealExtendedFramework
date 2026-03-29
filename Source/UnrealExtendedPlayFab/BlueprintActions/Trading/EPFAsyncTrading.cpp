// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncTrading.h"
#include "Engine/GameInstance.h"

// ── Open Trade ───────────────────────────────────────────────────────────────
UEPFAsyncOpenTrade* UEPFAsyncOpenTrade::OpenTrade(UObject* WorldContext, const TArray<FString>& OfferedItemIds, const TArray<FString>& RequestedCatalogItemIds, const TArray<FString>& AllowedPlayerIds)
{
	auto* A = NewObject<UEPFAsyncOpenTrade>(); A->Offered = OfferedItemIds; A->Requested = RequestedCatalogItemIds; A->Allowed = AllowedPlayerIds; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncOpenTrade::Activate()
{
	if (Offered.Num() == 0) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Must offer at least one item"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Trading subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnTradeOpened.AddDynamic(this, &UEPFAsyncOpenTrade::HandleComplete);
	S->OpenTrade(Offered, Requested, Allowed);
}
void UEPFAsyncOpenTrade::HandleComplete(const FEPFResult& Result, const FString& TradeId)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get())) S->OnTradeOpened.RemoveDynamic(this, &UEPFAsyncOpenTrade::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(TradeId) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to open trade")));
	SetReadyToDestroy();
}

// ── Accept Trade ─────────────────────────────────────────────────────────────
UEPFAsyncAcceptTrade* UEPFAsyncAcceptTrade::AcceptTrade(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId, const TArray<FString>& AcceptedItemIds)
{
	auto* A = NewObject<UEPFAsyncAcceptTrade>(); A->TradeId = TradeId; A->OfferingId = OfferingPlayerId; A->Accepted = AcceptedItemIds; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncAcceptTrade::Activate()
{
	if (TradeId.IsEmpty() || OfferingId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("TradeId and OfferingPlayerId required"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Trading subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnTradeAccepted.AddDynamic(this, &UEPFAsyncAcceptTrade::HandleComplete);
	S->AcceptTrade(TradeId, OfferingId, Accepted);
}
void UEPFAsyncAcceptTrade::HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get())) S->OnTradeAccepted.RemoveDynamic(this, &UEPFAsyncAcceptTrade::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Trade) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to accept trade")));
	SetReadyToDestroy();
}

// ── Cancel Trade ─────────────────────────────────────────────────────────────
UEPFAsyncCancelTrade* UEPFAsyncCancelTrade::CancelTrade(UObject* WorldContext, const FString& TradeId)
{
	auto* A = NewObject<UEPFAsyncCancelTrade>(); A->TradeId = TradeId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncCancelTrade::Activate()
{
	if (TradeId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("TradeId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Trading subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnTradeCanceled.AddDynamic(this, &UEPFAsyncCancelTrade::HandleComplete);
	S->CancelTrade(TradeId);
}
void UEPFAsyncCancelTrade::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get())) S->OnTradeCanceled.RemoveDynamic(this, &UEPFAsyncCancelTrade::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to cancel trade")));
	SetReadyToDestroy();
}

// ── Get Trades ───────────────────────────────────────────────────────────────
UEPFAsyncGetTrades* UEPFAsyncGetTrades::GetPlayerTrades(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncGetTrades>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetTrades::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Trading subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnTradesReceived.AddDynamic(this, &UEPFAsyncGetTrades::HandleComplete);
	S->GetPlayerTrades();
}
void UEPFAsyncGetTrades::HandleComplete(const FEPFResult& Result, const TArray<FEPFTradeInfo>& Trades)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get())) S->OnTradesReceived.RemoveDynamic(this, &UEPFAsyncGetTrades::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Trades) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get trades")));
	SetReadyToDestroy();
}

// ── Get Trade Status ─────────────────────────────────────────────────────────
UEPFAsyncGetTradeStatus* UEPFAsyncGetTradeStatus::GetTradeStatus(UObject* WorldContext, const FString& TradeId, const FString& OfferingPlayerId)
{
	auto* A = NewObject<UEPFAsyncGetTradeStatus>(); A->TradeId = TradeId; A->OfferingId = OfferingPlayerId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetTradeStatus::Activate()
{
	if (TradeId.IsEmpty() || OfferingId.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("TradeId and OfferingPlayerId required"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Trading subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnTradeInfoReceived.AddDynamic(this, &UEPFAsyncGetTradeStatus::HandleComplete);
	S->GetTradeStatus(TradeId, OfferingId);
}
void UEPFAsyncGetTradeStatus::HandleComplete(const FEPFResult& Result, const FEPFTradeInfo& Trade)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFTradingSubsystem>(WorldContext.Get())) S->OnTradeInfoReceived.RemoveDynamic(this, &UEPFAsyncGetTradeStatus::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Trade) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get trade status")));
	SetReadyToDestroy();
}
